/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file tmd2635_driver.h
 * @ingroup ring/drivers/ir_proximity_sensor_driver
 * @brief Header file for the TMD2635 IR proximity sensor driver.
 *
 * Datasheet: https://download.mikroe.com/documents/datasheets/TMD2635_Datasheet.pdf
 */

#ifndef TMD2635_DRIVER_H_
#define TMD2635_DRIVER_H_

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "common.h"
#include "i2c_driver_interface.h"
#include "ir_proximity_driver_interface.h"

#include <stdbool.h>
#include <stdint.h>

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/**
 * Time required for the device to become active after waking from sleep (in microseconds)
 * @note 1.5 ms typical according to datasheet, rounded up to 2 ms for safety
 */
#define TMD2635_ACTIVE_TIME_US (2000u)

/* --- I2C addressing ----------------------------------------------------------------------------------------------- */
/* 7-bit I2C addresses (depends on SCL/SDA pin routing) */
#define TMD2635_I2C_ADDR_SCL_SDA 0x39u /**< SCL = clock, SDA = data */
#define TMD2635_I2C_ADDR_SDA_SCL 0x38u /**< SDA = clock, SCL = data */

/* --- Registers ---------------------------------------------------------------------------------------------------- */

#define TMD2635_FULL_MASK 0xFFu // Full register clear mask for 8-bit registers

/* ENABLE (0x80) */
#define TMD2635_REG_ENABLE   0x80u     /**< Enables states and interrupts */
#define TMD2635_ENABLE_MASK  0x15u     /**< Mask for ENABLE register */
#define TMD2635_ENABLE_RESET 0x00u     /**< Reset value for ENABLE register */
#define TMD2635_ENABLE_PWEN  (1u << 4) /**< Activate the proximity wait feature set in PWTIME */
#define TMD2635_ENABLE_PEN   (1u << 2) /**< Activate the proximity detection */
#define TMD2635_ENABLE_PON   (1u << 0) /**< Activate the internal oscillator and ADC channel */

/* PRATE (0x82) */
#define TMD2635_REG_PRATE     0x82u             /**< Duration of a proximity sample. T_sample = (PRATE + 1) * 88us */
#define TMD2635_PRATE_MASK    TMD2635_FULL_MASK /**< Mask for PRATE register */
#define TMD2635_PRATE_RESET   0x1Fu             /**< Reset value for PRATE register (T_sample = ~2.8 ms) */
#define TMD2635_PRATE_TICK_US 88u               /**< Proximity sample time tick in microseconds */

/* Threshold register macros (0x88-0x8B) */
#define TMD2635_THR_H_MASK             0x3Fu
#define TMD2635_THR14_MAX              0x3FFFu
#define TMD2635_THR14_L(_val_)         ((uint8_t)((uint16_t)(_val_) & TMD2635_FULL_MASK))
#define TMD2635_THR14_H(_val_)         ((uint8_t)((((uint16_t)(_val_)) >> 8) & TMD2635_THR_H_MASK))
#define TMD2635_THR14_MAKE(_hi_, _lo_) ((uint16_t)((((uint16_t)((_hi_) & TMD2635_THR_H_MASK)) << 8) | (uint16_t)(_lo_)))

/* PILTL (0x88) */
#define TMD2635_REG_PILTL   0x88u             /**< Proximity interrupt low threshold low byte */
#define TMD2635_PILTL_MASK  TMD2635_FULL_MASK /**< Mask for PILTL register */
#define TMD2635_PILTL_RESET 0x00u             /**< Reset value for PILTL register */

/* PILTH (0x89) */
#define TMD2635_REG_PILTH   0x89u              /**< Proximity interrupt low threshold high byte */
#define TMD2635_PILTH_MASK  TMD2635_THR_H_MASK /**< Mask for PILTH register */
#define TMD2635_PILTH_RESET 0x00u              /**< Reset value for PILTH register */

/* PIHTL (0x8A) */
#define TMD2635_REG_PIHTL   0x8Au             /**< Proximity interrupt high threshold low byte */
#define TMD2635_PIHTL_MASK  TMD2635_FULL_MASK /**< Mask for PIHTL register */
#define TMD2635_PIHTL_RESET 0x00u             /**< Reset value for PIHTL register */

/* PIHTH (0x8B) */
#define TMD2635_REG_PIHTH   0x8Bu              /**< Proximity interrupt high threshold high byte */
#define TMD2635_PIHTH_MASK  TMD2635_THR_H_MASK /**< Mask for PIHTH register */
#define TMD2635_PIHTH_RESET 0x00u              /**< Reset value for PIHTH register */

