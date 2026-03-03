/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file npm1300.h
 * @ingroup pmic_driver
 * @brief List of registers and addresses for the Nordic PMIC NPM1300.
 */

#ifndef NPM1300_H_
#define NPM1300_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <stdint.h>
/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
/**
 * The datasheet (4490_483 v1.1) used as the source for all register values and addresses can be found at:
 * https://www.nordicsemi.com/Products/nPM1300/Downloads
 *
 * @note Register addresses are prefixed with REG_
 * @note Pre-calculated configurations are prefixed with CFG_
 * @note The register naming conventions are as follows:
 * - TASK signifies registers that initiate specific tasks.
 * - STATUS signifies read-only registers that provide status information.
 * - SET signifies registers that set specific configurations.
 * - CLR signifies registers that clear specific configurations. Set and clear should be used together. I.e. use set
 * registers for setting and clear registers for clearing.
 * - RESULTS signifies read-only registers that provide results from specific system monitoring tasks such as ADC
 * measurements from Vbat, battery NTC, etc.
 * - INT signifies registers that trigger specific interrupts.
 * - EVENTS signifies registers that handles the interrupt events statuses.
 *    - EVENTS<something>SET signifies an event register that indicates that an event has occurred. Usually these can be
 * written to for debugging purposes.
 *    - EVENTS<something>CLR signifies an event register that clears an event. Writing 1 to some specific event flag
 * will clear the flag.
 * - INTENEVENTS signifies registers that enable/disable specific interrupts.
 *
 * @note The datasheet does not provide registers for reading ibat and vbus. See Nordic devzone ticket here:
 * https://devzone.nordicsemi.com/f/nordic-q-a/115462/incomplete-documentation-for-npm1300
 */

// Base addresses for key instances
#define REG_EVENT_INTERRUPT_BASE 0x0000
#define REG_VBUSIN_BASE          0x0200
#define REG_BCHARGER_BASE        0x0300
#define REG_BUCK_BASE            0x0400
#define REG_ADC_BASE             0x0500
#define REG_GPIO_BASE            0x0600
#define REG_TIMER_BASE           0x0700
#define REG_POF_BASE             0x0900
#define REG_LOADSW_BASE          0x0800
#define REG_LEDDRV_BASE          0x0A00
#define REG_SHIP_BASE            0x0B00
#define REG_ERRLOG_BASE          0x0E00

////////////////////////////   Core  ////////////////////////////////////////////////
// core
// VBUSIN Register Offsets
#define REG_TASKUPDATEILIMSW_W   (REG_VBUSIN_BASE + 0x00) // Select Input Current limit for VBUS
#define REG_VBUSINILIM0_RW       (REG_VBUSIN_BASE + 0x01) // Current limit setting for VBUS
#define REG_VBUSINILIMSTARTUP_RW (REG_VBUSIN_BASE + 0x02) // VBUS input current limit at startup
#define REG_VBUSSUSPEND_RW       (REG_VBUSIN_BASE + 0x03) // Suspend mode enable
#define REG_USBCDETECTSTATUS_R   (REG_VBUSIN_BASE + 0x05) // VBUS CC comparator status flags
#define REG_VBUSINSTATUS_R       (REG_VBUSIN_BASE + 0x07) // VBUS status flags

#define VBUS_DETECTED_MASK           (1 << 0)
#define VBUSINCURRLIMACTIVE_MASK     (1 << 1)
#define VBUSINOVRPROTACTIVE_MASK     (1 << 2)
#define VBUSINUNDERVOLTAGE_MASK      (1 << 3)
#define VBUSINSUSPENDMODEACTIVE_MASK (1 << 4)
#define VBUS_OUT_ACTIVE_MASK         (1 << 5)
#define VBUS_GOOD_MASK               (0x21)

#define CFG_SUSPEND_CHARGING (0x01)
#define CFG_ENABLE_CHARGING  (0x00)

const uint16_t vbusin_register_configs[][2]
   = {{REG_TASKUPDATEILIMSW_W, 0x01},   // Select input current limit for VBUS
      {REG_VBUSINILIM0_RW, 0x04},       // Set VBUS current limit to 300mA
      {REG_VBUSINILIMSTARTUP_RW, 0x04}, // Set VBUS current limit at startup to 300mA
      {REG_VBUSSUSPEND_RW, CFG_ENABLE_CHARGING}};

