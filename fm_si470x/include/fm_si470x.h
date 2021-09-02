/*
 * Copyright (c) 2021 Valentin Milea <valentin.milea@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _SI470X_H_
#define _SI470X_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \file fm_si470x.h
 *
 * \brief Library for Si4702 / Si4703 FM radio chips.
 * 
 * Reference:
 * - Si4702/03-C19 - Broadcast FM Radio Tuner for Portable Applications (Rev. 1.1 7/09)
 * - AN230 - Si4700/01/02/03 Programming Guide (Rev. 0.9 6/09)
 */

/**
 * \brief Maximum volume.
 */
static const uint8_t FM_MAX_VOLUME = 15;

typedef struct i2c_inst i2c_inst_t;

/**
 * \brief FM frequency bands.
 */
typedef enum fm_band_t
{
    FM_BAND_COMMON, /**< 87.5-108 MHz */
    FM_BAND_JAPAN_WIDE, /**< 76-108 MHz */
    FM_BAND_JAPAN, /**< 76-90 MHz */
} fm_band_t;

/**
 * \brief How far apart FM channels are in kHz.
 */
typedef enum fm_channel_spacing_t
{
    FM_CHANNEL_SPACING_200, /**< For Americas, South Korea, Australia. */
    FM_CHANNEL_SPACING_100, /**< For Europe, Japan. */
    FM_CHANNEL_SPACING_50, /**< For Italy. */
} fm_channel_spacing_t;

/**
 * \brief FM de-emphasis in µs.
 */
typedef enum fm_deemphasis_t
{
    FM_DEEMPHASIS_75, /**< For Americas, South Korea. */
    FM_DEEMPHASIS_50, /**< For Europe, Japan, Australia. */
} fm_deemphasis_t;

/**
 * \brief FM regional settings.
 */
typedef struct fm_config_t
{
    fm_band_t band;
    fm_channel_spacing_t channel_spacing;
    fm_deemphasis_t deemphasis;
} fm_config_t;

static inline fm_config_t fm_config_usa() {
    return (fm_config_t){FM_BAND_COMMON, FM_CHANNEL_SPACING_200, FM_DEEMPHASIS_75};
}

static inline fm_config_t fm_config_europe() {
    return (fm_config_t){FM_BAND_COMMON, FM_CHANNEL_SPACING_100, FM_DEEMPHASIS_50};
}

static inline fm_config_t fm_config_japan_wide() {
    return (fm_config_t){FM_BAND_JAPAN_WIDE, FM_CHANNEL_SPACING_100, FM_DEEMPHASIS_50};
}

static inline fm_config_t fm_config_japan() {
    return (fm_config_t){FM_BAND_JAPAN, FM_CHANNEL_SPACING_100, FM_DEEMPHASIS_50};
}

/**
 * \brief Frequency range in MHz corresponding to an fm_band_t.
 */
typedef struct fm_frequency_range_t
{
    float bottom;
    float top;
    float spacing;
} fm_frequency_range_t;

/**
 * \brief Sensitivity settings used during seek.
 * 
 * See AN230: Seek Settings Recommendations.
 */
typedef enum fm_seek_sensitivity_t
{
    FM_SEEK_SENSITIVITY_STRONG_ONLY, /**< Finds only strong stations. */
    FM_SEEK_SENSITIVITY_RECOMMENDED, /**< Default sensitivity. */
    FM_SEEK_SENSITIVITY_MORE, /**< Finds stations with lower RSSI level. */
    FM_SEEK_SENSITIVITY_MOST, /**< Finds most valid stations. */
} fm_seek_sensitivity_t;

/**
 * \brief Direction of seek.
 */
typedef enum fm_seek_direction_t
{
    FM_SEEK_DOWN,
    FM_SEEK_UP,
} fm_seek_direction_t;

/**
 * \brief Volume reduction when not tuned to a station, in dB. 
 */
typedef enum fm_softmute_attenuation_t
{
    FM_SOFTMUTE_ATTENUATION_16,
    FM_SOFTMUTE_ATTENUATION_14,
    FM_SOFTMUTE_ATTENUATION_12,
    FM_SOFTMUTE_ATTENUATION_10,
} fm_softmute_attenuation_t;

/**
 * \brief How quickly volume attenuation is applied.
 */
typedef enum fm_softmute_rate_t
{
    FM_SOFTMUTE_FASTEST,
    FM_SOFTMUTE_FAST,
    FM_SOFTMUTE_SLOW,
    FM_SOFTMUTE_SLOWEST,
} fm_softmute_rate_t;

/**
 * \brief Radio chip information.
 * 
 * See datasheet: Register 01h.
 * 
 */
typedef struct si_chip_id_t
{
    uint8_t firmware; /**< Firmware version. */
    uint8_t dev; /**< Device ID. */
    uint8_t rev; /**< Chip revision. */
} si_chip_id_t;

/**
 * \brief Progress of an asynchronous task.
 */
typedef struct fm_async_progress_t
{
    bool done; /**< True if the task has completed or failed. */
    int result; /**< If done, stores the return value. Negative on error. */
} fm_async_progress_t;

struct si470x_t;

