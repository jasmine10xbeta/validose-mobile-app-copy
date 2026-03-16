/**
 * @file tilt_detection_test.cpp
 * @brief Unit tests for the tilt detection module
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include <gtest/gtest.h>
#include <math.h>

extern "C"
{
#include "common.h"
#include "imu_interface.h"
#include "tilt_detection.h"

#include "mock/imu/imu.h"
#include "mock/system_time/system_time.h"
}

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#ifndef TILT_DETECTION_LOWER_LIMIT_DEGREES
#   error "TILT_DETECTION_LOWER_LIMIT_DEGREES not defined"
#endif

#ifndef TILT_DETECTION_UPPER_LIMIT_DEGREES
#   error "TILT_DETECTION_UPPER_LIMIT_DEGREES not defined"
#endif

#define TEST_IMU_G_RANGE          ((int16_t)(2u))                                  // +/- 2g range
#define TEST_IMU_LSB_PER_G        ((int16_t)((INT16_MAX + 1u) / TEST_IMU_G_RANGE)) // LSBs per g for the test IMU
#define TEST_IMU_GRAV_MM_S_SQR    (int16_t)(9807)                                  // g = 9807 mm/s^2
#define TEST_IMU_MS_PER_SAMPLE    (1000u / IMU_ACTIVE_SAMPLING_RATE_HZ)            // ms per sample
#define TEST_IMU_MAX_DATA_ENTRIES (IMU_MAX_FIFO_FRAMES)

#define RAD_TO_DEG(_rad_) ((_rad_) * (180.0f / M_PI))
#define DEG_TO_RAD(_deg_) ((_deg_) * (M_PI / 180.0f))

#define ANGLE_MIN_STEP_DEG (0.1f) // Minimum step in degrees for angle-based IMU data generation

#define IMU_DATA_ENTRY(_acc_x_, _acc_y_, _acc_z_)                                                                      \
   (imu_data_entry_t)                                                                                                  \
   {                                                                                                                   \
      .acc_x_axis = (_acc_x_), .acc_y_axis = (_acc_y_), .acc_z_axis = (_acc_z_)                                        \
   }

#define CREATE_IMU_DATA_ENTRY_FROM_ANGLE(_angle_deg_, _upright_axis_)                                                  \
   ({                                                                                                                  \
      int16_t acc_x = 0;                                                                                               \
      int16_t acc_y = 0;                                                                                               \
      int16_t acc_z = 0;                                                                                               \
      const float angle_rad = DEG_TO_RAD(_angle_deg_);                                                                 \
      const int16_t g_to_lsb_factor = (int16_t)((TEST_IMU_GRAV_MM_S_SQR * TEST_IMU_LSB_PER_G) / 1000u);                \
      int16_t sign = 1;                                                                                                \
                                                                                                                       \
      if((TILT_DETECTION_AXIS_X_INVERTED == _upright_axis_) || (TILT_DETECTION_AXIS_Y_INVERTED == _upright_axis_)      \
         || (TILT_DETECTION_AXIS_Z_INVERTED == _upright_axis_))                                                        \
      {                                                                                                                \
         sign = -1;                                                                                                    \
      }                                                                                                                \
                                                                                                                       \
      switch(_upright_axis_)                                                                                           \
      {                                                                                                                \
         case TILT_DETECTION_AXIS_X:                                                                                   \
         /* Fallthrough */                                                                                             \
         case TILT_DETECTION_AXIS_X_INVERTED:                                                                          \
            acc_x = (int16_t)(sign * g_to_lsb_factor * cosf(angle_rad));                                               \
            acc_y = 0;                                                                                                 \
            acc_z = (int16_t)(sign * g_to_lsb_factor * sinf(angle_rad));                                               \
            break;                                                                                                     \
         case TILT_DETECTION_AXIS_Y:                                                                                   \
         /* Fallthrough */                                                                                             \
         case TILT_DETECTION_AXIS_Y_INVERTED:                                                                          \
            acc_x = 0;                                                                                                 \
            acc_y = (int16_t)(sign * g_to_lsb_factor * cosf(angle_rad));                                               \
            acc_z = (int16_t)(sign * g_to_lsb_factor * sinf(angle_rad));                                               \
            break;                                                                                                     \
         case TILT_DETECTION_AXIS_Z:                                                                                   \
         /* Fallthrough */                                                                                             \
         case TILT_DETECTION_AXIS_Z_INVERTED:                                                                          \
            acc_x = (int16_t)(sign * g_to_lsb_factor * sinf(angle_rad));                                               \
            acc_y = 0;                                                                                                 \
            acc_z = (int16_t)(sign * g_to_lsb_factor * cosf(angle_rad));                                               \
            break;                                                                                                     \
         case TILT_DETECTION_AXIS_UNKNOWN:                                                                             \
         /* Fallthrough */                                                                                             \
         case TILT_DETECTION_AXIS_MAX:                                                                                 \
         /* Fallthrough */                                                                                             \
         default:                                                                                                      \
            break;                                                                                                     \
      }                                                                                                                \
      IMU_DATA_ENTRY(acc_x, acc_y, acc_z);                                                                             \
   })

