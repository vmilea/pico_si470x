/*
 * Copyright (c) 2021 Valentin Milea <valentin.milea@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _SI470X_REGS_H_
#define _SI470X_REGS_H_

#include <stdint.h>

// I2C address
#define SI4703_ADDR 0x10

// Register names
#define DEVICEID   0x0
#define CHIPID     0x1
#define POWERCFG   0x2
#define CHANNEL    0x3
#define SYSCONFIG1 0x4
#define SYSCONFIG2 0x5
#define SYSCONFIG3 0x6
#define TEST1      0x7
#define TEST2      0x8
#define BOOTCONFIG 0x9
#define STATUSRSSI 0xA
#define READCHAN   0xB
#define RDSA       0xC
#define RDSB       0xD
#define RDSC       0xE
#define RDSD       0xF

// Register 0x00 - DEVICEID
#define MFGID_LSB  0
#define MFGID_BITS (0xFFF << MFGID_LSB)
#define PN_LSB     12
#define PN_BITS    (0xF << PN_LSB)

// Register 0x01 - CHIPID
#define FIRMWARE_LSB  0
#define FIRMWARE_BITS (0x3F << FIRMWARE_LSB)
#define DEV_LSB       6
#define DEV_BITS      (0xF << DEV_LSB)
#define REV_LSB       10
#define REV_BITS      (0x3F << REV_LSB)

// Register 0x02 - POWERCFG
#define ENABLE_BIT  (1 << 0)
#define DISABLE_BIT (1 << 6)
#define SEEK_BIT    (1 << 8)
#define SEEKUP_BIT  (1 << 9)
#define SKMODE_BIT  (1 << 10)
#define RDSM_BIT    (1 << 11)
#define MONO_BIT    (1 << 13)
#define DMUTE_BIT   (1 << 14)
#define DSMUTE_BIT  (1 << 15)

// Register 0x03 - CHANNEL
#define TUNE_BIT  (1 << 15)
#define CHAN_LSB  0
#define CHAN_BITS (0x3FF << CHAN_LSB)

// Register 0x04 - SYSCONFIG1
#define GPIO1_LSB    0
#define GPIO1_BITS   (0x3 << GPIO1_LSB)
#define GPIO2_LSB    2
#define GPIO2_BITS   (0x3 << GPIO2_LSB)
#define GPIO3_LSB    4
#define GPIO3_BITS   (0x3 << GPIO3_LSB)
#define BLNDADJ_LSB  6
#define BLNDADJ_BITS (0x3 << BLNDADJ_LSB)
#define AGCD_BIT     (1 << 10)
#define DE_BIT       (1 << 11)
#define RDS_BIT      (1 << 12)
#define STCIEN_BIT   (1 << 14)
#define RDSIEN_BIT   (1 << 15)

// Register 0x05 - SYSCONFIG2
#define VOLUME_LSB  0
#define VOLUME_BITS (0xF << VOLUME_LSB)
#define SPACE_LSB   4
#define SPACE_BITS  (0x3 << SPACE_LSB);
#define BAND_LSB    6
#define BAND_BITS   (0x3 << BAND_LSB)
#define SEEKTH_LSB  8
#define SEEKTH_BITS (0xFF << SEEKTH_LSB)

// Register 0x06 - SYSCONFIG3
#define SKCNT_LSB   0
#define SKCNT_BITS  (0xF << SKCNT_LSB)
#define SKSNR_LSB   4
#define SKSNR_BITS  (0xF << SKSNR_LSB)
#define VOLEXT_BIT  (1 << 8)
#define SMUTEA_LSB  12
#define SMUTEA_BITS (0x3 << SMUTEA_LSB)
#define SMUTER_LSB  14
#define SMUTER_BITS (0x3 << SMUTER_LSB)

// Register 0x07 - TEST1
#define AHIZEN_BIT (1 << 14)
#define XOSCEN_BIT (1 << 15)

// Register 0x0A - STATUSRSSI
#define RSSI_LSB   0
#define RSSI_BITS  (0xFF << RSSI_LSB)
#define ST_BIT     (1 << 8)
#define BLERA_LSB  9
#define BLERA_BITS (0x3 << BLERA_LSB)
#define RDSS_BIT   (1 << 11)
#define AFCRL_BIT  (1 << 12)
#define SFBL_BIT   (1 << 13)
#define STC_BIT    (1 << 14)
#define RDSR_BIT   (1 << 15)

// Register 0x0B - READCHAN
#define READCHAN_LSB  0
#define READCHAN_BITS (0x3FF << READCHAN_LSB)
#define BLERD_LSB     10
#define BLERD_BITS    (0x3 << BLERD_LSB)
#define BLERC_LSB     12
#define BLERC_BITS    (0x3 << BLERC_LSB)
#define BLERB_LSB     14
#define BLERB_BITS    (0x3 << BLERB_LSB)

#endif // _SI470X_REGS_H_