////////////////////////////   Charger  ////////////////////////////////////////////////
// BCHARGER Register Offsets with Access Types
#define REG_TASKRELEASEERR_W        (REG_BCHARGER_BASE + 0x00) // Write-only: Release charger from error state
#define REG_TASKCLEARCHGERR_W       (REG_BCHARGER_BASE + 0x01) // Write-only: Clear charger error registers
#define REG_TASKCLEARSAFETYTIMER_W  (REG_BCHARGER_BASE + 0x02) // Write-only: Clear safety timers
#define REG_BCHGENABLESET_W         (REG_BCHARGER_BASE + 0x04) // Write-only: Charger enable set
#define REG_BCHGENABLECLR_W         (REG_BCHARGER_BASE + 0x05) // Write-only: Charger enable clear
#define REG_BCHGDISABLESET_W        (REG_BCHARGER_BASE + 0x06) // Write-only: Charger disable set
#define REG_BCHGDISABLECLR_W        (REG_BCHARGER_BASE + 0x07) // Write-only: Charger disable clear
#define REG_BCHGISETMSB_RW          (REG_BCHARGER_BASE + 0x08) // Read/Write: Charger current setting MSB
#define REG_BCHGISETLSB_RW          (REG_BCHARGER_BASE + 0x09) // Read/Write: Charger current setting LSB
#define REG_BCHGISETDISCHARGEMSB_RW (REG_BCHARGER_BASE + 0x0A) // Read/Write: Discharge current setting MSB
#define REG_BCHGISETDISCHARGELSB_RW (REG_BCHARGER_BASE + 0x0B) // Read/Write: Discharge current setting LSB
#define REG_BCHGVTERM_RW            (REG_BCHARGER_BASE + 0x0C) // Read/Write: Termination voltage at normal temperature
#define REG_BCHGVTERMR_RW           (REG_BCHARGER_BASE + 0x0D) // Read/Write: Termination voltage at warm temperature
#define REG_BCHGVTRICKLESEL_RW      (REG_BCHARGER_BASE + 0x0E) // Read/Write: Trickle charge level select
#define REG_BCHGITERMSEL_RW         (REG_BCHARGER_BASE + 0x0F) // Read/Write: Termination current level select
#define REG_NTCCOLD_RW              (REG_BCHARGER_BASE + 0x10) // Read/Write: NTC threshold for cold region
#define REG_NTCCOLDLSB_RW           (REG_BCHARGER_BASE + 0x11) // Read/Write: NTC threshold for cold region LSB
#define REG_NTCCOOL_RW              (REG_BCHARGER_BASE + 0x12) // Read/Write: NTC threshold for cool region
#define REG_NTCCOOLLSB_RW           (REG_BCHARGER_BASE + 0x13) // Read/Write: NTC threshold for cool region LSB
#define REG_NTCWARM_RW              (REG_BCHARGER_BASE + 0x14) // Read/Write: NTC threshold for warm region
#define REG_NTCWARMLSB_RW           (REG_BCHARGER_BASE + 0x15) // Read/Write: NTC threshold for warm region LSB
#define REG_NTCHOT_RW               (REG_BCHARGER_BASE + 0x16) // Read/Write: NTC threshold for hot region
#define REG_NTCHOTLSB_RW            (REG_BCHARGER_BASE + 0x17) // Read/Write: NTC threshold for hot region LSB
#define REG_DIETEMPSTOP_RW          (REG_BCHARGER_BASE + 0x18) // Read/Write: DIE temperature stop threshold
#define REG_DIETEMPSTOPLSB_RW       (REG_BCHARGER_BASE + 0x19) // Read/Write: DIE temperature stop threshold LSB
#define REG_DIETEMPRESUME_RW        (REG_BCHARGER_BASE + 0x1A) // Read/Write: DIE temperature resume threshold
#define REG_DIETEMPRESUMELSB_RW     (REG_BCHARGER_BASE + 0x1B) // Read/Write: DIE temperature resume threshold LSB
#define REG_BCHGILIMSTATUS_R        (REG_BCHARGER_BASE + 0x2D) // Read-only: Charger current limit status
#define REG_NTCSTATUS_R             (REG_BCHARGER_BASE + 0x32) // Read-only: NTC comparator status
#define REG_DIETEMPSTATUS_R         (REG_BCHARGER_BASE + 0x33) // Read-only: DIE temperature comparator status
#define REG_BCHGCHARGESTATUS_R      (REG_BCHARGER_BASE + 0x34) // Read-only: Charging status
#define REG_BCHGERRREASON_R         (REG_BCHARGER_BASE + 0x36) // Read-only: Latched error reasons
#define REG_BCHGERRSENSOR_R         (REG_BCHARGER_BASE + 0x37) // Read-only: Latched sensor error values
#define REG_BCHGCONFIG_RW           (REG_BCHARGER_BASE + 0x3C) // Read/Write: Charger configuration register

#define NTC_COLD_MASK (1 << 0)
#define NTC_COOL_MASK (1 << 1)
#define NTC_WARM_MASK (1 << 2)
#define NTC_HOT_MASK  (1 << 3)

#define BATTERY_DETECTED_MASK                    (1 << 0)
#define BATTERY_CHARGING_COMPLETED_MASK          (1 << 1)
#define BATTERY_CHARGING_TRICKLE_MASK            (1 << 2)
#define BATTERY_CHARGING_CC_MASK                 (1 << 3)
#define BATTERY_CHARGING_CV_MASK                 (1 << 4)
#define BATTERY_CHARGING_STOP_HIGH_DIE_TEMP_MASK (1 << 6)

