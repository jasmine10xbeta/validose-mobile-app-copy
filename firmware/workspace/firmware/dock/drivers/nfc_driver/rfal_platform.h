
/**
 * @file rfal_platform.h
 * @brief RFAL platform abstraction required by the ST RFAL library.
 *
 * This header provides platform-specific macros, types, and function prototypes required by the STMicroelectronics
 * RFAL (RF Abstraction Layer) library. It is required for integration with the ST25R3916B NFC frontend and is
 * included by the external RFAL library. The implementation in this file is tailored for Nordic nRF52 series MCUs.
 *
 * - Defines IRQ pin, I2C address, and protection macros for critical sections.
 * - Provides hooks for IRQ enable/disable and worker protection (no-ops for single-threaded systems).
 * - Used by both the RFAL library and the platform glue code (rfal_nrf5_port.c/h).
 *
 * @note This file is required by the ST RFAL library and must be present in the project.
 */

#ifndef RFAL_PLATFORM
#define RFAL_PLATFORM

#include "SEGGER_RTT.h"
#include "app_util_platform.h"
#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_log.h"
#include "nrfx_gpiote.h"
#include "nrfx_twim.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "st25r3916_irq.h"

#include "../../bsp/custom_board.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define ST25R_INT_PIN ST25R_IRQ_PIN

/*---- Global device options ------------------------------------------------*/
// 7-bit I2C address for ST25R3916B
#define ST25R_I2C_ADDR (0x50u)

/* RFAL uses a single Tx/Rx transfer primitive into the device */
#ifndef ST25R_COM_SINGLETXRX
#   define ST25R_COM_SINGLETXRX
#endif

   /*---- Protection (critical sections) --------------------------------------*/
   static uint8_t s_comm_protect_cnt __attribute__((unused));

   /**
    * @brief Disable the ST25R3916 IRQ line (platform-specific).
    *
    * Disables the GPIOTE event for the IRQ pin, preventing further interrupts from the NFC frontend.
    */
   static inline void st25r_irq_disable(void)
   {
      nrfx_gpiote_in_event_disable(ST25R_IRQ_PIN);
   }

   /**
    * @brief Enable the ST25R3916 IRQ line (platform-specific).
    *
    * Enables the GPIOTE event for the IRQ pin, allowing interrupts from the NFC frontend.
    */
   static inline void st25r_irq_enable(void)
   {
      nrfx_gpiote_in_event_enable(ST25R_IRQ_PIN, true);
   }

/* RFAL uses these around multi-step I2C/SPI sequences */

/**
 * @brief Protect a critical section for ST25R communication (platform-specific).
 *
 * Disables the IRQ line and increments a protection counter. Used by RFAL to guard multi-step I2C/SPI sequences.
 */
#define platformProtectST25RComm()                                                                                     \
   do                                                                                                                  \
   {                                                                                                                   \
      if(s_comm_protect_cnt++ == 0)                                                                                    \
      {                                                                                                                \
         st25r_irq_disable();                                                                                          \
      }                                                                                                                \
   } while(0)

/**
 * @brief Unprotect a critical section for ST25R communication (platform-specific).
 *
 * Re-enables the IRQ line if the protection counter reaches zero, and drains any missed interrupts.
 */
#define platformUnprotectST25RComm()                                                                                   \
   do                                                                                                                  \
   {                                                                                                                   \
      if(s_comm_protect_cnt && --s_comm_protect_cnt == 0)                                                              \
      {                                                                                                                \
         /* Re-enable the IRQ line */                                                                                  \
         st25r_irq_enable();                                                                                           \
         /* Drain any level-asserted interrupt we missed */                                                            \
         while(nrf_gpio_pin_read(ST25R_IRQ_PIN))                                                                       \
         {                                                                                                             \
            st25r3916Isr();                                                                                            \
         }                                                                                                             \
      }                                                                                                                \
   } while(0)

#define platformProtectST25RIrqStatus()   platformProtectST25RComm()
#define platformUnprotectST25RIrqStatus() platformUnprotectST25RComm()

#define platformProtectWorker()      /* Protect RFAL Worker/Task/Process from concurrent execution on multi thread     \
                                        platforms   */
#define platformUnprotectWorker()    /* Unprotect RFAL Worker/Task/Process from concurrent execution on multi thread   \
                                        platforms */

/*---- LED / GPIO convenience ----------------------------------------------*/
#define platformLedOn(port, pin)     nrf_gpio_pin_set(pin)
#define platformLedOff(port, pin)    nrf_gpio_pin_clear(pin)
#define platformLedToogle(port, pin) nrf_gpio_pin_toggle(pin)

   /* Dock platform omits indicator LEDs; keep hook for RFAL linkage */
   static inline void platformLedsInitialize(void)
   {
      /* nothing to initialize */
   }
