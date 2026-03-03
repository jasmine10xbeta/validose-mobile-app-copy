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
#include "nrf_drv_spi.h"
#include "nrf_gpio.h"

/********************************************************************************************************************
 * Definitions
 *******************************************************************************************************************/
// VOD hardware
// Debug UART
#define UART_TX_SW_QUEUE_LEN     (20)
#define DEBUG_RX_SW_QUEUE_LEN    (3)
#define DEBUG_UARTE_INSTANCE     0
#define DEBUG_UARTE_RTC1_IDX     0
#define DEBUG_UARTE_RX_BUF_SIZE  255
#define DEBUG_UARTE_RX_BUF_COUNT 3
#define DEBUG_UARTE_TX_PIN       NRF_GPIO_PIN_MAP(0, 22)
#define DEBUG_UARTE_RX_PIN       NRF_GPIO_PIN_MAP(0, 23)
// Needs to be > APP_TIMER_CONFIG_IRQ_PRIORITY
#define DEBUG_UARTE_IRQ_PRIORITY 4

// Debug endpoints
#define DEBUG_ENDPOINT_IDX_UART         (ENDPOINT_UART)
#define DEBUG_ENDPOINT_IDX_DATA_MANAGER (ENDPOINT_USER_DEFINED_1)

#define SPI_IRQ_PRIORITY APP_IRQ_PRIORITY_HIGH

// SPI Flash
#define SPI_FLASH_INSTANCE 2
#define SPI_FLASH_MISO_PIN NRF_GPIO_PIN_MAP(0, 26)
#define SPI_FLASH_MOSI_PIN NRF_GPIO_PIN_MAP(1, 9)
#define SPI_FLASH_CLK_PIN  NRF_GPIO_PIN_MAP(1, 8)
#define SPI_FLASH_CS_PIN   NRF_GPIO_PIN_MAP(0, 25)
#define FLASH_WP_PIN       NRF_GPIO_PIN_MAP(0, 31)
#define FLASH_NRST_PIN     NRF_GPIO_PIN_MAP(0, 30)

// SPI ADC
#define SPI_ADC_INSTANCE  2 //
#define SPI_ADC_MISO_PIN  NRF_GPIO_PIN_MAP(0, 26)
#define SPI_ADC_MOSI_PIN  NRF_GPIO_PIN_MAP(1, 9)
#define SPI_ADC_SCK_PIN   NRF_GPIO_PIN_MAP(1, 8)
#define SPI_ADC_CS_PIN    NRF_GPIO_PIN_MAP(0, 27)
#define ADS1235_DRDY_PIN  NRF_GPIO_PIN_MAP(1, 7)
#define ADS1235_START_PIN NRF_GPIO_PIN_MAP(1, 5)
#define ADS1235_5V_EN_PIN NRF_GPIO_PIN_MAP(0, 6)

// Buzzer + Button
#define BUZZER_PWM_INSTANCE 1
#define BUZZER_PIN          NRF_GPIO_PIN_MAP(0, 19)
#define BUTTONS_COUNT       (1u)
#define BUTTON_PIN          NRF_GPIO_PIN_MAP(1, 14)

// LED driver
#define LEDDRV_SD1 NRF_GPIO_PIN_MAP(0, 13)
#define LEDDRV_SD2 NRF_GPIO_PIN_MAP(0, 7)
#define LED_RED    NRF_GPIO_PIN_MAP(1, 6)
#define LED_BLUE   NRF_GPIO_PIN_MAP(0, 29) // AIN5
#define LED_GREEN  NRF_GPIO_PIN_MAP(0, 28)

// IMU
#define IMU_INT1 NRF_GPIO_PIN_MAP(0, 18)
#define IMU_INT2 NRF_GPIO_PIN_MAP(0, 21)

// PMIC and QI charging
#define PMIC_GPIO_0 NRF_GPIO_PIN_MAP(0, 17)
#define PMIC_GPIO_1 NRF_GPIO_PIN_MAP(0, 20)
#define WC_EN1      NRF_GPIO_PIN_MAP(1, 1) // Wireless charging enable 1
#define WC_EN2      NRF_GPIO_PIN_MAP(1, 0) // Wireless charging enable 2

// NFC
#define NFC_ANT_A_EN NRF_GPIO_PIN_MAP(1, 3)
#define NFC_ANT_B_EN NRF_GPIO_PIN_MAP(1, 4)
#define NFC_IRQ_EN   NRF_GPIO_PIN_MAP(0, 12)
#define NFC_BSS_EN   NRF_GPIO_PIN_MAP(0, 11)

/**
 * I2C:
 * I2C_B (instance 0) is intended to be toggled on/off for power saving. It connects only
 *    - LED driver
 *    - NFC
 * I2C (Instance 1)
 *    - IMU
 *    - RTC
 *    - PMIC
 */
#define TWI_INSTANCE_0         0 // LEDs, NFC
#define TWI_INSTANCE_1         1 // IMU, RTC, PMIC
#define I2C_PULLUP             NRF_GPIO_PIN_MAP(1, 12)
#define I2C_SCL_PIN            NRF_GPIO_PIN_MAP(0, 16) // Instance 1
#define I2C_SDA_PIN            NRF_GPIO_PIN_MAP(0, 15) // Instance 1
#define I2C_SCL_B_PIN          NRF_GPIO_PIN_MAP(0, 5)  // Instance 0
#define I2C_SDA_B_PIN          NRF_GPIO_PIN_MAP(0, 4)  // Instance 0
#define PMIC_I2C_ADDRESS       (0x6B)
#define BMI323_I2C_ADDRESS     (0x68)
#define RTC_I2C_ADDRESS        (0x51)
#define NFC_READER_I2C_ADDRESS (0x50)
#define STS30_I2C_ADDRESS      (0x4A)

// Hardware timer instances
#define TIMER_INSTANCE_BLE_STACK   0
#define TIMER_INSTANCE_TX_COMMS    1
#define TIMER_INSTANCE_RX_COMMS    2
#define TIMER_INSTANCE_DEBUG_UARTE 4

// ---------------------------------- NFC driver compatibility macros -----------------------------
#define ST25R_TWI_INST    TWI_INSTANCE_0
#define ST25R_TWI_SCL_PIN I2C_SCL_B_PIN
#define ST25R_TWI_SDA_PIN I2C_SDA_B_PIN

// ST25R3916B IRQ into nRF52840 (active-high)
#define ST25R_IRQ_PIN NFC_IRQ_EN
// (Optional) HW reset pin if you wired it, else comment out
// #define ST25R_RESET_PIN        NRF_GPIO_PIN_MAP(0, 12)

// A LED to indicate a tag found
#define TAG_LED_PIN LED_BLUE

#endif // CUSTOM_BOARD_H