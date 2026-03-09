/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file ba25150.h
 * @ingroup pmic_driver
 * @brief List of registers and addresses for the TI BQ25150 PMIC.
 */

/**
 * Naming conventions
 * REG_*            – 8-bit register addresses
 * * _MASK          – single-bit masks or multi-bit field masks
 * * _SHIFT         – bit position of least-significant bit of a field
 */

/**
 * todo:
 * Disable IC HW watchdog.
 * Add reference to which datasheet was used.
 * Question: Should VINDPM be enabled (see datasheet p20)? It looks like a useful feature, especially since the ring
 * will be charged via induction. Q to Dennis: What is the expected input voltage? From schematic it shows 3.0V or 4.4V
 * but 3.9V would not be sufficient to charge the battery?
 */

#ifndef BQ25150_H_
#define BQ25150_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <stdint.h>
/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define REG_STAT0 0x00
#define REG_STAT1 0x01
#define REG_STAT2 0x02
#define REG_FLAG0 0x03
#define REG_FLAG1 0x04
#define REG_FLAG2 0x05
#define REG_FLAG3 0x06

/**
 * @note Regarding the MASK registers from the datasheet: If the interrupt condition occurs while the interrupt is
 * masked an interrupt pulse will not be sent. If the interrupt is unmasked while the fault condition is still present,
 * an interrupt pulse will not be sent until the INT trigger condition occurs while unmasked.
 *
 */
#define REG_MASK0 0x07
#define REG_MASK1 0x08
#define REG_MASK2 0x09
#define REG_MASK3 0x0A

#define REG_VBAT_CTRL 0x12
#define REG_ICHG_CTRL 0x13
#define REG_PCHG_CTRL 0x14 // PCHRGCTRL in datasheet
#define REG_TERM_CTRL 0x15
#define REG_BUVLO     0x16
#define REG_CHG_CTRL0 0x17
#define REG_CHG_CTRL1 0x18
#define REG_ILIM_CTRL 0x19
#define REG_LDO_CTRL  0x1D

#define REG_MR_CTRL  0x30
#define REG_IC_CTRL0 0x35
#define REG_IC_CTRL1 0x36
#define REG_IC_CTRL2 0x37

#define REG_ADC_CTRL0 0x40
#define REG_ADC_CTRL1 0x41

#define REG_ADC_VBAT_MSB  0x42
#define REG_ADC_VBAT_LSB  0x43
#define REG_ADC_TS_MSB    0x44
#define REG_ADC_TS_LSB    0x45
#define REG_ADC_ICHG_MSB  0x46
#define REG_ADC_ICHG_LSB  0x47
#define REG_ADC_ADCIN_MSB 0x48
#define REG_ADC_ADCIN_LSB 0x49
#define REG_ADC_VIN_MSB   0x4A
#define REG_ADC_VIN_LSB   0x4B
#define REG_ADC_PMID_MSB  0x4C
#define REG_ADC_PMID_LSB  0x4D
#define REG_ADC_IIN_MSB   0x4E
#define REG_ADC_IIN_LSB   0x4F

#define REG_ADCALARM_COMP1_MSB 0x52
#define REG_ADCALARM_COMP1_LSB 0x53
#define REG_ADCALARM_COMP2_MSB 0x54
#define REG_ADCALARM_COMP2_LSB 0x55
#define REG_ADCALARM_COMP3_MSB 0x56
#define REG_ADCALARM_COMP3_LSB 0x57

#define REG_ADC_READ_EN 0x58

#define REG_TS_FASTCHG_CTRL 0x61
#define REG_TS_COLD         0x62
#define REG_TS_COOL         0x63
#define REG_TS_WARM         0x64
#define REG_TS_HOT          0x65

#define REG_DEVICE_ID 0x6F

/* -------------------------------------------------------------------------- */
/*  STAT0 (0x00) – Charger status 0                                           */
/* -------------------------------------------------------------------------- */
#define CHRG_CV_STAT_MASK         (1U << 6)
#define CHARGE_DONE_STAT_MASK     (1U << 5)
#define IINLIM_ACTIVE_STAT_MASK   (1U << 4)
#define VDPPM_ACTIVE_STAT_MASK    (1U << 3)
#define VINDPM_ACTIVE_STAT_MASK   (1U << 2)
#define THERMREG_ACTIVE_STAT_MASK (1U << 1)
#define VIN_PGOOD_STAT_MASK       (1U << 0)

