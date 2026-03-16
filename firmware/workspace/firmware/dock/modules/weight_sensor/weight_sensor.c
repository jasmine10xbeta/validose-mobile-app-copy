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
#include "nrf_drv_spi.h"

// Custom includes
#include "weight_sensor.h"

static const uint8_t THIS_UNIT_ID = (uint8_t)SW_UNIT_ID_WEIGHT_SENSOR;

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
/**
 * @param UNSET_CALIBRATION_FACTOR Default value for the calibration factor indicating it has not been set.
 * @param WEIGHT_SENSOR_RING_REPLACED_DEBOUNCE_MS Debounce time in milliseconds after the ring is replaced to ensure
 * stable weight measurement. 20 seconds chosen as based on testing to allow sufficient time for weight readings to
 * stabilize after the disturbance caused by replacing the ring.
 * @param SLOW_SAMPLING_RATE_HZ Sampling frequency in Hz when the ring is present. Set to 0.1Hz to minimize power
 * consumption while still providing periodic weight updates when the ring is on.
 * @param FAST_SAMPLING_RATE_HZ Sampling frequency in Hz when the ring is absent. Set to 10Hz to provide responsive
 * weight updates when the ring is off, allowing for quick detection of changes in weight (e.g., during dosing).
 * @param TIME_MS_BETWEEN_SAMPLES_FAST Time in milliseconds between samples at the fast sampling rate. Calculated from
 * FAST_SAMPLING_RATE_HZ.
 * @param TIME_MS_BETWEEN_SAMPLES_SLOW Time in milliseconds between samples at the slow sampling rate. Calculated from
 * SLOW_SAMPLING_RATE_HZ.
 */
#define UNSET_CALIBRATION_FACTOR                (1)
#define WEIGHT_SENSOR_RING_REPLACED_DEBOUNCE_MS (20000u)
#define SLOW_SAMPLING_RATE_HZ                   (0.1) // 0.1Hz for slow sampling when ring is on
#define FAST_SAMPLING_RATE_HZ                   (10u) // 10Hz for fast sampling when ring is off
#define TIME_MS_BETWEEN_SAMPLES_FAST            ((uint32_t)(COMMON_1K_FACTOR / FAST_SAMPLING_RATE_HZ))
#define TIME_MS_BETWEEN_SAMPLES_SLOW            ((uint32_t)(COMMON_1K_FACTOR / SLOW_SAMPLING_RATE_HZ))
/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static function declarations
 **********************************************************************************************************************/
// Interface functions
static result_t process(const weight_sensor_interface_t *interface, bool is_ring_present);
static result_t reset_scale(const weight_sensor_interface_t *interface);

static result_t set_zero_offset(const weight_sensor_interface_t *interface, int32_t offset);
static result_t get_zero_offset(const weight_sensor_interface_t *interface, int32_t *out_offset);

static result_t set_calibration_factor(const weight_sensor_interface_t *interface, int32_t factor);
static result_t get_calibration_factor(const weight_sensor_interface_t *interface, int32_t *out_factor);
static result_t get_calibration_state(const weight_sensor_interface_t *interface,
                                      WEIGHT_SENSOR_CALIBRATION_STATE *state_out);

static result_t
   try_tare_if_stable(const weight_sensor_interface_t *interface, bool is_ring_present, bool *is_tare_successful);
static result_t try_calibrate_if_stable(const weight_sensor_interface_t *interface,
                                        uint32_t known_weight_mg,
                                        bool is_ring_present,
                                        bool *is_weight_detected,
                                        bool *is_calibration_successful);

static result_t
   read_weight_data(const weight_sensor_interface_t *interface, weight_data_t *weight_data_out, bool *is_stale);
static result_t get_is_stable(const weight_sensor_interface_t *interface, bool *is_stable);
static result_t get_state(const weight_sensor_interface_t *interface,
                          WEIGHT_SAMPLING_STATE *sampling_state,
                          SAMPLING_FREQUENCY *sampling_frequency);

// Non-interface functions
static result_t update_sampling_state(weight_sensor_t *const self, bool is_ring_present);
static result_t
   weight_sm_tick(weight_sensor_t *const self, uint64_t current_time_ms, int16_t temp_deciC, bool is_ring_present);
static uint32_t int_sqrt(uint64_t value);
static result_t calculate_sample_stddev_u16(int64_t sum, uint64_t sum2, uint32_t count, uint16_t *stddev_out);
static result_t is_moving_average_stable(const weight_sensor_t *const p_self, bool *is_stable_out);
static result_t reset_ma(weight_sensor_t *const p_self);
static result_t ingest_adc_sample(weight_sensor_t *const p_self, int32_t adc_value);
static result_t convert_adc_to_weight(weight_sensor_t *const p_self,
                                      int32_t adc_value,
                                      uint16_t adc_stddev,
                                      int32_t *weight_mg_out,
                                      uint16_t *stddev_mg_out);
static result_t
   sample(weight_sensor_t *const p_self, uint64_t current_time_ms, int16_t temp_deciC, bool is_ring_present);

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Static non-interface function definitions
 **********************************************************************************************************************/