/* Range of IMU data entries from below the lower limit to above the upper limit */
#define IMU_DATA_VALID_TILT_START(_upright_axis_)                                                                      \
   {CREATE_IMU_DATA_ENTRY_FROM_ANGLE(TILT_DETECTION_LOWER_LIMIT_DEGREES - ANGLE_MIN_STEP_DEG, (_upright_axis_)),       \
    CREATE_IMU_DATA_ENTRY_FROM_ANGLE(TILT_DETECTION_LOWER_LIMIT_DEGREES, (_upright_axis_)),                            \
    CREATE_IMU_DATA_ENTRY_FROM_ANGLE(TILT_DETECTION_LOWER_LIMIT_DEGREES + ANGLE_MIN_STEP_DEG, (_upright_axis_)),       \
    CREATE_IMU_DATA_ENTRY_FROM_ANGLE((TILT_DETECTION_LOWER_LIMIT_DEGREES + TILT_DETECTION_UPPER_LIMIT_DEGREES) / 2.0f, \
                                     (_upright_axis_)),                                                                \
    CREATE_IMU_DATA_ENTRY_FROM_ANGLE(TILT_DETECTION_UPPER_LIMIT_DEGREES - ANGLE_MIN_STEP_DEG, (_upright_axis_)),       \
    CREATE_IMU_DATA_ENTRY_FROM_ANGLE(TILT_DETECTION_UPPER_LIMIT_DEGREES, (_upright_axis_)),                            \
    CREATE_IMU_DATA_ENTRY_FROM_ANGLE(TILT_DETECTION_UPPER_LIMIT_DEGREES + ANGLE_MIN_STEP_DEG, (_upright_axis_))}

/* Number of entries in IMU_DATA_VALID_TILT_START that count as tilted */
#define IMU_DATA_VALID_TILT_START_NUM_TILTED_ENTRIES (2u)

/* Range of IMU data entries from above the upper limit to below the lower limit */
#define IMU_DATA_VALID_TILT_END(_upright_axis_)                                                                        \
   {CREATE_IMU_DATA_ENTRY_FROM_ANGLE(TILT_DETECTION_UPPER_LIMIT_DEGREES + ANGLE_MIN_STEP_DEG, (_upright_axis_)),       \
    CREATE_IMU_DATA_ENTRY_FROM_ANGLE(TILT_DETECTION_UPPER_LIMIT_DEGREES, (_upright_axis_)),                            \
    CREATE_IMU_DATA_ENTRY_FROM_ANGLE(TILT_DETECTION_UPPER_LIMIT_DEGREES - ANGLE_MIN_STEP_DEG, (_upright_axis_)),       \
    CREATE_IMU_DATA_ENTRY_FROM_ANGLE((TILT_DETECTION_LOWER_LIMIT_DEGREES + TILT_DETECTION_UPPER_LIMIT_DEGREES) / 2.0f, \
                                     (_upright_axis_)),                                                                \
    CREATE_IMU_DATA_ENTRY_FROM_ANGLE(TILT_DETECTION_LOWER_LIMIT_DEGREES + ANGLE_MIN_STEP_DEG, (_upright_axis_)),       \
    CREATE_IMU_DATA_ENTRY_FROM_ANGLE(TILT_DETECTION_LOWER_LIMIT_DEGREES, (_upright_axis_)),                            \
    CREATE_IMU_DATA_ENTRY_FROM_ANGLE(TILT_DETECTION_LOWER_LIMIT_DEGREES - ANGLE_MIN_STEP_DEG, (_upright_axis_))}

