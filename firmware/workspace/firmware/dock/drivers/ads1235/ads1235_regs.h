/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef ADS1235_REGS_H
#define ADS1235_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

   // ID Register
   typedef uint8_t ads1235_reg_id_t;
#define ADS1235_ID_REV_ID_MASK (0x0Fu)
#define ADS1235_ID_DEV_ID_MASK (0xF0u)
#define ADS1235_ID_REV_ID(val) ((val) & ADS1235_ID_REV_ID_MASK)
#define ADS1235_ID_DEV_ID(val) (((val) & ADS1235_ID_DEV_ID_MASK) >> 4)

   // STATUS Register
   typedef uint8_t ads1235_reg_status_t;
#define ADS1235_STATUS_RESET_MASK    (0x01u)
#define ADS1235_STATUS_CLOCK_MASK    (0x02u)
#define ADS1235_STATUS_DRDY_MASK     (0x04u)
#define ADS1235_STATUS_REFL_ALM_MASK (0x08u)
#define ADS1235_STATUS_PGAH_ALM_MASK (0x10u)
#define ADS1235_STATUS_PGAL_ALM_MASK (0x20u)
#define ADS1235_STATUS_CRCERR_MASK   (0x40u)
#define ADS1235_STATUS_LOCK_MASK     (0x80u)

/** Error mask for the STATUS register */
#define ADS1235_STATUS_ERROR_MASK                                                                                      \
   (ADS1235_STATUS_LOCK_MASK | ADS1235_STATUS_CRCERR_MASK | ADS1235_STATUS_PGAL_ALM_MASK                               \
    | ADS1235_STATUS_PGAH_ALM_MASK | ADS1235_STATUS_REFL_ALM_MASK)

/** Check if an error is present in the STATUS register */
#define ADS1235_STATUS_IS_ERROR_SET(status) (((status) & ADS1235_STATUS_ERROR_MASK) != 0u)

   // MODE0 Register
   typedef uint8_t ads1235_reg_mode0_t;
#define ADS1235_MODE0_FILTER_MASK    (0x07u)
#define ADS1235_MODE0_DATA_RATE_MASK (0x78u)
#define ADS1235_MODE0_FILTER(val)    ((val) & ADS1235_MODE0_FILTER_MASK)
#define ADS1235_MODE0_DATA_RATE(val) (((val) & ADS1235_MODE0_DATA_RATE_MASK) >> 3)

   // MODE1 Register
   typedef uint8_t ads1235_reg_mode1_t;
#define ADS1235_MODE1_DELAY_MASK  (0x0Fu)
#define ADS1235_MODE1_CONVRT_MASK (0x10u)
#define ADS1235_MODE1_CHOP_MASK   (0x60u)
#define ADS1235_MODE1_DELAY(val)  ((val) & ADS1235_MODE1_DELAY_MASK)
#define ADS1235_MODE1_CONVRT(val) (((val) & ADS1235_MODE1_CONVRT_MASK) >> 4)
#define ADS1235_MODE1_CHOP(val)   (((val) & ADS1235_MODE1_CHOP_MASK) >> 5)

   // MODE2 Register
   typedef uint8_t ads1235_reg_mode2_t;
#define ADS1235_MODE2_GPIO_DIR_MASK (0x0Fu)
#define ADS1235_MODE2_GPIO_CON_MASK (0xF0u)
#define ADS1235_MODE2_GPIO_DIR(val) ((val) & ADS1235_MODE2_GPIO_DIR_MASK)
#define ADS1235_MODE2_GPIO_CON(val) (((val) & ADS1235_MODE2_GPIO_CON_MASK) >> 4)

   // MODE3 Register
   typedef uint8_t ads1235_reg_mode3_t;
#define ADS1235_MODE3_GPIO_DAT_MASK (0x0Fu)
#define ADS1235_MODE3_SPITIM_MASK   (0x10u)
#define ADS1235_MODE3_CRCENB_MASK   (0x20u)
#define ADS1235_MODE3_STATENB_MASK  (0x40u)
#define ADS1235_MODE3_PWDN_MASK     (0x80u)
#define ADS1235_MODE3_GPIO_DAT(val) ((val) & ADS1235_MODE3_GPIO_DAT_MASK)
#define ADS1235_MODE3_SPITIM(val)   (((val) & ADS1235_MODE3_SPITIM_MASK) >> 4)
#define ADS1235_MODE3_CRCENB(val)   (((val) & ADS1235_MODE3_CRCENB_MASK) >> 5)
#define ADS1235_MODE3_STATENB(val)  (((val) & ADS1235_MODE3_STATENB_MASK) >> 6)
#define ADS1235_MODE3_PWDN(val)     (((val) & ADS1235_MODE3_PWDN_MASK) >> 7)

   // REF Register
   typedef uint8_t ads1235_reg_ref_t;
#define ADS1235_REF_RMUXN_MASK (0x03u)
#define ADS1235_REF_RMUXP_MASK (0x0Cu)
#define ADS1235_REF_RMUXN(val) ((val) & ADS1235_REF_RMUXN_MASK)
#define ADS1235_REF_RMUXP(val) (((val) & ADS1235_REF_RMUXP_MASK) >> 2)

   // PGA Register
   typedef uint8_t ads1235_reg_pga_t;
#define ADS1235_PGA_GAIN_MASK   (0x07u)
#define ADS1235_PGA_BYPASS_MASK (0x80u)
#define ADS1235_PGA_GAIN(val)   ((val) & ADS1235_PGA_GAIN_MASK)
#define ADS1235_PGA_BYPASS(val) (((val) & ADS1235_PGA_BYPASS_MASK) >> 7)

   // INPUT_MUX Register
   typedef uint8_t ads1235_reg_input_mux_t;
#define ADS1235_INPMUX_MUXN_MASK (0x0Fu)
#define ADS1235_INPMUX_MUXP_MASK (0xF0u)
#define ADS1235_INPMUX_MUXN(val) ((val) & ADS1235_INPMUX_MUXN_MASK)
#define ADS1235_INPMUX_MUXP(val) (((val) & ADS1235_INPMUX_MUXP_MASK) >> 4)

#ifdef __cplusplus
}
#endif

#endif // ADS1235_REGS_H
