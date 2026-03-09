/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include "app_timer.h"
#include "nrf_drv_gpiote.h"
#include "nrf_drv_twi.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"
#include <stdint.h>


// Custom includes
#include "../../bsp/custom_board.h"
#include "rfal_platform.h" // This connects our code with the RFAL library
#include "rfal_utils.h"

// External RFAL library includes
#include "rfal_analogConfig.h"
#include "rfal_nfc.h"
#include "rfal_nfcv.h"
#include "rfal_nrf5_port.h"
#include "rfal_platform.h"
#include "rfal_rf.h"
#include "rfal_st25xv.h"
#include "st25r3916.h"     /* for st25r3916Isr(), RFAL ST25R3916 API (read/write, commands, etc.) */
#include "st25r3916_aat.h"
#include "st25r3916_com.h" // low-level register access helpers
#include "st25r3916_irq.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_NFC_RFAL_GLUE_DOCK;
/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define I2C_TX_BUFFER_SIZE (150u)
#define I2C_TIMEOUT_US     (1000u)

/* TX driver resistance: d_res<3:0> in TX_DRIVER register
   Lower code => lower output resistance => higher current.
   0x00 = strongest drive; 0x0F = Hi-Z (off). */
#ifndef ST25R3916_REG_TX_DRIVER
#   define ST25R3916_REG_TX_DRIVER 0x28u /* fallback if not defined by your header */
#endif

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions

// Non-interface functions
static void (*st25r_isr_cb)(void) = NULL; // IRQ (GPIOTE)
static void gpiote_irq_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/
static const i2c_driver_interface_t *m_i2c_ifc;
static const system_time_interface_t *m_systick_ifc;

static uint8_t m_tx_stage[4]; // Small staging for the "register byte" when a read will follow.
static uint8_t m_tx_stage_len = 0;
static bool m_stage_valid = false;

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static void gpiote_irq_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
   (void)pin;
   (void)action;
   if(st25r_isr_cb != NULL)
   {
      st25r_isr_cb();
   }

   // As recommended by RFAL: if line is still high, loop to avoid missing events on edge-triggered HW. See UM2890 - Rev
   // 5, section 5.9
   while(nrf_gpio_pin_read(ST25R_IRQ_PIN))
   {
      st25r_isr_cb();
   }
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t rfal_nrf_platform_init(const i2c_driver_interface_t *i2c_interface,
                                const system_time_interface_t *systick_interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(i2c_interface, NFC_RFAL_GLUE_ERROR_PTR_NULL);
   RETURN_ERR_IF_INTERFACE_NULL(systick_interface, NFC_RFAL_GLUE_ERROR_PTR_NULL);

   m_i2c_ifc = i2c_interface;
   m_systick_ifc = systick_interface;

   return RESULT_OK;
}

// ---------------- The function declarations for the following functions live in rfal_platform.h --------------------
//--------------------------------------------------------------------------------------------------------------------

bool st25r_i2c_tx(const uint8_t *tx, uint16_t len, bool last, bool tx_only)
{
   if(NULL == tx)
   {
      return false;
   }

   if(len > UINT8_MAX)
   {
      DEBUG_ERROR("NFC I2C tx length %u exceeds 8-bit limit", len);
      return false;
   }

   const bool read_will_follow = (last && !tx_only);
   // Buffer used to gather all I2C transactions until bool last == true, then send them all at once.
   static uint8_t tx_buffer[I2C_TX_BUFFER_SIZE] = {0};
   static uint8_t tx_data_length = 0;

   bool is_err = false;
   result_t result = RESULT_OK;
   if(read_will_follow)
   {
      if(((len + tx_data_length) > sizeof(m_tx_stage)) || ((len + tx_data_length) > UINT8_MAX))
      {
         is_err = true;
         DEBUG_ERROR("Buffer overflow.");
      }
      else
      {
         // Defer this byte(s); It will be sent together with the RX as TXRX (NRF5 SDK I2C API)
         if(tx_data_length > 0)
         {
            // Already data in the tx buffer (i.e. multiple bytes will be sent before a read), add it first, followed by
            // tx received now.
            memcpy(m_tx_stage, tx_buffer, tx_data_length);
            memcpy(m_tx_stage + tx_data_length, tx, len);
            m_tx_stage_len = (uint8_t)(len + tx_data_length);
         }
         else
         {
            memcpy(m_tx_stage, tx, len);
            m_tx_stage_len = (uint8_t)len;
         }

         m_stage_valid = true;
         // Clear buffer contents after write
         tx_data_length = 0;
         memset(tx_buffer, 0, sizeof(tx_buffer));
      }
   }
   else
   {
      // Otherwise, do a normal TX now.
      // Stockpile tx data and inc length accordingly
      for(uint16_t idx = 0; idx < len; idx++)
      {
         if(tx_data_length >= I2C_TX_BUFFER_SIZE)
         {
            // Max buffer size reached
            DEBUG_ERROR("Max I2C buffer size reached. Max = %d, data length = %d", I2C_TX_BUFFER_SIZE, tx_data_length);
            break;
         }
         tx_buffer[tx_data_length] = tx[idx];
         tx_data_length++;
      }

      if(last && IS_OK(result))
      {
         // transmit the full tx buffer
         result = m_i2c_ifc->transmit(m_i2c_ifc, ST25R_I2C_ADDR, tx_buffer, tx_data_length, I2C_TIMEOUT_US);
         // Clear buffer contents after write
         tx_data_length = 0;
         memset(tx_buffer, 0, sizeof(tx_buffer));
      }

      if(TWI_DRV_TIMEOUT == GET_ERR_CODE(result))
      {
         DEBUG_ERROR("NFC I2C tx timeout");
      }
   }

   if(is_err)
   {
      return false;
   }
   else
   {
      return IS_OK(result);
   }

   // Should never reach this
   DEBUG_ERROR("Should never reach this section.");
   return false;
}