/* Number of entries in IMU_DATA_VALID_TILT_END that count as tilted */
#define IMU_DATA_VALID_TILT_END_NUM_TILTED_ENTRIES (5u)

/***********************************************************************************************************************
 * Helper Functions
 **********************************************************************************************************************/

/**
 * @brief Dirties a tilt detection instance
 */
static void dirty_tilt_detection_instance(tilt_detection_t *const p_instance)
{
   p_instance->interface.parent = NULL;
   p_instance->interface.auto_detect_upright_axis = NULL;
   p_instance->interface.set_upright_axis = NULL;
   p_instance->interface.clear_tilts = NULL;
   p_instance->interface.get_tilts = NULL;
   p_instance->interface.set_module_active = NULL;
   p_instance->interface.set_module_dormant = NULL;

   p_instance->_system_time_interface = NULL;
   p_instance->_imu_interface = NULL;

   p_instance->_upright_axis = TILT_DETECTION_AXIS_X;
   p_instance->_tilt_active = true;
   p_instance->_tilt_sample_count = 123u;

   p_instance->_is_initialized = false;
}

/**
 * @brief Asserts that a tilt detection instance is initialized correctly
 */
static void assert_tilt_detection_instance_initialized(const tilt_detection_t *const p_instance,
                                                       const system_time_interface_t *const p_system_time_interface,
                                                       const imu_interface_t *const p_imu_interface)
{
   // Check that the interface is assigned correctly
   ASSERT_EQ(p_instance->interface.parent, p_instance);
   ASSERT_NE(p_instance->interface.auto_detect_upright_axis, nullptr);
   ASSERT_NE(p_instance->interface.set_upright_axis, nullptr);
   ASSERT_NE(p_instance->interface.clear_tilts, nullptr);
   ASSERT_NE(p_instance->interface.get_tilts, nullptr);
   ASSERT_NE(p_instance->interface.set_module_active, nullptr);
   ASSERT_NE(p_instance->interface.set_module_dormant, nullptr);

   // Check that the dependencies are assigned correctly
   ASSERT_EQ(p_instance->_system_time_interface, p_system_time_interface);
   ASSERT_EQ(p_instance->_imu_interface, p_imu_interface);

   // Check that the private data is initialized correctly
   ASSERT_EQ(p_instance->_upright_axis, TILT_DETECTION_DEFAULT_UPRIGHT_AXIS);
   ASSERT_EQ(p_instance->_tilt_active, false);
   ASSERT_EQ(p_instance->_tilt_sample_count, 0u);
   ASSERT_EQ(p_instance->_is_initialized, true);
}

/**
 * @brief Mock function that does nothing to replace the IMU's `clear_imu_data` method when needed
 */
static result_t mock_clear_imu_data_noop(const imu_interface_t *const interface)
{
   (void)interface;
   return RESULT_OK;
}

/***********************************************************************************************************************
 * Test Fixtures
 **********************************************************************************************************************/

class TiltDetectionTestSuite: public testing::Test
{
protected:
   tilt_detection_t test_tilt_detection = {0};
   system_time_t mock_system_time = {0};
   imu_t mock_imu = {0};

   void SetUp() override
   {
      // Initialize dependencies
      result_t result = mock_system_time_init(&mock_system_time);
      ASSERT_EQ(result, RESULT_OK);

      result = mock_imu_init(&mock_imu);
      ASSERT_EQ(result, RESULT_OK);

      result = tilt_detection_init(&test_tilt_detection, &mock_system_time.interface, &mock_imu.interface);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_EQ(test_tilt_detection._is_initialized, true);
   }

