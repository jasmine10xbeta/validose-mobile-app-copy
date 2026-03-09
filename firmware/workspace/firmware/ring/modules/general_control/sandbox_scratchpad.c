
/***********************************************************************************************************************
 * SANDBOXING
 **********************************************************************************************************************/
#ifdef SANDBOXING

#   define DEBUG_DISABLE_UART 1

// #define ENABLE_TESTING_CAP_DETECTION
#   ifdef ENABLE_TESTING_CAP_DETECTION
static void sandbox_cap_detection(void);
#   endif

// #define ENABLE_DOSE_SANDBOX
#   ifdef ENABLE_DOSE_SANDBOX
static void sandbox_dose(void);
#   endif

// #define ENABLE_NFC_SANDBOX
#   ifdef ENABLE_NFC_SANDBOX
static void nfc_sandbox(void);
#   endif

// #define ENABLE_POWER_TESTING_SANDBOX
#   ifdef ENABLE_POWER_TESTING_SANDBOX
static void sandbox_power_testing(void);
/**
 * @brief Print one byte in binary on RTT channel 0.
 *
 * This routine “draws” each bit by sending the ASCII characters
 * ‘1’ and ‘0’ one-by-one with SEGGER_RTT_printf(), so the viewer
 * shows something like `0b10100101`.
 *
 * @param[in] value  Byte to be rendered.
 */
static void rtt_print_byte_binary(uint8_t value);
#   endif

// #define ENABLE_WIRELESS_COMMS_SANDBOX
#   ifdef ENABLE_WIRELESS_COMMS_SANDBOX
static void saadc_event_handler(nrfx_saadc_evt_t const *p_event);
uint16_t
   saadc_raw_to_mv(nrf_saadc_value_t sample, uint16_t vref_mv, uint8_t gain_num, uint8_t gain_den, uint8_t res_bits);
void sandbox_wireless_comms(void);

static nrf_saadc_value_t m_sample_buf;
static volatile bool m_saadc_done;

#   endif

// #define ENABLE_NFC_EEPROM_SANDBOX
#   ifdef ENABLE_NFC_EEPROM_SANDBOX
static void nfc_eeprom_sandbox();
#   endif

// #define ENABLE_PWR_MANAGEMENT_SANDBOX
#   ifdef ENABLE_PWR_MANAGEMENT_SANDBOX
static void sandbox_pwr_mgt();
#   endif

//#define ENABLE_FDS_MANAGER_SANDBOX
#   ifdef ENABLE_FDS_MANAGER_SANDBOX
static void sandbox_fds_manager(void);
#   endif

#   define ENABLE_RING_DATA_MANAGER_SANDBOX
#   ifdef ENABLE_RING_DATA_MANAGER_SANDBOX
static void sandbox_ring_data_manager(void);
#   endif

#endif

#ifdef SANDBOXING
#   ifdef ENABLE_TESTING_POWER_MANAGEMENT
sandbox_pwr_mgt();
#   endif /* ENABLE_TESTING_POWER_MANAGEMENT */

#   ifdef ENABLE_TESTING_DOSE
sandbox_dose();
#   endif /* ENABLE_TESTING_DOSE */

#   ifdef ENABLE_TESTING_WIRELESS_COMMS
sandbox_wireless_comms();
#   endif /* ENABLE_TESTING_WIRELESS_COMMS */

#   ifdef ENABLE_TESTING_CAP_DETECTION
sandbox_cap_detection();
#   endif /* ENABLE_TESTING_CAP_DETECTION */

#   ifdef ENABLE_TESTING_TILT_DETECTION
tilt_detection_sandbox();
#   endif /* ENABLE_TESTING_TILT_DETECTION */

#   ifdef ENABLE_TESTING_POWER_TESTING
sandbox_power_testing();
#   endif /* ENABLE_TESTING_POWER_TESTING */

#   ifdef ENABLE_TESTING_NFC
nfc_sandbox();
#   endif /* ENABLE_TESTING_NFC */

#   ifdef ENABLE_TESTING_NFC_EEPROM
nfc_eeprom_sandbox();
#   endif /* ENABLE_TESTING_NFC_EEPROM */

#   ifdef ENABLE_TESTING_NFC_MB
nfc_sandbox_mb();
#   endif /* ENABLE_TESTING_NFC_MB */

#   ifdef ENABLE_TESTING_NFC_EEPROM_QUEUE
nfc_eeprom_sandbox_queue();
#   endif /* ENABLE_TESTING_NFC_EEPROM_QUEUE */
#endif

#ifdef SANDBOXING

#   ifdef ENABLE_RING_DATA_MANAGER_SANDBOX
// ---------------------------------------------------------------------------------------------------------------------
// Ring Data Manager Sandbox
// ---------------------------------------------------------------------------------------------------------------------
#      define QUEUE_SIZE 50

void print_status(ring_status_t status)
{
   SEGGER_RTT_printf(0, "Ring Data Manager Status:\n");
   SEGGER_RTT_printf(0, "  Battery charge status: %lu\n", status.battery_charge_status);

   SEGGER_RTT_printf(0, "  Firmware version: %lu\n", status.firmware_version);
   SEGGER_RTT_printf(0, "  Hardware version: %lu\n", status.hardware_version);

   SEGGER_RTT_printf(0, "  Dose fifo count: %lu\n", status.dose_fifo_used_percent);
   SEGGER_RTT_printf(0, "  Battery data fifo count: %lu\n", status.battery_fifo_used_percent);
   SEGGER_RTT_printf(0, "  Docking data fifo count: %lu\n", status.docking_fifo_used_percent);
   SEGGER_RTT_printf(0, "  Error fifo count: %lu\n", status.error_fifo_used_percent);

   SEGGER_RTT_printf(0, "  Dose fifo watermark: %d\n", status.dose_fifo_used_percent_watermark);
   SEGGER_RTT_printf(0, "  Battery fifo watermark: %d\n", status.battery_fifo_used_percent_watermark);
   SEGGER_RTT_printf(0, "  Docking fifo watermark: %d\n", status.docking_fifo_used_percent_watermark);
   SEGGER_RTT_printf(0, "  Error fifo watermark: %d\n", status.error_fifo_used_percent_watermark);

   SEGGER_RTT_printf(0, "  Battery charge status: %d\n", status.battery_charge_status);

   SEGGER_RTT_printf(0, "  IMU sample frequency (mHz): %lu\n", status.imu_sample_frequency_millihz);
   SEGGER_RTT_printf(0, "  Battery sample frequency (mHz): %lu\n", status.battery_sample_frequency_millihz);

   SEGGER_RTT_printf(0, "  Temperature (C): %d\n", status.temperature_celsius);
   SEGGER_RTT_printf(0, "  Ship mode exit time (ms): %lu\n", status.ship_mode_exit_time_ms);
}