bool st25r_i2c_rx(uint8_t *rx, uint16_t len)
{
   if(NULL == rx)
   {
      return false;
   }

   if(len > UINT8_MAX)
   {
      DEBUG_ERROR("NFC I2C rx length %u exceeds 8-bit limit", len);
      return false;
   }

   result_t result = RESULT_OK;
   const uint8_t len8 = (uint8_t)len;

   if(m_stage_valid)
   {
      result = m_i2c_ifc->transmit_receive(m_i2c_ifc,
                                           ST25R_I2C_ADDR,
                                           m_tx_stage,
                                           m_tx_stage_len,
                                           rx,
                                           len8,
                                           I2C_TIMEOUT_US * 2u); // I2C_TIMEOUT_US x2 for Tx & Rx
      m_stage_valid = false;
      m_tx_stage_len = 0;
   }
   else
   {
      result = m_i2c_ifc->receive(m_i2c_ifc, ST25R_I2C_ADDR, rx, len8, I2C_TIMEOUT_US);
   }

   if(TWI_DRV_TIMEOUT == GET_ERR_CODE(result))
   {
      DEBUG_ERROR("NFC I2C rx timeout");
   }

   return !IS_ERR(result);
}

void nrf_rfal_irq_set_callback(void (*cb)(void))
{
   st25r_isr_cb = cb; // RFAL will pass st25r3916Isr
}

void nrf_rfal_irq_init(void)
{
   ret_code_t err = NRF_SUCCESS;
   static bool is_initialized = false;
   if(!nrf_drv_gpiote_is_init())
   {
      err = nrf_drv_gpiote_init();
      if(NRF_SUCCESS != err)
      {
         DEBUG_ERROR("Failed to init nrf gpiote.");
      }
   }

   if(!is_initialized && (NRF_SUCCESS == err))
   {
      nrf_gpio_cfg_input(ST25R_IRQ_PIN, NRF_GPIO_PIN_NOPULL);
      nrf_drv_gpiote_in_config_t in_cfg = GPIOTE_CONFIG_IN_SENSE_LOTOHI(true);
      in_cfg.pull = NRF_GPIO_PIN_NOPULL;
      err = nrf_drv_gpiote_in_init(ST25R_IRQ_PIN, &in_cfg, gpiote_irq_handler);
      if(NRF_SUCCESS != err)
      {
         DEBUG_ERROR("Failed to init nrf gpiote.");
      }
      else
      {
         nrf_drv_gpiote_in_event_enable(ST25R_IRQ_PIN, true);
         is_initialized = true;
      }
   }
}

uint32_t platformGetSysTick(void)
{
   uint64_t curr_ms = 0;
   (void)m_systick_ifc->get_time_ms(m_systick_ifc, &curr_ms);
   // Explicitly use the low 32 bits of the 64-bit millisecond counter. Since the RFAL library already does rollover
   // correctly (see timer.c in the lib) and both are in ms, passing the lower word preserves the modulo-2³² semantics
   // the RFAL expects on every platform.
   return (uint32_t)(curr_ms & UINT32_C(0xFFFFFFFF));
}