   void TearDown() override
   {
   }
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

/**
 * @brief Test initialization happy path
 *
 * This test ensures that the tilt detection module initializes correctly with valid parameters.
 */
TEST_F(TiltDetectionTestSuite, initialization_test_happy_path)
{
   // Note: Initialization is done in SetUp() and the result is checked there.

   assert_tilt_detection_instance_initialized(&test_tilt_detection, &mock_system_time.interface, &mock_imu.interface);
}

/**
 * @brief Test initialization with invalid parameters
 */
TEST_F(TiltDetectionTestSuite, initialization_test_invalid_parameters)
{
   system_time_t uninit_system_time = {0};
   imu_t uninit_imu = {0};

   result_t result = RESULT_OK;

   // Test null self pointer
   result = tilt_detection_init(NULL, &mock_system_time.interface, &mock_imu.interface);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_PTR_NULL);

   // Test null system time interface
   result = tilt_detection_init(&test_tilt_detection, NULL, &mock_imu.interface);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_PTR_NULL);

   // Test null IMU interface
   result = tilt_detection_init(&test_tilt_detection, &mock_system_time.interface, NULL);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_PTR_NULL);

   // Test uninitialized system time interface
   result = tilt_detection_init(&test_tilt_detection, &uninit_system_time.interface, &mock_imu.interface);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_PTR_NULL);

   // Test uninitialized IMU interface
   result = tilt_detection_init(&test_tilt_detection, &mock_system_time.interface, &uninit_imu.interface);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_PTR_NULL);
}

/**
 * @brief Test initialization of a "dirty" instance
 *
 * This test ensures that initializing an already initialized instance resets its state correctly.
 */
TEST_F(TiltDetectionTestSuite, initialization_test_dirty_instance)
{
   // Dirty the instance
   dirty_tilt_detection_instance(&test_tilt_detection);

   // Re-initialize the instance
   result_t result = tilt_detection_init(&test_tilt_detection, &mock_system_time.interface, &mock_imu.interface);
   ASSERT_EQ(result, RESULT_OK);

   assert_tilt_detection_instance_initialized(&test_tilt_detection, &mock_system_time.interface, &mock_imu.interface);
}

/**
 * @brief Test `auto_detect_upright_axis` method happy path
 */
TEST_F(TiltDetectionTestSuite, auto_detect_upright_axis_happy_path)
{
   tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;

   mock_imu.interface.clear_imu_data = mock_clear_imu_data_noop;

   const struct
   {
      TILT_DETECTION_AXIS expected_axis;
      imu_data_entry_t entry;
   } test_cases[] = {
      {TILT_DETECTION_AXIS_X, IMU_DATA_ENTRY(1000, 0, 0)},
      {TILT_DETECTION_AXIS_Y, IMU_DATA_ENTRY(0, 1000, 0)},
      {TILT_DETECTION_AXIS_Z, IMU_DATA_ENTRY(0, 0, 1000)},
      {TILT_DETECTION_AXIS_X_INVERTED, IMU_DATA_ENTRY(-1000, 0, 0)},
      {TILT_DETECTION_AXIS_Y_INVERTED, IMU_DATA_ENTRY(0, -1000, 0)},
      {TILT_DETECTION_AXIS_Z_INVERTED, IMU_DATA_ENTRY(0, 0, -1000)},
   };

   for(size_t idx = 0; idx < ARRAY_LEN(test_cases); idx++)
   {
      TILT_DETECTION_AXIS detected_axis = TILT_DETECTION_AXIS_UNKNOWN;

      mock_imu_set_data_entries(&mock_imu, &test_cases[idx].entry, 1u);

      result_t result = p_tilt_detection_ifc->auto_detect_upright_axis(p_tilt_detection_ifc, &detected_axis);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_EQ(detected_axis, test_cases[idx].expected_axis);
      ASSERT_EQ(mock_imu._state, IMU_STATE_DORMANT);
   }
}

/**
 * @brief Test `auto_detect_upright_axis` method with invalid parameters
 */
