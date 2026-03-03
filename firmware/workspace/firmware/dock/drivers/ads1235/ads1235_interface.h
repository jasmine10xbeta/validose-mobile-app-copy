/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef ADS1235_INTERFACE_H
#define ADS1235_INTERFACE_H
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
// Standard includes
#include <app_util.h>
#include <stddef.h>
#include <stdint.h>

// Custom includes
#include "ads1235_regs.h"
#include "result.h"
#include "spi_driver_interface.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

/**
 * @brief Maximum number of ADS1235 instances supported by the driver
 *
 * The driver depends on the `nrfx_gpiote` module for handling GPIO interrupts. The current version does not support
 * registering interrupt handlers with context, so we use internal static variables to manage multiple instances.
 */
#define ADS1235_INSTANCES_MAX (1u)

STATIC_ASSERT(ADS1235_INSTANCES_MAX > 0, "ADS1235_INSTANCES_MAX must be greater than 0");
STATIC_ASSERT(ADS1235_INSTANCES_MAX <= UINT8_MAX, "ADS1235_INSTANCES_MAX must be less than or equal to 255");

/**
 * @brief Number of samples in the moving average window.
 *
 * @note Must be 2 or more to compute standard deviation
 * */
#define ADS1235_MOVING_AVERAGE_SAMPLES (40u)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

struct ads1235_driver; // Forward declaration
typedef struct ads1235_interface ads1235_interface_t;

// Error codes specific to the module.
typedef enum
{
   ADS1235_ERROR_NONE = 0,
   ADS1235_ERROR_NOT_IMPLEMENTED,

   ADS1235_ERROR_NULL_POINTER,          /**< Unexpected NULL pointer */
   ADS1235_ERROR_NOT_INITIALIZED,       /**< Driver instance not initialized */
   ADS1235_ERROR_MAX_INSTANCES_REACHED, /**< Maximum number of ADS1235 instances reached */
   ADS1235_ERROR_INTERNAL,              /**< Internal error (e.g. SDK error) */
   ADS1235_ERROR_NOT_ENOUGH_SAMPLES,    /**< Not enough samples collected */

   ADS1235_ERROR_SPI_ERROR,
   ADS1235_ERROR_INVALID_ARGUMENT,
   ADS1235_ERROR_BUFFER_NOT_FULL,
   ADS1235_ERROR_DEVICE_ID_MISMATCH,
   ADS1235_ERROR_STATUS_REG_ERROR,
   ADS1235_ERROR_RESET_TIMEOUT,
   ADS1235_ERROR_ERROR_MAX,
} ADS1235_ERROR;

typedef enum
{
   ADS1235_CAL_SYSTEM_OFFSET = 0,
   ADS1235_CAL_GAIN,
   ADS1235_CAL_SELF_CALIBRATION_OFFSET
} ads1235_calibration_mode_t;

/**
 * @brief Input multiplexer settings for differential channel selection.
 */
typedef enum
{
   ADS1235_AIN0 = 0b0011,
   ADS1235_AIN1 = 0b0100,
   ADS1235_AIN2 = 0b0101,
   ADS1235_AIN3 = 0b0110,
   ADS1235_AIN4 = 0b0111,
   ADS1235_AIN5 = 0b1000,
   ADS1235_INTERNAL_TEMP_SENSOR = 0b1011,
   ADS1235_PGA_INPUT_OPEN = 0b1110,
   ADS1235_INTERNAL_TO_VCOM = 0b1111 // default
} ads1235_input_mux_t;

/**
 * @brief Operating modes for ADS1235 ADC.
 */
typedef enum
{
   ADS1235_MODE_NORMAL = 0b00,
   ADS1235_MODE_CHOP = 0b01,
   ADS1235_MODE_W2_AC = 0b10, /**< 2-wire ac bridge */
   ADS1235_MODE_W4_AC = 0b11, /**< 4-wire ac bridge */
   ADS1235_MAX_MODE
} ads1235_mode_t;

/**
 * @brief Data rates for ADS1235 ADC.
 */
typedef enum
{
   ADS1235_SPS_2_5 = 0b0000,
   ADS1235_SPS_5 = 0b0001,
   ADS1235_SPS_10 = 0b0010,
   ADS1235_SPS_16_6 = 0b0011,
   ADS1235_SPS_20 = 0b0100,
   ADS1235_SPS_50 = 0b0101,
   ADS1235_SPS_60 = 0b0110,
   ADS1235_SPS_100 = 0b0111,
   ADS1235_SPS_400 = 0b1000,
   ADS1235_SPS_1200 = 0b1001,
   ADS1235_SPS_2400 = 0b1010,
   ADS1235_SPS_4800 = 0b1011,
   ADS1235_SPS_7200 = 0b1100,
   ADS1235_MAX_DATA_RATE
} ads1235_data_rate_t;