/* PERS (0x8C) */
#define TMD2635_REG_PERS        0x8Cu             /**< Proximity interrupt persistence filters */
#define TMD2635_PERS_MASK       0x0Fu             /**< Mask for PERS register */
#define TMD2635_PERS_RESET      0x00u             /**< Reset value for PERS register */
#define TMD2635_PERS_PPERS_MASK TMD2635_PERS_MASK /**< Mask for PPERS field in PERS register */
/** Number of consecutive out-of-range proximity values to trigger an interrupt. Maximum 15 */
#define TMD2635_PERS_PPERS(_num_) ((uint8_t)((_num_) & TMD2635_PERS_PPERS_MASK))

/* CFG0 (0x8D) */
#define TMD2635_REG_CFG0    0x8Du     /**< Configuration zero */
#define TMD2635_CFG0_MASK   0x08u     /**< Mask for CFG0 register */
#define TMD2635_CFG0_RESET  0x40u     /**< Reset value for CFG0 register */
#define TMD2635_CFG0_PWLONG (1u << 3) /* Increase Wait period (PWTIME) by a factor of 12 */

/* PCFG0 (0x8E) */
#define TMD2635_REG_PCFG0   0x8Eu             /**< Proximity configuration zero */
#define TMD2635_PCFG0_MASK  TMD2635_FULL_MASK /**< Mask for PCFG0 register */
#define TMD2635_PCFG0_RESET 0x8Fu             /**< Reset value for PCFG0 register */

#define TMD2635_PCFG0_PGAIN_MASK  (0x3u << TMD2635_PCFG0_PGAIN_SHIFT) /**< Mask for PGAIN in PCFG0 register */
#define TMD2635_PCFG0_PGAIN_SHIFT 6u                                  /**< Position of PGAIN in PCFG0 register */
#define TMD2635_PCFG0_PGAIN_1X    (0x0u << TMD2635_PCFG0_PGAIN_SHIFT) /**< 1x Proximity gain */
#define TMD2635_PCFG0_PGAIN_2X    (0x1u << TMD2635_PCFG0_PGAIN_SHIFT) /**< 2x Proximity gain */
#define TMD2635_PCFG0_PGAIN_4X    (0x2u << TMD2635_PCFG0_PGAIN_SHIFT) /**< 4x Proximity gain (default) */
#define TMD2635_PCFG0_PGAIN_8X    (0x3u << TMD2635_PCFG0_PGAIN_SHIFT) /**< 8x Proximity gain */

#define TMD2635_PCFG0_PPULSE_MASK 0x3Fu /**< Mask for PPULSE in PCFG0 register */
#define TMD2635_MIN_PULSES        1u    /**< Minimum number of IR VCSEL pulses in a proximity cycle */
#define TMD2635_MAX_PULSES        64u   /**< Maximum number of IR VCSEL pulses in a proximity cycle */

/**
 * Max IR VCSEL pulses in a proximity cycle. 1 to 64. Default 16
 *
 * @note Defined as static inline function to avoid compiler warnings
 */
static inline uint8_t TMD2635_PCFG0_PPULSE(uint8_t num)
{
   return (uint8_t)((num - 1u) & TMD2635_PCFG0_PPULSE_MASK);
}

/* PCFG1 (0x8F) */
#define TMD2635_REG_PCFG1   0x8Fu /**< Proximity configuration one*/
#define TMD2635_PCFG1_MASK  0xEFu /**< Mask for PCFG1 register */
#define TMD2635_PCFG1_RESET 0x60u /**< Reset value for PCFG1 register */

#define TMD2635_PCFG1_PPULSE_LEN_SHIFT 5u /**< Position of PPULSE_LEN in PCFG1 register */
#define TMD2635_PCFG1_PPULSE_LEN_MASK                                                                                  \
   (0x7u << TMD2635_PCFG1_PPULSE_LEN_SHIFT) /**< Mask for PPULSE_LEN in PCFG1 register */