TEST_F(TiltDetectionTestSuite, auto_detect_upright_axis_invalid_parameters)
{
   tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;

   TILT_DETECTION_AXIS detected_axis = TILT_DETECTION_AXIS_UNKNOWN;

   // Test null interface
   result_t result = p_tilt_detection_ifc->auto_detect_upright_axis(NULL, &detected_axis);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_PTR_NULL);
   ASSERT_EQ(detected_axis, TILT_DETECTION_AXIS_UNKNOWN);

   // Test null parent pointer
   p_tilt_detection_ifc->parent = NULL;
   result = p_tilt_detection_ifc->auto_detect_upright_axis(p_tilt_detection_ifc, &detected_axis);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_PTR_NULL);
   ASSERT_EQ(detected_axis, TILT_DETECTION_AXIS_UNKNOWN);
   p_tilt_detection_ifc->parent = &test_tilt_detection;

   // Test null output pointer
   result = p_tilt_detection_ifc->auto_detect_upright_axis(p_tilt_detection_ifc, NULL);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_PTR_NULL);
   ASSERT_EQ(detected_axis, TILT_DETECTION_AXIS_UNKNOWN);
}

/**
 * @brief Test `auto_detect_upright_axis` method when IMU fails
 */
TEST_F(TiltDetectionTestSuite, auto_detect_upright_axis_imu_failure)
{
   tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;
   TILT_DETECTION_AXIS detected_axis = TILT_DETECTION_AXIS_UNKNOWN;

   mock_imu._fail_imu_reg_read = true;

   result_t result = p_tilt_detection_ifc->auto_detect_upright_axis(p_tilt_detection_ifc, &detected_axis);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_GET_IMU_DATA_ENTRY_COUNT);
   ASSERT_EQ(detected_axis, TILT_DETECTION_AXIS_UNKNOWN);
}

/**
 * @brief Test `auto_detect_upright_axis` method when no data is available
 */
TEST_F(TiltDetectionTestSuite, auto_detect_upright_axis_no_data)
{
   tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;
   TILT_DETECTION_AXIS detected_axis = TILT_DETECTION_AXIS_UNKNOWN;

   result_t result = p_tilt_detection_ifc->auto_detect_upright_axis(p_tilt_detection_ifc, &detected_axis);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_NO_IMU_DATA);
   ASSERT_EQ(detected_axis, TILT_DETECTION_AXIS_UNKNOWN);
}

/**
 * @brief Test `auto_detect_upright_axis` methon when the maximum data entries are provided
 */
TEST_F(TiltDetectionTestSuite, auto_detect_upright_axis_full_data)
{
   tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;
   TILT_DETECTION_AXIS detected_axis = TILT_DETECTION_AXIS_UNKNOWN;

   mock_imu.interface.clear_imu_data = mock_clear_imu_data_noop;

   imu_data_entry_t dummy_entries[TEST_IMU_MAX_DATA_ENTRIES] = {0};

   for(uint32_t idx = 0u; idx < ARRAY_LEN(dummy_entries); idx++)
   {
      dummy_entries[idx] = CREATE_IMU_DATA_ENTRY_FROM_ANGLE(0.0f, TILT_DETECTION_AXIS_X);
   }

   mock_imu_set_data_entries(&mock_imu, dummy_entries, ARRAY_LEN(dummy_entries));

   result_t result = p_tilt_detection_ifc->auto_detect_upright_axis(p_tilt_detection_ifc, &detected_axis);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(detected_axis, TILT_DETECTION_AXIS_X);
}

/**
 * @brief Test `set_upright_axis` method happy path
 */
TEST_F(TiltDetectionTestSuite, set_upright_axis_happy_path)
{
   tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;

   test_tilt_detection._upright_axis = TILT_DETECTION_AXIS_X;

   result_t result = p_tilt_detection_ifc->set_upright_axis(p_tilt_detection_ifc, TILT_DETECTION_AXIS_Z);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(test_tilt_detection._upright_axis, TILT_DETECTION_AXIS_Z);
}

/**
 * @brief Test `set_upright_axis` method with invalid parameters
 */
TEST_F(TiltDetectionTestSuite, set_upright_axis_invalid_parameters)
{
   tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;

   result_t result = p_tilt_detection_ifc->set_upright_axis(NULL, TILT_DETECTION_AXIS_X);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_PTR_NULL);

   p_tilt_detection_ifc->parent = NULL;
   result = p_tilt_detection_ifc->set_upright_axis(p_tilt_detection_ifc, TILT_DETECTION_AXIS_X);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_PTR_NULL);
}