const uint16_t charger_register_configs[][2] = {
   {REG_BCHGENABLECLR_W, 0x01},        // Charger disable fisrt to allow setting of charge current
   {REG_TASKRELEASEERR_W, 0x01},       // Release charger from error state
   {REG_TASKCLEARCHGERR_W, 0x01},      // Clear charger error registers
   {REG_TASKCLEARSAFETYTIMER_W, 0x01}, // Clear safety timers
   {REG_BCHGDISABLESET_W, 0x00},       // SKIP: Disable recharging of battery once charged
   {REG_BCHGDISABLECLR_W, 0x01},       // Enable recharging of battery once charged
   {REG_BCHGISETMSB_RW, 0x32},         // Charger current setting MSB ( Ichrg/4:  0x19=100mA: 0x32=200mA upper bits zero)
   {REG_BCHGISETLSB_RW, 0x00},         // Charger current setting LSB (LSB = 2mA) (Ichrg/2 : 0 for evern, 1 for uneven)
   {REG_BCHGISETDISCHARGEMSB_RW, 207}, // Battery discharge current limit (MSB=207, LSB=1 for 1A current limit)
   {REG_BCHGISETDISCHARGELSB_RW, 1},   // Battery discharge current limit (see datasheet)
   {REG_BCHGVTERM_RW, 7u},             // Termination voltage at normal temp (4.15V)
   {REG_BCHGVTERMR_RW, 2u},            // Termination voltage at warm temp (3.6V)
   {REG_BCHGVTRICKLESEL_RW, 0x00},     // Trickle charge voltage level (2.9V)
   {REG_BCHGITERMSEL_RW, 0x00},        // Termination current level select (10% of I_CHG)
   // Note the temperature values below refer to thermistor values. See the datasheet Table 13 to 15 for example values
   // or look at the equations to calculate custom values.
   {REG_NTCCOLD_RW, 0xBB},        // NTC cold threshold (NTC value (749) for 0°C)
   {REG_NTCCOLDLSB_RW, 0x01},     // NTC cold threshold LSB
   {REG_NTCCOOL_RW, 0xA4},        // NTC cool threshold (NTC value for 10°C)
   {REG_NTCCOOLLSB_RW, 0x02},     // NTC cool threshold LSB
   {REG_NTCWARM_RW, 0x54},        // NTC warm threshold (NTC value for 45°C)
   {REG_NTCWARMLSB_RW, 0x01},     // NTC warm threshold LSB
   {REG_NTCHOT_RW, 0x3B},         // NTC hot threshold (NTC value for 60°C)
   {REG_NTCHOTLSB_RW, 0x01},      // NTC hot threshold LSB
   {REG_DIETEMPSTOP_RW, 0x60},    // DIE temperature stop threshold (90°C) (see datasheet Table 15 for register value)
   {REG_DIETEMPSTOPLSB_RW, 0x00}, // DIE temperature stop threshold LSB
   {REG_DIETEMPRESUME_RW, 0x60},  // DIE temperature resume threshold (80°C)
   {REG_DIETEMPRESUMELSB_RW, 0x01}, // DIE temperature resume threshold LSB
   {REG_BCHGCONFIG_RW, 0x01},       // Disable charging when the battery is warm
   {REG_BCHGENABLESET_W, 0x01},     // Charger enabled, cool charging disabled
};
////////////////////////////   BUCK  ////////////////////////////////////////////////
// BUCK Register Offsets
#define REG_BUCK1ENASET       (REG_BUCK_BASE + 0x00) // BUCK1 Enable pulse
#define REG_BUCK1ENACLR       (REG_BUCK_BASE + 0x01) // BUCK1 Disable pulse
#define REG_BUCK2ENASET       (REG_BUCK_BASE + 0x02) // BUCK2 Enable pulse
#define REG_BUCK2ENACLR       (REG_BUCK_BASE + 0x03) // BUCK2 Disable pulse
#define REG_BUCK1PWMSET       (REG_BUCK_BASE + 0x04) // BUCK1 PWM mode enable pulse
#define REG_BUCK1PWMCLR       (REG_BUCK_BASE + 0x05) // BUCK1 PWM mode disable pulse
#define REG_BUCK2PWMSET       (REG_BUCK_BASE + 0x06) // BUCK2 PWM mode enable pulse
#define REG_BUCK2PWMCLR       (REG_BUCK_BASE + 0x07) // BUCK2 PWM mode disable pulse
#define REG_BUCK1NORMVOUT     (REG_BUCK_BASE + 0x08) // BUCK1 Output voltage Normal mode
#define REG_BUCK1RETVOUT      (REG_BUCK_BASE + 0x09) // BUCK1 Output voltage Retention mode
#define REG_BUCK2NORMVOUT     (REG_BUCK_BASE + 0x0A) // BUCK2 Output voltage Normal mode
#define REG_BUCK2RETVOUT      (REG_BUCK_BASE + 0x0B) // BUCK2 Output voltage Retention mode
#define REG_BUCKENCTRL        (REG_BUCK_BASE + 0x0C) // BUCK Enable GPIO Select
#define REG_BUCKVRETCTRL      (REG_BUCK_BASE + 0x0D) // BUCK Retention Voltage select
#define REG_BUCKPWMCTRL       (REG_BUCK_BASE + 0x0E) // BUCK Forced PWM mode GPIO select
#define REG_BUCKSWCTRLSEL     (REG_BUCK_BASE + 0x0F) // BUCK Software Control select
#define REG_BUCK1VOUTSTATUS_R (REG_BUCK_BASE + 0x10) // BUCK1 VOUT Status register
#define REG_BUCK2VOUTSTATUS_R (REG_BUCK_BASE + 0x11) // BUCK2 VOUT Status register
#define REG_BUCKCTRL0         (REG_BUCK_BASE + 0x15) // BUCK Auto PFM to PWM Control select
#define REG_BUCKSTATUS_R      (REG_BUCK_BASE + 0x34) // BUCK Status register

#define BUCK1_ENABLE_MASK (1 << 0)

const uint16_t buck_register_configs[][2] = {
   {REG_BUCK1ENASET, 0x00},   // Disable BUCK1
   {REG_BUCK1ENACLR, 0x01},   // Disable BUCK1
   {REG_BUCK2ENASET, 0x01},   // Enable BUCK2 (0x01 to enable)
   {REG_BUCK2ENACLR, 0x00},   // Disable BUCK2
   {REG_BUCK1PWMSET, 0x00},   // No effect. Force PWM mode for BUCK1 = 0x01
   {REG_BUCK1PWMCLR, 0x01},   // Set BUCK1 auto mode
   {REG_BUCK2PWMSET, 0x00},   // No effect. Force PWM mode for BUCK1 = 0x01
   {REG_BUCK2PWMCLR, 0x01},   // Set BUCK2 auto mode
   {REG_BUCK1NORMVOUT, 20u},  // BUCK1 Normal output voltage 3.0V
   {REG_BUCK1RETVOUT, 2u},    // BUCK1 Retention output voltage 1.2V (default)
   {REG_BUCK2NORMVOUT, 24u},  // BUCK2 Normal output voltage 3.3V
   {REG_BUCK2RETVOUT, 2u},    // BUCK2 Retention output voltage 1.2V
   {REG_BUCKENCTRL, 0x00},    // No GPIO control for BUCK enable
   {REG_BUCKVRETCTRL, 0x00},  // Retention voltage control via GPIO = off
   {REG_BUCKPWMCTRL, 0x00},   // No PWM control via GPIO
   {REG_BUCKSWCTRLSEL, 0x00}, // BUCK VSET res for BUCK2
   {REG_BUCKCTRL0, 0x00},     // Auto PFM to PWM control setting, no pull downs.
};
////////////////////////////   Load switch / LDO  ////////////////////////////////////////////////
// LOADSW/LDO Register Offsets
#define REG_TASKLDSW1SET (REG_LOADSW_BASE + 0x00) // Enable LDSW1
#define REG_TASKLDSW1CLR (REG_LOADSW_BASE + 0x01) // Disable LDSW1
#define REG_TASKLDSW2SET (REG_LOADSW_BASE + 0x02) // Enable LDSW2
#define REG_TASKLDSW2CLR (REG_LOADSW_BASE + 0x03) // Disable LDSW2
#define REG_LDSWSTATUS_R (REG_LOADSW_BASE + 0x04) // Load Switch Status
#define REG_LDSW1GPISEL  (REG_LOADSW_BASE + 0x05) // GPIO Control Select for LDSW1
#define REG_LDSW2GPISEL  (REG_LOADSW_BASE + 0x06) // GPIO Control Select for LDSW2
#define REG_LDSWCONFIG   (REG_LOADSW_BASE + 0x07) // Load Switch Configuration
#define REG_LDSW1LDOSEL  (REG_LOADSW_BASE + 0x08) // Load Switch1 / LDO1 Select
#define REG_LDSW2LDOSEL  (REG_LOADSW_BASE + 0x09) // Load Switch2 / LDO2 Select
#define REG_LDSW1VOUTSEL (REG_LOADSW_BASE + 0x0C) // LDO1 programmable output voltage
#define REG_LDSW2VOUTSEL (REG_LOADSW_BASE + 0x0D) // LDO2 programmable output voltage

