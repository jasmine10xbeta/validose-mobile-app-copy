
/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef STS30_COMMANDS_H
#define STS30_COMMANDS_H
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
// After sending a command to the sensor a minimal waiting time of 1ms is needed before another command can be received
// by the sensor
#define STS30_COMMAND_DELAY_MS (1u) // same as soft/hw reset delay - From datasheet section 4 and Section 2.2
#define COMMAND_SIZE_BYTES     (2u)

// Repeatability settings for single shot and periodic mode
typedef enum
{
   STS30_REP_LOW = 0,
   STS30_REP_MEDIUM,
   STS30_REP_HIGH,
   STS30_REP_MAX
} STS30_REP;

// CLock stretching settings for single shot mode
typedef enum
{
   STS30_CS_DISABLED = 0,
   STS30_CS_ENABLED,
   STS30_CS_MAX
} STS30_CS;

// Measurement rates for periodic mode
typedef enum
{
   STS30_MPS_0P5 = 0, /* 0.5 measurements per second */
   STS30_MPS_1,
   STS30_MPS_2,
   STS30_MPS_4,
   STS30_MPS_10,
   STS30_MPS_MAX
} STS30_MPS;

// -------------------- Commands (16-bit) --------------------
// Higher repeatability = longer measurement time = higher power consumption
// Single shot measurement commands (Table 7)
#define CMD_SS_HIGH_CS_EN  (0x2C06u)
#define CMD_SS_MED_CS_EN   (0x2C0Du)
#define CMD_SS_LOW_CS_EN   (0x2C10u)
#define CMD_SS_HIGH_CS_DIS (0x2400u)
#define CMD_SS_MED_CS_DIS  (0x240Bu)
#define CMD_SS_LOW_CS_DIS  (0x2416u)

// Periodic mode measurement commands (Table 8)
#define CMD_PER_H_0P5 (0x2032u)
#define CMD_PER_M_0P5 (0x2024u)
#define CMD_PER_L_0P5 (0x202Fu)

#define CMD_PER_H_1 (0x2130u)
#define CMD_PER_M_1 (0x2126u)
#define CMD_PER_L_1 (0x212Du)

#define CMD_PER_H_2 (0x2236u)
#define CMD_PER_M_2 (0x2220u)
#define CMD_PER_L_2 (0x222Bu)
#define CMD_PER_H_4 (0x2334u)
#define CMD_PER_M_4 (0x2322u)
#define CMD_PER_L_4 (0x2329u)

#define CMD_PER_H_10 (0x2737u)
#define CMD_PER_M_10 (0x2721u)
#define CMD_PER_L_10 (0x272Au)

// Fetch data (Table 9) - for periodic mode
#define CMD_FETCH_DATA (0xE000u)

// Break / stop periodic (Table 10)
#define CMD_BREAK (0x3093u)

// Soft reset (Table 11)
#define CMD_SOFT_RESET (0x30A2u)
// Heater (Table 13)
#define CMD_HEATER_ENABLE  (0x306Du)
#define CMD_HEATER_DISABLE (0x3066u)

// Status register (Table 14/15)
#define CMD_READ_STATUS  (0xF32Du)
#define CMD_CLEAR_STATUS (0x3041u)

// -------------------- Timing rules -------------------- //
// Datasheet requires at least 1 ms between commands
#define STS30_MIN_CMD_GAP_MS (1u)

// Max measurement durations in ms (Table 3): 4.5, 6.5, 15.5ms.
// Add a small margin and enforce min gap rule separately.
#define STS30_TMEAS_MAX_LOW_MS  (5u)
#define STS30_TMEAS_MAX_MED_MS  (7u)
#define STS30_TMEAS_MAX_HIGH_MS (16u)

// Reset recovery margins (Table 3 tPU/tSR up to 1.5ms): use 2ms
#define STS30_RESET_RECOVERY_MS (2u)

// CRC-8: poly 0x31, init 0xFF
#define STS30_CRC8_INIT (0xFFu)
#define STS30_CRC8_POLY (0x31u)

#endif // STS30_COMMANDS_H