/**
 * @brief Test `set_upright_axis` method with invalid axis
 */
TEST_F(TiltDetectionTestSuite, set_upright_axis_invalid_axis)
{
   tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;

   test_tilt_detection._upright_axis = TILT_DETECTION_AXIS_Y;

   result_t result = p_tilt_detection_ifc->set_upright_axis(p_tilt_detection_ifc, TILT_DETECTION_AXIS_UNKNOWN);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_INVALID_AXIS);
   ASSERT_EQ(test_tilt_detection._upright_axis, TILT_DETECTION_AXIS_Y);
}

/**
 * @brief Test `clear_tilts` method happy path
 */
TEST_F(TiltDetectionTestSuite, clear_tilts_happy_path)
{
   tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;

   mock_imu._state = IMU_STATE_ACTIVE;
   mock_imu._mock_imu_data_entry_count = 10u;

   result_t result = p_tilt_detection_ifc->clear_tilts(p_tilt_detection_ifc);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(mock_imu._mock_imu_data_entry_count, 0u);
}

/**
 * @brief Test `clear_tilts` method with invalid parameters
 */
TEST_F(TiltDetectionTestSuite, clear_tilts_invalid_parameters)
{
   tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;

   mock_imu._mock_imu_data_entry_count = 10u;

   // Test null interface pointer
   result_t result = p_tilt_detection_ifc->clear_tilts(NULL);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_PTR_NULL);
   ASSERT_EQ(mock_imu._mock_imu_data_entry_count, 10u);

   // Test null parent pointer
   p_tilt_detection_ifc->parent = NULL;
   result = p_tilt_detection_ifc->clear_tilts(p_tilt_detection_ifc);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_PTR_NULL);
   ASSERT_EQ(mock_imu._mock_imu_data_entry_count, 10u);
}

/**
 * @brief Test `clear_tilts` method when IMU fails
 */
TEST_F(TiltDetectionTestSuite, clear_tilts_imu_failure)
{
   tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;

   mock_imu._state = IMU_STATE_ACTIVE;
   mock_imu._fail_imu_reg_write = true;

   result_t result = p_tilt_detection_ifc->clear_tilts(p_tilt_detection_ifc);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_CLEAR_IMU_DATA);
   ASSERT_EQ(mock_imu._state, IMU_STATE_ACTIVE);
}

/**
 * @brief Test `get_tilts` method happy path
 *
 * Tests the `get_tilts` method with the expected conditions where tilts are detected correctly.
 */