#define TASKLDSW_ENABLE_MASK (1 << 0)
#define SWITCH1_STATUS_MASK  (1 << 0)
#define SWITCH2_STATUS_MASK  (1 << 2)

const uint16_t ldo_switch_register_configs[][2] = {
   {REG_TASKLDSW1SET, 0x00}, // Enable LDSW1
   {REG_TASKLDSW1CLR, 0x01}, // No effect. Disable = 0x01
   {REG_TASKLDSW2SET, 0x00}, // Enable LDSW2
   {REG_TASKLDSW2CLR, 0x01}, // No effect. Disable = 0x01
   {REG_LDSW1GPISEL, 0x00},  // Disabled (SW driven only)
   {REG_LDSW2GPISEL, 0x00},  // Disabled (SW driven only)
   {REG_LDSWCONFIG, 0x00},   // SW1&2: Softstart=10mA, no active discharge
   {REG_LDSW1LDOSEL, 0x00},  // Set as load switch
   {REG_LDSW2LDOSEL, 0x00},  // Set as load switch
   {REG_LDSW1VOUTSEL, 0x00}, // LDO1 output voltage set to 1.0V (if used)
   {REG_LDSW2VOUTSEL, 0x00}  // LDO2 output voltage set to 1.0V (if used)
};
////////////////////////////   LED  ////////////////////////////////////////////////
// LEDDRV Register Offsets
#define REG_LEDDRV0MODESEL (REG_LEDDRV_BASE + 0x00) // Select for LED_0 mode
#define REG_LEDDRV1MODESEL (REG_LEDDRV_BASE + 0x01) // Select for LED_1 mode
#define REG_LEDDRV2MODESEL (REG_LEDDRV_BASE + 0x02) // Select for LED_2 mode
#define REG_LEDDRV0SET     (REG_LEDDRV_BASE + 0x03) // Set LED_0 to be On
#define REG_LEDDRV0CLR     (REG_LEDDRV_BASE + 0x04) // Clear LED_0 to be Off
#define REG_LEDDRV1SET     (REG_LEDDRV_BASE + 0x05) // Set LED_1 to be On
#define REG_LEDDRV1CLR     (REG_LEDDRV_BASE + 0x06) // Clear LED_1 to be Off
#define REG_LEDDRV2SET     (REG_LEDDRV_BASE + 0x07) // Set LED_2 to be On
#define REG_LEDDRV2CLR     (REG_LEDDRV_BASE + 0x08) // Clear LED_2 to be Off