/**
 * @brief Register addresses for ADS1235 ADC.
 */
typedef enum
{
   ADS1235_REG_ID = 0x00,
   ADS1235_REG_STATUS = 0x01,
   ADS1235_REG_MODE0 = 0x02,
   ADS1235_REG_MODE1 = 0x03,
   ADS1235_REG_MODE2 = 0x04,
   ADS1235_REG_MODE3 = 0x05,
   ADS1235_REG_REF = 0x06,
   ADS1235_REG_OFCAL0 = 0x07,
   ADS1235_REG_OFCAL1 = 0x08,
   ADS1235_REG_OFCAL2 = 0x09,
   ADS1235_REG_FSCAL0 = 0x0A,
   ADS1235_REG_FSCAL1 = 0x0B,
   ADS1235_REG_FSCAL2 = 0x0C,
   ADS1235_REG_PGA = 0x10,
   ADS1235_REG_INPMUX = 0x11,
} ads1235_reg_t;

/**
 * @brief Filter settings for ADS1235 ADC.
 */
typedef enum
{
   ADS1235_FILTER_SINC1 = 0b000,
   ADS1235_FILTER_SINC2 = 0b001,
   ADS1235_FILTER_SINC3 = 0b010,
   ADS1235_FILTER_SINC4 = 0b011,
   ADS1235_FILTER_FIR = 0b100, // default
   ADS1235_FILTER_MAX
} ads1235_filter_t;

/**
 * @brief Reference settings for ADS1235 ADC (positive reference).
 */
typedef enum
{
   ADS1235_REF_RESERVED_POS = 0b00,
   ADS1235_REF_AVDD = 0b01,
   ADS1235_REF_REFP0 = 0b10,
   ADS1235_REF_REFP1_AIN0 = 0b11
} ads1235_reference_positive_t;

/**
 * @brief Reference settings for ADS1235 ADC (negative reference).
 */
typedef enum
{
   ADS1235_REF_RESERVED_NEG = 0b00,
   ADS1235_REF_AVSS = 0b01,
   ADS1235_REF_REFN0 = 0b10,
   ADS1235_REF_REFN1_AIN1 = 0b11
} ads1235_reference_negative_t;

typedef enum
{
   ADS1235_GAIN_1 = 0b000, // default
   ADS1235_GAIN_64 = 0b110,
   ADS1235_GAIN_128 = 0b111
} ads1235_gain_t;

/**
 * @brief ADS1235 Interface abstraction to decouple driver from platform-specific implementation.
 */