#define TMD2635_PCFG1_PPULSE_LEN_1US  (0x0u << TMD2635_PCFG1_PPULSE_LEN_SHIFT) /**< 1 microsecond pulse length */
#define TMD2635_PCFG1_PPULSE_LEN_2US  (0x1u << TMD2635_PCFG1_PPULSE_LEN_SHIFT) /**< 2 microsecond pulse length */
#define TMD2635_PCFG1_PPULSE_LEN_4US  (0x2u << TMD2635_PCFG1_PPULSE_LEN_SHIFT) /**< 4 microsecond pulse length */
#define TMD2635_PCFG1_PPULSE_LEN_8US  (0x3u << TMD2635_PCFG1_PPULSE_LEN_SHIFT) /**< 8 microsecond pulse length */
#define TMD2635_PCFG1_PPULSE_LEN_12US (0x4u << TMD2635_PCFG1_PPULSE_LEN_SHIFT) /**< 12 microsecond pulse length */
#define TMD2635_PCFG1_PPULSE_LEN_16US (0x5u << TMD2635_PCFG1_PPULSE_LEN_SHIFT) /**< 16 microsecond pulse length */
#define TMD2635_PCFG1_PPULSE_LEN_24US (0x6u << TMD2635_PCFG1_PPULSE_LEN_SHIFT) /**< 24 microsecond pulse length */
#define TMD2635_PCFG1_PPULSE_LEN_32US (0x7u << TMD2635_PCFG1_PPULSE_LEN_SHIFT) /**< 32 microsecond pulse length */

#define TMD2635_PCFG1_PLDRIVE_MASK 0x0Fu /**< Mask for PLDRIVE in PCFG1 register */
#define TMD2635_PCFG1_PLDRIVE_7MA  0x05u /**< 7 mA IR VCSEL drive current */
#define TMD2635_PCFG1_PLDRIVE_8MA  0x06u /**< 8 mA IR VCSEL drive current */
#define TMD2635_PCFG1_PLDRIVE_9MA  0x07u /**< 9 mA IR VCSEL drive current */
#define TMD2635_PCFG1_PLDRIVE_10MA 0x08u /**< 10 mA IR VCSEL drive current */

/* REVID (0x91) */
#define TMD2635_REG_REVID         0x91u              /**< Revision ID */
#define TMD2635_REVID_MASK        0x07u              /**< Mask for REVID register */
#define TMD2635_REVID_RESET       0x10u              /**< Reset value for REVID register */
#define TMD2635_REVID_REV_ID_MASK TMD2635_REVID_MASK /**< Mask for revision ID in REVID register */

/* ID (0x92) */
#define TMD2635_REG_ID       0x92u           /**< Device ID */
#define TMD2635_ID_MASK      0xFCu           /**< Mask for ID register */
#define TMD2635_ID_RESET     0x44u           /**< Reset value for ID register */
#define TMD2635_ID_TYPE_MASK TMD2635_ID_MASK /**< Mask for device type in ID register */

/* STATUS (0x9B) */
#define TMD2635_REG_STATUS   0x9Bu             /**< Device status */
#define TMD2635_STATUS_MASK  TMD2635_FULL_MASK /**< Mask for STATUS register */
#define TMD2635_STATUS_RESET 0x00u             /**< Reset value for STATUS register */
#define TMD2635_STATUS_PHIGH                                                                                           \
   (1u << 7) /**< Set when PINT is set and PDATA > high threshold (after persistence). Cleared when PINT is cleared.   \
              */
#define TMD2635_STATUS_PLOW                                                                                            \
   (1u << 6) /**< Set when PINT is set and PDATA < low threshold (after persistence). Cleared when PINT is cleared. */
#define TMD2635_STATUS_PSAT                                                                                            \
   (1u << 5) /**< Proximity saturation flag indicates that an ambient or reflective-saturation event occurred during a \
                previous proximity cycle. */
#define TMD2635_STATUS_PINT                                                                                            \
   (1u << 4) /**< Proximity interrupt flag indicates that proximity results have exceeded thresholds and persistence   \
                settings. */
#define TMD2635_STATUS_CINT (1u << 3) /**< Calibration interrupt flag indicates that calibration has completed. */
#define TMD2635_STATUS_ZINT                                                                                            \
   (1u << 2) /**< Zero detection interrupt flag indicates that a zero value in PDATA has caused the proximity offset   \
                to be decremented (if AUTO_OFFSET_ADJ = 1). */
#define TMD2635_STATUS_PSAT_REFLECTIVE                                                                                 \
   (1u << 1) /**< The Reflective Proximity Saturation Interrupt flag signals that the AFE has saturated during the IR  \
                VCSEL active portion of proximity integration. */