const uint16_t led_register_configs[][2] = {
   {REG_LEDDRV0MODESEL, 3u}, // LED_0 mode set to "Not used"
   {REG_LEDDRV1MODESEL, 3u}, // LED_1 mode set to "Not used"
   {REG_LEDDRV2MODESEL, 3u}, // LED_2 mode set to "Not used"
   {REG_LEDDRV0CLR, 0x01},   // Clear LED_0 (turn Off)
   {REG_LEDDRV1CLR, 0x01},   // Clear LED_1 (turn Off)
   {REG_LEDDRV2CLR, 0x01}    // Clear LED_2 (turn Off)
};
////////////////////////////   GPIO  ////////////////////////////////////////////////
// GPIO Register Offsets
#define REG_GPIOMODE0      (REG_GPIO_BASE + 0x00) // GPIO0 Mode Configuration
#define REG_GPIOMODE1      (REG_GPIO_BASE + 0x01) // GPIO1 Mode Configuration
#define REG_GPIOMODE2      (REG_GPIO_BASE + 0x02) // GPIO2 Mode Configuration
#define REG_GPIOMODE3      (REG_GPIO_BASE + 0x03) // GPIO3 Mode Configuration
#define REG_GPIOMODE4      (REG_GPIO_BASE + 0x04) // GPIO4 Mode Configuration
#define REG_GPIODRIVE0     (REG_GPIO_BASE + 0x05) // GPIO0 Drive Strength Configuration
#define REG_GPIODRIVE1     (REG_GPIO_BASE + 0x06) // GPIO1 Drive Strength Configuration
#define REG_GPIODRIVE2     (REG_GPIO_BASE + 0x07) // GPIO2 Drive Strength Configuration
#define REG_GPIODRIVE3     (REG_GPIO_BASE + 0x08) // GPIO3 Drive Strength Configuration
#define REG_GPIODRIVE4     (REG_GPIO_BASE + 0x09) // GPIO4 Drive Strength Configuration
#define REG_GPIOPUEN0      (REG_GPIO_BASE + 0x0A) // GPIO0 Pull-Up Enable
#define REG_GPIOPUEN1      (REG_GPIO_BASE + 0x0B) // GPIO1 Pull-Up Enable
#define REG_GPIOPUEN2      (REG_GPIO_BASE + 0x0C) // GPIO2 Pull-Up Enable
#define REG_GPIOPUEN3      (REG_GPIO_BASE + 0x0D) // GPIO3 Pull-Up Enable
#define REG_GPIOPUEN4      (REG_GPIO_BASE + 0x0E) // GPIO4 Pull-Up Enable
#define REG_GPIOPDEN0      (REG_GPIO_BASE + 0x0F) // GPIO0 Pull-Down Enable
#define REG_GPIOPDEN1      (REG_GPIO_BASE + 0x10) // GPIO1 Pull-Down Enable
#define REG_GPIOPDEN2      (REG_GPIO_BASE + 0x11) // GPIO2 Pull-Down Enable
#define REG_GPIOPDEN3      (REG_GPIO_BASE + 0x12) // GPIO3 Pull-Down Enable
#define REG_GPIOPDEN4      (REG_GPIO_BASE + 0x13) // GPIO4 Pull-Down Enable
#define REG_GPIOOPENDRAIN0 (REG_GPIO_BASE + 0x14) // GPIO0 Open Drain Configuration
#define REG_GPIOOPENDRAIN1 (REG_GPIO_BASE + 0x15) // GPIO1 Open Drain Configuration
#define REG_GPIOOPENDRAIN2 (REG_GPIO_BASE + 0x16) // GPIO2 Open Drain Configuration
#define REG_GPIOOPENDRAIN3 (REG_GPIO_BASE + 0x17) // GPIO3 Open Drain Configuration
#define REG_GPIOOPENDRAIN4 (REG_GPIO_BASE + 0x18) // GPIO4 Open Drain Configuration
#define REG_GPIODEBOUNCE0  (REG_GPIO_BASE + 0x19) // GPIO0 Debounce Configuration
#define REG_GPIODEBOUNCE1  (REG_GPIO_BASE + 0x1A) // GPIO1 Debounce Configuration
#define REG_GPIODEBOUNCE2  (REG_GPIO_BASE + 0x1B) // GPIO2 Debounce Configuration
#define REG_GPIODEBOUNCE3  (REG_GPIO_BASE + 0x1C) // GPIO3 Debounce Configuration
#define REG_GPIODEBOUNCE4  (REG_GPIO_BASE + 0x1D) // GPIO4 Debounce Configuration
#define REG_GPIOSTATUS     (REG_GPIO_BASE + 0x1E) // GPIO Status from GPIO Pads

const uint16_t gpio_register_configs[][2] = {
   {REG_GPIOMODE0, 7u},        // GPIO Mode: Output for power loss warning
   {REG_GPIOPUEN0, 0x00},      // GPIO Pull-up disabled
   {REG_GPIOPDEN0, 0x01},      // GPIO Pull-down enabled
   {REG_GPIODRIVE0, 0x00},     // GPIO Drive strength set to 1mA
   {REG_GPIOOPENDRAIN0, 0x00}, // GPIO Open drain disabled
   {REG_GPIODEBOUNCE0, 0x00},  // GPIO Debounce disabled

   {REG_GPIOMODE1, 0x00},      // GPIO Mode: input
   {REG_GPIOPUEN1, 0x00},      // GPIO Pull-up disabled
   {REG_GPIOPDEN1, 0x01},      // GPIO Pull-down enabled
   {REG_GPIODRIVE1, 0x00},     // GPIO Drive strength set to 1mA
   {REG_GPIOOPENDRAIN1, 0x00}, // GPIO Open drain disabled
   {REG_GPIODEBOUNCE1, 0x00},  // GPIO Debounce disabled

   {REG_GPIOMODE2, 0x00},      // GPIO Mode: input
   {REG_GPIOPUEN2, 0x00},      // GPIO Pull-up disabled
   {REG_GPIOPDEN2, 0x01},      // GPIO Pull-down enabled
   {REG_GPIODRIVE2, 0x00},     // GPIO Drive strength set to 1mA
   {REG_GPIOOPENDRAIN2, 0x00}, // GPIO Open drain disabled
   {REG_GPIODEBOUNCE2, 0x00},  // GPIO Debounce disabled

   {REG_GPIOMODE3, 0x00},      // GPIO Mode: input
   {REG_GPIOPUEN3, 0x00},      // GPIO Pull-up disabled
   {REG_GPIOPDEN3, 0x01},      // GPIO Pull-down enabled
   {REG_GPIODRIVE3, 0x00},     // GPIO Drive strength set to 1mA
   {REG_GPIOOPENDRAIN3, 0x00}, // GPIO Open drain disabled
   {REG_GPIODEBOUNCE3, 0x00},  // GPIO Debounce disabled

   {REG_GPIOMODE4, 0x00},      // GPIO Mode: input
   {REG_GPIOPUEN4, 0x00},      // GPIO Pull-up disabled
   {REG_GPIOPDEN4, 0x01},      // GPIO Pull-down enabled
   {REG_GPIODRIVE4, 0x00},     // GPIO Drive strength set to 1mA
   {REG_GPIOOPENDRAIN4, 0x00}, // GPIO Open drain disabled
   {REG_GPIODEBOUNCE4, 0x00},  // GPIO Debounce disabled
};

