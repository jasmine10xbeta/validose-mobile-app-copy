#include "unity.h"
#include <board.h>

static void test_board_init()
{
   /* Note: this test should be run before custom_board_init() is called. */
   TEST_ASSERT_EQUAL(NRF_GPIO_PIN_DIR_INPUT, nrf_gpio_pin_dir_get(PIN_STIM_PWR_EN));
   TEST_ASSERT_EQUAL(NRF_GPIO_PIN_DIR_INPUT, nrf_gpio_pin_dir_get(PIN_STIM_BOOST_EN));
   board_init();
   TEST_ASSERT_EQUAL(NRF_GPIO_PIN_DIR_OUTPUT, nrf_gpio_pin_dir_get(PIN_STIM_PWR_EN));
   TEST_ASSERT_EQUAL(NRF_GPIO_PIN_DIR_OUTPUT, nrf_gpio_pin_dir_get(PIN_STIM_BOOST_EN));
   TEST_ASSERT_EQUAL(0, nrf_gpio_pin_read(PIN_STIM_PWR_EN));
   TEST_ASSERT_EQUAL(0, nrf_gpio_pin_read(PIN_STIM_BOOST_EN));
}

/**
 * @brief Check that configuration defines have not been altered.
 */
static void test_board_defines()
{
#ifndef BOARD_CUSTOM
   TEST_ASSERT_FALSE();
#endif
}

void test_board()
{
   UNITY_BEGIN();
   RUN_TEST(test_board_defines);
   RUN_TEST(test_board_init);
   UNITY_END();
}