#define TMD2635_STATUS_PSAT_AMBIENT                                                                                    \
   (1u << 0) /**< The Ambient Proximity Saturation Interrupt flag signals that the AFE has saturated during the IR     \
                VCSEL inactive portion of proximity integration. */
#define TMD2635_STATUS_CLEAR_ALL 0xFFu /**< Mask to clear all status bits by reading the STATUS register */

/* PDATAL (0x9C) */
/* Note: If configured for 14-bit proximity data, PDATAL contains the lower 8 bits of the 14-bit value.
   If configured for 10-bit proximity data, PDATAL contains the upper 8 bits of the 10-bit value (bits 9:2). */
#define TMD2635_REG_PDATAL   0x9Cu             /**< Proximity ADC low data */
#define TMD2635_PDATAL_MASK  TMD2635_FULL_MASK /**< Mask for PDATAL register */
#define TMD2635_PDATAL_RESET 0x00u             /**< Reset value for PDATAL register */

/* PDATAH (0x9D) */
/* Note: If configured for 14-bit proximity data, PDATAH contains the upper 6 bits of the 14-bit value (bits 13:8).
   If configured for 10-bit proximity data, PDATAH contains the lower 2 bits of the 10-bit value (bits 1:0). */
#define TMD2635_REG_PDATAH   0x9Du             /**< Proximity ADC high data */
#define TMD2635_PDATAH_MASK  TMD2635_FULL_MASK /**< Mask for PDATAH register */
#define TMD2635_PDATAH_RESET 0x00u             /**< Reset value for PDATAH register */

/* PDATA (0x9C/0x9D) */
#define TMD2635_PDATA14_MASK 0x3FFFu /**< Mask for 14-bit proximity data */
#define TMD2635_PDATA14_MAKE(_hi_, _lo_)                                                                               \
   ((uint16_t)((uint16_t)(((uint16_t)(_hi_) << 8) | (uint16_t)(_lo_)) & TMD2635_PDATA14_MASK))
#define TMD2635_PDATA10_MASK 0x03FFu /**< Mask for 10-bit proximity data */
#define TMD2635_PDATA10_MAKE(_hi_, _lo_)                                                                               \
   ((uint16_t)((uint16_t)(((uint16_t)(_lo_) << 2) | ((uint16_t)(_hi_) & 0x03u)) & TMD2635_PDATA10_MASK))

/* REVID2 (0xA6) */
#define TMD2635_REG_REVID2         0xA6u               /**< Revision ID two */
#define TMD2635_REVID2_MASK        0x0Fu               /**< Mask for REVID2 register */
#define TMD2635_REVID2_A_RESET     0x01u               /**< First of two possible reset values for REVID2 register */
#define TMD2635_REVID2_B_RESET     0x0Eu               /**< Second of two possible reset values for REVID2 register */
#define TMD2635_REVID2_VER_ID_MASK TMD2635_REVID2_MASK /**< Mask for version ID in REVID2 register */

/* SOFTRST (0xA8) */
/* Note: Write 1 to immediately reset registers, terminate operation, and put the device into sleep. */
#define TMD2635_REG_SOFTRST 0xA8u /**< Soft reset */
#define TMD2635_SOFTRST     (1u << 0)

/* PWTIME (0xA9) */
#define TMD2635_REG_PWTIME     0xA9u             /**< Proximity wait time */
#define TMD2635_PWTIME_MASK    TMD2635_FULL_MASK /**< Mask for PWTIME register */
#define TMD2635_PWTIME_RESET   0x00u             /**< Reset value for PWTIME register */
#define TMD2635_PWTIME_TICK_US 2780u             /**< Proximity wait time unit in microseconds (2.78 ms) */

/* CFG8 (0xAA) */
#define TMD2635_REG_CFG8           0xAAu             /**< Configuration eight */
#define TMD2635_CFG8_MASK          0x03u             /**< Mask for CFG8 register */
#define TMD2635_CFG8_RESET         0x02u             /**< Reset value for CFG8 register */
#define TMD2635_CFG8_PDSELECT_MASK TMD2635_CFG8_MASK /**< Mask for PDSELECT in CFG8 register */
#define TMD2635_CFG8_PDSELECT_NONE 0x00u             /**< No photodiode */
#define TMD2635_CFG8_PDSELECT_FAR  0x01u             /**< Far photodiode */
#define TMD2635_CFG8_PDSELECT_NEAR 0x02u             /**< Near photodiode (default) */
#define TMD2635_CFG8_PDSELECT_BOTH 0x03u             /**< Both photodiodes */

