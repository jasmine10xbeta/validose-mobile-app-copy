/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */
#ifndef PCF85363A_H_
#define PCF85363A_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
// Register addresses
#define PCF85363A_REG_100TH_SECONDS 0x00
#define PCF85363A_REG_SECONDS       0x01
#define PCF85363A_REG_MINUTES       0x02
#define PCF85363A_REG_HOURS         0x03
#define PCF85363A_REG_DAYS          0x04
#define PCF85363A_REG_WEEKDAYS      0x05
#define PCF85363A_REG_MONTHS        0x06
#define PCF85363A_REG_YEARS         0x07

// Control registers and default values
#define PCF85363A_FUNC_CTRL_ADR 0x28
// Disable 100th seconds, disable periodic interrupt, set RTC mode, stop controlled by STOP bit only, default clock
// output frequency.
#define PCF85363A_FUNC_CTRL_EN_RTC_DIS_PI_DEF_FREQ 0x00
#define PCF85363A_CTRL_STOP_ADR                    0x2E
#define PCF85363A_CTRL_STOP_SET                    0x01
#define PCF85363A_CTRL_STOP_CLEAR                  0x00
#define PCF85363A_CTRL_OSC_ADR                     0x25
// CLKIV=non-inv, offset calibration mode=normal, 24h mode, LOWJ=normal, osc Rs=60kohm, osc Cl=6.0pF
#define PCF85363A_CTRL_OSC        0x05
#define PCF85363A_CTRL_PIN_IO_ADR 0x27
// Disable CLK pin, rest set to default.
#define PCF85363A_CTRL_PIN_IO                 0x80
#define PCF85363A_CTRL_INTA_EN_ADR            0x29
#define PCF85363A_CTRL_INTB_EN_ADR            0x2A
#define PCF85363A_CTRL_INT_EN_DISABLE_ALL_INT 0x00

#define PCF85363A_CTRL_BATT_ADR 0x26
// Enable automatic switch to battery at Vth=1.5V
#define PCF85363A_CTRL_BATT_DEFAULT          0x00
#define PCF85363A_CTRL_FLAGS_ADR             0x2B
#define PCF85363A_CTRL_RESET_REG_ADR         0x2F
#define PCF85363A_CTRL_RESET_CLEAR_PRESCALER 0xA4

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

#endif // PCF85363A_H_