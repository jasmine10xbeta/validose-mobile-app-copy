/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef CUSTOM_BOARD_H
#define CUSTOM_BOARD_H

/********************************************************************************************************************
 * Includes
 *******************************************************************************************************************/
#include "nrf_gpio.h"
#include "tmd2635_driver.h"

/********************************************************************************************************************
 * Definitions
 *******************************************************************************************************************/

// Debug UART
#define UART_TX_SW_QUEUE_LEN     (20)
#define DEBUG_RX_SW_QUEUE_LEN    (3)
#define DEBUG_UARTE_INSTANCE     0
#define DEBUG_UARTE_RTC1_IDX     0
#define DEBUG_UARTE_RX_BUF_SIZE  255
#define DEBUG_UARTE_RX_BUF_COUNT 3
#define DEBUG_UARTE_TX_PIN       NRF_GPIO_PIN_MAP(0, 26)
#define DEBUG_UARTE_RX_PIN       NRF_GPIO_PIN_MAP(0, 23)
// Needs to be > APP_TIMER_CONFIG_IRQ_PRIORITY
#define DEBUG_UARTE_IRQ_PRIORITY 4

// LEDs
#define LED_RED   NRF_GPIO_PIN_MAP(0, 11)
#define LED_GREEN  NRF_GPIO_PIN_MAP(0, 7)   // SCH error: Blue and green swapped
#define LED_BLUE NRF_GPIO_PIN_MAP(0, 27)    // Sch error: Blue and green swapped

// Hardware interrupts
#define INT1    NRF_GPIO_PIN_MAP(1, 15)  // IMU
#define INT2    NRF_GPIO_PIN_MAP(0, 12) // IMU

#define IMU_INT1 INT1
#define IMU_INT2 INT2

// Battery IC 
/**
 * Charge Enable. Drive CE low or leave disconnected to enable charging when VIN is valid. Drive CE high to disable
 * charge when VIN is present. CE is pulled low internally with 900-kΩ resistor. CE has no effect when VIN is not
 * present.
 */
#define BM_CE NRF_GPIO_PIN_MAP(0, 5)
/**
 * Low Power Mode Enable. Drive this pin low to set the device in low power mode when powered by the battery. This pin
 * must be driven high to allow I2C communication when VIN is not present. LP is pulled low internally with 900-kΩ
 * resistor. This pin has no effect when VIN is present.
 */
#define BM_LP  NRF_GPIO_PIN_MAP(0, 13)
/**
 * Open-drain Power Good status indication output. PG is pulled to GND when VIN is above
 * VBAT+ VSLP and less than VOVP. PG is high-impedance when the input power is not within
 * specified limits. Connect PG to the desired logic voltage rail using a 1-kΩ to 100-kΩ resistor,
 * or use with an LED for visual indication. PG can also be configured through I2C as a
 * push-button level shifted output ( MR), where the output of the PG pin reflects the status of
 * the MR input, but pulled up to the desired logic voltage rail using a 1-kΩ to 100-kΩ resistor.
 * The PG pin can also be configured as a general purpose open drain output.
 */
#define BM_PG  NRF_GPIO_PIN_MAP(0, 2)
/**
 * INT is an open-drain output that signals fault interrupts. When a fault occurs, a 128-µs pulse
 * is sent out as an interrupt for the host. INT is enabled/disabled using the MASK_INT bit in
 * the control register
 */
#define BM_INT NRF_GPIO_PIN_MAP(0, 29)
/**
 * Manual Reset Input. MR is a general purpose input that must be held low for greater than
 * tHWRESET to go into HW Reset and power cycle the output rails. If MR is also used to wake
 * up the device out of Ship Mode when pressed for at least tWAKE2. MR has in internal 125-kΩ
 * pull-up resistor to BAT.
 */
#define BM_MR  NRF_GPIO_PIN_MAP(0, 31)

// Proximity sensor
#define PS_VDD NRF_GPIO_PIN_MAP(1, 5) // VDD enable for proximity sensor. High = device ON
#define PS_INT NRF_GPIO_PIN_MAP(0, 10)  // IR proximity sensor & BMS

// NFC
#define NFC_GPO     NRF_GPIO_PIN_MAP(0, 4) // NFC tag interrupt
#define NFC_PROTECT NRF_GPIO_PIN_MAP(1, 12)

// I2C
#define TWI_INSTANCE_0       0
#define SCL0                 NRF_GPIO_PIN_MAP(0, 15)
#define SDA0                 NRF_GPIO_PIN_MAP(0, 22)
#define I2C0_SCL_PIN          SCL0
#define I2C0_SDA_PIN          SDA0
#define BMI323_I2C_ADDRESS   (0x68)                     // IMU
#define TMD26353_I2C_ADDRESS (TMD2635_I2C_ADDR_SCL_SDA) // Proximity sensor

// I2C
#define TWI_INSTANCE_1       1
#define SCL1                 NRF_GPIO_PIN_MAP(0, 20)
#define SDA1                 NRF_GPIO_PIN_MAP(1, 0)
#define I2C1_SCL_PIN          SCL1
#define I2C1_SDA_PIN          SDA1
#define PMIC_I2C_ADDRESS     (0x6B)
#define ST25DV04K_I2C_ADDRESS()  //NFC

// Hardware timer instances
#define TIMER_INSTANCE_BLE_STACK   0 // DO NOT USE FOR ANYTHING ELSE
#define TIMER_INSTANCE_TX_COMMS    1
#define TIMER_INSTANCE_RX_COMMS    2
#define TIMER_INSTANCE_DEBUG_UARTE 4

#endif // CUSTOM_BOARD_H