/* CFG3 (0xAB) */
#define TMD2635_REG_CFG3            0xABu     /**< Configuration three */
#define TMD2635_CFG3_MASK           0x90u     /**< Mask for CFG3 register */
#define TMD2635_CFG3_RESET          0x04u     /**< Reset value for CFG3 register */
#define TMD2635_CFG3_INT_READ_CLEAR (1u << 7) /* reading STATUS clears flags */
#define TMD2635_CFG3_SAI            (1u << 4) /* sleep-after-interrupt */

/* CFG6 (0xAE) */
#define TMD2635_REG_CFG6         0xAEu     /**< Configuration six */
#define TMD2635_CFG6_MASK        0x40u     /**< Mask for CFG6 register */
#define TMD2635_CFG6_RESET       0x3Fu     /**< Reset value for CFG6 register */
#define TMD2635_CFG6_APC_DISABLE (1u << 6) /**< Disable automatic pulse control */

/* PFILTER (0xB3) */
#define TMD2635_REG_PFILTER        0xB3u /**< Proximity filter */
#define TMD2635_PFILTER_MASK       0x03u /**< Mask for PFILTER register */
#define TMD2635_PFILTER_RESET      0x00u /**< Reset value for PFILTER register */
#define TMD2635_PFILTER_PMAVG_MASK 0x03u /**< Mask for PMAVG in PFILTER register */
#define TMD2635_PFILTER_PMAVG_OFF  0x00u /* moving average disabled */
#define TMD2635_PFILTER_PMAVG_2    0x01u /* 2 value moving average */
#define TMD2635_PFILTER_PMAVG_4    0x02u /* 4 value moving average */
#define TMD2635_PFILTER_PMAVG_8    0x03u /* 8 value moving average */

/* POFFSETL (0xC0) */
#define TMD2635_REG_POFFSETL      0xC0u                 /**< Proximity offset low data */
#define TMD2635_POFFSETL_MASK     TMD2635_FULL_MASK     /**< Mask for POFFSETL register */
#define TMD2635_POFFSETL_RESET    0x00u                 /**< Reset value for POFFSETL register */
#define TMD2635_POFFSETL_MAG_MASK TMD2635_POFFSETL_MASK /**< Mask for magnitude portion of offset */

/* POFFSETH (0xC1) */
#define TMD2635_REG_POFFSETH      0xC1u     /**< Proximity offset high data */
#define TMD2635_POFFSETH_MASK     0x01u     /**< Mask for POFFSETH register */
#define TMD2635_POFFSETH_RESET    0x00u     /**< Reset value for POFFSETH register */
#define TMD2635_POFFSETH_SIGN_BIT (1u << 0) /* sign portion (bit meaning per datasheet text) */

/* CALIB (0xD7) */
#define TMD2635_REG_CALIB              0xD7u     /**< Proximity offset calibration */
#define TMD2635_CALIB_MASK             0xB1u     /**< Mask for CALIB register */
#define TMD2635_CALIB_RESET            0x00u     /**< Reset value for CALIB register */
#define TMD2635_CALIB_CALAVG           (1u << 7) /**< Enable hardware averaging during calibration */
#define TMD2635_CALIB_ELECTRICAL_CAL   (1u << 5) /**< Calibration type. 0 = Optical, 1 = Electrical */
#define TMD2635_CALIB_CALPRATE         (1u << 4) /**< Enable PRATE during calibration */
#define TMD2635_CALIB_START_OFFSET_CAL (1u << 0) /**< Start calibration sequence */

/* CALIBCFG (0xD9) */
#define TMD2635_REG_CALIBCFG   0xD9u /**< Proximity offset calibration control */
#define TMD2635_CALIBCFG_MASK  0xEFu /**< Mask for CALIBCFG register */
#define TMD2635_CALIBCFG_RESET 0x50u /**< Reset value for CALIBCFG register */
#define TMD2635_CALIBCFG_BASE  0x10u
/* BINSRCH_TARGET - Proximity offset calibration result target. This is the target value for PDATA against which the
 * calibration procedure will tune the offset against.*/
