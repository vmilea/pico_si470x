/*
 * Copyright (c) 2021 Valentin Milea <valentin.milea@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "fm_si470x_regs.h"
#include <fm_si470x.h>
#include <hardware/i2c.h>
#include <pico/stdlib.h>
#include <math.h>
#include <string.h>

#define DEV_SI4702 0b0001
#define DEV_SI4703 0b1001

static const uint TUNE_POLL_INTERVAL_MS = 20;
static const uint SEEK_POLL_INTERVAL_MS = 200; // relatively large, to reduce electrical interference from I2C

//
// misc
//

static fm_frequency_range_t fm_frequency_range(fm_band_t band, fm_channel_spacing_t channel_spacing) {
    fm_frequency_range_t range;
    switch (band) {
    case FM_BAND_COMMON:
        range.bottom = 87.5f;
        range.top = 108.0f;
        break;
    case FM_BAND_JAPAN_WIDE:
        range.bottom = 76.0f;
        range.top = 108.0f;
        break;
    default: // FM_BAND_JAPAN
        range.bottom = 76.0f;
        range.top = 90.0f;
        break;
    }
    switch (channel_spacing) {
    case FM_CHANNEL_SPACING_200:
        range.spacing = 0.2f;
        break;
    case FM_CHANNEL_SPACING_100:
        range.spacing = 0.1f;
        break;
    default: // FM_CHANNEL_SPACING_50
        range.spacing = 0.05f;
        break;
    }
    return range;
}

static float fm_channel_to_frequency(uint16_t channel, fm_frequency_range_t range) {
    return channel * range.spacing + range.bottom;
}

static uint16_t fm_frequency_to_channel(float frequency, fm_frequency_range_t range) {
    return (uint16_t)roundf((frequency - range.bottom) / range.spacing);
}

//
// register access
//

static bool fm_read_registers(i2c_inst_t *i2c_inst, uint16_t *regs, size_t n) {
    assert(n <= 16); // registers 0xA..0xF, followed by 0x0..0x9

    uint8_t buf[32];
    size_t data_size = n * sizeof(uint16_t);
    int result = i2c_read_blocking(i2c_inst, SI4703_ADDR, buf, data_size, false);
    if (result != (int)data_size) {
        return false; // failed
    }

    uint16_t *p = regs + 0xA;
    for (size_t i = 0; i < data_size;) {
        uint16_t reg = buf[i++] << 8; // hi
        reg |= buf[i++]; // lo
        *p = reg;
        if (++p == regs + 16) {
            p = regs; // loop back to register 0
        }
    }
    return true;
}

static bool fm_read_registers_up_to(i2c_inst_t *i2c_inst, uint16_t *regs, uint8_t reg_index) {
    // read order: 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9
    assert(reg_index < 16);

    size_t n;
    if (reg_index < 0xA) {
        n = reg_index + 7;
    } else {
        n = reg_index - 9;
    }
    return fm_read_registers(i2c_inst, regs, n);
}

static bool fm_write_registers(i2c_inst_t *i2c_inst, uint16_t *regs, size_t n) {
    assert(n <= 14); // register 0x2..0xF

    uint8_t buf[28];
    size_t data_size = n * sizeof(uint16_t);
    uint16_t *p = regs + 0x2;
    for (size_t i = 0; i < data_size;) {
        uint16_t reg = *p++;
        buf[i++] = reg >> 8; // hi
        buf[i++] = reg & 0xFF; // lo
    }
    int result = i2c_write_blocking(i2c_inst, SI4703_ADDR, buf, data_size, false);
    return result == (int)data_size;
}

static bool fm_write_registers_up_to(i2c_inst_t *i2c_inst, uint16_t *regs, uint8_t reg_index) {
    // write order: 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF
    assert(0x2 <= reg_index && reg_index <= 0xF);

    size_t n = reg_index - 1;
    return fm_write_registers(i2c_inst, regs, n);
}

#define fm_set_bit(reg, bit, value) \
    if (value) {                    \
        reg |= bit##_BIT;           \
    } else {                        \
        reg &= ~bit##_BIT;          \
    }

#define fm_get_bit(reg, bit) \
    (((reg)&bit##_BIT) != 0)

#define fm_set_bits(reg, bits, value) \
    reg &= ~bits##_BITS;              \
    reg |= (value) << bits##_LSB;

#define fm_get_bits(reg, bits) \
    (((reg)&bits##_BITS) >> bits##_LSB)

static void fm_set_seek_sensitivity_bits(uint16_t *regs, fm_seek_sensitivity_t seek_sensitivity) {
    uint8_t seekth;
    uint8_t sksnr;
    uint8_t skcnt;
    switch (seek_sensitivity) {
    case FM_SEEK_SENSITIVITY_STRONG_ONLY:
        seekth = 0xC;
        sksnr = 0x7;
        skcnt = 0xF;
        break;
    case FM_SEEK_SENSITIVITY_RECOMMENDED:
        seekth = 0x19;
        sksnr = 0x4;
        skcnt = 0x8;
        break;
    case FM_SEEK_SENSITIVITY_MORE:
        seekth = 0xC;
        sksnr = 0x4;
        skcnt = 0x8;
        break;
    default: // FM_SEEK_SENSITIVITY_MOST
        seekth = 0x0;
        sksnr = 0x4;
        skcnt = 0xF;
        break;
    }
    fm_set_bits(regs[SYSCONFIG2], SEEKTH, seekth);
    fm_set_bits(regs[SYSCONFIG3], SKSNR, sksnr);
    fm_set_bits(regs[SYSCONFIG3], SKCNT, skcnt);
}

//
// public interface
//

void fm_init(si470x_t *radio, i2c_inst_t *i2c_inst, uint8_t reset_pin, uint8_t sdio_pin, uint8_t sclk_pin, bool enable_pull_ups) {
    memset(radio, 0, sizeof(si470x_t));

    radio->i2c_inst = i2c_inst;
    radio->reset_pin = reset_pin;
    radio->sdio_pin = sdio_pin;
    radio->sclk_pin = sclk_pin;
    radio->enable_pull_ups = enable_pull_ups;
    radio->seek_sensitivity = FM_SEEK_SENSITIVITY_RECOMMENDED;
    radio->mute = true;
    radio->softmute = true;
}

void fm_power_up(si470x_t *radio, fm_config_t config) {
    assert(!fm_is_powered_up(radio));

    i2c_inst_t *i2c_inst = radio->i2c_inst;
    uint16_t *regs = radio->regs;

    if (fm_get_bit(regs[POWERCFG], DISABLE)) {
        // waking up after power down, assume registers have been preserved
        if (memcmp(&radio->config, &config, sizeof(config)) == 0) {
            fm_set_bit(regs[POWERCFG], ENABLE, true);
            fm_set_bit(regs[POWERCFG], DISABLE, false);
            fm_set_bit(regs[POWERCFG], DMUTE, !radio->mute);
            fm_write_registers_up_to(i2c_inst, regs, POWERCFG);
            sleep_ms(110); // wait for device powerup

            // restore RDS
            if (fm_is_rds_supported(radio)) {
                fm_set_bit(regs[SYSCONFIG1], RDS, true);
                fm_write_registers_up_to(i2c_inst, regs, SYSCONFIG1);
            }
            return;
        }
    }

    radio->config = config;

    gpio_init(radio->reset_pin);
    gpio_set_dir(radio->reset_pin, GPIO_OUT);
    gpio_init(radio->sdio_pin);
    gpio_set_dir(radio->sdio_pin, GPIO_OUT);

    // see AN230 - Powerup Configuration Sequence
    gpio_put(radio->sdio_pin, false);
    gpio_put(radio->reset_pin, false);
    sleep_ms(5);
    gpio_put(radio->reset_pin, true);
    sleep_ms(5);

    gpio_set_function(radio->sdio_pin, GPIO_FUNC_I2C);
    gpio_set_function(radio->sclk_pin, GPIO_FUNC_I2C);
    if (radio->enable_pull_ups) {
        gpio_pull_up(radio->sdio_pin);
        gpio_pull_up(radio->sclk_pin);
    }

    if (!fm_read_registers(i2c_inst, regs, 16)) {
        panic("FM - couldn't read from I2C bus, check your wiring");
    }
#ifndef NDEBUG
    uint16_t mfgid = fm_get_bits(regs[DEVICEID], MFGID);
    uint8_t pn = fm_get_bits(regs[DEVICEID], PN);
    assert(mfgid == 0x242); // manufacturer ID check
    assert(pn == 0x1); // part number check
#endif

    regs[TEST1] |= XOSCEN_BIT; // enable internal oscillator
    regs[RDSD] = 0; // Si4703-C19 errata - ensure RDSD register is zero
    fm_write_registers_up_to(i2c_inst, regs, RDSD);
    sleep_ms(500); // wait for oscillator to stabilize

    // enable
    regs[POWERCFG] = ENABLE_BIT;
    fm_write_registers_up_to(i2c_inst, regs, POWERCFG);
    sleep_ms(110); // wait for device powerup

    fm_read_registers(i2c_inst, regs, 16);
#ifndef NDEBUG
    uint8_t dev = fm_get_bits(regs[CHIPID], DEV);
    assert(dev == DEV_SI4702 || dev == DEV_SI4703);
    // Si4700 / Si4701 lack the SYSCONFIG3 and TEST1 settings. They should work with minor tweaks.
#endif

    // setup
    fm_set_bit(regs[POWERCFG], MONO, radio->mono);
    fm_set_bit(regs[POWERCFG], DMUTE, !radio->mute);
    fm_set_bit(regs[POWERCFG], DSMUTE, !radio->softmute);
    if (fm_is_rds_supported(radio)) {
        fm_set_bit(regs[SYSCONFIG1], RDS, true);
    }
    fm_set_bit(regs[SYSCONFIG1], DE, config.deemphasis == FM_DEEMPHASIS_50);
    fm_set_bits(regs[SYSCONFIG2], VOLUME, radio->volume);
    fm_set_bits(regs[SYSCONFIG2], BAND, config.band);
    fm_set_bits(regs[SYSCONFIG2], SPACE, config.channel_spacing);
    fm_set_bit(regs[SYSCONFIG3], VOLEXT, radio->volext);
    fm_set_bits(regs[SYSCONFIG3], SMUTEA, radio->softmute_attenuation);
    fm_set_bits(regs[SYSCONFIG3], SMUTER, radio->softmute_rate);
    fm_set_seek_sensitivity_bits(regs, radio->seek_sensitivity);
    fm_write_registers_up_to(i2c_inst, regs, SYSCONFIG3);

    if (radio->frequency != 0.0f) {
        float frequency = radio->frequency;
        radio->frequency = 0.0f;
        fm_set_frequency_blocking(radio, frequency);
    }
}

void fm_power_down(si470x_t *radio) {
    assert(fm_is_powered_up(radio));

    if (radio->async.task != NULL) {
        fm_async_task_cancel(radio);
    }

    // TODO: consider disabling the internal oscillator

    uint16_t *regs = radio->regs;
    if (fm_is_rds_supported(radio)) {
        // on Si4703 it's recommended to disable RDS before powering down (AN230 - Hardware Powerdown)
        fm_set_bit(regs[SYSCONFIG1], RDS, false);
        fm_write_registers_up_to(radio->i2c_inst, regs, SYSCONFIG1);
    }

    fm_set_bit(regs[POWERCFG], DMUTE, false);
    fm_set_bit(regs[POWERCFG], DISABLE, true);
    fm_write_registers_up_to(radio->i2c_inst, regs, POWERCFG);

    // update shadow register for internal bookkeeping
    fm_set_bit(regs[POWERCFG], ENABLE, false);
}

bool fm_is_powered_up(si470x_t *radio) {
    return fm_get_bit(radio->regs[POWERCFG], ENABLE);
}

si_chip_id_t fm_get_chip_id(si470x_t *radio) {
    uint16_t *regs = radio->regs;
    return (si_chip_id_t){
        .firmware = fm_get_bits(regs[CHIPID], FIRMWARE),
        .dev = fm_get_bits(regs[CHIPID], DEV),
        .rev = fm_get_bits(regs[CHIPID], REV),
    };
}

fm_config_t fm_get_config(si470x_t *radio) {
    return radio->config;
}

fm_frequency_range_t fm_get_frequency_range(si470x_t *radio) {
    return fm_frequency_range(radio->config.band, radio->config.channel_spacing);
}

float fm_get_frequency(si470x_t *radio) {
    return radio->frequency;
}

void fm_set_frequency_blocking(si470x_t *radio, float frequency) {
    assert(fm_is_powered_up(radio));
    assert(radio->async.task == NULL); // disallowed during async task

    if (radio->frequency == frequency) {
        return;
    }
    fm_set_frequency_async(radio, frequency);
    fm_async_progress_t progress;
    do {
        sleep_ms(TUNE_POLL_INTERVAL_MS);
        progress = fm_async_task_tick(radio);
    } while (!progress.done);
}

static fm_async_progress_t fm_set_frequency_async_task(si470x_t *radio, bool cancel) {
    assert(radio->async.task == &fm_set_frequency_async_task);
    assert(radio->async.state == 1);

    i2c_inst_t *i2c_inst = radio->i2c_inst;
    uint16_t *regs = radio->regs;
    int result = 0;
    if (cancel) {
        result = -1;
    } else {
        fm_read_registers_up_to(i2c_inst, regs, STATUSRSSI);
        if (!fm_get_bit(regs[STATUSRSSI], STC)) {
            radio->async.resume_time = time_us_64() + TUNE_POLL_INTERVAL_MS * 1000;
            return (fm_async_progress_t){.done = false};
        }
    }

    // clear tune bit
    fm_set_bit(regs[CHANNEL], TUNE, false);
    fm_write_registers_up_to(i2c_inst, regs, CHANNEL);
    // wait until STC bit cleared; shouldn't take longer than 1.5ms
    do {
        fm_read_registers_up_to(i2c_inst, regs, STATUSRSSI);
    } while (fm_get_bit(regs[STATUSRSSI], STC));

    fm_read_registers_up_to(i2c_inst, regs, READCHAN);
    uint16_t channel = fm_get_bits(regs[READCHAN], READCHAN);
    radio->frequency = fm_channel_to_frequency(channel, fm_get_frequency_range(radio));
    return (fm_async_progress_t){.done = true, result};
}

void fm_set_frequency_async(si470x_t *radio, float frequency) {
    assert(fm_is_powered_up(radio));
    assert(radio->async.task == NULL); // disallowed during async task

    uint16_t channel = fm_frequency_to_channel(frequency, fm_get_frequency_range(radio));
    uint16_t *regs = radio->regs;
    // set channel and start tuning
    fm_set_bits(regs[CHANNEL], CHAN, channel);
    fm_set_bit(regs[CHANNEL], TUNE, true);
    fm_write_registers_up_to(radio->i2c_inst, regs, CHANNEL);

    radio->async.task = fm_set_frequency_async_task;
    radio->async.state = 1;
    radio->async.resume_time = time_us_64() + TUNE_POLL_INTERVAL_MS * 1000;
}

fm_seek_sensitivity_t fm_get_seek_sensitivity(si470x_t *radio) {
    return radio->seek_sensitivity;
}

void fm_set_seek_sensitivity(si470x_t *radio, fm_seek_sensitivity_t seek_sensitivity) {
    assert(fm_is_powered_up(radio));
    assert(radio->async.task == NULL); // disallowed during async task

    if (radio->seek_sensitivity == seek_sensitivity) {
        return;
    }
    uint16_t *regs = radio->regs;
    fm_set_seek_sensitivity_bits(regs, seek_sensitivity);
    fm_write_registers_up_to(radio->i2c_inst, regs, SYSCONFIG3);
    radio->seek_sensitivity = seek_sensitivity;
}

bool fm_seek_blocking(si470x_t *radio, fm_seek_direction_t direction) {
    assert(radio->async.task == NULL); // disallowed during async task

    fm_seek_async(radio, direction);
    fm_async_progress_t progress;
    do {
        sleep_ms(SEEK_POLL_INTERVAL_MS);
        progress = fm_async_task_tick(radio);
    } while (!progress.done);
    bool success = (progress.result == 0);
    return success;
}

static fm_async_progress_t fm_seek_async_task(si470x_t *radio, bool cancel) {
    assert(radio->async.task == &fm_seek_async_task);
    assert(radio->async.state == 1);

    i2c_inst_t *i2c_inst = radio->i2c_inst;
    uint16_t *regs = radio->regs;
    int result = 0;
    if (cancel) {
        result = -1;
    } else {
        fm_read_registers_up_to(i2c_inst, regs, READCHAN);
        if (!fm_get_bit(regs[STATUSRSSI], STC)) {
            uint16_t channel = fm_get_bits(regs[READCHAN], READCHAN);
            radio->frequency = fm_channel_to_frequency(channel, fm_get_frequency_range(radio));
            radio->async.resume_time = time_us_64() + SEEK_POLL_INTERVAL_MS * 1000;
            return (fm_async_progress_t){.done = false};
        }

        // seek done, check seek-failed / band-limit flag
        if (fm_get_bit(regs[STATUSRSSI], SFBL)) {
            result = -1;
        }
    }

    // clear seek bit
    fm_set_bit(regs[POWERCFG], SEEK, false);
    fm_write_registers_up_to(i2c_inst, regs, POWERCFG);
    // wait until STC bit cleared; shouldn't take longer than 1.5ms
    do {
        fm_read_registers_up_to(i2c_inst, regs, STATUSRSSI);
    } while (fm_get_bit(regs[STATUSRSSI], STC));

    fm_read_registers_up_to(i2c_inst, regs, READCHAN);
    uint16_t channel = fm_get_bits(regs[READCHAN], READCHAN);
    radio->frequency = fm_channel_to_frequency(channel, fm_get_frequency_range(radio));
    return (fm_async_progress_t){.done = true, result};
}

void fm_seek_async(si470x_t *radio, fm_seek_direction_t direction) {
    assert(fm_is_powered_up(radio));
    assert(radio->async.task == NULL); // disallowed during async task

    uint16_t *regs = radio->regs;
    fm_set_bit(regs[POWERCFG], SKMODE, false); // wrap mode
    fm_set_bit(regs[POWERCFG], SEEKUP, direction == FM_SEEK_UP);
    fm_set_bit(regs[POWERCFG], SEEK, true); // start seek
    fm_write_registers_up_to(radio->i2c_inst, regs, POWERCFG);

    radio->async.task = fm_seek_async_task;
    radio->async.state = 1;
    radio->async.resume_time = time_us_64() + SEEK_POLL_INTERVAL_MS * 1000;
}

bool fm_get_mute(si470x_t *radio) {
    return radio->mute;
}

void fm_set_mute(si470x_t *radio, bool mute) {
    assert(fm_is_powered_up(radio));
    assert(radio->async.task == NULL); // disallowed during async task

    if (radio->mute == mute) {
        return;
    }
    uint16_t *regs = radio->regs;
    fm_set_bit(regs[POWERCFG], DMUTE, !mute);
    fm_write_registers_up_to(radio->i2c_inst, regs, POWERCFG);
    radio->mute = mute;
}

bool fm_get_softmute(si470x_t *radio) {
    return radio->softmute;
}

void fm_set_softmute(si470x_t *radio, bool softmute) {
    assert(fm_is_powered_up(radio));
    assert(radio->async.task == NULL); // disallowed during async task

    if (radio->softmute == softmute) {
        return;
    }
    uint16_t *regs = radio->regs;
    fm_set_bit(regs[POWERCFG], DSMUTE, !softmute);
    fm_write_registers_up_to(radio->i2c_inst, regs, POWERCFG);
    radio->softmute = softmute;
}

fm_softmute_rate_t fm_get_softmute_rate(si470x_t *radio) {
    return radio->softmute_rate;
}

void fm_set_softmute_rate(si470x_t *radio, fm_softmute_rate_t softmute_rate) {
    assert(fm_is_powered_up(radio));
    assert(radio->async.task == NULL); // disallowed during async task

    if (radio->softmute_rate == softmute_rate) {
        return;
    }
    uint16_t *regs = radio->regs;
    fm_set_bits(regs[SYSCONFIG3], SMUTER, softmute_rate);
    fm_write_registers_up_to(radio->i2c_inst, regs, SYSCONFIG3);
    radio->softmute_rate = softmute_rate;
}

fm_softmute_attenuation_t fm_get_softmute_attenuation(si470x_t *radio) {
    return radio->softmute_attenuation;
}

void fm_set_softmute_attenuation(si470x_t *radio, fm_softmute_attenuation_t softmute_attenuation) {
    assert(fm_is_powered_up(radio));
    assert(radio->async.task == NULL); // disallowed during async task

    if (radio->softmute_attenuation == softmute_attenuation) {
        return;
    }
    uint16_t *regs = radio->regs;
    fm_set_bits(regs[SYSCONFIG3], SMUTEA, softmute_attenuation);
    fm_write_registers_up_to(radio->i2c_inst, regs, SYSCONFIG3);
    radio->softmute_attenuation = softmute_attenuation;
}

bool fm_get_mono(si470x_t *radio) {
    return radio->mono;
}

void fm_set_mono(si470x_t *radio, bool mono) {
    assert(fm_is_powered_up(radio));
    assert(radio->async.task == NULL); // disallowed during async task

    if (radio->mono == mono) {
        return;
    }
    uint16_t *regs = radio->regs;
    fm_set_bit(regs[POWERCFG], MONO, mono);
    fm_write_registers_up_to(radio->i2c_inst, regs, POWERCFG);
    radio->mono = mono;
}

uint8_t fm_get_volume(si470x_t *radio) {
    return radio->volume;
}

bool fm_get_volext(si470x_t *radio) {
    return radio->volext;
}

void fm_set_volume(si470x_t *radio, uint8_t volume, bool volext) {
    assert(fm_is_powered_up(radio));
    assert(radio->async.task == NULL); // disallowed during async task

    volume = MIN(volume, FM_MAX_VOLUME);
    if (radio->volume == volume && radio->volext == volext) {
        return;
    }
    uint16_t *regs = radio->regs;
    fm_set_bits(regs[SYSCONFIG2], VOLUME, volume);
    fm_set_bit(regs[SYSCONFIG3], VOLEXT, volext);
    fm_write_registers_up_to(radio->i2c_inst, regs, SYSCONFIG3);
    radio->volume = volume;
    radio->volext = volext;
}

uint8_t fm_get_rssi(si470x_t *radio) {
    assert(fm_is_powered_up(radio));

    uint16_t *regs = radio->regs;
    fm_read_registers_up_to(radio->i2c_inst, regs, STATUSRSSI);
    uint8_t rssi = (uint8_t)fm_get_bits(regs[STATUSRSSI], RSSI);
    return rssi;
}

bool fm_get_stereo_indicator(si470x_t *radio) {
    assert(fm_is_powered_up(radio));

    uint16_t *regs = radio->regs;
    fm_read_registers_up_to(radio->i2c_inst, regs, STATUSRSSI);
    bool stereo = fm_get_bit(regs[STATUSRSSI], ST);
    return stereo;
}

bool fm_is_rds_supported(si470x_t *radio) {
    uint8_t dev = fm_get_bits(radio->regs[CHIPID], DEV);
    return dev == DEV_SI4703;
}

bool fm_read_rds_group(si470x_t *radio, uint16_t *blocks) {
    assert(fm_is_powered_up(radio));
    assert(fm_is_rds_supported(radio));

    uint16_t *regs = radio->regs;
    fm_read_registers_up_to(radio->i2c_inst, regs, RDSD);
    bool rdsr = fm_get_bit(regs[STATUSRSSI], RDSR);
    if (!rdsr) {
        return false; // not ready
    }
    memcpy(blocks, regs + RDSA, 4 * sizeof(uint16_t));
    return true;
}

fm_async_progress_t fm_async_task_tick(si470x_t *radio) {
    assert(radio->async.task != NULL); // must have an async task running

    if (time_us_64() < radio->async.resume_time) {
        // skip until resume time
        return (fm_async_progress_t){.done = false};
    }
    fm_async_progress_t progress = radio->async.task(radio, false /* cancel */);
    if (progress.done) {
        radio->async = (fm_async_state_t){};
    }
    return progress;
}

void fm_async_task_cancel(si470x_t *radio) {
    assert(radio->async.task != NULL); // must have an async task running

    radio->async.task(radio, true /* cancel */);
    radio->async = (fm_async_state_t){};
}