////////////////////////////   System monitor (measurements via ADC)  //////////////////////////////////////
// ADC Register Offsets
#define REG_TASKVBATMEASURE        (REG_ADC_BASE + 0x00) // Start VBAT measurement
#define REG_TASKNTCMEASURE         (REG_ADC_BASE + 0x01) // Start NTC thermistor measurement
#define REG_TASKTEMPMEASURE        (REG_ADC_BASE + 0x02) // Start Die Temperature measurement
#define REG_TASKVSYSMEASURE        (REG_ADC_BASE + 0x03) // Start VSYS measurement
#define REG_TASKIBATMEASURE        (REG_ADC_BASE + 0x06) // Start IBAT measurement
#define REG_TASKVBUS7MEASURE       (REG_ADC_BASE + 0x07) // Start VBUS measurement in 7V range
#define REG_TASKDELAYEDVBATMEASURE (REG_ADC_BASE + 0x08) // Start delayed VBAT measurement
#define REG_ADCCONFIG              (REG_ADC_BASE + 0x09) // ADC Configuration
#define REG_ADCNTCRSEL             (REG_ADC_BASE + 0x0A) // Select Battery NTC register
#define REG_ADCAUTOTIMCONF         (REG_ADC_BASE + 0x0B) // Configure auto measurement intervals
#define REG_TASKAUTOTIMUPDATE      (REG_ADC_BASE + 0x0C) // Toggle NTC and Die temp auto time
#define REG_ADCDELTIMCONF          (REG_ADC_BASE + 0x0D) // Vbat delay timer control
#define REG_ADCIBATMEASSTATUS_R    (REG_ADC_BASE + 0x10) // Battery current measurement status
#define REG_ADCVBATRESULTMSB_R     (REG_ADC_BASE + 0x11) // VBAT measurement result MSB
#define REG_ADCNTCRESULTMSB_R      (REG_ADC_BASE + 0x12) // NTC measurement result MSB
#define REG_ADCTEMPRESULTMSB_R     (REG_ADC_BASE + 0x13) // Die TEMP measurement result MSB
#define REG_ADCVSYSRESULTMSB_R     (REG_ADC_BASE + 0x14) // VSYS measurement result MSB
#define REG_ADCGP0RESULTLSBS_R     (REG_ADC_BASE + 0x15) // LSBs for Single-shot mode
#define REG_ADCVBAT0RESULTMSB_R    (REG_ADC_BASE + 0x16) // VBAT0 burst measurement result MSB
#define REG_ADCVBAT1RESULTMSB_R    (REG_ADC_BASE + 0x17) // VBAT1 burst measurement result MSB
#define REG_ADCVBAT2RESULTMSB_R    (REG_ADC_BASE + 0x18) // VBAT2 burst measurement result MSB
#define REG_ADCVBAT3RESULTMSB_R    (REG_ADC_BASE + 0x19) // VBAT3 burst measurement result MSB
#define REG_ADCGP1RESULTLSBS_R     (REG_ADC_BASE + 0x1A) // LSBs for burst mode results
#define REG_ADCIBATMEASEN          (REG_ADC_BASE + 0x24) // Enable auto IBAT measurement

const uint16_t system_monitor_register_configs[][2] = {
   // Start ADC measurements early to have something during initialization
   {REG_TASKVBATMEASURE, 0x01}, // Start VBAT measurement - auto
   {REG_TASKNTCMEASURE, 0x01},  // Start NTC thermistor measurement - auto
   {REG_TASKTEMPMEASURE, 0x01}, // Start Die Temperature measurement - auto
   {REG_TASKVSYSMEASURE, 0x01}, // Start VSYS measurement
   {REG_TASKIBATMEASURE, 0x01}, // Start IBAT measurement - auto

   {REG_ADCCONFIG, 0x03},      // Enable VBAT Auto measurement every 1 Second, burst mode enabled
   {REG_ADCAUTOTIMCONF, 0x03}, // Set NTC auto measurement interval = 1024ms, die temp auto measurement interval = 4ms

   {REG_TASKDELAYEDVBATMEASURE, 0x00}, // Start delayed VBAT measurement(0x01). No effect = 0x00
   {REG_ADCNTCRSEL, 0x01},             // Select Battery NTC register
   {REG_TASKAUTOTIMUPDATE, 0x00},      // toggle, handshake signal to flag NtcAutoTim and TempAutoTim (no effect=0x00)
   {REG_ADCDELTIMCONF, 0x00},          // Vbat delay timer control
   {REG_ADCIBATMEASEN, 0x01}           // Enable Auto IBAT measurement after VBAT task
};

const uint16_t system_monitor_disable_auto_measurement[][2] = {
   {REG_ADCCONFIG, 0x00},    // Single measuremet
   {REG_ADCIBATMEASEN, 0x00} // Disable Auto IBAT measurement after VBAT task
};

const uint16_t system_monitor_enable_auto_measurement[][2] = {
   {REG_ADCCONFIG, 0x03},    // Auto measurements
   {REG_ADCIBATMEASEN, 0x01} // Enable Auto IBAT measurement after VBAT task
};

////////////////////////////   POF  ////////////////////////////////////////////////
// POF Register Offsets
#define REG_POFCONFIG (REG_POF_BASE + 0x00)

const uint16_t pof_register_configs[][2] = {
   {REG_POFCONFIG, 0x03} // Enable POF, polarity active high, threshold at 2.8V,
};
////////////////////////////   TIMER  ////////////////////////////////////////////////
// TIMER Register Offsets
#define REG_TIMERSET          (REG_TIMER_BASE + 0x00) // Start Timer
#define REG_TIMERCLR          (REG_TIMER_BASE + 0x01) // Stop Timer
#define REG_TIMERTARGETSTROBE (REG_TIMER_BASE + 0x03) // Strobe for timer target
#define REG_WATCHDOGKICK      (REG_TIMER_BASE + 0x04) // Watchdog kick
#define REG_TIMERCONFIG       (REG_TIMER_BASE + 0x05) // Timer mode selection
#define REG_TIMERSTATUS_R     (REG_TIMER_BASE + 0x06) // Timer status
#define REG_TIMERHIBYTE       (REG_TIMER_BASE + 0x08) // Timer Most Significant Byte
#define REG_TIMERMIDBYTE      (REG_TIMER_BASE + 0x09) // Timer Middle Byte
#define REG_TIMERLOBYTE       (REG_TIMER_BASE + 0x0A) // Timer Least Significant Byte