#define TMD2635_CALIBCFG_BINSRCH_SHIFT 5u /**< Position of BINSRCH_TARGET in CALIBCFG register */
/** Mask for BINSRCH_TARGET in CALIBCFG register */
#define TMD2635_CALIBCFG_BINSRCH_MASK    (0x7u << TMD2635_CALIBCFG_BINSRCH_SHIFT)
#define TMD2635_CALIBCFG_BINSRCH_3       (0u << TMD2635_CALIBCFG_BINSRCH_SHIFT) /**< Target PDATA = 3 */
#define TMD2635_CALIBCFG_BINSRCH_7       (1u << TMD2635_CALIBCFG_BINSRCH_SHIFT) /**< Target PDATA = 7 */
#define TMD2635_CALIBCFG_BINSRCH_15      (2u << TMD2635_CALIBCFG_BINSRCH_SHIFT) /**< Target PDATA = 15 */
#define TMD2635_CALIBCFG_BINSRCH_31      (3u << TMD2635_CALIBCFG_BINSRCH_SHIFT) /**< Target PDATA = 31 */
#define TMD2635_CALIBCFG_BINSRCH_63      (4u << TMD2635_CALIBCFG_BINSRCH_SHIFT) /**< Target PDATA = 63 */
#define TMD2635_CALIBCFG_BINSRCH_127     (5u << TMD2635_CALIBCFG_BINSRCH_SHIFT) /**< Target PDATA = 127 */
#define TMD2635_CALIBCFG_BINSRCH_255     (6u << TMD2635_CALIBCFG_BINSRCH_SHIFT) /**< Target PDATA = 255 */
#define TMD2635_CALIBCFG_BINSRCH_511     (7u << TMD2635_CALIBCFG_BINSRCH_SHIFT) /**< Target PDATA = 511 */
#define TMD2635_CALIBCFG_AUTO_OFFSET_ADJ (1u << 3) /**< If set, causes POFFSETL to decrease if PDATA ever is 0. */
/* PROX_AVG - Defines the ADC multisampling behaviour */
#define TMD2635_CALIBCFG_PROX_AVG_MASK 0x07u /**< Mask for PROX_AVG in CALIBCFG register */
#define TMD2635_CALIBCFG_PROX_AVG_0    0x00u /* disable multisampling */
#define TMD2635_CALIBCFG_PROX_AVG_2    0x01u /* 2x ADC multisampling */
#define TMD2635_CALIBCFG_PROX_AVG_4    0x02u /* 4x ADC multisampling */
#define TMD2635_CALIBCFG_PROX_AVG_8    0x03u /* 8x ADC multisampling */
#define TMD2635_CALIBCFG_PROX_AVG_16   0x04u /* 16x ADC multisampling */
#define TMD2635_CALIBCFG_PROX_AVG_32   0x05u /* 32x ADC multisampling */
#define TMD2635_CALIBCFG_PROX_AVG_64   0x06u /* 64x ADC multisampling */
#define TMD2635_CALIBCFG_PROX_AVG_128  0x07u /* 128x ADC multisampling */

/* CALIBSTAT (0xDC) */
#define TMD2635_REG_CALIBSTAT             0xDCu     /**< Proximity offset calibration status */
#define TMD2635_CALIBSTAT_MASK            0x05u     /**< Mask for CALIBSTAT register */
#define TMD2635_CALIBSTAT_RESET           0x00u     /**< Reset value for CALIBSTAT register */
#define TMD2635_CALIBSTAT_OFFSET_ADJUSTED (1u << 2) /**< Indicates that POFFSETL/H has been adjusted automatically. */
#define TMD2635_CALIBSTAT_CALIB_FINISHED  (1u << 0) /**< Indicates that a calibration cycle has completed. */

/* INTENAB (0xDD) */
#define TMD2635_REG_INTENAB      0xDDu /**< Interrupt enables */
#define TMD2635_INTENAB_MASK     0x3Eu /**< Mask for INTENAB register */
#define TMD2635_INTENAB_RESET    0x00u
#define TMD2635_INTENAB_DISABLED TMD2635_INTENAB_RESET /**< All interrupts disabled */
#define TMD2635_INTENAB_PIM      (1u << 5)             /**< Proximity interrupt mode (0=level, 1=state) */
#define TMD2635_INTENAB_PIEN     (1u << 4)             /**< Proximity interrupt enable */
#define TMD2635_INTENAB_PSIEN    (1u << 3)             /**< Proximity saturation interrupt enable */
#define TMD2635_INTENAB_CIEN     (1u << 2)             /**< Calibration interrupt enable */
#define TMD2635_INTENAB_ZIEN     (1u << 1)             /**< Zero-detect interrupt enable */