void sandbox_ring_data_manager(void)
{
   DEBUG_INFO("Starting Ring Data Manager sandbox...");

   result_t result = RESULT_OK;

   fds_manager_t test_fds_manager = {0};
   ring_data_manager_t ring_data_manager = {0};
   ring_data_manager_interface_t *ifc = NULL;

   nfc_tag_eeprom_queue_t test_dose_queue = {0};
   queue_t test_battery_data_queue = {0};
   queue_t test_docking_data_queue = {0};
   queue_t test_err_queue = {0};

   uint8_t test_battery_data_queue_buffer[QUEUE_SIZE * sizeof(ring_battery_data_t)] = {0};
   uint8_t test_docking_data_queue_buffer[QUEUE_SIZE * sizeof(ring_docking_event_t)] = {0};
   uint8_t test_err_queue_buffer[QUEUE_SIZE * sizeof(ring_error_t)] = {0};

   result = fds_manager_init(&test_fds_manager);
   ON_ERR_DEBUG_ERROR(result, "Error initializing FDS manager");

   result = nfc_tag_eeprom_queue_init(&test_dose_queue, sizeof(ring_dose_event_t), 4u, &m_nfc.interface, true);

   result = queue_init(&test_battery_data_queue,
                       test_battery_data_queue_buffer,
                       QUEUE_SIZE * sizeof(ring_battery_data_t),
                       sizeof(ring_battery_data_t));
   ON_ERR_DEBUG_ERROR(result, "Error initializing battery data queue");

   result = queue_init(&test_docking_data_queue,
                       test_docking_data_queue_buffer,
                       QUEUE_SIZE * sizeof(ring_docking_event_t),
                       sizeof(ring_docking_event_t));
   ON_ERR_DEBUG_ERROR(result, "Error initializing docking data queue");

   result = queue_init(&test_err_queue, test_err_queue_buffer, QUEUE_SIZE * sizeof(ring_error_t), sizeof(ring_error_t));
   ON_ERR_DEBUG_ERROR(result, "Error initializing error queue");

   result = init_ring_data_manager(&ring_data_manager,
                                   &m_systick.interface,
                                   &m_rtc.interface,
                                   &test_fds_manager.interface,
                                   &m_imu.interface,
                                   &m_battery.interface,
                                   &m_dose_detect.interface,
                                   &test_dose_queue.interface,
                                   &test_battery_data_queue.interface,
                                   &test_docking_data_queue.interface,
                                   &test_err_queue.interface);
   ON_ERR_DEBUG_ERROR(result, "Error initializing ring data manager");

   DEBUG_INFO("Ring Data Manager sandbox initialized successfully.");

   ifc = &ring_data_manager.interface;

   // Get and print the ring data status
   ring_status_t status = {0};
   result = ifc->get_status(ifc, &status);
   ON_ERR_DEBUG_ERROR(result, "Error getting ring data status");

   // Print the status
   print_status(status);

   ring_error_t ring_error = {0};
   ring_error.timestamp_unix = 123456789;
   ring_error.error_code[0] = DOCK_MANAGER_ERROR_INCORRECT_PPI_FOR_RX;

   // Enqueue 5 errors
   ifc->enqueue_error(ifc, DOCK_MANAGER_ERROR_INCORRECT_PPI_FOR_RX, sizeof(ring_error));
   ifc->enqueue_error(ifc, DOCK_MANAGER_ERROR_INCORRECT_PPI_FOR_RX, sizeof(ring_error));
   ifc->enqueue_error(ifc, DOCK_MANAGER_ERROR_INCORRECT_PPI_FOR_RX, sizeof(ring_error));
   ifc->enqueue_error(ifc, DOCK_MANAGER_ERROR_INCORRECT_PPI_FOR_RX, sizeof(ring_error));
   ifc->enqueue_error(ifc, DOCK_MANAGER_ERROR_INCORRECT_PPI_FOR_RX, sizeof(ring_error));

   // Check that there are 5 errors in the fifo queue
   // and that the watermark is updated
   result = ifc->get_status(ifc, &status);

   if(status.error_fifo_used_percent != 10) // 5/50 = 10%
   {
      DEBUG_ERROR("Error fifo used percent incorrect after enqueueing errors. Expected 10, got %lu",
                  status.error_fifo_used_percent);
   }

   if(status.error_fifo_used_percent_watermark != 10)
   {
      DEBUG_ERROR("Error fifo watermark incorrect after enqueueing errors. Expected 10, got %lu",
                  status.error_fifo_used_percent_watermark);
   }

   // Infinite loop to hold the program here until reset
   SEGGER_RTT_printf(0, "Awaiting reset.\n");
   while(1)
   {
      nrf_delay_ms(1000);
      SEGGER_RTT_printf(0, "...\n");
      feed_watchdog();
   }
}
// ---------------------------------------------------------------------------------------------------------------------
#   endif

#   ifdef ENABLE_FDS_MANAGER_SANDBOX
void sandbox_fds_manager()
{
   // Test the FDS Manager
   fds_manager_t fds_manager = {0};
   fds_manager_interface_t *ifc = NULL;

   result_t result = fds_manager_init(&fds_manager);
   ifc = &fds_manager.interface;

   if(IS_OK(result))
   {
      SEGGER_RTT_printf(0, "FDS Manager initialized successfully.\n");
   }
   else
   {
      SEGGER_RTT_printf(0, "FDS Manager initialization failed. Error Code: %d\n", GET_ERR_CODE(result));
   }

   // Retrieve all records
   uint64_t ship_exit = 0;
   uint64_t imu_hz = 0;
   uint64_t battery = 0;

   IF_OK_RUN_AND_UPDATE(result, ifc->retrieve_uint64_t(ifc, RECORD_ID_SHIP_MODE_EXIT_TIME, &ship_exit));
   IF_OK_RUN_AND_UPDATE(result, ifc->retrieve_uint64_t(ifc, RECORD_ID_IMU_SAMPLE_FREQUENCY, &imu_hz));
   IF_OK_RUN_AND_UPDATE(result, ifc->retrieve_uint64_t(ifc, RECORD_ID_BATTERY_SAMPLE_FREQUENCY, &battery));

   if(IS_OK(result))
   {
      SEGGER_RTT_printf(0, "FDS Manager Records retrieved successfully.\n");
      SEGGER_RTT_printf(0, "Ship Mode Exit Time: %llu \n", ship_exit);
      SEGGER_RTT_printf(0, "IMU Sample Frequency: %llu Hz\n", imu_hz);
      SEGGER_RTT_printf(0, "Battery Sample Frequency: %llu Hz\n", battery);
   }
   else
   {
      SEGGER_RTT_printf(0, "FDS Manager Records retrieval failed. Error Code: %d\n", GET_ERR_CODE(result));
   }

   // Increment and store all records
   ship_exit += 1u;
   imu_hz += 5u;
   battery += 10u;

   IF_OK_RUN_AND_UPDATE(result, ifc->store_uint64_t(ifc, RECORD_ID_SHIP_MODE_EXIT_TIME, ship_exit));
   IF_OK_RUN_AND_UPDATE(result, ifc->store_uint64_t(ifc, RECORD_ID_IMU_SAMPLE_FREQUENCY, imu_hz));
   IF_OK_RUN_AND_UPDATE(result, ifc->store_uint64_t(ifc, RECORD_ID_BATTERY_SAMPLE_FREQUENCY, battery));

   feed_watchdog();
   if(IS_OK(result))
   {
      SEGGER_RTT_printf(0, "FDS Manager Records updated successfully.\n");
      nrf_delay_ms(1000);
   }
   else
   {
      SEGGER_RTT_printf(0, "FDS Manager Records update failed. Error Code: %d\n", GET_ERR_CODE(result));
      nrf_delay_ms(1000);
   }

   uint64_t ship_exit1 = 42;
   uint64_t imu_hz1 = 42;
   uint64_t battery1 = 42;

   IF_OK_RUN_AND_UPDATE(result, ifc->retrieve_uint64_t(ifc, RECORD_ID_SHIP_MODE_EXIT_TIME, &ship_exit1));
   IF_OK_RUN_AND_UPDATE(result, ifc->retrieve_uint64_t(ifc, RECORD_ID_IMU_SAMPLE_FREQUENCY, &imu_hz1));
   IF_OK_RUN_AND_UPDATE(result, ifc->retrieve_uint64_t(ifc, RECORD_ID_BATTERY_SAMPLE_FREQUENCY, &battery1));

   if(IS_OK(result))
   {
      SEGGER_RTT_printf(0, "FDS Manager Records retrieved successfully.\n");
      SEGGER_RTT_printf(0, "FDS Manager Records retrieved successfully.\n");
      SEGGER_RTT_printf(0, "Ship Mode Exit Time: %llu \n", ship_exit1);
      SEGGER_RTT_printf(0, "IMU Sample Frequency: %llu Hz\n", imu_hz1);
      SEGGER_RTT_printf(0, "Battery Sample Frequency: %llu Hz\n", battery1);
   }
   else
   {
      SEGGER_RTT_printf(0, "FDS Manager Records retrieval failed. Error Code: %d\n", GET_ERR_CODE(result));
   }

   // Infinite loop to hold the program here until reset
   SEGGER_RTT_printf(0, "Awaiting reset.\n");
   while(1)
   {
      nrf_delay_ms(1000);
      SEGGER_RTT_printf(0, "...\n");
      feed_watchdog();
   }
}
#   endif /* ENABLE_TESTING_POWER_MANAGEMENT */