const uint16_t timer_register_configs[][2] = {
   {REG_TIMERSET, 0x00},          // Start Timer (initialize as stopped) (start=0x01)
   {REG_TIMERCLR, 0x01},          // Stop Timer (no effect=0x00)
   {REG_TIMERTARGETSTROBE, 0x00}, // Strobe for target (unused in this configuration)
   {REG_WATCHDOGKICK, 0x00},      // Watchdog kick placeholder (kick dog=0x01, no effect=0x00)
   {REG_TIMERCONFIG, 0x03},       // Mode set to General purpose, prescaler set to Slow
   {REG_TIMERHIBYTE, 0x00},       // High byte of period (set to 0 for full timer period)
   {REG_TIMERMIDBYTE, 0x00},      // Middle byte of period (set to 0 for full timer period)
   {REG_TIMERLOBYTE, 0x00}        // Low byte of period (set to 0 for full timer period)
};

////////////////////////////   SHIP mode  ////////////////////////////////////////////////
// SHIP Register Offsets
#define REG_TASKENTERHIBERNATE  (REG_SHIP_BASE + 0x00) // Task Enter Hibernate
#define REG_TASKSHPHLDCFGSTROBE (REG_SHIP_BASE + 0x01) // Task Ship Hold Config Strobe
#define REG_TASKENTERSHIPMODE   (REG_SHIP_BASE + 0x02) // Task Enter ShipMode
#define REG_TASKRESETCFG        (REG_SHIP_BASE + 0x03) // Request Reset Config
#define REG_SHPHLDCONFIG        (REG_SHIP_BASE + 0x04) // Ship Hold Button Press Timer Config
#define REG_SHPHLDSTATUS_R      (REG_SHIP_BASE + 0x05) // Status of the SHPHLD pin
#define REG_LPRESETCONFIG       (REG_SHIP_BASE + 0x06) // Long Press Reset Config Register

const uint16_t ship_register_configs[][2] = {
   {REG_TASKENTERHIBERNATE, 0x00},  // Enter Hibernate Task=0x01, no effect=0x00
   {REG_TASKSHPHLDCFGSTROBE, 0x00}, // Load the SHPHLD Config: strobe config=0x01, no effect=0x00
   {REG_TASKENTERSHIPMODE, 0x00},   // Enter Shipmode (without Wakeup timer)=0x01, no effect=0x00
   {REG_TASKRESETCFG, 0x00},        // Request Reset Config=0x01, no effect=0x00
   {REG_SHPHLDCONFIG, 0x03},        // Ship-Hold button press timer duration = 96ms
   {REG_LPRESETCONFIG, 0x01}        // Disable long press reset
};
////////////////////////////   Event / interrupt  ////////////////////////////////////////////////
// Event and Interrupt Register Offsets
#define REG_TASKSWRESET (REG_EVENT_INTERRUPT_BASE + 0x01) // Task Force a full reboot power-cycle

// ADC Event Registers
#define REG_EVENTSADCSET      (REG_EVENT_INTERRUPT_BASE + 0x02) // Check which events are set
#define REG_EVENTSADCCLR      (REG_EVENT_INTERRUPT_BASE + 0x03) // Clear ADC measurement finished events
#define REG_INTENEVENTSADCSET (REG_EVENT_INTERRUPT_BASE + 0x04) // ADC Events Interrupt Enable Set
#define REG_INTENEVENTSADCCLR (REG_EVENT_INTERRUPT_BASE + 0x05) // ADC Events Interrupt Enable Clear

#define VBAT_MEASUREMENT_READY_MASK (1 << 0) // VBAT measurement ready event mask

// Battery Charger Event Registers
#define REG_EVENTSBCHARGER0SET (REG_EVENT_INTERRUPT_BASE + 0x06) // Check which events are set
#define REG_EVENTSBCHARGER0CLR (REG_EVENT_INTERRUPT_BASE + 0x07) // Battery Charger Temperature Events Clear
#define REG_INTENEVENTSBCHARGER0SET                                                                                    \
   (REG_EVENT_INTERRUPT_BASE + 0x08) // Battery Charger Temperature Events Interrupt Enable Set
#define REG_INTENEVENTSBCHARGER0CLR                                                                                    \
   (REG_EVENT_INTERRUPT_BASE + 0x09) // Battery Charger Temperature Events Interrupt Enable Clear

#define REG_EVENTSBCHARGER1SET (REG_EVENT_INTERRUPT_BASE + 0x0A) // Check which events are set
#define REG_EVENTSBCHARGER1CLR (REG_EVENT_INTERRUPT_BASE + 0x0B) // Battery Charger Status Events Clear
#define REG_INTENEVENTSBCHARGER1SET                                                                                    \
   (REG_EVENT_INTERRUPT_BASE + 0x0C) // Battery Charger Status Events Interrupt Enable Set
#define REG_INTENEVENTSBCHARGER1CLR                                                                                    \
   (REG_EVENT_INTERRUPT_BASE + 0x0D) // Battery Charger Status Events Interrupt Enable Clear

#define REG_EVENTSBCHARGER2SET (REG_EVENT_INTERRUPT_BASE + 0x0E) // Check which events are set
#define REG_EVENTSBCHARGER2CLR (REG_EVENT_INTERRUPT_BASE + 0x0F) // Battery Charger Battery Events Clear
#define REG_INTENEVENTSBCHARGER2SET                                                                                    \
   (REG_EVENT_INTERRUPT_BASE + 0x10) // Battery Charger Battery Events Interrupt Enable Set
#define REG_INTENEVENTSBCHARGER2CLR                                                                                    \
   (REG_EVENT_INTERRUPT_BASE + 0x11) // Battery Charger Battery Events Interrupt Enable Clear