typedef struct ads1235_interface
{
   struct ads1235_driver *parent; // Reference to the containing instance.

   result_t (*reset)(const ads1235_interface_t *interface);

   /**
    * @brief Set the ADC's operating mode (e.g., normal, chop).
    *
    * @param interface Pointer to this interface
    * @param mode Desired operating mode
    * @return result_t Result of the operation
    */
   result_t (*set_mode)(const ads1235_interface_t *interface, ads1235_mode_t mode);

   /**
    * @brief Get the current ADC operating mode.
    *
    * @param interface Pointer to this interface
    * @param out_mode Pointer to store current mode
    * @return result_t Result of the operation
    */
   result_t (*get_mode)(const ads1235_interface_t *interface, ads1235_mode_t *out_mode);

   /**
    * @brief Set the ADC's data rate.
    *
    * @param interface Pointer to this interface
    * @param data_rate Desired data rate
    * @return result_t Result of the operation
    */
   result_t (*set_data_rate)(const ads1235_interface_t *interface, ads1235_data_rate_t data_rate);

   /**
    * @brief Get the current ADC data rate.
    *
    * @param interface Pointer to this interface
    * @param out_data_rate Pointer to store current data rate
    * @return result_t Result of the operation
    */
   result_t (*get_data_rate)(const ads1235_interface_t *interface, ads1235_data_rate_t *out_data_rate);

   /**
    * @brief Set the differential input channel configuration via MUX.
    *
    * @param interface Pointer to this interface
    * @param positive Positive input selection
    * @param negative Negative input selection
    * @return result_t Result of the operation
    */
   result_t (*set_input_mux)(const ads1235_interface_t *interface,
                             ads1235_input_mux_t positive,
                             ads1235_input_mux_t negative);

   /**
    * @brief Get the current differential input channel configuration.
    *
    * @param interface Pointer to this interface
    * @param out_positive Pointer to store positive input selection
    * @param out_negative Pointer to store negative input selection
    * @return result_t Result of the operation
    */
   result_t (*get_input_mux)(const ads1235_interface_t *interface,
                             ads1235_input_mux_t *out_positive,
                             ads1235_input_mux_t *out_negative);

   /**
    * @brief Set the averaging filter.
    *
    * @param interface Pointer to this interface
    * @param filter Filter selection
    * @return result_t Result of the operation
    */
   result_t (*set_filter)(const ads1235_interface_t *interface, ads1235_filter_t filter);

   /**
    * @brief Get the current averaging filter.
    *
    * @param interface Pointer to this interface
    * @param out_filter Pointer to store current filter
    * @return result_t Result of the operation
    */
   result_t (*get_filter)(const ads1235_interface_t *interface, ads1235_filter_t *out_filter);

   /**
    * @brief Set the ADC reference.
    *
    * @param interface Pointer to this interface
    * @param positive Positive reference selection
    * @param negative Negative reference selection
    * @return result_t Result of the operation
    */
   result_t (*set_reference)(const ads1235_interface_t *interface,
                             ads1235_reference_positive_t positive,
                             ads1235_reference_negative_t negative);

   /**
    * @brief Get the current ADC reference.
    *
    * @param interface Pointer to this interface
    * @param out_positive Pointer to store positive reference selection
    * @param out_negative Pointer to store negative reference selection
    * @return result_t Result of the operation
    */
   result_t (*get_reference)(const ads1235_interface_t *interface,
                             ads1235_reference_positive_t *out_positive,
                             ads1235_reference_negative_t *out_negative);

   /**
    * @brief Set the ADC gain.
    *
    * @param interface Pointer to this interface
    * @param gain Gain selection
    * @return result_t Result of the operation
    */
   result_t (*set_gain)(const ads1235_interface_t *interface, ads1235_gain_t gain);

   /**
    * @brief Get the current ADC gain.
    *
    * @param interface Pointer to this interface
    * @param out_gain Pointer to store current gain
    * @return result_t Result of the operation
    */
   result_t (*get_gain)(const ads1235_interface_t *interface, ads1235_gain_t *out_gain);

   /**
    * @brief Check if the ADC data is ready
    *
    * This function checks whether the moving average buffer has been filled and data is ready to be read.
    *
    * @param interface Pointer to this interface
    * @param data_ready_out Pointer to store whether the data is ready
    *
    * @return result_t Result of the operation
    */
   result_t (*is_data_ready)(const ads1235_interface_t *interface, bool *data_ready_out);

   /**
    * @brief Get the current conversion state of the ADC.
    *
    * This function queries whether the ADS1235 is currently performing conversions.
    *
    * @param interface Pointer to this interface
    * @param is_conversion_enabled Pointer to store whether conversion is enabled (true) or disabled (false)
    * @return result_t Result of the operation
    */
   result_t (*is_conversion_enabled)(const ads1235_interface_t *interface, bool *is_enabled);

   /**
    * @brief Enable or disable ADC conversions.
    *
    * This function starts or stops the conversion process on the ADS1235.
    *
    * @param interface Pointer to this interface
    * @param is_conversion_enabled Set to true to start conversions, false to stop
    * @return result_t Result of the operation
    */
   result_t (*set_conversion_state)(const ads1235_interface_t *interface, bool is_conversion_enabled);

   /**
    * @brief Get the latest ADC data
    *
    * Returns the latest ADC data processed through the moving average filter.
    * If new ADC samples are available since the last read, it is calculated on the fly. Otherwise returns the last
    * computed value and indicates that the data is stale.
    *
    * @param[in,out] interface Pointer to the interface instance
    * @param[out] ma_out Pointer to store the moving average output value
    * @param[out] stddev_out Pointer to store the standard deviation of the samples in the moving average window
    * @param[out] is_data_stale Pointer to store whether the data is stale since the last read
    *
    * @return result_t Result of the operation
    */
   result_t (*get_adc_data)(const ads1235_interface_t *interface,
                            int32_t *ma_out,
                            uint16_t *stddev_out,
                            bool *is_data_stale);
} ads1235_interface_t;

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/

#endif // ADS1235_INTERFACE_H