/**
 * @brief Updates the sampling state of the weight sensor based on ring presence.
 *
 * This function manages the sampling frequency transitions depending on whether the ring is present or absent.
 * It handles debounce logic when the ring is replaced and sets a flag to force immediate sampling on state changes
 * that affect frequency.
 *
 * @note `state_changed` indicates whether the sampling state has changed in this function call. If true, this
 * triggers a reset of the last sample time to force immediate sampling on state change. Only transitions that affect
 * the sampling frequency (e.g., ring present <-> absent) set this flag true. Used to synchronize sampling behavior
 * with physical state changes.
 *
 * @param self Pointer to the weight_sensor_t instance.
 * @param is_ring_present Boolean indicating whether the ring is currently present.
 *
 * @return result_t RESULT_OK on success, or an error code if self is NULL or a system time error occurs.
 */
static result_t update_sampling_state(weight_sensor_t *const self, bool is_ring_present)
{
   RETURN_ERR_IF_NULL(self, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   result_t result = RESULT_OK;

   bool debounce_complete = false;

   bool state_changed = false;
   uint64_t current_time_ms = 0u;

   switch(self->_current_sampling_state)
   {
      case WEIGHT_SAMPLING_STATE_RING_PRESENT:
         if(!is_ring_present)
         {
            self->_current_sampling_state = WEIGHT_SAMPLING_STATE_RING_ABSENT;
            state_changed = true;
         }
         else
         {
            // remain in present state
            state_changed = false;
         }
         break;

      case WEIGHT_SAMPLING_STATE_RING_ABSENT:
         if(is_ring_present)
         {
            self->_current_sampling_state = WEIGHT_SAMPLING_STATE_RING_REPLACED;
            state_changed = false;

            result = self->_system_time->get_time_ms(self->_system_time, &current_time_ms);
            if(IS_OK(result))
            {
               self->_ring_replaced_time_ms = current_time_ms;
            }
         }
         break;

      case WEIGHT_SAMPLING_STATE_RING_REPLACED:

         result = self->_system_time->get_time_ms(self->_system_time, &current_time_ms);
         if(IS_OK(result))
         {
            uint64_t time_elapsed_ms = current_time_ms - self->_ring_replaced_time_ms;
            if(time_elapsed_ms >= WEIGHT_SENSOR_RING_REPLACED_DEBOUNCE_MS)
            {
               debounce_complete = true;
            }

            if(is_ring_present && debounce_complete)
            {
               self->_current_sampling_state = WEIGHT_SAMPLING_STATE_RING_PRESENT;
               state_changed = true; // affects sampling frequency
            }
            else if(!is_ring_present && debounce_complete)
            {
               self->_current_sampling_state = WEIGHT_SAMPLING_STATE_RING_ABSENT;
               state_changed = false; // does not affect sampling frequency
            }
         }
         break;

      case WEIGHT_SAMPLING_STATE_DSD_IDLE:
         // Fallthrough
      case WEIGHT_SAMPLING_STATE_MAX:
         // Fallthrough
      default:
         self->_current_sampling_state = WEIGHT_SAMPLING_STATE_RING_PRESENT;
         break;
   }

   if(state_changed)
   {
      // reset last sample time to force immediate sampling on state change
      self->_last_sample_time_ms = 0u;
   }

   return result;
}

/**
 * @brief Periodically samples weight based on the current frequency.
 *
 * Enables ADC if needed, samples weight if the interval has elapsed, and disables ADC in slow mode to save power.
 *
 * @param self Pointer to weight_sensor_t instance.
 * @param current_time_ms Current system time in milliseconds.
 * @return result_t RESULT_OK on success, or error code.
 */
static result_t
   weight_sm_tick(weight_sensor_t *const p_self, uint64_t current_time_ms, int16_t temp_deciC, bool is_ring_present)
{
   RETURN_ERR_IF_NULL(p_self, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   const ads1235_interface_t *p_adc_ifc = &p_self->_adc_driver.interface;

   result_t result = RESULT_OK;
   uint32_t sample_interval_ms = 0u;
   bool slow_sampling = false;
   bool adc_data_ready = false;
   bool sampled = false;

   switch(p_self->_current_sampling_frequency)
   {
      case SAMPLING_FREQUENCY_SLOW_RING_ON_HZ:
         sample_interval_ms = TIME_MS_BETWEEN_SAMPLES_SLOW;
         slow_sampling = true;
         break;
      case SAMPLING_FREQUENCY_FAST_RING_OFF_HZ:
         sample_interval_ms = TIME_MS_BETWEEN_SAMPLES_FAST;
         break;
      case SAMPLING_FREQUENCY_MAX:
      // Fallthrough
      case SAMPLING_FREQUENCY_DSD_IDLE:
      // Fallthrough
      default:
         // should not happen, but default to safe state
         sample_interval_ms = TIME_MS_BETWEEN_SAMPLES_SLOW;
         slow_sampling = true;
         break;
   }

   uint64_t time_elapsed_ms = current_time_ms - p_self->_last_sample_time_ms;

   if((time_elapsed_ms >= sample_interval_ms) || (0u == p_self->_last_sample_time_ms))
   {
      // Mark old data as stale
      p_self->_is_weight_data_stale = true;

      // Ensure the ADC is enabled
      p_self->_is_adc_enabled = false; // default to false in case of error
      result = p_adc_ifc->is_conversion_enabled(p_adc_ifc, &p_self->_is_adc_enabled);
      if(IS_OK(result) && (false == p_self->_is_adc_enabled))
      {
         result = p_adc_ifc->set_conversion_state(p_adc_ifc, true);
         p_self->_is_adc_enabled = IS_OK(result);
      }

      IF_OK_RUN_AND_UPDATE(result, p_adc_ifc->is_data_ready(p_adc_ifc, &adc_data_ready));

      if(IS_OK(result) && adc_data_ready)
      {
         result = sample(p_self, current_time_ms, temp_deciC, is_ring_present);
         sampled = IS_OK(result);
      }

      // stop ADC conversion to save power - only for slow sampling, not for back-to-back fast sampling
      if(slow_sampling && p_self->_is_adc_enabled && sampled)
      {
         IF_OK_RUN_AND_UPDATE(
            result, p_self->_adc_driver.interface.set_conversion_state(&p_self->_adc_driver.interface, false));
      }
   }

   return result;
}

/**
 * @brief Computes the integer square root of a 64-bit unsigned integer using the binary method
 *
 * @param[in] value The 64-bit unsigned integer for which to compute the integer square root
 *
 * @return The integer square root of the input value
 */
static uint32_t int_sqrt(uint64_t value)
{
   uint64_t remainder = value;
   uint64_t out = 0u;
   uint64_t bit = 1ULL << 62u;

   while(bit > remainder)
   {
      bit >>= 2u;
   }

   while(bit != 0u)
   {
      if(remainder >= (out + bit))
      {
         remainder -= (out + bit);
         out = (out >> 1u) + bit;
      }
      else
      {
         out >>= 1u;
      }
      bit >>= 2u;
   }

   return (out > UINT32_MAX) ? UINT32_MAX : (uint32_t)out;
}

/**
 * @brief Calculates the sample standard deviation from the given sum, sum of squares, and count
 *
 * @param[in] sum The sum of the samples
 * @param[in] sum2 The sum of squares of the samples
 * @param[in] count The number of samples. Must be at least 2
 * @param[out] stddev_out Pointer where the calculated standard deviation will be stored
 *
 * @retval `RESULT_OK` if the calculation was successful
 * @retval `WEIGHT_SENSOR_ERROR_NULL_POINTER` if `stddev_out` is NULL
 * @retval `WEIGHT_SENSOR_ERROR_INVALID_ARGUMENT` if `count` is less than 2
 * @retval `WEIGHT_SENSOR_ERROR_INVALID_ARGUMENT` if the combination of sum, sum2, and count is implausible (e.g., `sum2
 * * count < (sum*sum)`)
 * @retval `WEIGHT_SENSOR_ERROR_INTERNAL` if the intermediate calculations overflow 64-bit integer limits
 */
static result_t calculate_sample_stddev_u16(int64_t sum, uint64_t sum2, uint32_t count, uint16_t *stddev_out)
{
   RETURN_ERR_IF_NULL(stddev_out, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(count < 2u, WEIGHT_SENSOR_ERROR_INVALID_ARGUMENT);

   uint64_t n64 = (uint64_t)count;
   result_t result = RESULT_OK;

   // Calculate variance accounting for potential overflow.
   // var = ((n * sum2) - (sum * sum)) / (n * (n - 1))
   uint64_t sum_abs = (sum >= 0) ? (uint64_t)sum : (uint64_t)(-(sum + 1LL)) + 1u;

   uint64_t sum_sq = 0u;
   uint64_t n_sum2 = 0u;
   uint64_t var = 0u;
   uint64_t var_num = 0u;
   uint64_t var_den = 0u;

   if((sum_abs > 0u) && (sum_abs > (UINT64_MAX / sum_abs)))
   {
      SET_ERR(result, WEIGHT_SENSOR_ERROR_INTERNAL); // sum*sum would overflow
   }

   if(sum2 > 0u && n64 > (UINT64_MAX / sum2))
   {
      SET_ERR(result, WEIGHT_SENSOR_ERROR_INTERNAL); // n*sum2 would overflow
   }

   if(IS_OK(result))
   {
      sum_sq = sum_abs * sum_abs;
      n_sum2 = n64 * sum2;

      if(n_sum2 < sum_sq)
      {
         SET_ERR(result, WEIGHT_SENSOR_ERROR_INVALID_ARGUMENT); // implausible combination of sum, sum2, and count
      }
   }

   if(IS_OK(result))
   {
      var_num = n_sum2 - sum_sq;
      var_den = n64 * (n64 - 1u);

      if(var_num > (UINT64_MAX - (var_den >> 1u)))
      {
         SET_ERR(result, WEIGHT_SENSOR_ERROR_INTERNAL); // rounding addition would overflow
      }
   }

   if(IS_OK(result))
   {
      var = (var_num + (var_den >> 1u)) / var_den; // rounding division
   }

   // Calculate standard deviation
   if(IS_OK(result))
   {
      uint32_t stddev32 = int_sqrt(var);
      *stddev_out = (stddev32 > UINT16_MAX) ? UINT16_MAX : (uint16_t)stddev32;
   }

   return result;
}

/**
 * @brief Determine whether the current ADC moving average is stable.
 *
 * @param[in] p_self Pointer to the weight sensor instance
 * @param[out] is_stable_out Whether the moving average is stable and the moving average buffer is full.
 *
 * @return result_t RESULT_OK on success, or an error code
 */
static result_t is_moving_average_stable(const weight_sensor_t *const p_self, bool *is_stable_out)
{
   RETURN_ERR_IF_NULL(p_self, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(is_stable_out, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   *is_stable_out = false;

   if(p_self->_adc_ma_count < WS_MOVING_AVERAGE_SAMPLES)
   {
      return RESULT_OK;
   }

   uint16_t stddev = 0u;
   result_t result
      = calculate_sample_stddev_u16(p_self->_adc_ma_sum, p_self->_adc_ma_sum2, p_self->_adc_ma_count, &stddev);
   if(IS_OK(result))
   {
      *is_stable_out = (stddev <= WS_STABLE_STDDEV_THRESHOLD_RAW);
   }

   return result;
}

static result_t reset_ma(weight_sensor_t *const p_self)
{
   RETURN_ERR_IF_NULL(p_self, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   p_self->_adc_ma_head = 0u;
   p_self->_adc_ma_count = 0u;
   p_self->_adc_ma_sum = 0LL;
   p_self->_adc_ma_sum2 = 0u;

   return RESULT_OK;
}

static result_t ingest_adc_sample(weight_sensor_t *const p_self, int32_t adc_value)
{
   RETURN_ERR_IF_NULL(p_self, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   int64_t new64 = (int64_t)adc_value;
   int64_t old64 = 0;

   if(p_self->_adc_ma_count == WS_MOVING_AVERAGE_SAMPLES)
   {
      old64 = (int64_t)p_self->_adc_ma[p_self->_adc_ma_head];
   }

   p_self->_adc_ma[p_self->_adc_ma_head] = adc_value;
   p_self->_adc_ma_head = (p_self->_adc_ma_head + 1) % WS_MOVING_AVERAGE_SAMPLES;
   if(p_self->_adc_ma_count < WS_MOVING_AVERAGE_SAMPLES)
   {
      p_self->_adc_ma_count++;
   }

   p_self->_adc_ma_sum += (new64 - old64);
   p_self->_adc_ma_sum2 += ((uint64_t)(new64 * new64) - (uint64_t)(old64 * old64));

   return RESULT_OK;
}

static result_t convert_adc_to_weight(weight_sensor_t *const p_self,
                                      int32_t adc_value,
                                      uint16_t adc_stddev,
                                      int32_t *weight_mg_out,
                                      uint16_t *stddev_mg_out)
{
   RETURN_ERR_IF_NULL(p_self, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(0 == p_self->_calibration_factor, WEIGHT_SENSOR_ERROR_INVALID_ARGUMENT);

   if(WEIGHT_SENSOR_CALIBRATION_STATE_CALIBRATED != p_self->_calibration_state)
   {
      // If not fully calibrated, return raw ADC values instead
      *weight_mg_out = adc_value;
      *stddev_mg_out = adc_stddev;
   }
   else
   {
      // Note: cpg already checked to be non-zero in guard clause
      int64_t cpg = (int64_t)p_self->_calibration_factor;
      int64_t cpg_abs = (cpg >= 0) ? cpg : -cpg;

      int64_t zeroed_value = (int64_t)adc_value - (int64_t)p_self->_zero_offset;

      // ADC samples are 24-bit, but scaling can exceed int32_t. Guard overflow after rounding.
      // Round to nearest integer
      int32_t weight_mg = 0;
      int64_t numerator = zeroed_value * (int64_t)COMMON_1K_FACTOR;
      int64_t rounded = 0;
      if(numerator > 0)
      {
         rounded = (numerator + (cpg_abs / 2)) / cpg;
      }
      else
      {
         rounded = (numerator - (cpg_abs / 2)) / cpg;
      }

      if(rounded > (int64_t)INT32_MAX)
      {
         rounded = (int64_t)INT32_MAX;
      }
      else if(rounded < (int64_t)INT32_MIN)
      {
         rounded = (int64_t)INT32_MIN;
      }

      weight_mg = (int32_t)rounded;
      uint64_t stddev_scaled = ((uint64_t)adc_stddev * (uint64_t)COMMON_1K_FACTOR) / (uint64_t)cpg_abs;
      uint16_t stddev_mg = (stddev_scaled > UINT16_MAX) ? UINT16_MAX : (uint16_t)stddev_scaled;

      *weight_mg_out = weight_mg;
      *stddev_mg_out = stddev_mg;
   }

   return RESULT_OK;
}

/**
 * @brief Take a single sample
 *
 * This function is used to sample the ADC and update the latest weight data.
 * Also updates the data stale flag and timestamps accordingly.
 *
 * @param[in,out] p_self Pointer to the instance
 * @param[in] current_time_ms Current system time in milliseconds
 * @param[in] temp_deciC Current temperature in deci-degrees Celsius
 * @param[in] is_ring_present Whether the ring is currently present
 *
 * @return Result of the operation
 *
 * @note Pre-calibration, the output values will be in ADC counts rather than milligrams, since the calibration factor
 * is not yet set.
 */
static result_t
   sample(weight_sensor_t *const p_self, uint64_t current_time_ms, int16_t temp_deciC, bool is_ring_present)
{
   RETURN_ERR_IF_NULL(p_self, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   const ads1235_interface_t *p_adc_ifc = &p_self->_adc_driver.interface;

   int32_t adc_value = 0;
   uint16_t adc_stddev = 0u;
   bool adc_data_is_stale = false;

   int32_t adc_value_out = 0;
   uint16_t adc_stddev_out = 0u;
   bool output_data_is_stale = false;
   bool publish_output_data = true;

   int32_t weight_mg = 0;
   uint16_t stddev_mg = 0;

   // Sample the ADC
   result_t result = p_adc_ifc->get_adc_data(p_adc_ifc, &adc_value, &adc_stddev, &adc_data_is_stale);
   if(IS_OK(result) && adc_data_is_stale)
   {
      SET_ERR(result, WEIGHT_SENSOR_ERROR_ADC_DATA_STALE);
   }

   if(IS_OK(result))
   {
      adc_value_out = adc_value;
      adc_stddev_out = adc_stddev;

      // If fast sampling, average over a window to reduce noise.
      if(SAMPLING_FREQUENCY_FAST_RING_OFF_HZ == p_self->_current_sampling_frequency)
      {
         result = ingest_adc_sample(p_self, adc_value);
         if(IS_OK(result))
         {
            adc_value_out = (int32_t)(p_self->_adc_ma_sum / (int64_t)p_self->_adc_ma_count);

            if(p_self->_adc_ma_count >= 2u)
            {
               result = calculate_sample_stddev_u16(
                  p_self->_adc_ma_sum, p_self->_adc_ma_sum2, p_self->_adc_ma_count, &adc_stddev_out);
            }
            else
            {
               adc_stddev_out = 0u;
            }
         }

         if(IS_OK(result))
         {
            output_data_is_stale = (p_self->_adc_ma_count < WS_MOVING_AVERAGE_SAMPLES);
         }
      }
   }

   // Convert ADC data to weight (if calibrated)
   if(IS_OK(result))
   {
      result = convert_adc_to_weight(p_self, adc_value_out, adc_stddev_out, &weight_mg, &stddev_mg);
   }

   // Keep sampled raw ADC values and cadence bookkeeping current, regardless of output staleness.
   if(IS_OK(result))
   {
      p_self->_latest_adc_value = adc_value;
      p_self->_latest_adc_stddev = adc_stddev;
      p_self->_last_sample_time_ms = current_time_ms;
   }

   // Update reported weight data only when valid for publication.
   if(IS_OK(result))
   {
      publish_output_data
         = !((SAMPLING_FREQUENCY_FAST_RING_OFF_HZ == p_self->_current_sampling_frequency) && output_data_is_stale);

      if(publish_output_data)
      {
         p_self->_latest_weight_data.weight_mg = weight_mg;
         p_self->_latest_weight_data.stddev_mg = stddev_mg;
         p_self->_latest_weight_data.time_ms = current_time_ms;
         p_self->_latest_weight_data.is_ring_present = is_ring_present;
         p_self->_latest_weight_data.temp_deciC = temp_deciC;
      }

      p_self->_is_weight_data_stale = output_data_is_stale;

      // Testing only: Todo: remove before production release - Start
      static uint64_t last_debug_time_ms = 0;
      if(current_time_ms - last_debug_time_ms >= 1000)
      { // 1 second rate limit
         DEBUG_DEBUG(
            "[ws] Sampled weight: %d mg (stddev: %u mg, raw ADC: %d, ADC stddev: %u), temp: %d deciC, ring: %s",
            weight_mg,
            stddev_mg,
            adc_value,
            adc_stddev,
            temp_deciC,
            is_ring_present);
         last_debug_time_ms = current_time_ms;
      }
   }
   // Testing only - end

   return result;
}

/***********************************************************************************************************************
 * Static interface function definitions
 **********************************************************************************************************************/

static result_t process(const weight_sensor_interface_t *interface, bool is_ring_present)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   weight_sensor_t *const p_self = interface->parent;
   RETURN_ERR_IF_TRUE(false == p_self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   const sts30_dis_temp_sensor_interface_t *p_temp_sensor_ifc = p_self->_temp_sensor;
   const system_time_interface_t *p_systick_ifc = p_self->_system_time;

   uint64_t current_time_ms = 0u;
   int16_t temp_deciC = 0;
   bool is_temp_stale = false; // unused
   SAMPLING_FREQUENCY previous_sampling_frequency = p_self->_current_sampling_frequency;

   result_t result = p_systick_ifc->get_time_ms(p_systick_ifc, &current_time_ms);

   IF_OK_RUN_AND_UPDATE(result, p_temp_sensor_ifc->get_temperature(p_temp_sensor_ifc, &temp_deciC, &is_temp_stale));

   IF_OK_RUN_AND_UPDATE(result, update_sampling_state(p_self, is_ring_present));

   switch(p_self->_current_sampling_state)
   {
      case WEIGHT_SAMPLING_STATE_RING_PRESENT:
         p_self->_current_sampling_frequency = SAMPLING_FREQUENCY_SLOW_RING_ON_HZ;
         break;
      case WEIGHT_SAMPLING_STATE_RING_ABSENT:
      case WEIGHT_SAMPLING_STATE_RING_REPLACED:
         p_self->_current_sampling_frequency = SAMPLING_FREQUENCY_FAST_RING_OFF_HZ;
         break;
      case WEIGHT_SAMPLING_STATE_DSD_IDLE:
         p_self->_current_sampling_frequency = SAMPLING_FREQUENCY_DSD_IDLE;
         break;
      case WEIGHT_SAMPLING_STATE_MAX:
      default:
         // should not happen, but default to safe state
         p_self->_current_sampling_frequency = SAMPLING_FREQUENCY_SLOW_RING_ON_HZ;
         break;
   }

   // Reset the moving average and mark data as stale if we just switched to fast sampling
   if((SAMPLING_FREQUENCY_FAST_RING_OFF_HZ == p_self->_current_sampling_frequency)
      && (SAMPLING_FREQUENCY_FAST_RING_OFF_HZ != previous_sampling_frequency))
   {
      IF_OK_RUN_AND_UPDATE(result, reset_ma(p_self));
      p_self->_is_weight_data_stale = true;
   }

   IF_OK_RUN_AND_UPDATE(result, weight_sm_tick(p_self, current_time_ms, temp_deciC, is_ring_present));

   return result;
}

static result_t reset_scale(const weight_sensor_interface_t *interface)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   weight_sensor_t *const self = interface->parent;
   RETURN_ERR_IF_TRUE(false == self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   return self->_adc_driver.interface.reset(&self->_adc_driver.interface);
}

static result_t set_zero_offset(const weight_sensor_interface_t *interface, int32_t offset)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   weight_sensor_t *const self = interface->parent;
   RETURN_ERR_IF_TRUE(false == self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   self->_zero_offset = offset;

   if(WEIGHT_SENSOR_CALIBRATION_STATE_CALIBRATED != self->_calibration_state)
   {
      self->_calibration_state = WEIGHT_SENSOR_CALIBRATION_STATE_ZEROED;
   }

   return RESULT_OK;
}

static result_t get_zero_offset(const weight_sensor_interface_t *interface, int32_t *out_offset)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(out_offset, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   weight_sensor_t *const self = interface->parent;
   RETURN_ERR_IF_TRUE(false == self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   *out_offset = self->_zero_offset;

   return RESULT_OK;
}

static result_t set_calibration_factor(const weight_sensor_interface_t *interface, int32_t factor)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(0 == factor, WEIGHT_SENSOR_ERROR_INVALID); // avoid divide by 0 in weight calculation

   weight_sensor_t *const self = interface->parent;
   RETURN_ERR_IF_TRUE(false == self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_TRUE(WEIGHT_SENSOR_CALIBRATION_STATE_UNCALIBRATED == self->_calibration_state,
                      WEIGHT_SENSOR_ERROR_INVALID); // must be zeroed or calibrated before setting calibration factor

   self->_calibration_factor = factor;

   self->_calibration_state = WEIGHT_SENSOR_CALIBRATION_STATE_CALIBRATED;

   return RESULT_OK;
}

static result_t get_calibration_factor(const weight_sensor_interface_t *interface, int32_t *out_factor)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(out_factor, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   weight_sensor_t *const self = interface->parent;
   RETURN_ERR_IF_TRUE(false == self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   *out_factor = self->_calibration_factor;

   return RESULT_OK;
}

static result_t get_calibration_state(const weight_sensor_interface_t *interface,
                                      WEIGHT_SENSOR_CALIBRATION_STATE *state_out)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(state_out, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   const weight_sensor_t *const p_self = interface->parent;
   RETURN_ERR_IF_TRUE(false == p_self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   *state_out = p_self->_calibration_state;

   return RESULT_OK;
}

static result_t
   try_tare_if_stable(const weight_sensor_interface_t *interface, bool is_ring_present, bool *is_tare_successful)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(is_tare_successful, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   weight_sensor_t *const p_self = interface->parent;
   RETURN_ERR_IF_TRUE(false == interface->parent->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   const system_time_interface_t *p_systick_ifc = p_self->_system_time;
   result_t result = RESULT_OK;
   bool is_first_call = (WEIGHT_SENSOR_CALIBRATION_STATE_ZEROING != p_self->_calibration_state);

   *is_tare_successful = false;

   // Initialize the taring process of first call or if ring is present
   if(is_first_call || is_ring_present)
   {
      if(is_first_call)
      {
         p_self->_calibration_state_before_start = p_self->_calibration_state;
      }

      result = p_systick_ifc->get_time_ms(p_systick_ifc, &p_self->_calibration_start_time_ms);
      IF_OK_RUN_AND_UPDATE(result, reset_ma(p_self));
      if(IS_OK(result))
      {
         p_self->_is_weight_data_stale = true;
         p_self->_last_calibration_sample_time_ms = 0u;
         p_self->_calibration_state = WEIGHT_SENSOR_CALIBRATION_STATE_ZEROING;
      }
   }

   // Consume only fresh non-stale samples from the shared sampling pipeline.
   if(IS_OK(result) && (false == is_ring_present) && (false == p_self->_is_weight_data_stale)
      && (p_self->_latest_weight_data.time_ms != 0u)
      && (p_self->_latest_weight_data.time_ms != p_self->_last_calibration_sample_time_ms))
   {
      p_self->_last_calibration_sample_time_ms = p_self->_latest_weight_data.time_ms;

      if(IS_OK(result))
      {
         bool is_stable = false;
         result = is_moving_average_stable(p_self, &is_stable);
         if(IS_OK(result) && is_stable)
         {
            // Perform tare
            int32_t adc_ma = (int32_t)(p_self->_adc_ma_sum / (int64_t)p_self->_adc_ma_count);
            p_self->_zero_offset = adc_ma;
            *is_tare_successful = true;

            if(WEIGHT_SENSOR_CALIBRATION_STATE_CALIBRATED == p_self->_calibration_state_before_start)
            {
               p_self->_calibration_state = WEIGHT_SENSOR_CALIBRATION_STATE_CALIBRATED;
            }
            else
            {
               p_self->_calibration_state = WEIGHT_SENSOR_CALIBRATION_STATE_ZEROED;
            }
         }
      }
   }

   if(IS_OK(result) && !*is_tare_successful)
   {
      // Check for taring timeout
      uint64_t current_time_ms = 0u;
      result = p_systick_ifc->get_time_ms(p_systick_ifc, &current_time_ms);
      if(IS_OK(result) && ((current_time_ms - p_self->_calibration_start_time_ms) >= WS_CALIBRATION_TIMEOUT_MS))
      {
         SET_ERR(result, WEIGHT_SENSOR_ERROR_TIMEOUT);
      }
   }

   if(IS_ERR(result))
   {
      p_self->_calibration_state = p_self->_calibration_state_before_start;
   }

   return result;
}

static result_t try_calibrate_if_stable(const weight_sensor_interface_t *interface,
                                        uint32_t known_weight_mg,
                                        bool is_ring_present,
                                        bool *is_weight_detected,
                                        bool *is_calibration_successful)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_TRUE(0u == known_weight_mg, WEIGHT_SENSOR_ERROR_INVALID_ARGUMENT);
   RETURN_ERR_IF_NULL(is_weight_detected, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(is_calibration_successful, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   weight_sensor_t *const p_self = interface->parent;
   RETURN_ERR_IF_TRUE(false == interface->parent->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);
   RETURN_ERR_IF_TRUE(((WEIGHT_SENSOR_CALIBRATION_STATE_UNCALIBRATED == p_self->_calibration_state)
                       || (WEIGHT_SENSOR_CALIBRATION_STATE_ZEROING == p_self->_calibration_state)),
                      WEIGHT_SENSOR_ERROR_ZERO_NOT_SET);

   const system_time_interface_t *p_systick_ifc = p_self->_system_time;
   result_t result = RESULT_OK;
   bool is_first_call = (WEIGHT_SENSOR_CALIBRATION_STATE_CALIBRATING != p_self->_calibration_state);

   *is_weight_detected = false;
   *is_calibration_successful = false;

   // Initialize the calibration process on first call or if ring is present
   if(is_first_call || is_ring_present)
   {
      if(is_first_call)
      {
         p_self->_calibration_state_before_start = p_self->_calibration_state;
      }

      result = p_systick_ifc->get_time_ms(p_systick_ifc, &p_self->_calibration_start_time_ms);
      IF_OK_RUN_AND_UPDATE(result, reset_ma(p_self));
      if(IS_OK(result))
      {
         p_self->_is_weight_data_stale = true;
         p_self->_last_calibration_sample_time_ms = 0u;
         p_self->_calibration_state = WEIGHT_SENSOR_CALIBRATION_STATE_CALIBRATING;
      }
   }

   // Consume only fresh non-stale samples from the shared sampling pipeline.
   if(IS_OK(result) && (false == is_ring_present) && (false == p_self->_is_weight_data_stale)
      && (p_self->_latest_weight_data.time_ms != 0u)
      && (p_self->_latest_weight_data.time_ms != p_self->_last_calibration_sample_time_ms))
   {
      p_self->_last_calibration_sample_time_ms = p_self->_latest_weight_data.time_ms;

      int64_t adc_delta = (int64_t)p_self->_latest_adc_value - (int64_t)p_self->_zero_offset;
      uint64_t adc_delta_abs = (adc_delta >= 0) ? (uint64_t)adc_delta : (uint64_t)(-adc_delta);

      // Keep resetting MA until the signal crosses the weight-detection threshold.
      // (MA ingestion is already handled by the normal sampling path.)
      if(adc_delta_abs < WS_WEIGHT_PRESENT_DETECTION_THRESHOLD_RAW)
      {
         result = reset_ma(p_self);
         if(IS_OK(result))
         {
            p_self->_is_weight_data_stale = true;
         }
      }

      // If moving average buffer is full, weight detected
      if(IS_OK(result) && p_self->_adc_ma_count >= WS_MOVING_AVERAGE_SAMPLES)
      {
         *is_weight_detected = true;

         bool is_stable = false;
         result = is_moving_average_stable(p_self, &is_stable);

         // If moving average is stable, calculate calibration factor
         if(IS_OK(result) && is_stable)
         {
            int32_t adc_ma = (int32_t)(p_self->_adc_ma_sum / (int64_t)p_self->_adc_ma_count);
            int64_t adc_ma_delta = (int64_t)adc_ma - (int64_t)p_self->_zero_offset;

            int64_t num = adc_ma_delta * (int64_t)COMMON_1K_FACTOR;
            int64_t den = (int64_t)known_weight_mg;
            int64_t cpg = (num >= 0) ? (num + den / 2LL) / den : (num - den / 2LL) / den; // Round to nearest integer

            if((cpg == 0) || (cpg > INT32_MAX) || (cpg < INT32_MIN))
            {
               DEBUG_ERROR("Invalid calibration factor calculated for %d mg weight: %lld", known_weight_mg, cpg);
               SET_ERR(result, WEIGHT_SENSOR_ERROR_INVALID_ARGUMENT);
            }
            else
            {
               p_self->_calibration_factor = (int32_t)cpg; // Safe to cast after range check
               *is_calibration_successful = true;
               p_self->_calibration_state = WEIGHT_SENSOR_CALIBRATION_STATE_CALIBRATED;
            }
         }
      }
   }

   if(IS_OK(result) && !*is_calibration_successful)
   {
      // Check for calibration timeout
      uint64_t current_time_ms = 0u;
      result = p_systick_ifc->get_time_ms(p_systick_ifc, &current_time_ms);
      if(IS_OK(result) && ((current_time_ms - p_self->_calibration_start_time_ms) >= WS_CALIBRATION_TIMEOUT_MS))
      {
         SET_ERR(result, WEIGHT_SENSOR_ERROR_TIMEOUT);
      }
   }

   if(IS_ERR(result))
   {
      p_self->_calibration_state = p_self->_calibration_state_before_start;
   }

   return result;
}

static result_t
   read_weight_data(const weight_sensor_interface_t *interface, weight_data_t *weight_data_out, bool *is_stale)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(weight_data_out, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(is_stale, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   weight_sensor_t *const p_self = interface->parent;
   RETURN_ERR_IF_TRUE(false == p_self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   weight_data_out->weight_mg = p_self->_latest_weight_data.weight_mg;
   weight_data_out->stddev_mg = p_self->_latest_weight_data.stddev_mg;
   weight_data_out->time_ms = p_self->_latest_weight_data.time_ms;
   weight_data_out->is_ring_present = p_self->_latest_weight_data.is_ring_present;
   weight_data_out->temp_deciC = p_self->_latest_weight_data.temp_deciC;

   *is_stale = p_self->_is_weight_data_stale;

   return RESULT_OK;
}

static result_t get_is_stable(const weight_sensor_interface_t *interface, bool *is_stable)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(is_stable, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   weight_sensor_t *const p_self = interface->parent;
   RETURN_ERR_IF_TRUE(false == p_self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   return is_moving_average_stable(p_self, is_stable);
}

static result_t get_state(const weight_sensor_interface_t *interface,
                          WEIGHT_SAMPLING_STATE *sampling_state,
                          SAMPLING_FREQUENCY *sampling_frequency)
{
   RETURN_ERR_IF_INTERFACE_NULL(interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(sampling_state, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_NULL(sampling_frequency, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   weight_sensor_t *const self = interface->parent;
   RETURN_ERR_IF_TRUE(false == self->_initialized, WEIGHT_SENSOR_ERROR_NOT_INITIALIZED);

   *sampling_state = self->_current_sampling_state;
   *sampling_frequency = self->_current_sampling_frequency;

   return RESULT_OK;
}

/***********************************************************************************************************************
 * Global function definitions
 **********************************************************************************************************************/

result_t weight_sensor_init(weight_sensor_t *const self,
                            const system_time_interface_t *const system_time,
                            const spi_driver_interface_t *const spi_interface,
                            const sts30_dis_temp_sensor_interface_t *const temp_sensor_interface)
{
   RETURN_ERR_IF_NULL(self, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_INTERFACE_NULL(system_time, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_INTERFACE_NULL(spi_interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);
   RETURN_ERR_IF_INTERFACE_NULL(temp_sensor_interface, WEIGHT_SENSOR_ERROR_NULL_POINTER);

   self->_initialized = false;

   self->interface.parent = self;

   self->interface.reset_scale = reset_scale;
   self->interface.set_zero_offset = set_zero_offset;
   self->interface.get_zero_offset = get_zero_offset;
   self->interface.set_calibration_factor = set_calibration_factor;
   self->interface.get_calibration_factor = get_calibration_factor;
   self->interface.get_calibration_state = get_calibration_state;
   self->interface.get_state = get_state;
   self->interface.try_tare_if_stable = try_tare_if_stable;
   self->interface.try_calibrate_if_stable = try_calibrate_if_stable;
   self->interface.read_weight_data = read_weight_data;
   self->interface.get_is_stable = get_is_stable;
   self->interface.process = process;

   self->_system_time = system_time;
   self->_temp_sensor = temp_sensor_interface;

   self->_zero_offset = 0;
   self->_calibration_factor = UNSET_CALIBRATION_FACTOR;
   self->_calibration_state = WEIGHT_SENSOR_CALIBRATION_STATE_UNCALIBRATED;

   result_t result = reset_ma(self);
   self->_last_calibration_sample_time_ms = 0u;
   self->_calibration_start_time_ms = 0u;
   self->_calibration_state_before_start = WEIGHT_SENSOR_CALIBRATION_STATE_UNCALIBRATED;

   self->_current_sampling_state = WEIGHT_SAMPLING_STATE_RING_PRESENT;
   self->_current_sampling_frequency = SAMPLING_FREQUENCY_SLOW_RING_ON_HZ;
   self->_ring_replaced_time_ms = 0u;

   self->_last_sample_time_ms = 0u;
   self->_latest_weight_data.weight_mg = 0;
   self->_latest_weight_data.stddev_mg = 0u;
   self->_latest_weight_data.temp_deciC = 0u;
   self->_latest_weight_data.is_ring_present = true;
   self->_latest_weight_data.time_ms = 0u;
   self->_is_weight_data_stale = true;

   IF_OK_RUN_AND_UPDATE(result, ads1235_init(&self->_adc_driver, spi_interface));

   if(IS_OK(result))
   {
      self->_initialized = true;
   }

   return result;
}