TEST_F(TiltDetectionTestSuite, get_tilts_happy_path)
{
   TILT_DETECTION_AXIS axes[] = {TILT_DETECTION_AXIS_X,
                                 TILT_DETECTION_AXIS_Y,
                                 TILT_DETECTION_AXIS_Z,
                                 TILT_DETECTION_AXIS_X_INVERTED,
                                 TILT_DETECTION_AXIS_Y_INVERTED,
                                 TILT_DETECTION_AXIS_Z_INVERTED};

   for(size_t axis_idx = 0; axis_idx < ARRAY_LEN(axes); axis_idx++)
   {
      // Tilt start and end data
      const imu_data_entry_t tilt_start_data[] = IMU_DATA_VALID_TILT_START(axes[axis_idx]);
      const imu_data_entry_t tilt_end_data[] = IMU_DATA_VALID_TILT_END(axes[axis_idx]);

      // Calculate number of additional IMU data entries needed to satisfy duration threshold
      const uint32_t num_required_samples = (TILT_DETECTION_DURATION_THRESHOLD_MS / TEST_IMU_MS_PER_SAMPLE);
      const uint32_t num_current_samples
         = IMU_DATA_VALID_TILT_START_NUM_TILTED_ENTRIES + IMU_DATA_VALID_TILT_END_NUM_TILTED_ENTRIES;
      const uint32_t num_remaining_samples
         = (num_required_samples > num_current_samples) ? (num_required_samples - num_current_samples) : 0u;

      // Generate remaining IMU data entries to satisfy duration threshold
      imu_data_entry_t remaining_data[num_remaining_samples] = {0};
      for(uint32_t idx = 0; idx < num_remaining_samples; idx++)
      {
         remaining_data[idx]
            = CREATE_IMU_DATA_ENTRY_FROM_ANGLE(TILT_DETECTION_UPPER_LIMIT_DEGREES + ANGLE_MIN_STEP_DEG, axes[axis_idx]);
      }

      uint8_t new_tilt_count = 0u;
      tilt_data_t tilts[TILT_DETECTION_MAX_TILTS] = {0};

      tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;

      // Set upright axis
      test_tilt_detection._upright_axis = axes[axis_idx];

      // Load the mock IMU with valid tilt start data
      mock_imu_set_data_entries(&mock_imu, tilt_start_data, ARRAY_LEN(tilt_start_data));

      // Call get_tilts to process the data
      result_t result = p_tilt_detection_ifc->get_tilts(p_tilt_detection_ifc, &new_tilt_count, tilts);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_EQ(new_tilt_count, 0u);

      // Load remaining data to satisfy duration threshold
      mock_imu_set_data_entries(&mock_imu, remaining_data, num_remaining_samples);

      // Call get_tilts to process the data
      result = p_tilt_detection_ifc->get_tilts(p_tilt_detection_ifc, &new_tilt_count, tilts);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_EQ(new_tilt_count, 0u);

      // Load the mock IMU with valid tilt end data
      mock_imu_set_data_entries(&mock_imu, tilt_end_data, ARRAY_LEN(tilt_end_data));

      // Set system time for when the tilt is detected
      mock_system_time._current_time_ms = 1234u;

      // Call get_tilts to process the data
      result = p_tilt_detection_ifc->get_tilts(p_tilt_detection_ifc, &new_tilt_count, tilts);
      ASSERT_EQ(result, RESULT_OK);
      ASSERT_EQ(new_tilt_count, 1u);
      ASSERT_EQ(tilts[0].detected_at_time_ms, 1234u);
      // Duration is quantized to IMU sample periods and can include the boundary-closing sample.
      ASSERT_GE(tilts[0].duration_ms, TILT_DETECTION_DURATION_THRESHOLD_MS);
      ASSERT_LE(tilts[0].duration_ms, (TILT_DETECTION_DURATION_THRESHOLD_MS + TEST_IMU_MS_PER_SAMPLE));
   }
}

/**
 * @brief Test `get_tilts` method with invalid parameters
 */
TEST_F(TiltDetectionTestSuite, get_tilts_invalid_parameters)
{
   uint8_t new_tilt_count = 0u;
   tilt_data_t tilts[TILT_DETECTION_MAX_TILTS] = {0};

   tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;

   // Test null interface pointer
   result_t result = p_tilt_detection_ifc->get_tilts(NULL, &new_tilt_count, tilts);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_PTR_NULL);

   // Test null parent pointer
   p_tilt_detection_ifc->parent = NULL;
   result = p_tilt_detection_ifc->get_tilts(p_tilt_detection_ifc, &new_tilt_count, tilts);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_PTR_NULL);
   p_tilt_detection_ifc->parent = &test_tilt_detection;

   // Test null new_tilt_count pointer
   result = p_tilt_detection_ifc->get_tilts(p_tilt_detection_ifc, NULL, tilts);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_PTR_NULL);

   // Test null tilts pointer
   result = p_tilt_detection_ifc->get_tilts(p_tilt_detection_ifc, &new_tilt_count, NULL);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_PTR_NULL);
}

/**
 * @brief Test `get_tilts` method with invalid axis set
 */
TEST_F(TiltDetectionTestSuite, get_tilts_invalid_axis_set)
{
   uint8_t new_tilt_count = 0u;
   tilt_data_t tilts[TILT_DETECTION_MAX_TILTS] = {0};

   tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;

   test_tilt_detection._upright_axis = TILT_DETECTION_AXIS_UNKNOWN;

   result_t result = p_tilt_detection_ifc->get_tilts(p_tilt_detection_ifc, &new_tilt_count, tilts);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_INVALID_AXIS);
}

/**
 * @brief Test `get_tilts` method when IMU fails
 */