#   ifdef ENABLE_TESTING_DOSE
static void sandbox_dose(void)
{
   result_t result = RESULT_OK;

   // Tilt detection
   result = tilt_detection_init(&m_tilt_detect, &m_systick.interface, &m_imu.interface);
   if(IS_ERR(result))
   {
      DEBUG_ERROR("tilt_detection_init failed. unit: %d, code: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }
   {
      DEBUG_INFO("tilt_detection_init successful.");
   }
   tilt_data_t tilts[10u] = {0};
   uint8_t new_tilt_count = 0u;

   // test cap detection
   result = cap_detection_module_init(&m_cap_detect,
                                      &m_i2c_driver_0.interface,
                                      TMD26353_I2C_ADDRESS,
                                      INT3,
                                      &m_proximity_sensor); // use for init, dont use m_proximity_sensor again
   if(IS_ERR(result))
   {
      DEBUG_ERROR("cap_detection_module_init failed. unit: %d, code: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }
   {
      DEBUG_INFO("cap_detection_module_init successful.");
   }

   result
      = dose_detection_init(&m_dose_detect, &m_systick.interface, &m_cap_detect.interface, &m_tilt_detect.interface);

   // RETURN_ON_ERR(result);
   if(IS_ERR(result))
   {
      DEBUG_ERROR("dose_detection_init failed. unit: %d, code: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }
   {
      DEBUG_INFO("dose_detection_init successful.");
   }

   // auto detect axis of most upright position and set it

   tilt_detection_axis_t axis;
   result = m_tilt_detect.interface.auto_detect_upright_axis(&m_tilt_detect.interface, &axis);
   if(IS_ERR(result))
   {
      DEBUG_ERROR("auto_detect_upright_axis failed. unit: %d, code: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }
   {
      DEBUG_INFO("upright axis: %d", axis);
      result = m_tilt_detect.interface.set_upright_axis(&m_tilt_detect.interface, axis);
      if(IS_ERR(result))
      {
         DEBUG_ERROR("set_upright_axis failed. unit: %d, code: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
      }
      {
         DEBUG_INFO("set_upright_axis successful.");
      }
   }

   uint8_t test_value = 0;
   while(true)
   {
      DEBUG_INFO("---------------%d---------------", test_value);
      test_value++;

      // dose detection
      /* bool is_new_dose_event_available = false;
      dose_detection_event_t event;
      result = m_dose_detect.interface.get_dose_events(&m_dose_detect.interface, &event,
      &is_new_dose_event_available); if(IS_ERR(result))
      {
         DEBUG_ERROR("get_dose_events failed. unit: %d, code: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
      }
      if(is_new_dose_event_available)
      {
         DEBUG_INFO("DOSE DETECTED");
      }
 */
      // tilt detection

      /*  result = m_tilt_detect.interface.get_tilts(&m_tilt_detect.interface, &new_tilt_count, tilts);
       if(IS_ERR(result))
       {
          DEBUG_ERROR("get_tilts failed. unit: %d, code: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
       }

       for(uint8_t idx = 0u; idx < new_tilt_count; idx++)
       {
          DEBUG_INFO("Tilt detected at time: %d ms", tilts[idx].detected_at_time_ms);
          DEBUG_INFO("Tilt detected - duration: %d ms", tilts[idx].duration_ms);
       }

       if(0 == new_tilt_count)
       {
          DEBUG_INFO("No tilts detected");
       } */

      // Todo: Should the module be set to active? See interface. It's never set to active in the driver.
      // Todo: check the default tilt detection axis

      // Cap detection
      // cara test code
      // _______________________________________________________________________________________

      CAP_STATE cap_state;

      result = m_cap_detect.interface.get_cap_status(&m_cap_detect.interface, &cap_state);
      if(IS_ERR(result))
      {
         DEBUG_ERROR("get_cap_status failed. unit: %d, code: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
      }

      // (note) gets printed from inside driver as well
      switch(cap_state)
      {
         case CAP_STATE_OPEN:
            DEBUG_INFO("CAP_STATE_OPEN");
            break;
         case CAP_STATE_CLOSED:
            DEBUG_INFO("CAP_STATE_CLOSED");
            break;
         case CAP_STATE_UNKNOWN:
            DEBUG_INFO("CAP_STATE_UNKNOWN - NO INTERRUPT TRIGGERED");
            break;

         default:
            DEBUG_ERROR("Bad cap state!");
            break;
      }
   }

   // proximity sensor

   // note that reading the status value here messes with the get_cap_status function as it clears the status
   // register and interrupt pin
   // don't call proximity driver intf functions directly (rather debug_info from inside cap detection module)

   // _______________________________________________________________________________________________
   // Cara test code

   nrf_delay_ms(500);
   nrf_gpio_pin_clear(LED_BLUE);
   nrf_delay_ms(9500);
   nrf_gpio_pin_set(LED_BLUE);
}
#   endif /* ENABLE_TESTING_DOSE */

#   ifdef ENABLE_TESTING_WIRELESS_COMMS
/**
 * @brief Convert a raw SAADC reading to millivolts.
 *
 * The formula is derived from the SAADC transfer function
 * \f[
 * V_\text{in} = \frac{\text{sample}}{2^{N-1}-1} \times
 *               \frac{V_\text{REF}}{\text{gain}}
 * \f]
 *
 * @param[in]  sample       Raw two’s-complement SAADC reading
 * @param[in]  vref_mv      Reference voltage in **mV**
 *                          (e.g. `600` for the 0.6 V internal ref)
 * @param[in]  gain_num     Gain numerator   – 1, 2, or 4
 * @param[in]  gain_den     Gain denominator – 1, 2, 3, 6, 8
 *                          (`gain = gain_num / gain_den`)
 * @param[in]  res_bits     ADC resolution in bits (8, 10, or 12)
 *
 * @return Input voltage in millivolts (signed).
 *
 * @note Uses only fixed-point integer math—no <math.h> required.
 */
uint16_t
   saadc_raw_to_mv(nrf_saadc_value_t sample, uint16_t vref_mv, uint8_t gain_num, uint8_t gain_den, uint8_t res_bits)
{
   feed_watchdog();
   m_systick_period_ms = 20u; // Fast systick for testing

   result_t result = RESULT_OK;
   nfc_tag_driver_interface_t *nfc_ifc = &m_nfc.interface;

   pmic_status_t stat;
   pmic_battery_measurements_t meas;

   st25dv_mb_status_t status = {0};
   result = m_nfc.interface.get_status(nfc_ifc, &status);
   if(IS_ERR(result))
   {
      SEGGER_RTT_printf(0, "NFC get_status failed, err code: %d\n", GET_ERR_CODE(result));
   }

   DEBUG_INFO("NFC STATUS: MB EN: %d, Inbox: %d, Outbox: %d", status.enabled, status.rf_put_msg, status.host_put_msg);

   uint64_t curr_ms = 0;
   uint64_t prev_ms = 0;
   bool interrupt_fired = false;
   uint8_t nfc_read_buff[50] = {0};
   uint8_t nfc_read_len = 0;
   static uint8_t tx_data[5] = {1, 2, 3, 4, 5};
   bool is_blue_on = false;

   /* Re-queue the buffer so it is ready for the next measurement. */
   (void)nrfx_saadc_buffer_convert(p_event->data.done.p_buffer, p_event->data.done.size);

   m_saadc_done = true;
}
}
#   endif /* ENABLE_TESTING_WIRELESS_COMMS */

#   ifdef EEPROM_SANDBOX
void sandbox_wireless_comms(void)
{
   /**
    * Test sequence:
    * 1. Enable wireless power transfer on Dock and leave on high WPT
    * 2. Test WPT reception on ring for charging
    * 3. Test Tx from ring to dock: On ring: toggle TX line. On dock, measure ADC and report.
    * 4. Test Tx from dock to ring: On dock: toggle TX line. On ring, measure ADC and report.
    *
    * Common elements:
    * - ADC
    * - TX toggling (Pin name might change)
    * - RX + adc reporting (pin name might change)
    */
   // Enable wirless charging
   // nrf_gpio_pin_set(WPT_RX_EN);
   // nrf_gpio_cfg_input(WPT_RX, NRF_GPIO_PIN_NOPULL);

   // Init ADC
   nrf_saadc_channel_config_t channel_config = {.resistor_p = NRF_SAADC_RESISTOR_DISABLED,
                                                .resistor_n = NRF_SAADC_RESISTOR_DISABLED,
                                                /* Gain is 1/6 for Dock, 1/3 for Neck Ring. */
                                                .gain = NRF_SAADC_GAIN1_3,
                                                .reference = NRF_SAADC_REFERENCE_INTERNAL,
                                                .acq_time = NRF_SAADC_ACQTIME_10US,
                                                .mode = NRF_SAADC_MODE_SINGLE_ENDED,
                                                .burst = NRF_SAADC_BURST_DISABLED,
                                                .pin_p = NRF_SAADC_INPUT_AIN4, // Adjust pin: AIN4(ring)
                                                .pin_n = NRF_SAADC_INPUT_DISABLED};
   nrfx_saadc_config_t config = {.resolution = NRF_SAADC_RESOLUTION_12BIT,
                                 .oversample = NRF_SAADC_OVERSAMPLE_DISABLED,
                                 .interrupt_priority = NRFX_SAADC_CONFIG_IRQ_PRIORITY,
                                 .low_power_mode = false};

   nrfx_saadc_init(&config, saadc_event_handler);
   nrfx_saadc_channel_init(0, &channel_config);
   nrfx_saadc_buffer_convert(&m_sample_buf, 1);

   // Read report ADC voltage in mv
   uint16_t adc_val_mv = 0;
   battery_status_t battery;

   while(true)
   {
      (void)m_systick.interface.get_time_ms(&m_systick.interface, &curr_ms);
      /*  if(curr_ms - prev_ms > 2000u)
       {
          prev_ms = curr_ms;
          // Report battery status
          result = m_pmic.interface.get_pmic_status(&m_pmic.interface, &stat);
          result = m_pmic.interface.get_measurements(&m_pmic.interface, &meas);
          SEGGER_RTT_printf(0,
                            "PMIC stats:\n charger connected: %d\n Charger good: %d\n battery detected: %d\n charging: "
                            "%d\n vbat_measurement_ready: %d\n",
                            stat.charger_connected,
                            stat.charger_good,
                            stat.battery_detected,
                            stat.charging,
                            stat.vbat_measurement_ready);
          SEGGER_RTT_printf(
             0, "PMIC  measurements:\n vbat_mv: %d\n vin_mv: %d\n iin_ma: %d\n", meas.vbat_mv, meas.vin_mv,
       meas.iin_ma); if(stat.charger_connected)
          {
             nrf_gpio_pin_set(LED_GREEN);
             nrf_delay_ms(10);
             nrf_gpio_pin_clear(LED_GREEN);
          }
       } */

      // Continiously check for NFC interrupt status

      result = nfc_ifc->get_interrupt_status(nfc_ifc, &interrupt_fired);
      if(IS_ERR(result))
      {
         SEGGER_RTT_printf(0, "NFC get_interrupt_status failed, err code: %d\n", GET_ERR_CODE(result));
      }
      if(interrupt_fired)
      {
         if(is_blue_on)
         {
            is_blue_on = false;
            nrf_gpio_pin_clear(LED_BLUE);
         }
         else
         {
            is_blue_on = true;
            nrf_gpio_pin_set(LED_BLUE);
         }
         DEBUG_INFO("\n\n-----------------------------Interrupt fired\n");
         result = m_nfc.interface.get_status(nfc_ifc, &status);
         if(IS_ERR(result))
         {
            SEGGER_RTT_printf(0, "NFC get_status failed, err code: %d\n", GET_ERR_CODE(result));
         }
         DEBUG_INFO(
            "NFC STATUS: MB EN: %d, Inbox: %d, Outbox: %d", status.enabled, status.rf_put_msg, status.host_put_msg);

         nrf_gpio_pin_clear(WPT_TX); // Toggle the Tx pin
         nrf_delay_ms(20);
         nrfx_saadc_sample();
         nrf_delay_ms(500);
         if(!m_saadc_done)
         {
            DEBUG_ERROR("ADC sample not done.");
         }
         else
         {
            // 0.6 V internal reference, gain = 1/3, 12-bit resolution
            adc_val_mv = saadc_raw_to_mv(m_sample_buf, 600, 1, 3, 12);
            m_saadc_done = false;
            DEBUG_INFO("WPT RX val mV (WPT_TX = clear): %d", adc_val_mv);
         }
         * /
      }
   }
#   endif /* ENABLE_TESTING_WIRELESS_COMMS */

#   ifdef ENABLE_TESTING_POWER_TESTING
   static void rtt_print_byte_binary(uint8_t value)
   {
      // Optional visual prefix
      SEGGER_RTT_printf(0, "0b");

      // Walk from MSB to LSB and emit the corresponding character
      for(int8_t bit = 7; bit >= 0; --bit)
      {
         SEGGER_RTT_printf(0, "%c", (value & (1u << bit)) ? '1' : '0');
      }

      // Line break for readability (CR-LF works well in RTT Viewer)
      SEGGER_RTT_printf(0, "\r\n");
   }

   void sandbox_power_testing(void)
   {
      result_t result;
      uint64_t current_ms = 0;
      uint64_t prev_ms = 0;
      (void)m_systick.interface.get_time_ms(&m_systick.interface, &current_ms);
      pmic_status_t stat;
      pmic_battery_measurements_t meas;
      m_systick_period_ms = 1000u;
      // nrf_gpio_pin_set(WPT_TX);
      while(true)
      {
         // charging mode
         feed_watchdog();
         idle_state_handle();
      }

      // respond
      if(0xAA == nfc_read_buff[0])
      {
         tx_data[0] = 0xAB;
         tx_data[1] = nfc_read_buff[1]; // respond with the received counter value
         // respond with the received counter value + 1
         if(nfc_read_buff[1] == UINT8_MAX)
         {
            tx_data[2] = 0;
         }
         else
         {
            tx_data[2] = nfc_read_buff[1] + 1;
         }

         tx_data[3] = 0xAB;
         tx_data[4] = 0xAB;

         // Send a message if not busy
         result = m_nfc.data_ifc.send_data(&m_nfc.data_ifc, tx_data, sizeof(tx_data));
         if(GET_ERR_CODE(result) == ST25DV_DRV_ERROR_BUSY)
         {
            SEGGER_RTT_printf(0, "No write, busy\n");
         }
      }
   }
   feed_watchdog();
   idle_state_handle();
}
}
#   endif /* ENABLE_TESTING_POWER_TESTING */

#   ifdef ENABLE_TESTING_NFC_EEPROM
/**
 * @brief Helper to write to EEPROM and verify the data matches when read back.
 *
 * @param nfc_ifc         NFC tag interface.
 * @param label           Human-readable test label.
 * @param address         Start address for the transaction.
 * @param length          Number of bytes to write/read.
 * @param pattern_seed    Seed used to populate the transmit buffer.
 * @param tx_buf          Transmit buffer (pre-allocated by caller).
 * @param rx_buf          Receive buffer (pre-allocated by caller).
 * @param max_buf_length  Maximum size of tx/rx buffers.
 *
 * @return true if the write/read/compare cycle succeeds, false otherwise.
 */
#      define EEPROM_SANDBOX_MAX_LEN (320u)
static bool nfc_eeprom_write_and_verify(const nfc_tag_driver_interface_t *const nfc_ifc,
                                        const char *label,
                                        uint16_t address,
                                        uint16_t length,
                                        uint8_t pattern_seed,
                                        uint8_t *tx_buf,
                                        uint8_t *rx_buf,
                                        uint16_t max_buf_length)
{
   if(length > max_buf_length)
   {
      SEGGER_RTT_printf(0, "NFC EEPROM sandbox: length %u exceeds buffer %u \n", length, max_buf_length);
      return false;
   }

   for(uint16_t idx = 0u; idx < length; idx++)
   {
      tx_buf[idx] = (uint8_t)(pattern_seed + (uint8_t)idx);
   }

   result_t result = nfc_ifc->write_eeprom_blocking(nfc_ifc, address, tx_buf, length);
   if(IS_OK(result))
   {
      memset(rx_buf, 0, length);
      result = nfc_ifc->read_eeprom_blocking(nfc_ifc, address, rx_buf, length);
   }

   if(IS_OK(result) && ((0u == length) || (0 == memcmp(tx_buf, rx_buf, length))))
   {
      SEGGER_RTT_printf(0, "\t\tPASS (addr=%u len=%u)\n", address, length);
      return true;
   }

   SEGGER_RTT_printf(0,
                     "NFC EEPROM sandbox FAIL (addr=%u len=%u, unit=%d code=%d)\n",
                     address,
                     length,
                     GET_ERR_UNIT(result),
                     GET_ERR_CODE(result));
   return false;
}

/**
 * @brief Helper to confirm the driver returns an error when presented with out-of-range access.
 */
static bool nfc_eeprom_expect_failure(const nfc_tag_driver_interface_t *const nfc_ifc,
                                      const char *label,
                                      uint16_t address,
                                      uint16_t length,
                                      bool is_write,
                                      uint8_t *buffer)
{
   result_t result = is_write ? nfc_ifc->write_eeprom_blocking(nfc_ifc, address, buffer, length) :
                                nfc_ifc->read_eeprom_blocking(nfc_ifc, address, buffer, length);

   if(IS_ERR(result))
   {
      SEGGER_RTT_printf(0,
                        "\t\tPASS! NFC EEPROM sandbox: failed as expected (addr=%u len=%u, unit=%d code=%d)\n",
                        address,
                        length,
                        GET_ERR_UNIT(result),
                        GET_ERR_CODE(result));
      return true;
   }

   SEGGER_RTT_printf(0, "NFC EEPROM sandbox expected failure but got OK (addr=%u len=%u)\n", address, length);
   return false;
}

// Standalone EEPROM exercise for the NFC tag driver
void nfc_eeprom_sandbox(void)
{
   nrf_delay_ms(2000u);
   SEGGER_RTT_printf(0, "Starting NFC EEPROM sandbox\n");
   feed_watchdog();
   m_systick_period_ms = 20u; // Fast systick for testing

   result_t result = m_i2c_driver_1.interface.i2c_bus_scan(&m_i2c_driver_1.interface);

   const nfc_tag_driver_interface_t *nfc_ifc = &m_nfc.interface;
   ST25DV_MEM_SIZE mem_info = {0};
   // Sandbox-only: direct vendor call used to learn EEPROM geometry. Application code should only use the driver
   // interface functions below for any EEPROM use.
   const int32_t mem_status = ST25DV_ReadMemSize(&m_nfc._st_device, &mem_info);

   if(NFCTAG_OK != mem_status)
   {
      SEGGER_RTT_printf(0, "NFC EEPROM sandbox: failed to read memory size (%ld)\n", (long int)mem_status);
      return;
   }

   const uint16_t block_bytes = (uint16_t)(mem_info.BlockSize + 1u);
   const uint32_t total_bytes = (uint32_t)(mem_info.Mem_Size + 1u) * block_bytes;

   SEGGER_RTT_printf(
      0, "NFC EEPROM sandbox: block size %u bytes, total %lu bytes\n", block_bytes, (unsigned long)total_bytes);

   uint32_t area1_end_bytes = 31u; // default expectation after driver init
   uint32_t area2_end_bytes = (total_bytes > 0u) ? (total_bytes - 1u) : 0u;
   uint32_t area3_end_bytes = (total_bytes > 0u) ? (total_bytes - 1u) : 0u;

   // Read area boundaries to understand current protection layout.
   uint8_t enda1 = 0;
   uint8_t enda2 = 0;
   uint8_t enda3 = 0;
   if((NFCTAG_OK == ST25DV_ReadEndZonex(&m_nfc._st_device, ST25DV_ZONE_END1, &enda1))
      && (NFCTAG_OK == ST25DV_ReadEndZonex(&m_nfc._st_device, ST25DV_ZONE_END2, &enda2))
      && (NFCTAG_OK == ST25DV_ReadEndZonex(&m_nfc._st_device, ST25DV_ZONE_END3, &enda3)))
   {
      uint32_t area1_end = (((uint32_t)enda1 + 1u) * 32u);
      uint32_t area2_end = (((uint32_t)enda2 + 1u) * 32u);
      uint32_t area3_end = (((uint32_t)enda3 + 1u) * 32u);
      area1_end = (area1_end > 0u) ? (area1_end - 1u) : 0u;
      area2_end = (area2_end > 0u) ? (area2_end - 1u) : 0u;
      area3_end = (area3_end > 0u) ? (area3_end - 1u) : 0u;
      if(area1_end >= total_bytes)
      {
         area1_end = (total_bytes > 0u) ? (total_bytes - 1u) : 0u;
      }
      if(area2_end >= total_bytes)
      {
         area2_end = (total_bytes > 0u) ? (total_bytes - 1u) : 0u;
      }
      if(area3_end >= total_bytes)
      {
         area3_end = (total_bytes > 0u) ? (total_bytes - 1u) : 0u;
      }
      area1_end_bytes = area1_end;
      area2_end_bytes = area2_end;
      area3_end_bytes = area3_end;
      SEGGER_RTT_printf(0,
                        "NFC EEPROM sandbox: Area1 end=%lu, Area2 end=%lu, Area3 end=%lu, Area4 end=%lu\n",
                        (unsigned long)area1_end,
                        (unsigned long)area2_end,
                        (unsigned long)area3_end,
                        (unsigned long)((total_bytes > 0u) ? (total_bytes - 1u) : 0u));
   }
   else
   {
      SEGGER_RTT_printf(0, "NFC EEPROM sandbox: failed to read area boundaries\n");
   }

   static uint8_t tx_buf[EEPROM_SANDBOX_MAX_LEN] = {0};
   static uint8_t rx_buf[EEPROM_SANDBOX_MAX_LEN] = {0};
   uint32_t pass_count = 0;
   uint32_t fail_count = 0;

   // Test 0: Area 1 writes should be blocked by the driver.
   SEGGER_RTT_printf(0, "Test 0: Area 1 write should fail.\n");
   if(nfc_eeprom_expect_failure(nfc_ifc, "area1 write blocked", 0u, 4u, true, tx_buf))
   {
      pass_count++;
   }
   else
   {
      fail_count++;
   }

   // The calls below show how the application should use EEPROM: only via
   // nfc_ifc->write_eeprom_blocking/read_eeprom_blocking. Test 1: no-op write/read should succeed and leave memory
   // unchanged.
   SEGGER_RTT_printf(0, "Test 1: no-op write/read should succeed and leave memory unchanged.\n");
   if(nfc_eeprom_write_and_verify(nfc_ifc, "zero-length no-op", 0u, 0u, 0x00, tx_buf, rx_buf, EEPROM_SANDBOX_MAX_LEN))
   {
      pass_count++;
   }
   else
   {
      fail_count++;
   }

   if(total_bytes > 0u)
   {
      // Test 2: basic read/write at start of memory.
      SEGGER_RTT_printf(0, "Test 2: basic read/write at start of memory.\n");
      const uint32_t start_addr = (area1_end_bytes + 1u);
      uint16_t data_size = 32u; // bytes
      if(start_addr >= total_bytes)
      {
         data_size = 0u;
      }
      else if((start_addr + data_size) > total_bytes)
      {
         data_size = (uint16_t)(total_bytes - start_addr);
      }
      if(data_size > 0u
         && nfc_eeprom_write_and_verify(nfc_ifc,
                                        "write/read at start",
                                        (uint16_t)start_addr,
                                        data_size,
                                        0xA0,
                                        tx_buf,
                                        rx_buf,
                                        EEPROM_SANDBOX_MAX_LEN))
      {
         pass_count++;
      }
      else
      {
         fail_count++;
      }
   }

   if(total_bytes > block_bytes)
   {
      // Test 3: cross a block boundary to confirm multi-block transfers.
      SEGGER_RTT_printf(0, "Test 3: cross a block boundary to confirm multi-block transfers.\n");
      const uint32_t cross_start_32 = (area1_end_bytes + ((block_bytes > 8u) ? (block_bytes - 8u) : block_bytes));
      const uint16_t cross_start = (uint16_t)((cross_start_32 < total_bytes) ? cross_start_32 : area1_end_bytes + 1u);
      uint16_t cross_len = (uint16_t)(block_bytes + 16u);
      if((uint32_t)(cross_start + cross_len) > total_bytes)
      {
         cross_len = (uint16_t)(total_bytes - cross_start);
      }
      if(cross_len > EEPROM_SANDBOX_MAX_LEN)
      {
         cross_len = EEPROM_SANDBOX_MAX_LEN;
      }
      if(nfc_eeprom_write_and_verify(
            nfc_ifc, "cross-block write/read", cross_start, cross_len, 0xB0, tx_buf, rx_buf, EEPROM_SANDBOX_MAX_LEN))
      {
         pass_count++;
      }
      else
      {
         fail_count++;
      }
   }

   if(total_bytes > 0u)
   {
      // Test 4: long payload (>256B) to exercise chunking/polling.
      SEGGER_RTT_printf(0, "Test 4: long payload (>256B) to exercise chunking/polling.\n");
      uint16_t long_len
         = (uint16_t)((total_bytes > (ST25DV_MAX_WRITE_BYTE + 32u)) ? (ST25DV_MAX_WRITE_BYTE + 32u) : total_bytes);
      if(long_len > EEPROM_SANDBOX_MAX_LEN)
      {
         long_len = EEPROM_SANDBOX_MAX_LEN;
      }
      const uint16_t long_start = (uint16_t)((total_bytes > long_len) ? (uint16_t)((total_bytes - long_len) / 2u) : 0u);
      if(nfc_eeprom_write_and_verify(
            nfc_ifc, "long chunky write/read", long_start, long_len, 0xC0, tx_buf, rx_buf, EEPROM_SANDBOX_MAX_LEN))
      {
         pass_count++;
      }
      else
      {
         fail_count++;
      }
   }

   if(total_bytes > 1u)
   {
      // Test 5: tail-of-memory access near the end.
      SEGGER_RTT_printf(0, "Test 5: tail-of-memory access near the end.\n");
      uint16_t tail_len = (uint16_t)((total_bytes >= 24u) ? 24u : total_bytes);
      if(tail_len > EEPROM_SANDBOX_MAX_LEN)
      {
         tail_len = EEPROM_SANDBOX_MAX_LEN;
      }
      const uint16_t tail_start = (uint16_t)((total_bytes > tail_len) ? (total_bytes - tail_len) : 0u);
      if(nfc_eeprom_write_and_verify(
            nfc_ifc, "tail-end write/read", tail_start, tail_len, 0xD0, tx_buf, rx_buf, EEPROM_SANDBOX_MAX_LEN))
      {
         pass_count++;
      }
      else
      {
         fail_count++;
      }
   }

   // Test 6: out-of-range write should fail.
   SEGGER_RTT_printf(0, "Test 6: out-of-range write should fail.\n");
   const uint16_t overflow_start = (uint16_t)((total_bytes > 4u) ? (total_bytes - 4u) : (uint32_t)total_bytes);
   const uint16_t overflow_len = 16u;
   if(nfc_eeprom_expect_failure(nfc_ifc, "overflow write", overflow_start, overflow_len, true, tx_buf))
   {
      pass_count++;
   }
   else
   {
      fail_count++;
   }

   // Test 7: out-of-range read should fail.
   SEGGER_RTT_printf(0, "Test 7: out-of-range read should fail.\n");
   if(nfc_eeprom_expect_failure(nfc_ifc, "overflow read", overflow_start, overflow_len, false, rx_buf))
   {
      pass_count++;
   }
   else
   {
      fail_count++;
   }

   SEGGER_RTT_printf(
      0, "NFC EEPROM sandbox complete: %lu passed, %lu failed\n", (unsigned long)pass_count, (unsigned long)fail_count);

   while(true)
   {
      feed_watchdog();

#      ifndef DEBUG_DISABLE_UART
      if(NULL != m_debug_uart_driver_interface)
      {
         (void)m_debug_uart_driver_interface->process(&m_debug_uart_driver.interface);
      }
#      endif

      idle_state_handle();
   }
}
#   endif /* ENABLE_TESTING_NFC_EEPROM */

#   ifdef ENABLE_TESTING_NFC_EEPROM_QUEUE
// NFC EEPROM sandbox
void nfc_eeprom_sandbox_queue(void)
{
   DEBUG_INFO("\n\n\n\n\n\n");
   feed_watchdog();
   m_systick_period_ms = 20u; // Fast systick for testing

   // ***********************************************************************************************************
   // Initialize NFC EEPROM queue test
   // ***********************************************************************************************************
   result_t result = RESULT_OK;
   nfc_tag_eeprom_queue_t m_nfc_eeprom_queue;
   nfc_tag_eeprom_queue_interface_t *ifc;

   result = nfc_tag_eeprom_queue_init(&m_nfc_eeprom_queue, sizeof(dose_event_t), 4u, &m_nfc.interface, true);

   feed_watchdog();

   while(IS_ERR(result))
   {
      DEBUG_INFO("nfc_tag_eeprom_queue_init failed.");
      idle_state_handle();
   }
   ifc = &m_nfc_eeprom_queue.interface;

   DEBUG_INFO("NFC EEPROM queue initialized successful.");

   // ***********************************************************************************************************
   // Check EEPROM Queue configuration
   // ***********************************************************************************************************
   uint8_t element_size = 0;
   uint8_t max_elements = 0;
   uint8_t status_slot_count = 0;

   result = ifc->nfc_read_config(&m_nfc_eeprom_queue.interface, &max_elements, &element_size, &status_slot_count);

   feed_watchdog();

   DEBUG_INFO("NFC EEPROM queue config");
   DEBUG_INFO("Elements: %d | Expected 41", max_elements);
   DEBUG_INFO("Element size: %d | Expected 11", element_size);
   DEBUG_INFO("Status slots: %d | Expected 4", status_slot_count);

   while(IS_ERR(result) || max_elements != 41u || element_size != sizeof(dose_event_t) || status_slot_count != 4u)
   {
      DEBUG_INFO("NFC EEPROM queue nfc_read_config failed.");
      idle_state_handle();
   }

   feed_watchdog();

   // ***********************************************************************************************************
   // Enqueue dequeue and pop all test
   // ***********************************************************************************************************

   uint8_t elements = 0;

   dose_event_t dose_event = {0};
   dose_event.dose_start = 123456;
   dose_event.dose_duration_seconds = 10;
   dose_event.dose_completed_within_time_limit = true;
   dose_event.dose_start_proximity_sensor_value = 200;
   dose_event.dose_end_proximity_sensor_value = 800;

   dose_event_t multiple_dose_events[4] = {dose_event, dose_event, dose_event, dose_event};

   multiple_dose_events[0].dose_start = 223456;
   multiple_dose_events[1].dose_start = 323456;
   multiple_dose_events[2].dose_start = 423456;
   multiple_dose_events[3].dose_start = 523456;

   feed_watchdog();

   do
   {
      // Enqueue a single dose event and check the element count
      result = ifc->nfc_enqueue(&m_nfc_eeprom_queue.interface, &dose_event);
      ON_ERR_DEBUG_ERROR(result, "NFC EEPROM queue enqueue failed.");
      BREAK_ON_ERR(result);

      result = ifc->nfc_get_element_count(&m_nfc_eeprom_queue.interface, &elements);
      ON_ERR_DEBUG_ERROR(result, "NFC EEPROM queue get element count failed.");
      BREAK_ON_ERR(result);

      if(elements != 1u)
      {
         DEBUG_INFO("Incorrect element count after enqueue. Expected 1, got %d", elements);
         SET_ERR(result, GC_ERROR_MAX);
         break;
      }

      feed_watchdog();
      DEBUG_INFO("SINGLE ENQUEUE: PASSED");

      // Enqueue multiple dose events and check the element count
      result = ifc->nfc_enqueue_multiple(&m_nfc_eeprom_queue.interface, multiple_dose_events, 4u);
      ON_ERR_DEBUG_ERROR(result, "NFC EEPROM queue enqueue multiple failed.");
      BREAK_ON_ERR(result);

      result = ifc->nfc_get_element_count(&m_nfc_eeprom_queue.interface, &elements);
      ON_ERR_DEBUG_ERROR(result, "NFC EEPROM queue get element count failed.");
      BREAK_ON_ERR(result);

      if(elements != 5u)
      {
         DEBUG_INFO("Incorrect element count after enqueue. Expected 5, got %d", elements);
         SET_ERR(result, GC_ERROR_MAX);
         break;
      }

      feed_watchdog();
      DEBUG_INFO("MULTIPLE ENQUEUE: PASSED");

      // Single Peek
      dose_event_t peeked_dose_event = {0};
      result = ifc->nfc_peek(&m_nfc_eeprom_queue.interface, &peeked_dose_event);
      ON_ERR_DEBUG_ERROR(result, "NFC EEPROM queue peek failed.");
      BREAK_ON_ERR(result);

      if(memcmp(&dose_event, &peeked_dose_event, sizeof(dose_event_t)) != 0)
      {
         DEBUG_INFO("Peeked dose event does not match enqueued dose event.");
         SET_ERR(result, GC_ERROR_MAX);
         break;
      }

      feed_watchdog();
      DEBUG_INFO("SINGLE PEEK: PASSED");

      // Multiple Peek
      dose_event_t peeked_multiple_dose_events[3] = {0};
      result = ifc->nfc_peek_multiple(&m_nfc_eeprom_queue.interface, peeked_multiple_dose_events, 3u);
      ON_ERR_DEBUG_ERROR(result, "NFC EEPROM queue peek multiple failed.");
      BREAK_ON_ERR(result);

      if(memcmp(multiple_dose_events, &peeked_multiple_dose_events[1], sizeof(dose_event_t) * 2) != 0)
      {
         DEBUG_INFO("Peeked multiple dose events do not match enqueued dose events.");
         SET_ERR(result, GC_ERROR_MAX);
         break;
      }

      feed_watchdog();
      DEBUG_INFO("MULTIPLE PEEK: PASSED");

      // Dequeue a dose event and verify data
      dose_event_t dequeued_dose_event = {0};
      result = ifc->nfc_dequeue(&m_nfc_eeprom_queue.interface, &dequeued_dose_event);
      ON_ERR_DEBUG_ERROR(result, "NFC EEPROM queue dequeue failed.");
      BREAK_ON_ERR(result);

      if(memcmp(&dose_event, &dequeued_dose_event, sizeof(dose_event_t)) != 0)
      {
         DEBUG_INFO("Dequeued dose event does not match enqueued dose event.");
         SET_ERR(result, GC_ERROR_MAX);
         break;
      }

      result = ifc->nfc_get_element_count(&m_nfc_eeprom_queue.interface, &elements);
      ON_ERR_DEBUG_ERROR(result, "NFC EEPROM queue get element count failed.");
      BREAK_ON_ERR(result);

      if(elements != 4u)
      {
         DEBUG_INFO("Incorrect element count after enqueue. Expected 4, got %d", elements);
         SET_ERR(result, GC_ERROR_MAX);
         break;
      }

      feed_watchdog();
      DEBUG_INFO("SINGLE DEQUEUE: PASSED");

      // Dequeue multiple dose events and verify data
      dose_event_t dequeued_multiple_dose_events[2] = {0};
      result = ifc->nfc_dequeue_multiple(&m_nfc_eeprom_queue.interface, dequeued_multiple_dose_events, 2u);
      ON_ERR_DEBUG_ERROR(result, "NFC EEPROM queue dequeue failed.");
      BREAK_ON_ERR(result);

      if(memcmp(multiple_dose_events, dequeued_multiple_dose_events, sizeof(dose_event_t) * 2) != 0)
      {
         DEBUG_INFO("Double dequeued dose event does not match enqueued dose event.");
         SET_ERR(result, GC_ERROR_MAX);
         break;
      }

      result = ifc->nfc_get_element_count(&m_nfc_eeprom_queue.interface, &elements);
      ON_ERR_DEBUG_ERROR(result, "NFC EEPROM queue get element count failed.");
      BREAK_ON_ERR(result);

      if(elements != 2u)
      {
         DEBUG_INFO("Incorrect element count after enqueue. Expected 2, got %d", elements);
         SET_ERR(result, GC_ERROR_MAX);
         break;
      }

      feed_watchdog();
      DEBUG_INFO("SINGLE ENQUEUE: PASSED");

      // Pop multiple dose events
      result = ifc->nfc_pop_multiple(&m_nfc_eeprom_queue.interface, 2u);
      ON_ERR_DEBUG_ERROR(result, "NFC EEPROM queue pop multiple failed.");
      BREAK_ON_ERR(result);

      result = ifc->nfc_get_element_count(&m_nfc_eeprom_queue.interface, &elements);
      ON_ERR_DEBUG_ERROR(result, "NFC EEPROM queue get element count failed.");
      BREAK_ON_ERR(result);

      if(elements != 0u)
      {
         DEBUG_INFO("Incorrect element count after enqueue. Expected 0, got %d", elements);
         SET_ERR(result, GC_ERROR_MAX);
         break;
      }

      DEBUG_INFO("MULTIPLE POP: PASSED");
   } while(false);

   while(true)
   {
      // charging mode
      feed_watchdog();
      idle_state_handle();
   }
}
#   endif /* ENABLE_TESTING_NFC_EEPROM_QUEUE */

#   ifdef ENABLE_TESTING_NFC_MB
#      define NFC_MB_TEST_MAGIC0        (0xA5u)
#      define NFC_MB_TEST_MAGIC1        (0x5Au)
#      define NFC_MB_TEST_VERSION       (0x01u)
#      define NFC_MB_TEST_TYPE_PING     (0x01u)
#      define NFC_MB_TEST_TYPE_PONG     (0x02u)
#      define NFC_MB_TEST_HEADER_LEN    (6u)
#      define NFC_MB_TEST_MAX_PAYLOAD   (12u)
#      define NFC_MB_TEST_FRAME_MAX_LEN (NFC_MB_TEST_HEADER_LEN + NFC_MB_TEST_MAX_PAYLOAD + 1u)

static uint8_t nfc_mb_test_checksum(const uint8_t *data, uint16_t len)
{
   uint8_t checksum = 0u;

   for(uint16_t idx = 0u; idx < len; idx++)
   {
      checksum ^= data[idx];
   }

   return checksum;
}

static uint8_t nfc_mb_test_payload_seed(uint8_t msg_type)
{
   if(msg_type == NFC_MB_TEST_TYPE_PING)
   {
      return 0x11u;
   }

   if(msg_type == NFC_MB_TEST_TYPE_PONG)
   {
      return 0x77u;
   }

   return 0u;
}

static uint16_t nfc_mb_test_build_frame(uint8_t *buf, uint8_t msg_type, uint8_t seq, uint8_t payload_len)
{
   if(payload_len > NFC_MB_TEST_MAX_PAYLOAD)
   {
      payload_len = NFC_MB_TEST_MAX_PAYLOAD;
   }

   buf[0] = NFC_MB_TEST_MAGIC0;
   buf[1] = NFC_MB_TEST_MAGIC1;
   buf[2] = NFC_MB_TEST_VERSION;
   buf[3] = msg_type;
   buf[4] = seq;
   buf[5] = payload_len;

   const uint8_t seed = nfc_mb_test_payload_seed(msg_type);
   for(uint8_t idx = 0u; idx < payload_len; idx++)
   {
      buf[NFC_MB_TEST_HEADER_LEN + idx] = (uint8_t)(seed + seq + (idx * 3u));
   }

   const uint16_t frame_len = (uint16_t)(NFC_MB_TEST_HEADER_LEN + payload_len + 1u);
   buf[frame_len - 1u] = nfc_mb_test_checksum(buf, (uint16_t)(frame_len - 1u));

   return frame_len;
}

static bool nfc_mb_test_parse_frame(const uint8_t *buf, uint8_t *msg_type, uint8_t *seq, uint8_t *payload_len)
{
   if((buf[0] != NFC_MB_TEST_MAGIC0) || (buf[1] != NFC_MB_TEST_MAGIC1) || (buf[2] != NFC_MB_TEST_VERSION))
   {
      return false;
   }

   const uint8_t type = buf[3];
   if((type != NFC_MB_TEST_TYPE_PING) && (type != NFC_MB_TEST_TYPE_PONG))
   {
      return false;
   }

   const uint8_t length = buf[5];
   if(length > NFC_MB_TEST_MAX_PAYLOAD)
   {
      return false;
   }

   const uint16_t frame_len = (uint16_t)(NFC_MB_TEST_HEADER_LEN + length + 1u);
   const uint8_t expected_checksum = nfc_mb_test_checksum(buf, (uint16_t)(frame_len - 1u));
   if(expected_checksum != buf[frame_len - 1u])
   {
      return false;
   }

   const uint8_t seed = nfc_mb_test_payload_seed(type);
   for(uint8_t idx = 0u; idx < length; idx++)
   {
      const uint8_t expected = (uint8_t)(seed + buf[4] + (idx * 3u));
      if(buf[NFC_MB_TEST_HEADER_LEN + idx] != expected)
      {
         return false;
      }
   }

   if(msg_type != NULL)
   {
      *msg_type = type;
   }

   if(seq != NULL)
   {
      *seq = buf[4];
   }

   if(payload_len != NULL)
   {
      *payload_len = length;
   }

   return true;
}
static void nfc_sandbox_mb(void)
{
   feed_watchdog();
   m_systick_period_ms = 20u; // Fast systick for testing

   result_t result = RESULT_OK;
   nfc_tag_driver_interface_t *nfc_ifc = &m_nfc.interface;

   st25dv_mb_status_t status = {0};
   result = nfc_ifc->get_status(nfc_ifc, &status);
   if(IS_ERR(result))
   {
      SEGGER_RTT_printf(0, "NFC get_status failed. Unit %d, code %d\n", GET_ERR_UNIT(result), GET_ERR_CODE(result));
   }

   SEGGER_RTT_printf(0,
                     "NFC MB init: enabled=%d rf_put=%d host_put=%d len=%u\n",
                     status.enabled,
                     status.rf_put_msg,
                     status.host_put_msg,
                     status.msg_len);

   const uint32_t poll_interval_ms = 50u;
   const uint32_t send_interval_ms = 1500u;
   const uint32_t busy_retry_ms = 200u;
   const uint32_t response_timeout_ms = 2500u;
   const uint32_t stats_interval_ms = 5000u;
   const uint32_t heartbeat_ms = 1000u;

   uint64_t now_ms = 0u;
   uint64_t last_poll_ms = 0u;
   uint64_t next_send_ms = 0u;
   uint64_t last_send_ms = 0u;
   uint64_t last_stats_ms = 0u;
   uint64_t last_heartbeat_ms = 0u;

   uint8_t tx_seq = 0u;
   uint8_t awaiting_seq = 0u;
   bool awaiting_pong = false;

   uint32_t tx_ok = 0u;
   uint32_t tx_busy = 0u;
   uint32_t tx_err = 0u;
   uint32_t rx_ok = 0u;
   uint32_t rx_invalid = 0u;
   uint32_t ping_rx = 0u;
   uint32_t pong_rx = 0u;
   uint32_t pong_timeout = 0u;
   uint32_t unexpected_pong = 0u;

   uint8_t tx_buf[NFC_MB_TEST_FRAME_MAX_LEN] = {0};
   uint8_t rx_buf[300] = {0};

   nrf_gpio_pin_clear(LED_RED);
   nrf_gpio_pin_clear(LED_GREEN);
   nrf_gpio_pin_clear(LED_BLUE);

   while(true)
   {
      (void)m_systick.interface.get_time_ms(&m_systick.interface, &now_ms);

      if(now_ms - last_poll_ms >= poll_interval_ms)
      {
         last_poll_ms = now_ms;

         memset(rx_buf, 0, sizeof(rx_buf));
         result = m_nfc.data_ifc.get_packet(&m_nfc.data_ifc, rx_buf, sizeof(rx_buf));
         if(IS_OK(result))
         {
            if((rx_buf[0] == NFC_MB_TEST_MAGIC0) && (rx_buf[1] == NFC_MB_TEST_MAGIC1))
            {
               uint8_t rx_type = 0u;
               uint8_t rx_seq = 0u;
               uint8_t rx_payload_len = 0u;
               if(nfc_mb_test_parse_frame(rx_buf, &rx_type, &rx_seq, &rx_payload_len))
               {
                  rx_ok++;

                  if(rx_type == NFC_MB_TEST_TYPE_PING)
                  {
                     ping_rx++;
                     const uint16_t frame_len
                        = nfc_mb_test_build_frame(tx_buf, NFC_MB_TEST_TYPE_PONG, rx_seq, rx_payload_len);
                     result = m_nfc.data_ifc.send_packet(&m_nfc.data_ifc, tx_buf, frame_len);
                     if(IS_OK(result))
                     {
                        tx_ok++;
                     }
                     else if(GET_ERR_CODE(result) == COMMS_DRIVER_ERROR_BUSY)
                     {
                        tx_busy++;
                     }
                     else
                     {
                        tx_err++;
                        SEGGER_RTT_printf(
                           0, "MB PONG send failed. Unit %d, code %d\n", GET_ERR_UNIT(result), GET_ERR_CODE(result));
                     }
                  }
                  else if(rx_type == NFC_MB_TEST_TYPE_PONG)
                  {
                     pong_rx++;
                     if(awaiting_pong && (rx_seq == awaiting_seq))
                     {
                        const uint32_t rtt_ms = (uint32_t)(now_ms - last_send_ms);
                        SEGGER_RTT_printf(0, "MB PONG rx seq=%u rtt=%lu ms\n", rx_seq, (unsigned long)rtt_ms);
                        awaiting_pong = false;
                     }
                     else
                     {
                        unexpected_pong++;
                        SEGGER_RTT_printf(0, "MB unexpected PONG seq=%u (awaiting=%u)\n", rx_seq, awaiting_seq);
                     }
                  }
               }
               else
               {
                  rx_invalid++;
               }
            }
         }
         else if(GET_ERR_CODE(result) != COMMS_DRIVER_ERROR_BUSY)
         {
            SEGGER_RTT_printf(
               0, "MB get_packet failed. Unit %d, code %d\n", GET_ERR_UNIT(result), GET_ERR_CODE(result));
         }
      }

      if(awaiting_pong && ((now_ms - last_send_ms) >= response_timeout_ms))
      {
         pong_timeout++;
         SEGGER_RTT_printf(0, "MB PONG timeout seq=%u\n", awaiting_seq);
         awaiting_pong = false;
         next_send_ms = now_ms + busy_retry_ms;
      }

      if(!awaiting_pong && (now_ms >= next_send_ms))
      {
         const uint8_t payload_len = (uint8_t)(8u + (tx_seq & 0x03u));
         const uint16_t frame_len = nfc_mb_test_build_frame(tx_buf, NFC_MB_TEST_TYPE_PING, tx_seq, payload_len);

         result = m_nfc.data_ifc.send_packet(&m_nfc.data_ifc, tx_buf, frame_len);
         if(IS_OK(result))
         {
            SEGGER_RTT_printf(0, "MB PING tx seq=%u len=%u\n", tx_seq, payload_len);
            awaiting_pong = true;
            awaiting_seq = tx_seq;
            last_send_ms = now_ms;
            tx_seq = (uint8_t)(tx_seq + 1u);
            tx_ok++;
            next_send_ms = now_ms + send_interval_ms;
         }
         else if(GET_ERR_CODE(result) == COMMS_DRIVER_ERROR_BUSY)
         {
            tx_busy++;
            next_send_ms = now_ms + busy_retry_ms;
         }
         else
         {
            tx_err++;
            SEGGER_RTT_printf(0, "MB PING send failed. Unit %d, code %d\n", GET_ERR_UNIT(result), GET_ERR_CODE(result));
            next_send_ms = now_ms + send_interval_ms;
         }
      }

      if(now_ms - last_heartbeat_ms >= heartbeat_ms)
      {
         last_heartbeat_ms = now_ms;
         nrf_gpio_pin_toggle(LED_GREEN);
      }

      if(now_ms - last_stats_ms >= stats_interval_ms)
      {
         last_stats_ms = now_ms;
         SEGGER_RTT_printf(
            0,
            "MB stats: tx_ok=%lu tx_busy=%lu tx_err=%lu rx_ok=%lu rx_invalid=%lu ping_rx=%lu pong_rx=%lu timeout=%lu\n",
            (unsigned long)tx_ok,
            (unsigned long)tx_busy,
            (unsigned long)tx_err,
            (unsigned long)rx_ok,
            (unsigned long)rx_invalid,
            (unsigned long)ping_rx,
            (unsigned long)pong_rx,
            (unsigned long)pong_timeout);
      }

      feed_watchdog();
      idle_state_handle();
   }
}
#   endif /* ENABLE_TESTING_NFC_MB */

#   ifdef ENABLE_TESTING_TILT_DETECTION
/**
 * @brief Standalone tilt detection exercise.
 *
 * This sandbox initializes the tilt detection module and periodically checks for tilt events,
 * printing any detected events to the debug output.
 */
static void tilt_detection_sandbox(void)
{
   // Optional visual prefix
   SEGGER_RTT_printf(0, "0b");

   // Walk from MSB to LSB and emit the corresponding character
   for(int8_t bit = 7; bit >= 0; --bit)
   {
      SEGGER_RTT_printf(0, "%c", (value & (1u << bit)) ? '1' : '0');
   }

   // Line break for readability (CR-LF works well in RTT Viewer)
   SEGGER_RTT_printf(0, "\r\n");
}

//***********************************************************************************************************************
#   endif

#   ifdef ENABLE_WIRELESS_COMMS_SANDBOX
// *****************************************************************************************************************************
void sandbox_wireless_comms(void)
{
   /**
    * Test sequence:
    * 1. Enable wireless power transfer on Dock and leave on high WPT
    * 2. Test WPT reception on ring for charging
    * 3. Test Tx from ring to dock: On ring: toggle TX line. On dock, measure ADC and report.
    * 4. Test Tx from dock to ring: On dock: toggle TX line. On ring, measure ADC and report.
    *
    * Common elements:
    * - ADC
    * - TX toggling (Pin name might change)
    * - RX + adc reporting (pin name might change)
    */
   // Enable wirless charging
   // nrf_gpio_pin_set(WPT_RX_EN);
   // nrf_gpio_cfg_input(WPT_RX, NRF_GPIO_PIN_NOPULL);

   // Init ADC
   nrf_saadc_channel_config_t channel_config = {.resistor_p = NRF_SAADC_RESISTOR_DISABLED,
                                                .resistor_n = NRF_SAADC_RESISTOR_DISABLED,
                                                /* Gain is 1/6 for Dock, 1/3 for Neck Ring. */
                                                .gain = NRF_SAADC_GAIN1_3,
                                                .reference = NRF_SAADC_REFERENCE_INTERNAL,
                                                .acq_time = NRF_SAADC_ACQTIME_10US,
                                                .mode = NRF_SAADC_MODE_SINGLE_ENDED,
                                                .burst = NRF_SAADC_BURST_DISABLED,
                                                .pin_p = NRF_SAADC_INPUT_AIN4, // Adjust pin: AIN4(ring)
                                                .pin_n = NRF_SAADC_INPUT_DISABLED};
   nrfx_saadc_config_t config = {.resolution = NRF_SAADC_RESOLUTION_12BIT,
                                 .oversample = NRF_SAADC_OVERSAMPLE_DISABLED,
                                 .interrupt_priority = NRFX_SAADC_CONFIG_IRQ_PRIORITY,
                                 .low_power_mode = false};

   nrfx_saadc_init(&config, saadc_event_handler);
   nrfx_saadc_channel_init(0, &channel_config);
   nrfx_saadc_buffer_convert(&m_sample_buf, 1);

   // Read report ADC voltage in mv
   uint16_t adc_val_mv = 0;
   battery_status_t battery;

   feed_watchdog();
   idle_state_handle();
}
}
#   endif /* ENABLE_TESTING_TILT_DETECTION */

#   ifdef ENABLE_TESTING_CAP_DETECTION
/**
 * @brief Sandbox for cap detection testing.
 *
 * This sandbox function replaces the control loop to specifically test the cap detection module. It performs the
 * following functions:
 * - Measures the proximity sensor value every 500 ms and logs it.
 * - Reads the cap status (open/closed/unknown) every 500 ms and logs together with the returned proximity value (should
 * be similar to the raw proximity value).
 * - Controls the RGB LED to indicate cap status:
 *    - Green when closed
 *    - Blue when open
 *    - Cyan when unknown
 *    - Red on error
 */
static void sandbox_cap_detection(void)
{
   feed_watchdog();
   m_systick_period_ms = 20u; // Fast systick for testing

   uint64_t curr_ms = 0u;
   uint64_t prev_ms = 0u;

   result_t result = tmd2635_driver_init(&m_prox, &m_prox_cfg, &m_i2c_driver_0.interface);
   if(IS_ERR(result))
   {
      result_t result = m_battery.interface.get_battery_status(&m_battery.interface, &battery);

      if(battery.charger_connected)
      {
         nrf_gpio_pin_set(LED_GREEN);
      }
      else
      {
         nrf_gpio_pin_clear(LED_GREEN);
      }
      // nrf_gpio_pin_clear(WPT_TX);
      nrfx_saadc_sample();
      nrf_delay_ms(100);
      if(!m_saadc_done)
      {
         DEBUG_ERROR("ADC sample not done.");
      }
      else
      {
         // 0.6 V internal reference, gain = 1/3, 12-bit resolution
         adc_val_mv = saadc_raw_to_mv(m_sample_buf, 600, 1, 3, 12);
         m_saadc_done = false;
         DEBUG_INFO("WPT RX val mV (WPT_TX = clear): %d", adc_val_mv);
      }

      /* // Test effect of WPT_TX
      nrf_gpio_pin_set(WPT_TX); // Toggle the Tx pin
      nrf_delay_ms(20);
      nrfx_saadc_sample();
      nrf_delay_ms(500);
      if(!m_saadc_done)
      {
         DEBUG_ERROR("ADC sample not done.");
      }
      else
      {
         // 0.6 V internal reference, gain = 1/3, 12-bit resolution
         adc_val_mv = saadc_raw_to_mv(m_sample_buf, 600, 1, 3, 12);
         m_saadc_done = false;
         DEBUG_INFO("WPT RX val mV (WPT_TX = set): %d", adc_val_mv);
      }

      nrf_gpio_pin_clear(WPT_TX); // Toggle the Tx pin
      nrf_delay_ms(20);
      nrfx_saadc_sample();
      nrf_delay_ms(500);
      if(!m_saadc_done)
      {
         DEBUG_ERROR("ADC sample not done.");
      }
      else
      {
         // 0.6 V internal reference, gain = 1/3, 12-bit resolution
         adc_val_mv = saadc_raw_to_mv(m_sample_buf, 600, 1, 3, 12);
         m_saadc_done = false;
         DEBUG_INFO("WPT RX val mV (WPT_TX = clear): %d", adc_val_mv);
      } */
   }
}
#   endif /* ENABLE_TESTING_CAP_DETECTION */
#endif