// private
typedef fm_async_progress_t (*fm_async_task_t)(struct si470x_t *radio, bool cancel);

// private
typedef struct fm_async_state_t
{
    fm_async_task_t task;
    uint8_t state;
    uint64_t resume_time;
} fm_async_state_t;

/**
 * \brief FM radio.
 */
typedef struct si470x_t
{
    i2c_inst_t *i2c_inst;
    uint8_t reset_pin;
    uint8_t sdio_pin;
    uint8_t sclk_pin;
    bool enable_pull_ups;
    fm_config_t config;
    fm_seek_sensitivity_t seek_sensitivity;
    float frequency;
    bool mute;
    bool softmute;
    fm_softmute_rate_t softmute_rate;
    fm_softmute_attenuation_t softmute_attenuation;
    bool mono;
    bool volext;
    uint8_t volume;
    uint16_t regs[16];
    fm_async_state_t async;
} si470x_t;

/**
 * \brief Initialize the radio state.
 * 
 * @param radio Radio handle.
 * @param i2c_inst I2C instance.
 * @param reset_pin Reset pin.
 * @param sdio_pin SDIO pin.
 * @param sclk_pin SCLK pin.
 * @param enable_pull_ups Whether to enable the internal pull-ups on I2C pads.
 */
void fm_init(si470x_t *radio, i2c_inst_t *i2c_inst, uint8_t reset_pin, uint8_t sdio_pin, uint8_t sclk_pin, bool enable_pull_ups);

/**
 * \brief Power up the radio chip.
 * 
 * If waking after power down, the previous state is restored.
 * 
 * @param radio Radio handle.
 * @param config FM regional settings.
 */
void fm_power_up(si470x_t *radio, fm_config_t config);

/**
 * \brief Power down the radio chip.
 * 
 * Puts the chip in a low power state while maintaining register configuration.
 * 
 * @param radio Radio handle.
 */
void fm_power_down(si470x_t *radio);

/**
 * \brief Check if the radio is powered up.
 * 
 * @param radio Radio handle.
 */
bool fm_is_powered_up(si470x_t *radio);

/**
 * \brief Get radio chip information.
 * 
 * @param radio Radio handle.
 */
si_chip_id_t fm_get_chip_id(si470x_t *radio);

/**
 * \brief Get the FM regional configuration.
 * 
 * @param radio Radio handle.
 */
fm_config_t fm_get_config(si470x_t *radio);

/**
 * \brief Get the frequency range for the configured FM band.
 * 
 * @param radio Radio handle.
 */
fm_frequency_range_t fm_get_frequency_range(si470x_t *radio);

/**
 * \brief Get the current FM frequency.
 * 
 * @param radio Radio handle.
 * @return FM frequency in MHz.
 */
float fm_get_frequency(si470x_t *radio);

/**
 * \brief Set the current FM frequency.
 * 
 * Tuning to a new frequency may take up to 60ms. To avoid blocking, use fm_set_frequency_async().
 * 
 * @param radio Radio handle.
 * @param frequency FM frequency in MHz.
 */
void fm_set_frequency_blocking(si470x_t *radio, float frequency);

/**
 * \brief Set the current FM frequency without blocking.
 *
 * If canceled before completion, the tuner is stopped without restoring the original frequency.
 * 
 * May not be called while another async task is running.
 * 
 * @param radio Radio handle.
 * @param frequency FM frequency in MHz.
 *
 * @sa fm_async_task_tick(), fm_async_task_cancel()
 */
void fm_set_frequency_async(si470x_t *radio, float frequency);

/**
 * \brief Get seek sensitivity.
 * 
 * The default is FM_SEEK_SENSITIVITY_RECOMMENDED.
 * 
 * @param radio Radio handle.
 */
fm_seek_sensitivity_t fm_get_seek_sensitivity(si470x_t *radio);

/**
 * \brief Set seek sensitivity.
 * 
 * @param radio Radio handle.
 * @param seek_sensitivity Seek sensitivity.
 */
void fm_set_seek_sensitivity(si470x_t *radio, fm_seek_sensitivity_t seek_sensitivity);

/**
 * \brief Seek the next station.
 * 
 * Seeks in the given direction until a station is detected. If the frequency range limit
 * is reached, it will wrap to the other end.
 * 
 * Seeking may take a few seconds depending on how far the next station is. To avoid blocking,
 * use fm_seek_async().
 * 
 * @param radio Radio handle.
 * @param direction Seek direction.
 * @return true A strong enough station was found.
 * @return false No station was found.
 */
bool fm_seek_blocking(si470x_t *radio, fm_seek_direction_t direction);

/**
 * \brief Seek the next radio station without blocking.
 * 
 * Seeks in the given direction until a station is detected. If the frequency range limit
 * is reached, it will wrap to the other end.
 * 
 * fm_get_frequency() may be used during the seek operation to monitor progress.
 * 
 * If canceled before completion, the tuner is stopped without restoring the original frequency.
 * 
 * May not be called while another async task is running.
 * 
 * @param radio Radio handle.
 * @param direction Seek direction.
 * 
 * @sa fm_async_task_tick(), fm_async_task_cancel()
 */