/* -------------------------------------------------------------------------- */
/*  STAT1 (0x01) – Charger status 1                                           */
/* -------------------------------------------------------------------------- */
#define VIN_OVP_FAULT_STAT_MASK  (1U << 7)
#define BAT_OCP_FAULT_STAT_MASK  (1U << 5)
#define BAT_UVLO_FAULT_STAT_MASK (1U << 4)
#define TS_COLD_STAT_MASK        (1U << 3)
#define TS_COOL_STAT_MASK        (1U << 2)
#define TS_WARM_STAT_MASK        (1U << 1)
#define TS_HOT_STAT_MASK         (1U << 0)

/* -------------------------------------------------------------------------- */
/*  STAT2 (0x02) – ADC comparator status                                      */
/* -------------------------------------------------------------------------- */
#define COMP1_ALARM_STAT_MASK (1U << 6)
#define COMP2_ALARM_STAT_MASK (1U << 5)
#define COMP3_ALARM_STAT_MASK (1U << 4)
#define TS_OPEN_STAT_MASK     (1U << 0)

/* -------------------------------------------------------------------------- */
/*  FLAG0/FLAG1/FLAG2/FLAG3 – sticky event flags (clear-on-read)              */
/*     Bits mirror STAT registers; names are identical with _FLAG suffix.    */
/* -------------------------------------------------------------------------- */
#define CHRG_CV_FLAG_MASK         (1U << 6)
#define CHARGE_DONE_FLAG_MASK     (1U << 5)
#define IINLIM_ACTIVE_FLAG_MASK   (1U << 4)
#define VDPPM_ACTIVE_FLAG_MASK    (1U << 3)
#define VINDPM_ACTIVE_FLAG_MASK   (1U << 2)
#define THERMREG_ACTIVE_FLAG_MASK (1U << 1)
#define VIN_PGOOD_FLAG_MASK       (1U << 0)

#define VIN_OVP_FAULT_FLAG_MASK  (1U << 7)
#define BAT_OCP_FAULT_FLAG_MASK  (1U << 5)
#define BAT_UVLO_FAULT_FLAG_MASK (1U << 4)
#define TS_COLD_FLAG_MASK        (1U << 3)
#define TS_COOL_FLAG_MASK        (1U << 2)
#define TS_WARM_FLAG_MASK        (1U << 1)
#define TS_HOT_FLAG_MASK         (1U << 0)
#define TS_OPEN_FAULT_MASK       (1U << 0)

#define WD_FAULT_FLAG_MASK         (1U << 6)
#define SAFETY_TMR_FAULT_FLAG_MASK (1U << 5)
#define LDO_OCP_FAULT_FLAG_MASK    (1U << 4)
#define IMAX_FAULT_FLAG_MASK       (1U << 3)
#define MRWAKE1_TIMEOUT_FLAG_MASK  (1U << 2)
#define MRWAKE2_TIMEOUT_FLAG_MASK  (1U << 1)
#define MRRESET_WARN_FLAG_MASK     (1U << 0)

/* -------------------------------------------------------------------------- */
/*  MASK0...MASK3 – interrupt mask bits mirror FLAG definitions               */
/* -------------------------------------------------------------------------- */
#define CHRG_CV_MASK_MASK         (1U << 6)
#define CHARGE_DONE_MASK_MASK     (1U << 5)
#define IINLIM_ACTIVE_MASK_MASK   (1U << 4)
#define VDPPM_ACTIVE_MASK_MASK    (1U << 3)
#define VINDPM_ACTIVE_MASK_MASK   (1U << 2)
#define THERMREG_ACTIVE_MASK_MASK (1U << 1)
#define VIN_PGOOD_MASK_MASK       (1U << 0)

/* -------------------------------------------------------------------------- */
/*  LDOCTRL (0x1D) – LDO / load-switch                                        */
/* -------------------------------------------------------------------------- */
#define EN_LS_LDO_MASK      (1U << 7)
#define VLDO_SHIFT          2
#define VLDO_MASK           (0x1FU << VLDO_SHIFT) /* 600 mV + N·100 mV */
#define LDO_SWITCH_CFG_MASK (1U << 1)

/* -------------------------------------------------------------------------- */
/*  ICCTRL0 (0x35)                                                            */
/* -------------------------------------------------------------------------- */
#define EN_SHIP_MODE_MASK    (1U << 7)
#define GLOBAL_INT_MASK_MASK (1U << 2)
#define HW_RESET_MASK        (1U << 1)
#define SW_RESET_MASK        (1U << 0)
/* -------------------------------------------------------------------------- */
/*  ILIMCTRL (0x19) – input current limit                                     */
/* -------------------------------------------------------------------------- */
#define ILIM_150MA (0x1U << 1)