#define platformGpioSet(port, pin)    nrf_gpio_pin_set(pin)
#define platformGpioClear(port, pin)  nrf_gpio_pin_clear(pin)
#define platformGpioToogle(port, pin) nrf_gpio_pin_toggle(pin)
#define platformGpioIsHigh(port, pin) (nrf_gpio_pin_read(pin) != 0)
#define platformGpioIsLow(port, pin)  (!platformGpioIsHigh(port, pin))

   /*---- Timer / delay / logging ---------------------------------------------*/
   uint32_t platformGetSysTick(void);
#define platformTimerCreate(t_ms)      (platformGetSysTick() + (uint32_t)(t_ms))
#define platformTimerIsExpired(tmr)    ((int32_t)platformGetSysTick() >= (int32_t)(tmr))
#define platformTimerGetRemaining(tmr) platform_timer_get_remaining(tmr)
#define platformTimerDestroy(tmr)                                                                                      \
   do                                                                                                                  \
   {                                                                                                                   \
      (void)(tmr);                                                                                                     \
   } while(0)
#define platformDelay(ms) nrf_delay_ms(ms)

   static inline uint32_t platform_timer_get_remaining(uint32_t expiry)
   {
      int32_t diff = (int32_t)expiry - (int32_t)platformGetSysTick();
      return (diff > 0) ? (uint32_t)diff : 0U;
   }

#define platformAssert(exp)                                                                                            \
   do                                                                                                                  \
   {                                                                                                                   \
      if(!(exp))                                                                                                       \
      {                                                                                                                \
         SEGGER_RTT_printf(0, "RFAL assert @%s:%u", __FILE__, __LINE__);                                               \
         __BKPT(0);                                                                                                    \
      }                                                                                                                \
   } while(0)
#define platformErrorHandle()                                                                                          \
   do                                                                                                                  \
   {                                                                                                                   \
      SEGGER_RTT_printf(0, "RFAL fatal");                                                                              \
      __BKPT(0);                                                                                                       \
   } while(0)
#define platformLog(...) NRF_LOG_INFO(__VA_ARGS__)

/*---- (Unused in I2C mode) SPI hooks --------------------------------------*/
#define platformSpiSelect()
#define platformSpiDeselect()
#define platformSpiTxRx(txBuf, rxBuf, len)

   /*---- I2C hooks used by st25r3916_com.c -----------------------------------*/
   bool st25r_i2c_tx(const uint8_t *tx, uint16_t len, bool last, bool tx_only);
   bool st25r_i2c_rx(uint8_t *rx, uint16_t len);
#define platformI2CTx(txBuf, len, last, tx_only)                                                                       \
   st25r_i2c_tx((const uint8_t *)(txBuf), (uint16_t)(len), (bool)(last), (bool)(tx_only))
#define platformI2CRx(rxBuf, len) st25r_i2c_rx((uint8_t *)(rxBuf), (uint16_t)(len))
#define platformI2CStart()        // Not needed. This condition is catered for in the I2C glue logic
#define platformI2CRepeatStart()  // Not needed. This condition is catered for in the I2C glue logic
#define platformI2CStop()         // Not needed. This condition is catered for in the I2C glue logic
#define platformI2CSlaveAddrWR(add)
#define platformI2CSlaveAddrRD(add)

   /*---- IRQ glue (RFAL calls these) -----------------------------------------*/
   void nrf_rfal_irq_init(void);
   void nrf_rfal_irq_set_callback(void (*cb)(void));
#define platformIrqST25RPinInitialize() nrf_rfal_irq_init()
#define platformIrqST25RSetCallback(cb) nrf_rfal_irq_set_callback(cb)

/*---- RFAL feature selection (only what we need) --------------------------*/
#define RFAL_FEATURE_LISTEN_MODE   false
#define RFAL_FEATURE_WAKEUP_MODE   true
#define RFAL_FEATURE_LOWPOWER_MODE true
#define RFAL_FEATURE_NFCA          true
#define RFAL_FEATURE_NFCB          false
#define RFAL_FEATURE_NFCF          false
#define RFAL_FEATURE_NFCV          true /* ISO15693 / Type-5 */
#define RFAL_FEATURE_T1T           false
#define RFAL_FEATURE_T2T           false
#define RFAL_FEATURE_T4T           false
#define RFAL_FEATURE_ST25TB        false
#define RFAL_FEATURE_ST25xV        true /* ST25TV/DV extensions */

#define RFAL_FEATURE_ISO_DEP        false
#define RFAL_FEATURE_ISO_DEP_POLL   false
#define RFAL_FEATURE_ISO_DEP_LISTEN false
#define RFAL_FEATURE_NFC_DEP        false

#define RFAL_FEATURE_ISO_DEP_IBLOCK_MAX_LEN 256U
#define RFAL_FEATURE_NFC_DEP_BLOCK_MAX_LEN  254U
#define RFAL_FEATURE_NFC_RF_BUF_LEN         256U
#define RFAL_FEATURE_ISO_DEP_APDU_MAX_LEN   512U

#ifdef __cplusplus
}
#endif
#endif /* RFAL_PLATFORM */