void fm_seek_async(si470x_t *radio, fm_seek_direction_t direction);

/**
 * \brief Check whether audio is muted.
 * 
 * The audio is muted by default. After power up, you should disable mute and set the desired volume.
 * 
 * @param radio Radio handle.
 */
bool fm_get_mute(si470x_t *radio);

/**
 * \brief Set whether audio is muted.
 * 
 * @param radio Radio handle.
 * @param mute Mute value.
 */
void fm_set_mute(si470x_t *radio, bool mute);

/**
 * \brief Check whether softmute is enabled.
 * 
 * Softmute is enabled by default.
 * 
 * @param radio Radio handle.
 */
bool fm_get_softmute(si470x_t *radio);

/**
 * \brief Set whether softmute is enabled.
 * 
 * Softmute reduces noise when the FM signal is too weak.
 * 
 * @param radio Radio handle.
 * @param softmute Softmute value.
 */
void fm_set_softmute(si470x_t *radio, bool softmute);

/**
 * \brief Get softmute rate.
 * 
 * The default is FM_SOFTMUTE_FASTEST.
 * 
 * @param radio Radio handle.
 */
fm_softmute_rate_t fm_get_softmute_rate(si470x_t *radio);

/**
 * \brief Set softmute rate.
 *
 * @param radio Radio handle.
 * @param softmute_rate Softmute rate value.
 */
void fm_set_softmute_rate(si470x_t *radio, fm_softmute_rate_t softmute_rate);

/**
 * \brief Get softmute attenuation.
 * 
 * The default is FM_SOFTMUTE_ATTENUATION_16.
 * 
 * @param radio Radio handle.
 */
fm_softmute_attenuation_t fm_get_softmute_attenuation(si470x_t *radio);

/**
 * \brief Set softmute attenuation.
 * 
 * @param radio Radio handle.
 * @param softmute_rate Softmute attenuation value.
 */
void fm_set_softmute_attenuation(si470x_t *radio, fm_softmute_attenuation_t softmute_attenuation);

/**
 * \brief Check whether mono output is enabled.
 * 
 * The default is stereo output.
 * 
 * @param radio Radio handle.
 */
bool fm_get_mono(si470x_t *radio);

/**
 * \brief Set whether mono output is enabled.
 * 
 * Forcing mono output may improve reception of weak stations.
 * 
 * @param radio Radio handle.
 * @param mono Mono value.
 */
void fm_set_mono(si470x_t *radio, bool mono);

/**
 * \brief Get audio volume.
 * 
 * The default volume is 0.
 * 
 * @param radio Radio handle.
 * @return uint8_t Volume in range 0-15.
 */
uint8_t fm_get_volume(si470x_t *radio);

/**
 * \brief Check whether extended volume range is active.
 *
 * Extended volume range is disabled by default.
 *
 * @param radio Radio handle.
 */
bool fm_get_volext(si470x_t *radio);

/**
 * \brief Set audio volume.
 * 
 * Values above 15 are clamped. Setting volume to 0 will effectively mute audio.
 * 
 * volext allows for finer control at low volume.
 * 
 * @param radio Radio handle.
 * @param volume Volume value in range 0-15.
 * @param volext If true, volume is reduced by 30 dbFS.
 */
void fm_set_volume(si470x_t *radio, uint8_t volume, bool volext);

/**
 * \brief Get current FM signal strength.
 * 
 * @param radio Radio handle.
 * @return RSSI level, up to 75dBµV.
 */
uint8_t fm_get_rssi(si470x_t *radio);

/**
 * \brief Check whether audio output is stereo.
 * 
 * If mono output is forced, this always returns false. Otherwise, it indicates
 * the current station is stereo. Note that the radio chip may fall back to mono
 * when the reception is weak.
 * 
 * @param radio Radio handle.
 */
bool fm_get_stereo_indicator(si470x_t *radio);

/**
 * \brief Check whether the radio chip supports RDS.
 * 
 * @param Radio handle. 
 * @return true For Si4703.
 * @return false For Si4702.
 */
bool fm_is_rds_supported(si470x_t *radio);

/**
 * \brief Read an RDS data group.
 * 
 * Should be called every 40ms.
 * 
 * @param radio Radio handle.
 * @param blocks Output buffer.
 * @return true RDS data ready, blocks filled.
 * @return false RDS data not yet ready.
 */
bool fm_read_rds_group(si470x_t *radio, uint16_t *blocks);

/**
 * \brief Update the current asynchronous task.
 * 
 * Long running operations like seeking can be run asynchronously to free up the
 * CPU for other work. After calling fm_xxx_async(), the tick function must be
 * called periodically until the task is done. The tick interval is up to the user
 * (every 40ms should be fine).
 * 
 * @param radio Radio handle.
 * @return Task status.
 */
fm_async_progress_t fm_async_task_tick(si470x_t *radio);

/**
 * \brief Abort the current asynchronous task.
 * 
 * @param radio Radio handle.
 */
void fm_async_task_cancel(si470x_t *radio);

#ifdef __cplusplus
}
#endif

#endif // _SI470X_H_