// ShipHold Pin Event Registers
#define REG_EVENTSSHPHLDSET      (REG_EVENT_INTERRUPT_BASE + 0x12) // Check which events are set
#define REG_EVENTSSHPHLDCLR      (REG_EVENT_INTERRUPT_BASE + 0x13) // ShipHold Pin Events Clear
#define REG_INTENEVENTSSHPHLDSET (REG_EVENT_INTERRUPT_BASE + 0x14) // ShipHold Pin Events Interrupt Enable Set
#define REG_INTENEVENTSSHPHLDCLR (REG_EVENT_INTERRUPT_BASE + 0x15) // ShipHold Pin Events Interrupt Enable Clear

// VBUSIN Event Registers
#define REG_EVENTSVBUSIN0SET (REG_EVENT_INTERRUPT_BASE + 0x16) // Check which events are set
#define REG_EVENTSVBUSIN0CLR (REG_EVENT_INTERRUPT_BASE + 0x17) // VBUSIN Voltage Detection Events Clear
#define REG_INTENEVENTSVBUSIN0SET                                                                                      \
   (REG_EVENT_INTERRUPT_BASE + 0x18) // VBUSIN Voltage Detection Events Interrupt Enable Set
#define REG_INTENEVENTSVBUSIN0CLR                                                                                      \
   (REG_EVENT_INTERRUPT_BASE + 0x19) // VBUSIN Voltage Detection Events Interrupt Enable Clear

#define REG_EVENTSVBUSIN1SET (REG_EVENT_INTERRUPT_BASE + 0x1A) // Check which events are set
#define REG_EVENTSVBUSIN1CLR (REG_EVENT_INTERRUPT_BASE + 0x1B) // VBUSIN Thermal and USB Events Clear
#define REG_INTENEVENTSVBUSIN1SET                                                                                      \
   (REG_EVENT_INTERRUPT_BASE + 0x1C) // VBUSIN Thermal and USB Events Interrupt Enable Set
#define REG_INTENEVENTSVBUSIN1CLR                                                                                      \
   (REG_EVENT_INTERRUPT_BASE + 0x1D) // VBUSIN Thermal and USB Events Interrupt Enable Clear

// GPIO Event Registers
#define REG_EVENTSGPIOSET      (REG_EVENT_INTERRUPT_BASE + 0x22) // Check which events are set
#define REG_EVENTSGPIOCLR      (REG_EVENT_INTERRUPT_BASE + 0x23) // GPIO Events Clear
#define REG_INTENEVENTSGPIOSET (REG_EVENT_INTERRUPT_BASE + 0x24) // GPIO Events Interrupt Enable Set
#define REG_INTENEVENTSGPIOCLR (REG_EVENT_INTERRUPT_BASE + 0x25) // GPIO Events Interrupt Enable Clear

const uint16_t event_interrupt_register_configs[][2] = {
   {REG_INTENEVENTSADCSET, 0x01}, // Only enable ADC VBAT ready interrupt. All other meas. will be read along with VBAT.
   {REG_INTENEVENTSBCHARGER0SET, 0x3F}, // Enable all events (cold, cool, warm, hot, die temp high, die temp resume)
   {REG_INTENEVENTSBCHARGER1SET, 0x3E}, // EN charging states: trickle, cc, cv, complete, error
   {REG_INTENEVENTSBCHARGER1CLR, 0x01}, // Dis supplement mode
   {REG_INTENEVENTSBCHARGER2SET, 0x07}, // Enable all events (bat detected, bat lost, bat recharge)
   {REG_INTENEVENTSSHPHLDSET, 0x00},    // Disable all ship button events
   {REG_INTENEVENTSVBUSIN0SET, 0x3F},   // EN all events (VBUS: detected, lost, OV det, OV removed, UV det, UV removed)
   {REG_INTENEVENTSVBUSIN1SET, 0x0F},   // EN events(Thermal: warn det, warn removed, shutdown det, shtdwn removed)
   {REG_INTENEVENTSVBUSIN1CLR, 0x30},   // DIS events(VBUS cc events)
   {REG_INTENEVENTSGPIOCLR, 0x1F},      // Disable all GPIO edge detect events
};

// Clears all interrupt events.
const uint16_t clear_event_interrupt_registers[][2] = {
   {REG_EVENTSADCCLR, 0xFF},
   {REG_EVENTSBCHARGER0CLR, 0x3F},
   {REG_EVENTSBCHARGER1CLR, 0x3F},
   {REG_EVENTSBCHARGER2CLR, 0x3F},
   {REG_EVENTSSHPHLDCLR, 0x0F},
   {REG_EVENTSVBUSIN0CLR, 0x3F},
   {REG_EVENTSVBUSIN1CLR, 0x3F},
};

// Reset and error registers
// Error and Reset Register Offsets
#define REG_TASKCLRERRLOG      (REG_ERRLOG_BASE + 0x00) // Task to clear the error log registers
#define REG_SCRATCH0           (REG_ERRLOG_BASE + 0x01) // Scratch register 0
#define REG_RSTCAUSE_R         (REG_ERRLOG_BASE + 0x03) // Internal reset cause log, cleared with TASKCLRERRLOG
#define REG_CHARGERERRREASON_R (REG_ERRLOG_BASE + 0x04) // Charger error reason log, cleared with TASKCLRERRLOG
#define REG_CHARGERERRSENSOR_R (REG_ERRLOG_BASE + 0x05) // Charger sensor error log, cleared with TASKCLRERRLOG

#define SHIP_MODE_RESET_MASK (1 << 0) // Ship mode reset mask

const uint16_t error_registers_config[][2] = {
   {REG_TASKCLRERRLOG, 0x01}, // Clear error log registers
   {REG_SCRATCH0, 0x00},      // Disable boot monitor timer
};
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // NPM1300_H_