TEST_F(TiltDetectionTestSuite, get_tilts_imu_failure)
{
   uint8_t new_tilt_count = 0u;
   tilt_data_t tilts[TILT_DETECTION_MAX_TILTS] = {0};

   tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;

   mock_imu._fail_imu_reg_read = true;

   result_t result = p_tilt_detection_ifc->get_tilts(p_tilt_detection_ifc, &new_tilt_count, tilts);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_GET_IMU_DATA_ENTRY_COUNT);
}

/**
 * @brief Test `get_tilts` method with no IMU data
 */
TEST_F(TiltDetectionTestSuite, get_tilts_no_imu_data)
{
   uint8_t new_tilt_count = 0u;
   tilt_data_t tilts[TILT_DETECTION_MAX_TILTS] = {0};

   tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;

   result_t result = p_tilt_detection_ifc->get_tilts(p_tilt_detection_ifc, &new_tilt_count, tilts);
   ASSERT_EQ(new_tilt_count, 0u);
}

/**
 * @brief Test `set_module_active` method happy path
 */
TEST_F(TiltDetectionTestSuite, set_module_active_happy_path)
{
   mock_imu._state = IMU_STATE_DORMANT;

   tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;

   result_t result = p_tilt_detection_ifc->set_module_active(p_tilt_detection_ifc);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(mock_imu._state, IMU_STATE_ACTIVE);
}

/**
 * @brief Test `set_module_active` method with invalid parameters
 */
TEST_F(TiltDetectionTestSuite, set_module_active_invalid_parameters)
{
   tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;

   result_t result = p_tilt_detection_ifc->set_module_active(NULL);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_PTR_NULL);

   p_tilt_detection_ifc->parent = NULL;
   result = p_tilt_detection_ifc->set_module_active(p_tilt_detection_ifc);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_PTR_NULL);
}

/**
 * @brief Test `set_module_active` method when IMU fails
 */
TEST_F(TiltDetectionTestSuite, set_module_active_imu_failure)
{
   mock_imu._state = IMU_STATE_DORMANT;
   mock_imu._fail_imu_reg_write = true;

   tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;

   result_t result = p_tilt_detection_ifc->set_module_active(p_tilt_detection_ifc);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_SET_IMU_ACTIVE);
   ASSERT_EQ(mock_imu._state, IMU_STATE_DORMANT);
}

/**
 * @brief Test `set_module_dormant` method happy path
 */
TEST_F(TiltDetectionTestSuite, set_module_dormant_happy_path)
{
   mock_imu._state = IMU_STATE_ACTIVE;

   tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;

   result_t result = p_tilt_detection_ifc->set_module_dormant(p_tilt_detection_ifc);
   ASSERT_EQ(result, RESULT_OK);
   ASSERT_EQ(mock_imu._state, IMU_STATE_DORMANT);
}

/**
 * @brief Test `set_module_dormant` method with invalid parameters
 */
TEST_F(TiltDetectionTestSuite, set_module_dormant_invalid_parameters)
{
   tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;

   result_t result = p_tilt_detection_ifc->set_module_dormant(NULL);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_PTR_NULL);

   p_tilt_detection_ifc->parent = NULL;
   result = p_tilt_detection_ifc->set_module_dormant(p_tilt_detection_ifc);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_PTR_NULL);
}

/**
 * @brief Test `set_module_dormant` method when IMU fails
 */
TEST_F(TiltDetectionTestSuite, set_module_dormant_imu_failure)
{
   mock_imu._state = IMU_STATE_ACTIVE;
   mock_imu._fail_imu_reg_write = true;

   tilt_detection_interface_t *p_tilt_detection_ifc = &test_tilt_detection.interface;

   result_t result = p_tilt_detection_ifc->set_module_dormant(p_tilt_detection_ifc);
   ASSERT_EQ(GET_ERR_UNIT(result), SW_UNIT_ID_TILT_DETECTION_MODULE);
   ASSERT_EQ(GET_ERR_CODE(result), TILT_DETECTION_ERROR_SET_IMU_DORMANT);
   ASSERT_EQ(mock_imu._state, IMU_STATE_ACTIVE);
}