/* Factory data registers (0xE4-0xE6) */
#define TMD2635_REG_FAC_L 0xE5u /**< Factory data low (lot code data). Read-only */
#define TMD2635_REG_FAC_H 0xE6u /**< Factory data high (lot code data). Read-only */

/* TEST9 (0xF9) */
#define TMD2635_REG_TEST9      0xF9u             /**< Test nine (must be set to 0x07) */
#define TMD2635_TEST9_MASK     TMD2635_FULL_MASK /**< Mask for TEST9 register */
#define TMD2635_TEST9_RESET    0x00u             /**< Reset value for TEST9 register */
#define TMD2635_TEST9_REQUIRED 0x07u             /**< Required value for TEST9 register */

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief TMD2635 photodiode configuration options
 *
 * Selects which receiver photodiode(s) feed the proximity ADC. This changes the spatial response of the sensor.
 */
typedef enum
{
   TMD2635_PD_FAR = 0, /**< Far photodiode */
   TMD2635_PD_NEAR,    /**< Near photodiode */
   TMD2635_PD_BOTH,    /**< Both photodiodes */

   TMD2635_PD_MAX /**< Sentinel value */
} TMD2635_PD_CONFIG_OPTION;

/**
 * @brief TMD2635 gain configuration options
 *
 * Sets the gain applied to the proximity measurements. Higher gain increases sensitivity, but also increases
 * noise/crosstalk and reduces saturation headroom.
 */
typedef enum
{
   TMD2635_GAIN_1X = 0, /**< 1x gain */
   TMD2635_GAIN_2X,     /**< 2x gain */
   TMD2635_GAIN_4X,     /**< 4x gain */
   TMD2635_GAIN_8X,     /**< 8x gain */

   TMD2635_GAIN_MAX /**< Sentinel value */
} TMD2635_GAIN_CONFIG_OPTION;

/**
 * @brief TMD2635 pulse length configuration options
 *
 * Sets the pulse width of each IR VCSEL pulse during proximity measurements. Longer pulses increase proximity range and
 * typically reduce electrical noise, but increase power consumption and reduces saturation headroom.
 */
typedef enum
{
   TMD2635_PULSE_LEN_1US = 0, /**< 1 microsecond pulse length */
   TMD2635_PULSE_LEN_2US,     /**< 2 microsecond pulse length */
   TMD2635_PULSE_LEN_4US,     /**< 4 microsecond pulse length */
   TMD2635_PULSE_LEN_8US,     /**< 8 microsecond pulse length */
   TMD2635_PULSE_LEN_12US,    /**< 12 microsecond pulse length */
   TMD2635_PULSE_LEN_16US,    /**< 16 microsecond pulse length */
   TMD2635_PULSE_LEN_24US,    /**< 24 microsecond pulse length */
   TMD2635_PULSE_LEN_32US,    /**< 32 microsecond pulse length */

   TMD2635_PULSE_LEN_MAX /**< Sentinel value */
} TMD2635_PULSE_LEN_CONFIG_OPTION;

/**
 * @brief TMD2635 drive current configuration options
 *
 * Sets the IR VCSEL drive current. Higher current increases emitted IR and thus proximity range,
 * but increases emitter power and reduces saturation headroom.
 *
 * @note 7 mA is recommended to reduce part-to-part variation.
 */
typedef enum
{
   TMD2635_DRIVE_7MA = 0, /**< 7 mA IR VCSEL drive current. Recommended to reduce part-to-part variation */
   TMD2635_DRIVE_8MA,     /**< 8 mA IR VCSEL drive current */
   TMD2635_DRIVE_9MA,     /**< 9 mA IR VCSEL drive current */
   TMD2635_DRIVE_10MA,    /**< 10 mA IR VCSEL drive current */

   TMD2635_DRIVE_MAX /**< Sentinel value */
} TMD2635_DRIVE_CONFIG_OPTION;

/**
 * @brief TMD2635 hardware averaging configuration options
 *
 * Sets the number of samples averaged in hardware for a single proximity measurement. Each sample transmits the
 * configured proximity pulses. Higher values improve noise rejection/repeatability but increase measurement time and
 * power consumption.
 */
typedef enum
{
   TMD2635_HWAVG_OFF = 0, /**< Hardware averaging disabled */
   TMD2635_HWAVG_2,       /**< 2-sample hardware average */
   TMD2635_HWAVG_4,       /**< 4-sample hardware average */
   TMD2635_HWAVG_8,       /**< 8-sample hardware average */
   TMD2635_HWAVG_16,      /**< 16-sample hardware average */
   TMD2635_HWAVG_32,      /**< 32-sample hardware average */
   TMD2635_HWAVG_64,      /**< 64-sample hardware average */
   TMD2635_HWAVG_128,     /**< 128-sample hardware average */

   TMD2635_HWAVG_MAX /**< Sentinel value */
} TMD2635_HWAVG_CONFIG_OPTION;