// ADC
#define ADC_READY_FLAG_MASK (1U << 7)

/**  @note
 *  – Read-only/status/ID registers are **not** included (writes would be ignored).
 *  – Adjust currents / voltages to suit your design limits before production.
 */
/**
 * @note Make sure to update the documentation for each register value implemented.
 */
const uint8_t bq25150_register_configs[][2] = {
   /* ---------- Interrupt masks – unmask everything for full visibility ---- */
   {REG_MASK0, 0x00}, // Don’t mask CHG/VIN/DPPM/THERM status IRQs
   {REG_MASK1, 0x00}, // Don’t mask OVP/UVLO/JEITA IRQs
   {REG_MASK2, 0x00}, // Don’t mask comparator/TS-open IRQs
   {REG_MASK3, 0x00}, // Don’t mask timer/OCP/LDO/RESET IRQs

   /* ---------- Charging / power-path core --------------------------------- */
   {REG_VBAT_CTRL, 0x3C}, // VBAT regulation = 4.200 V  (Vbat regulation Voltage = 3.6V + VBAT_REG code x 10mV)
   {REG_PCHG_CTRL,
    0x01}, // ICHARGE_RANGE = 1.25mA step. Pre-Charge Current = ICHARGE_RANGE(in mA) x IPRECHG code = 1.25 mA
   {REG_ICHG_CTRL,
    0x09}, // Fast-charge current = 11.25mA mA = Fast Charge Current = 1.25mA x ICHG code (ICHARGE_RANGE=0)
   {REG_TERM_CTRL, 0x08}, // Termination current = 4% of ICHRG (0x04<<1)
   {REG_BUVLO, 0x00},     // UVLO = 3V, OC = 1.2A, pre-charge to fast-charge threshold = 3V
   {REG_CHG_CTRL0, 0x92}, // JEITA/TS enabled | RCHG Vth = 140mV | watchdog OFF | 6 h safety timer
   {REG_CHG_CTRL1, 0x10}, // VIN-DPM threshold = 4.3 V | VINDPM loop enabled | DPPM en | 70degC
   //{REG_CHG_CTRL1, 0x58}, // VINDPM and DPPM disabled | 70degC
   {REG_ILIM_CTRL, 0x01}, // Input current limit = 100 mA

   /* ---------- LDO / load-switch block ------------------------------------ */
   {REG_LDO_CTRL, 0x60}, // Disable LS/LDO | Regulated-LDO mode | 3V
                         // Todo: create mask to enable / disable LS/LDO

   /* ---------- Push-button / MR behaviour --------------------------------- */
   {REG_MR_CTRL, 0x00}, // Not used.

   /* ---------- Global IC control ------------------------------------------ */
   {REG_IC_CTRL0, 0x00}, // Normal mode (no ship, no resets, IRQs un-masked)
   // todo: create mask for shipping mode and software reset.
   {REG_IC_CTRL1, 0x00}, // Defaults. (PG not used)
   {REG_IC_CTRL2, 0x00}, // I2C WDT disabled, Charging enabled (if /CE pin is low)

   /* ---------- ADC engine -------------------------------------------------- */
   {REG_ADC_CTRL0, 0x80},   // Continuous conversions @1 Hz, ADC conv time = 24ms, comp disabled.
   {REG_ADC_CTRL1, 0x00},   // Disable ADC Comparators 2 and 3.
   {REG_ADC_READ_EN, 0x98}, // Enable reads for IIN | VIN | VBAT channels

   /* ---------- JEITA temperature windows ---------------------------------- */
   {REG_TS_FASTCHG_CTRL, 0x34}, // Reduced target battery voltage during Warm: VBAT_REG - 150mV
                                // Fast charge current when decreased by TS function: 0.500 x ICHG
   {REG_TS_COLD, 0x7C},         // 0 °C
   {REG_TS_COOL, 0x5D},         // 10 °C
   {REG_TS_WARM, 0x38},         // 45 °C
   {REG_TS_HOT, 0x27},          // 60 °C

   /* ---------- Optional ADC-comparator alarms (disabled by default) ------- */
   {REG_ADCALARM_COMP1_MSB, 0x00}, // All comparators inactive
   {REG_ADCALARM_COMP1_LSB, 0x00},
   {REG_ADCALARM_COMP2_MSB, 0x00},
   {REG_ADCALARM_COMP2_LSB, 0x00},
   {REG_ADCALARM_COMP3_MSB, 0x00},
   {REG_ADCALARM_COMP3_LSB, 0x00}};
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // BQ25150_H_