/**
 * @brief TMD2635 moving average configuration options
 *
 * Sets the number of samples to include in the moving average filter for proximity measurements. Improves noise
 * rejection and reduces bouncing/chatter at threshold crossings, at the cost of slower response time.
 *
 * @note If moving averaging is enabled. the sensor must fill the moving average buffer before proximity data will be
 * valid. Until then, the proximity data will read as zero.
 * @note The fill time is: `t_wait ~= sampling_period * num_moving_avg_samples`
 */
typedef enum
{
   TMD2635_MAVG_OFF = 0, /**< Moving average disabled */
   TMD2635_MAVG_2,       /**< 2-sample moving average */
   TMD2635_MAVG_4,       /**< 4-sample moving average */
   TMD2635_MAVG_8,       /**< 8-sample moving average */

   TMD2635_MAVG_MAX /**< Sentinel value */
} TMD2635_MAVG_CONFIG_OPTION;

/**
 * @brief TMD2635 power mode options
 *
 * Defines the power mode of the TMD2635 proximity sensor.
 */
typedef enum
{
   TMD2635_POWER_MODE_SLEEP = 0, /**< Low power sleep mode. Will not respond to I2C until in IDLE or ACTIVE mode */
   TMD2635_POWER_MODE_IDLE,      /**< Idle mode. Not measuring proximity data. */
   TMD2635_POWER_MODE_ACTIVE,    /**< Active mode. Actively measuring proximity data. */

   TMD2635_POWER_MODE_MAX /**< Sentinel value */
} TMD2635_POWER_MODE;

/**
 * @brief Configuration for the TMD2635 proximity sensor
 *
 * This structure defines the configuration options for the TMD2635 proximity sensor. Most configuration is static and
 * dependent on the hardware configuration. Some can be adjusted dynamically during runtime using interface functions.
 *
 * @note If moving averaging is enabled. the sensor must fill the moving average buffer before proximity data will be
 * valid. Until then, the proximity data will read as zero.
 * @note The fill time is: `t_wait ~= sampling_period * num_moving_avg_samples`
 */
typedef struct
{
   uint8_t i2c_addr; /**< I2C address of the TMD2635 sensor */

   TMD2635_PD_CONFIG_OPTION photodiode;
   TMD2635_GAIN_CONFIG_OPTION gain;
   uint8_t max_pulses; /**< Maximum number of pulses per measurement. 1-64 */
   TMD2635_PULSE_LEN_CONFIG_OPTION pulse_len;
   TMD2635_DRIVE_CONFIG_OPTION drive_current;
   TMD2635_HWAVG_CONFIG_OPTION hw_avg;
   TMD2635_MAVG_CONFIG_OPTION moving_avg;
} tmd2635_config_t;

/**
 * @brief TMD2635 proximity sensor driver instance
 */
typedef struct tmd2635_driver tmd2635_driver_t;
struct tmd2635_driver
{
   // Interface
   ir_proximity_driver_interface_t interface; /**< Instance of proximity driver interface */

   // Dependencies
   const i2c_driver_interface_t *_p_i2c_ifc; /**< Pointer to I2C interface instance to use */

   // Private data
   tmd2635_config_t _cfg;          /**< Local copy of the configuration */
   TMD2635_POWER_MODE _power_mode; /**< Current power mode of the proximity sensor */
   uint16_t _sampling_period_ms;   /**< Current proximity sampling period in milliseconds */

   bool _is_initialized; /**< Whether the instance is initialized */
};

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

/**
 * @brief Initializes a TMD2635 proximity sensor driver instance
 *
 * @param[in,out] p_self Pointer to the driver instance to initialize
 * @param[in] p_cfg Pointer to the configuration structure
 * @param[in] p_i2c_ifc Pointer to the I2C driver interface to use
 *
 * @return Status code indicating the result of the operation
 */
result_t tmd2635_driver_init(tmd2635_driver_t *const p_self,
                             const tmd2635_config_t *p_cfg,
                             const i2c_driver_interface_t *p_i2c_ifc);

#endif // TMD2635_DRIVER_H_
