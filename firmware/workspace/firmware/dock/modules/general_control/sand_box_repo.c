// /*
//  * Copyright (C) 10XBETA - All Rights Reserved
//  *
//  * Unauthorized copying of this file, via any medium is strictly prohibited
//  * Proprietary and confidential
//  */

// /**
//  * This file is intended to be a repository for sandbox code related to the general control module. Code in this file
//  * is not intended to be production ready and may be deleted at any time. This file is not intended to be compiled
//  into
//  * the final firmware binary and should only be used for testing and development purposes. The intention is to copy
//  any
//  * sandbox used during development into this file for future reference and potential reuse.
//  */

////////////////////////       List of tests in this repo (incomplete/work in progress)    ///////////////////////
// static void app_ble_sandbox(void);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////                                     Tests                         ///////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// static void app_ble_sandbox(void)
// {
//    SEGGER_RTT_printf(0, "\n\nStarting App-BLE sandbox test\n");

//    // Assume m_ble_control is a global or accessible instance
//    ble_control_interface_t *ble_ifc = &m_ble_control.interface;

//    bool bond = true; // Set to true to test bonding

//    result_t result;
//    ble_control_status_t status = {0};

//    // Test: Get BLE status
//    result = ble_ifc->get_ble_status(ble_ifc, &status);
//    SEGGER_RTT_printf(0,
//                      "BLE status:\n bonded=%d\n connected=%d\n advertising=%d\n allow_new_bond=%d\n overflow    =
//                      %d\n", status.is_bonded, status.is_connected, status.is_advertising, status.allow_new_bond,
//                      status.received_data_overflow);

//    // Test: Start advertising (allow new bond)
//    if(bond)
//    {
//       uint8_t decrypted_passkey_text[64] = {0x00};
//       result = load_ble_passkey((char *)decrypted_passkey_text);

//       if(IS_ERR(result))
//       {
//          DEBUG_ERROR("Failed to load BLE passkey!");
//       }

//       // Start advertising
//       result = ble_ifc->start_advertising(ble_ifc, true, (char *)decrypted_passkey_text);
//       memset(decrypted_passkey_text, 0, 64);
//    }
//    else
//    {
//       result = ble_ifc->start_advertising(ble_ifc, false, NULL);
//    }

//    SEGGER_RTT_printf(0, "Start advertising result: unit: %d, err: %d\n", GET_ERR_UNIT(result), GET_ERR_CODE(result));

//    // Simulate connection event (would normally be handled by BLE stack)
//    // For sandbox, just check status again
//    result = ble_ifc->get_ble_status(ble_ifc, &status);
//    SEGGER_RTT_printf(0, "After advertising, BLE status: connected=%d\n", status.is_connected);

//    while(!status.is_connected && !status.is_bonded)
//    {
//       result = ble_ifc->get_ble_status(ble_ifc, &status);
//       feed_watchdog();
//       nrf_delay_ms(250);
//    }

//    SEGGER_RTT_printf(0, "Successfully connected via BLE!\n");
//    result = m_app_manager.interface.set_comms_link_status(&m_app_manager.interface, true);

//    uint8_t counter = 0;
//    uint64_t last_send_time = 0;
//    uint64_t current_time = 0;

//    (void)m_systick.interface.get_time_ms(&m_systick.interface, &last_send_time);

//    while(1)
//    {
//       result = ble_ifc->get_ble_status(ble_ifc, &status);
//       result = m_app_manager.interface.set_comms_link_status(&m_app_manager.interface, status.is_connected);
//       (void)m_debug_uart_driver_interface->process(&m_debug_uart_driver.interface);
//       IF_OK_RUN_AND_UPDATE(result, m_msg_prot_ble.interface.process(&m_msg_prot_ble.interface));
//       IF_OK_RUN_AND_UPDATE(result, m_app_manager.interface.process(&m_app_manager.interface));
//       // Send data every T seconds
//       (void)m_systick.interface.get_time_ms(&m_systick.interface, &current_time);

//       // Check if there are any incoming messages from the app manager
//       size_t rx_count = 0;
//       result = m_queue_app_manager_rx.interface.get_count(&m_queue_app_manager_rx.interface, &rx_count);

//       if(IS_OK(result) && (rx_count > 0))
//       {
//          DEBUG_TRACE("Processing %u incoming messages from app manager.", rx_count);

//          for(uint16_t idx = 0u; idx < rx_count; idx++)
//          {
//             // Pop message from queue
//             mp_packet_payload_t rx_msg = {0};
//             result = m_queue_app_manager_rx.interface.dequeue(&m_queue_app_manager_rx.interface, &rx_msg);
//             ON_ERR_DEBUG_ERROR(result,
//                                "Failed to dequeue message from app manager. Unit %d, Error: %d",
//                                GET_ERR_UNIT(result),
//                                GET_ERR_CODE(result));
//             SEGGER_RTT_SetTerminal(1);
//             SEGGER_RTT_printf(0, "Received msg time: %u s\n", current_time / 1000);
//             SEGGER_RTT_printf(0, "Received BLE msg Type: %u\n", rx_msg.type);
//             SEGGER_RTT_printf(0, "Received BLE msg PPI: %u\n", rx_msg.ppi);
//             SEGGER_RTT_printf(0, "Received BLE msg Length: %u\n", rx_msg.pkt_payload_len);
//             SEGGER_RTT_printf(0, "Received BLE msg Payload [0]: %u\n", rx_msg.payload[0]);
//             SEGGER_RTT_printf(0, "Received BLE msg Payload [1]: %u\n", rx_msg.payload[1]);
//             SEGGER_RTT_SetTerminal(0);

//             result = m_queue_app_manager_tx.interface.enqueue(&m_queue_app_manager_tx.interface, &rx_msg);
//          }
//       }

//       if(current_time - last_send_time >= (10 * 1000))
//       {
//          // Update data with incrementing counter
//          counter++;
//          last_send_time = current_time;
//       }

//       feed_watchdog();
//       nrf_delay_ms(1);
//    }

//    // Test: Stop advertising (disconnect)
//    result = ble_ifc->stop_advertising(ble_ifc);
//    SEGGER_RTT_printf(0, "Stop advertising result: %d\n", result);

//    // Final status
//    result = ble_ifc->get_ble_status(ble_ifc, &status);
//    SEGGER_RTT_printf(0, "Final BLE status: connected=%d, advertising=%d\n", status.is_connected,
//    status.is_advertising);
// }

//**************************************************************************************************************************/
//                                     NFC sandboxes start - Copy the entire set
//**************************************************************************************************************************/

// static const char *nfc_power_state_to_str(NFC_POWER_STATE state)
// {
//    switch(state)
//    {
//       case NFC_POWER_STATE_CHARGER_HIGH:
//          return "CHARGER_HIGH";
//       case NFC_POWER_STATE_CHARGER_LOW:
//          return "CHARGER_LOW";
//       case NFC_POWER_STATE_LOWPOWER_OFF:
//          return "LOWPOWER_OFF";
//       case NFC_POWER_STATE_LOWPOWER_POLL:
//          return "LOWPOWER_POLL";
//       case NFC_POWER_STATE_LOWPOWER_ACTIVE:
//          return "LOWPOWER_ACTIVE";
//       case NFC_POWER_STATE_UNKNOWN:
//       // Fall through
//       case NFC_POWER_STATE_MAX:
//       // Fall through
//       default:
//          return "UNKNOWN";
//    }
// }

// static void sandbox_set_nfc_power_led(NFC_POWER_STATE state)
// {
//    nrf_gpio_pin_clear(LED_RED);

//    switch(state)
//    {
//       case NFC_POWER_STATE_CHARGER_HIGH:
//          nrf_gpio_pin_set(LED_GREEN);
//          nrf_gpio_pin_clear(LED_BLUE);
//          break;

//       case NFC_POWER_STATE_CHARGER_LOW:
//       // Fall through
//       case NFC_POWER_STATE_LOWPOWER_POLL:
//       // Fall through
//       case NFC_POWER_STATE_LOWPOWER_ACTIVE:
//          nrf_gpio_pin_set(LED_BLUE);
//          nrf_gpio_pin_clear(LED_GREEN);
//          break;

//       case NFC_POWER_STATE_LOWPOWER_OFF:
//       // Fall through
//       case NFC_POWER_STATE_UNKNOWN:
//       // Fall through
//       case NFC_POWER_STATE_MAX:
//       // Fall through
//       default:
//          nrf_gpio_pin_clear(LED_BLUE);
//          nrf_gpio_pin_clear(LED_GREEN);
//          break;
//    }
// }

// static bool sandbox_uid_has_value(const uint8_t *uid, uint8_t uid_len)
// {
//    for(uint8_t uid_idx = 0u; uid_idx < uid_len; uid_idx++)
//    {
//       if(uid[uid_idx] != 0u)
//       {
//          return true;
//       }
//    }

//    return false;
// }

// static void sandbox_log_uid(const uint8_t *uid, uint8_t uid_len)
// {
//    for(uint8_t uid_idx = 0u; uid_idx < uid_len; uid_idx++)
//    {
//       SEGGER_RTT_printf(0, "%02X", uid[uid_idx]);
//    }
// }

// static void sandbox_handle_nfc_step(battery_status_t *battery_status,
//                                     status_update_t *ring_status,
//                                     uint32_t *ring_status_time_s,
//                                     bool allow_power_saving,
//                                     bool *is_ring_present,
//                                     uint8_t *ring_uid,
//                                     uint8_t *medication_uid,
//                                     bool *medication_uid_stale)
// {
//    /**
//     * The NFC power-state machine treats ring status as "fresh" only when
//     * update_time_unix_s increases. This helper increments that timestamp on
//     * every call so each sandbox iteration can deterministically drive the
//     * activity-dependent transitions in handle_nfc().
//     */
//    ring_status->update_time_unix_s = *ring_status_time_s;
//    *ring_status_time_s = *ring_status_time_s + 1u;

//    handle_nfc(is_ring_present,
//               ring_uid,
//               medication_uid,
//               medication_uid_stale,
//               *battery_status,
//               *ring_status,
//               allow_power_saving);
// }

// /**
//  * @brief Sandbox for alternating NFC Type-V and Type-A tag detection.
//  *
//  * This is a blocking manual-validation sandbox intended for bring-up and diagnostics of the dock NFC reader when
//  * switching between tag technologies.
//  *
//  * What this sandbox does:
//  * - Powers the NFC chip up and starts in Type-V detection mode.
//  * - Forces RF output to @ref NFC_POWER_LEVEL_LOW for the entire run.
//  * - Runs continuously with a fixed loop cadence (@c loop_step_ms).
//  * - Toggles detection mode every 10 seconds between Type-V and Type-A.
//  * - Processes the NFC driver each loop to advance discovery/presence state.
//  * - Reads and logs detected tag UIDs over SEGGER RTT.
//  * - For Type-V operation, also logs @c is_ring_present and includes that value
//  *   alongside Type-V UID prints.
//  *
//  * How it works:
//  * - A monotonic systick timestamp gates two schedules:
//  *   - Fast schedule: poll/process NFC every ~20 ms.
//  *   - Slow schedule: switch detection type every 10,000 ms.
//  * - On each successful mode switch, UID/presence change tracking is reset to avoid cross-mode stale comparisons.
//  * - UID logging is edge-triggered (new UID/length only) to prevent repetitive spam while the same tag remains in
//  field.
//  * - Watchdog feeding and idle handling are preserved in the main loop so the sandbox can run indefinitely on target
//  * hardware.
//  *
//  * Intended verification:
//  * - Verify that the reader can repeatedly transition between Type-V and Type-A without lockup or driver-state
//  * corruption.
//  * - Verify that low RF power operation still supports stable detection in both modes for nearby tags.
//  * - Verify that UID acquisition remains valid after each 10-second mode flip.
//  * - Verify that Type-V presence reporting (@c is_ring_present) tracks physical dock/undock changes and is coherent
//  with
//  * Type-V UID detections.
//  *
//  * @note This routine never returns and intentionally blocks normal boot flow
//  * while active.
//  */
// static void sandbox_nfc_tag_type_a_v_detection(void)
// {
//    (void)m_nfc.interface.auto_tune_antenna(&m_nfc.interface);
//    // Sandbox cadence:
//    // - `loop_step_ms` drives how often we service the NFC driver.
//    // - `toggle_interval_ms` flips between Type-V and Type-A detection modes.
//    const uint16_t loop_step_ms = 20u;
//    const uint32_t toggle_interval_ms = 10000u;

//    // Time checkpoints used to schedule periodic work without blocking delays.
//    uint64_t now_ms = 0u;
//    uint64_t last_step_ms = 0u;
//    uint64_t last_toggle_ms = 0u;

//    // Active mode starts as Type-V. This matches dock/ring presence behavior.
//    NFC_TAG_TYPE active_tag_type = NFC_TAG_TYPE_V;

//    // Type-V-only observable output.
//    bool is_ring_present = false;

//    // Presence and UID snapshots are used as change detectors so logs only
//    // print transitions/new values instead of flooding RTT every loop.
//    bool last_presence_valid = false;
//    bool last_presence = false;
//    bool last_uid_valid = false;
//    uint8_t last_uid[NFC_TAG_MAX_UID_SIZE] = {0};
//    uint8_t last_uid_len = 0u;

//    SEGGER_RTT_printf(0, "\n[NFC_SANDBOX][A_V] Starting Type-A/Type-V detection sandbox.\n");
//    SEGGER_RTT_printf(0, "[NFC_SANDBOX][A_V] NFC ON, output power LOW, toggling detection type every 10s.\n");

//    // Bring NFC online and configure Type-V first.
//    result_t result = nfc_power_up_and_select_type_v(NFC_POWER_LEVEL_LOW);
//    ON_ERR_DEBUG_ERROR(result,
//                       "Failed to power up/select Type-V for sandbox. Unit %d, Error: %d",
//                       GET_ERR_UNIT(result),
//                       GET_ERR_CODE(result));

//    if(IS_OK(result))
//    {
//       // Keep RF output low in both modes to validate low-power read behavior.
//       result = m_nfc.interface.set_output_power(&m_nfc.interface, NFC_POWER_LEVEL_LOW);
//       ON_ERR_DEBUG_ERROR(result,
//                          "Failed to set NFC low output power for sandbox. Unit %d, Error: %d",
//                          GET_ERR_UNIT(result),
//                          GET_ERR_CODE(result));
//    }

//    if(IS_OK(result))
//    {
//       SEGGER_RTT_printf(0, "[NFC_SANDBOX][A_V] Detection mode: TYPE_V\n");
//    }

//    m_systick_period_ms = loop_step_ms;
//    (void)m_systick.interface.get_time_ms(&m_systick.interface, &last_toggle_ms);
//    last_step_ms = last_toggle_ms;

//    // Intentional infinite sandbox loop:
//    // - service NFC periodically
//    // - alternate A/V mode every 10s
//    // - publish key observables over RTT
//    while(true)
//    {
//       (void)m_systick.interface.get_time_ms(&m_systick.interface, &now_ms);
//       if((now_ms - last_step_ms) >= loop_step_ms)
//       {
//          last_step_ms = now_ms;

//          // Periodic mode toggle to exercise repeated A <-> V transitions.
//          if((now_ms - last_toggle_ms) >= toggle_interval_ms)
//          {
//             NFC_TAG_TYPE next_tag_type = (NFC_TAG_TYPE_V == active_tag_type) ? NFC_TAG_TYPE_A : NFC_TAG_TYPE_V;
//             uint8_t power = (NFC_TAG_TYPE_V == next_tag_type) ? NFC_POWER_LEVEL_LOW : NFC_POWER_LEVEL_HIGH;
//             result_t switch_result = m_nfc.interface.set_detection_type(&m_nfc.interface, next_tag_type, power);
//             ON_ERR_DEBUG_ERROR(switch_result,
//                                "Failed to toggle NFC detection type in sandbox. Unit %d, Error: %d",
//                                GET_ERR_UNIT(switch_result),
//                                GET_ERR_CODE(switch_result));

//             if(IS_OK(switch_result))
//             {
//                // Re-assert low output power after every mode change to keep
//                // sandbox conditions stable and explicit.
//                switch_result = m_nfc.interface.set_output_power(&m_nfc.interface, NFC_POWER_LEVEL_LOW);
//                ON_ERR_DEBUG_ERROR(switch_result,
//                                   "Failed to keep NFC low output power in sandbox. Unit %d, Error: %d",
//                                   GET_ERR_UNIT(switch_result),
//                                   GET_ERR_CODE(switch_result));
//             }

//             if(IS_OK(switch_result))
//             {
//                active_tag_type = next_tag_type;

//                // Reset edge-detection state on mode switch so the first new
//                // sample in that mode is reported cleanly.
//                memset(last_uid, 0, NFC_TAG_MAX_UID_SIZE);
//                last_uid_valid = false;
//                last_uid_len = 0u;
//                last_presence_valid = false;
//                SEGGER_RTT_printf(0,
//                                  "[NFC_SANDBOX][A_V] Detection mode: %s\n",
//                                  (NFC_TAG_TYPE_V == active_tag_type) ? "TYPE_V" : "TYPE_A");
//             }

//             last_toggle_ms = now_ms;
//          }

//          // Drive NFC internal polling/discovery state machine.
//          result_t process_result = m_nfc.interface.process(&m_nfc.interface, true);
//          ON_ERR_DEBUG_ERROR(process_result,
//                             "Failed to process NFC in sandbox. Unit %d, Error: %d",
//                             GET_ERR_UNIT(process_result),
//                             GET_ERR_CODE(process_result));

//          bool tag_present = false;
//          if(IS_OK(process_result))
//          {
//             // Presence query is used for both modes. In Type-V we treat this
//             // as `is_ring_present` and explicitly report changes.
//             result_t presence_result = m_nfc.interface.is_ring_present(&m_nfc.interface, &tag_present);
//             ON_ERR_DEBUG_ERROR(presence_result,
//                                "Failed to read NFC tag presence in sandbox. Unit %d, Error: %d",
//                                GET_ERR_UNIT(presence_result),
//                                GET_ERR_CODE(presence_result));

//             if(IS_OK(presence_result) && (NFC_TAG_TYPE_V == active_tag_type))
//             {
//                is_ring_present = tag_present;
//                if(!last_presence_valid || (last_presence != is_ring_present))
//                {
//                   SEGGER_RTT_printf(0, "[NFC_SANDBOX][TYPE_V] is_ring_present=%u\n", is_ring_present ? 1u : 0u);
//                   last_presence = is_ring_present;
//                   last_presence_valid = true;
//                }
//             }

//             if(IS_OK(presence_result) && tag_present)
//             {
//                // Read UID only when presence is true. Buffer is sized to max
//                // UID length supported by the NFC layer.
//                uint8_t uid_len = NFC_TAG_MAX_UID_SIZE;
//                uint8_t uid_local[NFC_TAG_MAX_UID_SIZE] = {0};

//                result_t uid_result = m_nfc.interface.get_tag_uid(&m_nfc.interface, uid_local, &uid_len);
//                ON_ERR_DEBUG_ERROR(uid_result,
//                                   "Failed to read NFC tag UID in sandbox. Unit %d, Error: %d",
//                                   GET_ERR_UNIT(uid_result),
//                                   GET_ERR_CODE(uid_result));

//                bool uid_changed = !last_uid_valid || (uid_len != last_uid_len)
//                                   || (0 != memcmp(last_uid, uid_local, NFC_TAG_MAX_UID_SIZE));
//                if(IS_OK(uid_result) && (uid_len > 0u) && uid_changed)
//                {
//                   // Store the latest UID snapshot to suppress duplicate logs
//                   // while the same tag remains in field.
//                   memcpy(last_uid, uid_local, NFC_TAG_MAX_UID_SIZE);
//                   last_uid_valid = true;
//                   last_uid_len = uid_len;

//                   if(NFC_TAG_TYPE_V == active_tag_type)
//                   {
//                      // Type-V log intentionally includes is_ring_present so
//                      // ring presence state can be correlated with the UID.
//                      SEGGER_RTT_printf(0, "[NFC_SANDBOX][TYPE_V] UID=");
//                      sandbox_log_uid(uid_local, uid_len);
//                      SEGGER_RTT_printf(0, " is_ring_present=%u\n", is_ring_present ? 1u : 0u);
//                   }
//                   else
//                   {
//                      // Type-A log focuses on UID only.
//                      SEGGER_RTT_printf(0, "[NFC_SANDBOX][TYPE_A] UID=");
//                      sandbox_log_uid(uid_local, uid_len);
//                      SEGGER_RTT_printf(0, "\n");
//                   }
//                }
//             }
//             else if(IS_OK(presence_result) && !tag_present)
//             {
//                // Clear cached UID when tag leaves field so the next detection
//                // is treated as a fresh event and logged again.
//                memset(last_uid, 0, NFC_TAG_MAX_UID_SIZE);
//                last_uid_valid = false;
//                last_uid_len = 0u;
//             }
//          }
//       }

//       // Keep platform housekeeping alive while sandbox runs indefinitely.
//       feed_watchdog();
//       idle_state_handle();
//    }
// }

// #define NFC_MB_TEST_MAGIC0        (0xA5u)
// #define NFC_MB_TEST_MAGIC1        (0x5Au)
// #define NFC_MB_TEST_VERSION       (0x01u)
// #define NFC_MB_TEST_TYPE_PING     (0x01u)
// #define NFC_MB_TEST_TYPE_PONG     (0x02u)
// #define NFC_MB_TEST_HEADER_LEN    (6u)
// #define NFC_MB_TEST_MAX_PAYLOAD   (12u)
// #define NFC_MB_TEST_FRAME_MAX_LEN (NFC_MB_TEST_HEADER_LEN + NFC_MB_TEST_MAX_PAYLOAD + 1u)

// static uint8_t nfc_mb_test_checksum(const uint8_t *data, uint16_t len)
// {
//    uint8_t checksum = 0u;

//    for(uint16_t idx = 0u; idx < len; idx++)
//    {
//       checksum ^= data[idx];
//    }

//    return checksum;
// }

// static uint8_t nfc_mb_test_payload_seed(uint8_t msg_type)
// {
//    if(msg_type == NFC_MB_TEST_TYPE_PING)
//    {
//       return 0x11u;
//    }

//    if(msg_type == NFC_MB_TEST_TYPE_PONG)
//    {
//       return 0x77u;
//    }

//    return 0u;
// }

// static uint16_t nfc_mb_test_build_frame(uint8_t *buf, uint8_t msg_type, uint8_t seq, uint8_t payload_len)
// {
//    if(payload_len > NFC_MB_TEST_MAX_PAYLOAD)
//    {
//       payload_len = NFC_MB_TEST_MAX_PAYLOAD;
//    }

//    buf[0] = NFC_MB_TEST_MAGIC0;
//    buf[1] = NFC_MB_TEST_MAGIC1;
//    buf[2] = NFC_MB_TEST_VERSION;
//    buf[3] = msg_type;
//    buf[4] = seq;
//    buf[5] = payload_len;

//    const uint8_t seed = nfc_mb_test_payload_seed(msg_type);
//    for(uint8_t idx = 0u; idx < payload_len; idx++)
//    {
//       buf[NFC_MB_TEST_HEADER_LEN + idx] = (uint8_t)(seed + seq + (idx * 3u));
//    }

//    const uint16_t frame_len = (uint16_t)(NFC_MB_TEST_HEADER_LEN + payload_len + 1u);
//    buf[frame_len - 1u] = nfc_mb_test_checksum(buf, (uint16_t)(frame_len - 1u));

//    return frame_len;
// }

// static bool nfc_mb_test_parse_frame(const uint8_t *buf, uint8_t *msg_type, uint8_t *seq, uint8_t *payload_len)
// {
//    if((buf[0] != NFC_MB_TEST_MAGIC0) || (buf[1] != NFC_MB_TEST_MAGIC1) || (buf[2] != NFC_MB_TEST_VERSION))
//    {
//       return false;
//    }

//    const uint8_t type = buf[3];
//    if((type != NFC_MB_TEST_TYPE_PING) && (type != NFC_MB_TEST_TYPE_PONG))
//    {
//       return false;
//    }

//    const uint8_t length = buf[5];
//    if(length > NFC_MB_TEST_MAX_PAYLOAD)
//    {
//       return false;
//    }

//    const uint16_t frame_len = (uint16_t)(NFC_MB_TEST_HEADER_LEN + length + 1u);
//    const uint8_t expected_checksum = nfc_mb_test_checksum(buf, (uint16_t)(frame_len - 1u));
//    if(expected_checksum != buf[frame_len - 1u])
//    {
//       return false;
//    }

//    const uint8_t seed = nfc_mb_test_payload_seed(type);
//    for(uint8_t idx = 0u; idx < length; idx++)
//    {
//       const uint8_t expected = (uint8_t)(seed + buf[4] + (idx * 3u));
//       if(buf[NFC_MB_TEST_HEADER_LEN + idx] != expected)
//       {
//          return false;
//       }
//    }

//    if(msg_type != NULL)
//    {
//       *msg_type = type;
//    }

//    if(seq != NULL)
//    {
//       *seq = buf[4];
//    }

//    if(payload_len != NULL)
//    {
//       *payload_len = length;
//    }

//    return true;
// }

// static void __attribute__((unused)) nfc_sandbox_mb(void)
// {
//    feed_watchdog();
//    m_systick_period_ms = 20u;

//    result_t result = nfc_driver_init(&m_nfc, &m_i2c_driver_b.interface, &m_systick.interface);
//    if(IS_ERR(result))
//    {
//       SEGGER_RTT_printf(0, "nfc init failed. Code %d\n", GET_ERR_CODE(result));
//    }

//    if(IS_ERR(result))
//    {
//       SEGGER_RTT_printf(0, "set_output_power failed. Unit %d, code %d\n", GET_ERR_UNIT(result),
//       GET_ERR_CODE(result));
//    }
//    result = m_nfc.interface.set_detection_type(&m_nfc.interface, NFC_TAG_TYPE_V, NFC_POWER_LEVEL_LOW);
//    if(IS_ERR(result))
//    {
//       SEGGER_RTT_printf(0, "set_detection_type failed. Unit %d, code %d\n", GET_ERR_UNIT(result),
//       GET_ERR_CODE(result));
//    }
//    // result = m_nfc.interface.auto_tune_antenna(&m_nfc.interface);
//    // if(IS_ERR(result))
//    // {
//    //    SEGGER_RTT_printf(0, "auto_tune_antenna failed. Unit %d, code %d\n", GET_ERR_UNIT(result),
//    //    GET_ERR_CODE(result));
//    // }

//    const uint32_t process_interval_ms = 20u;
//    const uint32_t poll_interval_ms = 50u;
//    const uint32_t send_interval_ms = 2000u;
//    const uint32_t busy_retry_ms = 200u;
//    const uint32_t response_timeout_ms = 2500u;
//    const uint32_t stats_interval_ms = 5000u;
//    const uint32_t heartbeat_ms = 1000u;

//    uint64_t now_ms = 0u;
//    uint64_t last_process_ms = 0u;
//    uint64_t last_poll_ms = 0u;
//    uint64_t next_send_ms = 0u;
//    uint64_t last_send_ms = 0u;
//    uint64_t last_stats_ms = 0u;
//    uint64_t last_heartbeat_ms = 0u;

//    uint8_t tx_seq = 0u;
//    uint8_t awaiting_seq = 0u;
//    bool awaiting_pong = false;

//    uint32_t tx_ok = 0u;
//    uint32_t tx_busy = 0u;
//    uint32_t tx_err = 0u;
//    uint32_t rx_ok = 0u;
//    uint32_t rx_invalid = 0u;
//    uint32_t ping_rx = 0u;
//    uint32_t pong_rx = 0u;
//    uint32_t pong_timeout = 0u;
//    uint32_t unexpected_pong = 0u;

//    uint8_t tx_buf[NFC_MB_TEST_FRAME_MAX_LEN] = {0};
//    uint8_t rx_buf[300] = {0};
//    uint8_t tag_uid[NFC_TAG_MAX_UID_SIZE] = {0};
//    uint8_t tag_uid_len = 0u;

//    bool is_ring_present = false;
//    bool last_ring_present = false;

//    nrf_gpio_pin_clear(LED_RED);
//    nrf_gpio_pin_clear(LED_GREEN);
//    nrf_gpio_pin_clear(LED_BLUE);

//    while(true)
//    {
//       (void)m_systick.interface.get_time_ms(&m_systick.interface, &now_ms);

//       if(now_ms - last_process_ms >= process_interval_ms)
//       {
//          last_process_ms = now_ms;
//          result = m_nfc.interface.process(&m_nfc.interface, true);
//          if(IS_ERR(result))
//          {
//             SEGGER_RTT_printf(0, "nfc process failed. Code %d\n", GET_ERR_CODE(result));
//          }

//          result = m_nfc.interface.is_ring_present(&m_nfc.interface, &is_ring_present);
//          if(IS_ERR(result))
//          {
//             SEGGER_RTT_printf(0, "nfc is_ring_present failed. Code %d\n", GET_ERR_CODE(result));
//          }
//       }

//       if(is_ring_present != last_ring_present)
//       {
//          last_ring_present = is_ring_present;
//          SEGGER_RTT_printf(0, "NFC ring present: %d\n", is_ring_present);
//          if(!is_ring_present)
//          {
//             awaiting_pong = false;
//             next_send_ms = now_ms + send_interval_ms;
//          }
//       }

//       if(is_ring_present && (now_ms - last_poll_ms >= poll_interval_ms))
//       {
//          last_poll_ms = now_ms;
//          memset(rx_buf, 0, sizeof(rx_buf));
//          result = m_nfc.data_ifc.get_packet(&m_nfc.data_ifc, rx_buf, sizeof(rx_buf));
//          if(IS_OK(result))
//          {
//             uint8_t rx_type = 0u;
//             uint8_t rx_seq = 0u;
//             uint8_t rx_payload_len = 0u;
//             if(nfc_mb_test_parse_frame(rx_buf, &rx_type, &rx_seq, &rx_payload_len))
//             {
//                rx_ok++;
//                if(rx_type == NFC_MB_TEST_TYPE_PING)
//                {
//                   ping_rx++;
//                   const uint16_t frame_len
//                      = nfc_mb_test_build_frame(tx_buf, NFC_MB_TEST_TYPE_PONG, rx_seq, rx_payload_len);
//                   result = m_nfc.data_ifc.send_packet(&m_nfc.data_ifc, tx_buf, frame_len);
//                   if(IS_OK(result))
//                   {
//                      tx_ok++;
//                   }
//                   else if(GET_ERR_CODE(result) == COMMS_DRIVER_ERROR_BUSY)
//                   {
//                      tx_busy++;
//                   }
//                   else
//                   {
//                      tx_err++;
//                      SEGGER_RTT_printf(
//                         0, "MB PONG send failed. Unit %d, code %d\n", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//                   }
//                }
//                else if(rx_type == NFC_MB_TEST_TYPE_PONG)
//                {
//                   pong_rx++;
//                   if(awaiting_pong && (rx_seq == awaiting_seq))
//                   {
//                      const uint32_t rtt_ms = (uint32_t)(now_ms - last_send_ms);
//                      SEGGER_RTT_printf(0, "MB PONG rx seq=%u rtt=%lu ms\n", rx_seq, (unsigned long)rtt_ms);
//                      awaiting_pong = false;
//                   }
//                   else
//                   {
//                      unexpected_pong++;
//                      SEGGER_RTT_printf(0, "MB unexpected PONG seq=%u (awaiting=%u)\n", rx_seq, awaiting_seq);
//                   }
//                }
//             }
//             else
//             {
//                rx_invalid++;
//             }
//          }
//          else if(GET_ERR_CODE(result) != COMMS_DRIVER_ERROR_BUSY)
//          {
//             SEGGER_RTT_printf(
//                0, "MB get_packet failed. Unit %d, code %d\n", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//          }
//       }

//       if(awaiting_pong && ((now_ms - last_send_ms) >= response_timeout_ms))
//       {
//          pong_timeout++;
//          SEGGER_RTT_printf(0, "MB PONG timeout seq=%u\n", awaiting_seq);
//          awaiting_pong = false;
//          next_send_ms = now_ms + busy_retry_ms;
//       }

//       if(is_ring_present && !awaiting_pong && (now_ms >= next_send_ms))
//       {
//          const uint8_t payload_len = (uint8_t)(8u + (tx_seq & 0x03u));
//          const uint16_t frame_len = nfc_mb_test_build_frame(tx_buf, NFC_MB_TEST_TYPE_PING, tx_seq, payload_len);
//          result = m_nfc.data_ifc.send_packet(&m_nfc.data_ifc, tx_buf, frame_len);
//          if(IS_OK(result))
//          {
//             SEGGER_RTT_printf(0, "MB PING tx seq=%u len=%u\n", tx_seq, payload_len);
//             awaiting_pong = true;
//             awaiting_seq = tx_seq;
//             last_send_ms = now_ms;
//             tx_seq = (uint8_t)(tx_seq + 1u);
//             tx_ok++;
//             next_send_ms = now_ms + send_interval_ms;
//          }
//          else if(GET_ERR_CODE(result) == COMMS_DRIVER_ERROR_BUSY)
//          {
//             tx_busy++;
//             next_send_ms = now_ms + busy_retry_ms;
//          }
//          else
//          {
//             tx_err++;
//             SEGGER_RTT_printf(0, "MB PING send failed. Unit %d, code %d\n", GET_ERR_UNIT(result),
//             GET_ERR_CODE(result)); next_send_ms = now_ms + send_interval_ms;
//          }
//       }

//       if(now_ms - last_heartbeat_ms >= heartbeat_ms)
//       {
//          last_heartbeat_ms = now_ms;
//          if(is_ring_present)
//          {
//             nrf_gpio_pin_toggle(LED_GREEN);
//             nrf_gpio_pin_clear(LED_RED);
//          }
//          else
//          {
//             nrf_gpio_pin_set(LED_RED);
//             nrf_gpio_pin_clear(LED_GREEN);
//          }
//       }

//       if(now_ms - last_stats_ms >= stats_interval_ms)
//       {
//          last_stats_ms = now_ms;
//          SEGGER_RTT_printf(0,
//                            "MB stats: present=%d tx_ok=%lu tx_busy=%lu tx_err=%lu rx_ok=%lu rx_invalid=%lu "
//                            "ping_rx=%lu pong_rx=%lu timeout=%lu\n",
//                            is_ring_present,
//                            (unsigned long)tx_ok,
//                            (unsigned long)tx_busy,
//                            (unsigned long)tx_err,
//                            (unsigned long)rx_ok,
//                            (unsigned long)rx_invalid,
//                            (unsigned long)ping_rx,
//                            (unsigned long)pong_rx,
//                            (unsigned long)pong_timeout);

//          if(is_ring_present)
//          {
//             memset(tag_uid, 0, sizeof(tag_uid));
//             tag_uid_len = 0u;
//             result = m_nfc.interface.get_tag_uid(&m_nfc.interface, tag_uid, &tag_uid_len);
//             if(IS_OK(result) && (tag_uid_len > 0u))
//             {
//                SEGGER_RTT_printf(0, "NFC UID (%uB): ", tag_uid_len);
//                for(uint8_t idx = 0u; idx < tag_uid_len; idx++)
//                {
//                   SEGGER_RTT_printf(0, "%02X", tag_uid[idx]);
//                }
//                SEGGER_RTT_printf(0, "\n");
//             }
//             else if(IS_ERR(result))
//             {
//                SEGGER_RTT_printf(
//                   0, "get_tag_uid failed. Unit %d, code %d\n", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//             }
//          }
//       }

//       feed_watchdog();
//       idle_state_handle();
//    }
// }

// static void __attribute__((unused)) sandbox_nfc_power_optimization_testing(void)
// {
//    /**
//     * Sandbox goals:
//     * 1) Validate Type-V ring presence sampling path (is_ring_present output).
//     * 2) Validate Type-A medication UID read path and stale/non-stale behavior.
//     * 3) Validate NFC power optimization transitions under controlled stimuli.
//     * 4) End in a continuous visualization loop of NFC power states using LEDs.
//     *
//     * This sandbox is intentionally blocking and isolates handle_nfc() as the
//     * unit under test by repeatedly calling it with controlled battery/ring
//     * inputs.
//     */
//    const uint16_t loop_step_ms = 20u;

//    result_t result = nfc_driver_init(&m_nfc, &m_i2c_driver_b.interface, &m_systick.interface);

//    bool is_ring_present = false;
//    uint8_t ring_uid[NFC_TAG_MAX_UID_SIZE] = {0};
//    uint8_t medication_uid[NFC_TAG_MAX_UID_SIZE] = {0};
//    bool medication_uid_stale = true;

//    battery_status_t battery_status = {.charger_connected = false, .battery_state = BATTERY_STATE_SOC_GOOD};
//    status_update_t ring_status = {0};
//    uint32_t ring_status_time_s = 1u;

//    ring_status.ring_status.battery_charge_status = BATTERY_STATE_CHARGING;

//    SEGGER_RTT_printf(0, "\n[NFC_SANDBOX] Starting NFC power optimization sandbox.\n");
//    SEGGER_RTT_printf(0, "[NFC_SANDBOX] NOTE: This sandbox blocks normal boot while active.\n");
//    SEGGER_RTT_printf(0, "[NFC_SANDBOX] Test cadence: %u ms loop period.\n", loop_step_ms);

//    sandbox_set_nfc_power_led(NFC_POWER_STATE_LOWPOWER_OFF);
//    m_systick_period_ms = loop_step_ms;

//    bool skip_test_1 = false;
//    bool skip_to_final_test = true;

//    /**************************************************************************
//     * Test 1 - Type-V ring presence behavior
//     *
//     * Intent:
//     * - Verify that handle_nfc() updates is_ring_present from Type-V presence
//     *   checks and that transitions are observable when ring presence changes.
//     *
//     * Method:
//     * - Run for 10 seconds with allow_power_saving=false to keep NFC fully
//     *   responsive (no OFF windows masking transitions).
//     * - Ask operator to remove/re-dock ring during the window.
//     * - Track whether both presence states are observed.
//     *
//     * Pass criteria:
//     * - Both "present" and "absent" are seen at least once.
//     **************************************************************************/
//    SEGGER_RTT_printf(0,
//                      "[NFC_SANDBOX][TEST1] Type-V ring presence test (%ds). Remove the ring during the test and see
//                      if " "it detects the removal. After detecting the removal, replace the ring." "window.\n",
//                      (NFC_LOWPOWER_POLL_INTERVAL_MS + (1000u * 10u)) / 1000u);

//    bool test1_saw_present = false;
//    bool test1_saw_absent = false;
//    bool test1_last_presence_valid = false;
//    bool test1_last_presence = false;

//    uint64_t now_ms = 0u;
//    uint64_t test_start_ms = 0u;
//    uint64_t last_step_ms = 0u;
//    (void)m_systick.interface.get_time_ms(&m_systick.interface, &test_start_ms);
//    now_ms = test_start_ms;
//    last_step_ms = test_start_ms;

//    while(((now_ms - test_start_ms) < (NFC_LOWPOWER_POLL_INTERVAL_MS + (1000u * 10u))) && !skip_test_1
//          && !skip_to_final_test)
//    {
//       (void)m_systick.interface.get_time_ms(&m_systick.interface, &now_ms);
//       if((now_ms - last_step_ms) >= loop_step_ms)
//       {
//          last_step_ms = now_ms;

//          ring_status.ring_status.dose_fifo_used_percent = 0u;
//          ring_status.ring_status.battery_fifo_used_percent = 0u;
//          ring_status.ring_status.error_fifo_used_percent = 0u;
//          battery_status.charger_connected = false;

//          sandbox_handle_nfc_step(&battery_status,
//                                  &ring_status,
//                                  &ring_status_time_s,
//                                  false,
//                                  &is_ring_present,
//                                  ring_uid,
//                                  medication_uid,
//                                  &medication_uid_stale);

//          if(is_ring_present)
//          {
//             test1_saw_present = true;
//          }
//          else
//          {
//             test1_saw_absent = true;
//          }

//          if(!test1_last_presence_valid || (test1_last_presence != is_ring_present))
//          {
//             SEGGER_RTT_printf(0, "[NFC_SANDBOX][TEST1] is_ring_present=%u\n", is_ring_present ? 1u : 0u);
//             test1_last_presence = is_ring_present;
//             test1_last_presence_valid = true;
//          }
//       }

//       feed_watchdog();
//       idle_state_handle();
//    }

//    SEGGER_RTT_printf(0,
//                      "[NFC_SANDBOX][TEST1] %s (saw_present=%u saw_absent=%u)\n",
//                      (test1_saw_present && test1_saw_absent) ? "PASS" : "FAIL",
//                      test1_saw_present ? 1u : 0u,
//                      test1_saw_absent ? 1u : 0u);

//    /**************************************************************************
//     * Test 2 - Medication UID (Type-A) read behavior
//     *
//     * Intent:
//     * - Verify that handle_nfc() performs Type-A medication UID acquisition
//     *   when ring presence has a rising edge and that medication_uid_stale is
//     *   cleared on successful UID read.
//     *
//     * Method:
//     * - Step A: hold ring removed for 2 seconds to force a known "not present"
//     *   baseline and guarantee the next docking event is a rising edge.
//     * - Step B: dock ring with medication tag for 10 seconds and poll outputs.
//     * - Detect success when medication_uid_stale==false and medication UID has
//     *   non-zero content.
//     *
//     * Pass criteria:
//     * - At least one valid medication UID is observed during Step B.
//     **************************************************************************/
//    SEGGER_RTT_printf(0,
//                      "[NFC_SANDBOX][TEST2] Medication UID test. Step A: keep ring removed for 2s to force a fresh "
//                      "rising edge.\n");

//    (void)m_systick.interface.get_time_ms(&m_systick.interface, &test_start_ms);
//    now_ms = test_start_ms;
//    last_step_ms = test_start_ms;
//    while(((now_ms - test_start_ms) < 2000u) && !skip_to_final_test)
//    {
//       (void)m_systick.interface.get_time_ms(&m_systick.interface, &now_ms);
//       if((now_ms - last_step_ms) >= loop_step_ms)
//       {
//          last_step_ms = now_ms;

//          battery_status.charger_connected = false;
//          ring_status.ring_status.dose_fifo_used_percent = 0u;
//          ring_status.ring_status.battery_fifo_used_percent = 0u;
//          ring_status.ring_status.error_fifo_used_percent = 0u;

//          sandbox_handle_nfc_step(&battery_status,
//                                  &ring_status,
//                                  &ring_status_time_s,
//                                  false,
//                                  &is_ring_present,
//                                  ring_uid,
//                                  medication_uid,
//                                  &medication_uid_stale);
//       }

//       feed_watchdog();
//       idle_state_handle();
//    }

//    SEGGER_RTT_printf(0, "[NFC_SANDBOX][TEST2] Step B: dock ring with medication tag for 10s (watch for UID
//    print).\n");

//    bool test2_uid_detected = false;
//    uint8_t test2_last_uid[NFC_TAG_MAX_UID_SIZE] = {0};

//    (void)m_systick.interface.get_time_ms(&m_systick.interface, &test_start_ms);
//    now_ms = test_start_ms;
//    last_step_ms = test_start_ms;
//    while(((now_ms - test_start_ms) < 10000u) && !skip_to_final_test)
//    {
//       (void)m_systick.interface.get_time_ms(&m_systick.interface, &now_ms);
//       if((now_ms - last_step_ms) >= loop_step_ms)
//       {
//          last_step_ms = now_ms;

//          battery_status.charger_connected = false;
//          ring_status.ring_status.dose_fifo_used_percent = 0u;
//          ring_status.ring_status.battery_fifo_used_percent = 0u;
//          ring_status.ring_status.error_fifo_used_percent = 0u;

//          sandbox_handle_nfc_step(&battery_status,
//                                  &ring_status,
//                                  &ring_status_time_s,
//                                  false,
//                                  &is_ring_present,
//                                  ring_uid,
//                                  medication_uid,
//                                  &medication_uid_stale);

//          if(!medication_uid_stale && sandbox_uid_has_value(medication_uid, NFC_TAG_MAX_UID_SIZE))
//          {
//             test2_uid_detected = true;
//             if(0 != memcmp(test2_last_uid, medication_uid, NFC_TAG_MAX_UID_SIZE))
//             {
//                memcpy(test2_last_uid, medication_uid, NFC_TAG_MAX_UID_SIZE);
//                SEGGER_RTT_printf(0, "[NFC_SANDBOX][TEST2] Medication UID detected: ");
//                sandbox_log_uid(test2_last_uid, NFC_TAG_MAX_UID_SIZE);
//                SEGGER_RTT_printf(0, "\n");
//             }
//          }
//       }

//       feed_watchdog();
//       idle_state_handle();
//    }

//    SEGGER_RTT_printf(0,
//                      "[NFC_SANDBOX][TEST2] %s (uid_detected=%u)\n",
//                      test2_uid_detected ? "PASS" : "FAIL",
//                      test2_uid_detected ? 1u : 0u);

//    /**************************************************************************
//     * Test 3 - NFC power optimization state-machine behavior
//     *
//     * Intent:
//     * - Verify the policy logic in handle_nfc() that transitions among:
//     *   CHARGER_HIGH, CHARGER_LOW, LOWPOWER_POLL, LOWPOWER_ACTIVE, and
//     *   LOWPOWER_OFF.
//     *
//     * Method:
//     * - Drive battery_status.charger_connected and ring_status FIFO usage to
//     *   emulate charger events and ring activity.
//     * - Observe internal NFC state exported by sandbox hook.
//     * - Execute focused subtests to cover key transitions:
//     *   3.1 Charger connected + not full battery -> CHARGER_HIGH
//     *   3.2 Charger removed, no activity -> POLL then OFF
//     *   3.3 Activity while undocked -> ACTIVE
//     *   3.4 Activity removed -> ACTIVE hold timeout then OFF
//     *   3.5 Wait poll interval -> OFF wakes to POLL
//     *   3.6 Charger connected + full battery -> CHARGER_LOW
//     *
//     * Pass criteria:
//     * - All targeted states are observed at least once.
//     **************************************************************************/
//    SEGGER_RTT_printf(0, "[NFC_SANDBOX][TEST3] Verifying NFC power optimization state transitions.\n");

//    bool test3_saw_poll = false;
//    bool test3_saw_active = false;
//    bool test3_saw_off = false;
//    bool test3_saw_interval_poll = false;
//    bool test3_saw_charger_high = false;
//    bool test3_saw_charger_low = false;

//    NFC_POWER_STATE test3_prev_state = NFC_POWER_STATE_MAX;

//    // 3.1 Force charger high state (charging allowed, higher RF power target).
//    battery_status.charger_connected = true;
//    ring_status.ring_status.battery_charge_status = BATTERY_STATE_CHARGING;
//    ring_status.ring_status.dose_fifo_used_percent = 0u;
//    ring_status.ring_status.battery_fifo_used_percent = 0u;
//    ring_status.ring_status.error_fifo_used_percent = 0u;

//    (void)m_systick.interface.get_time_ms(&m_systick.interface, &test_start_ms);
//    now_ms = test_start_ms;
//    last_step_ms = test_start_ms;
//    while(((now_ms - test_start_ms) < 1500u) && !skip_to_final_test)
//    {
//       (void)m_systick.interface.get_time_ms(&m_systick.interface, &now_ms);
//       if((now_ms - last_step_ms) >= loop_step_ms)
//       {
//          last_step_ms = now_ms;

//          sandbox_handle_nfc_step(&battery_status,
//                                  &ring_status,
//                                  &ring_status_time_s,
//                                  true,
//                                  &is_ring_present,
//                                  ring_uid,
//                                  medication_uid,
//                                  &medication_uid_stale);

//          if(NFC_POWER_STATE_CHARGER_HIGH == m_sandbox_last_nfc_state)
//          {
//             test3_saw_charger_high = true;
//          }

//          if(test3_prev_state != m_sandbox_last_nfc_state)
//          {
//             SEGGER_RTT_printf(
//                0, "[NFC_SANDBOX][TEST3] state -> %s\n", nfc_power_state_to_str(m_sandbox_last_nfc_state));
//             test3_prev_state = m_sandbox_last_nfc_state;
//          }
//       }

//       feed_watchdog();
//       idle_state_handle();
//    }

//    // 3.2 Disconnect charger with no activity: expect low-power POLL then OFF.
//    battery_status.charger_connected = false;
//    ring_status.ring_status.dose_fifo_used_percent = 0u;
//    ring_status.ring_status.battery_fifo_used_percent = 0u;
//    ring_status.ring_status.error_fifo_used_percent = 0u;

//    (void)m_systick.interface.get_time_ms(&m_systick.interface, &test_start_ms);
//    now_ms = test_start_ms;
//    last_step_ms = test_start_ms;
//    while(((now_ms - test_start_ms) < 3500u) && !skip_to_final_test)
//    {
//       (void)m_systick.interface.get_time_ms(&m_systick.interface, &now_ms);
//       if((now_ms - last_step_ms) >= loop_step_ms)
//       {
//          last_step_ms = now_ms;

//          sandbox_handle_nfc_step(&battery_status,
//                                  &ring_status,
//                                  &ring_status_time_s,
//                                  true,
//                                  &is_ring_present,
//                                  ring_uid,
//                                  medication_uid,
//                                  &medication_uid_stale);

//          if(NFC_POWER_STATE_LOWPOWER_POLL == m_sandbox_last_nfc_state)
//          {
//             test3_saw_poll = true;
//          }
//          if(NFC_POWER_STATE_LOWPOWER_OFF == m_sandbox_last_nfc_state)
//          {
//             test3_saw_off = true;
//          }

//          if(test3_prev_state != m_sandbox_last_nfc_state)
//          {
//             SEGGER_RTT_printf(
//                0, "[NFC_SANDBOX][TEST3] state -> %s\n", nfc_power_state_to_str(m_sandbox_last_nfc_state));
//             test3_prev_state = m_sandbox_last_nfc_state;
//          }
//       }

//       feed_watchdog();
//       idle_state_handle();
//    }

//    // 3.3 Re-enter low-power flow and force ACTIVE by simulating FIFO activity.
//    battery_status.charger_connected = true;
//    ring_status.ring_status.battery_charge_status = BATTERY_STATE_CHARGING;

//    (void)m_systick.interface.get_time_ms(&m_systick.interface, &test_start_ms);
//    now_ms = test_start_ms;
//    last_step_ms = test_start_ms;
//    while(((now_ms - test_start_ms) < 1000u) && !skip_to_final_test)
//    {
//       (void)m_systick.interface.get_time_ms(&m_systick.interface, &now_ms);
//       if((now_ms - last_step_ms) >= loop_step_ms)
//       {
//          last_step_ms = now_ms;

//          sandbox_handle_nfc_step(&battery_status,
//                                  &ring_status,
//                                  &ring_status_time_s,
//                                  true,
//                                  &is_ring_present,
//                                  ring_uid,
//                                  medication_uid,
//                                  &medication_uid_stale);
//       }

//       feed_watchdog();
//       idle_state_handle();
//    }

//    battery_status.charger_connected = false;
//    ring_status.ring_status.dose_fifo_used_percent = 40u;
//    ring_status.ring_status.battery_fifo_used_percent = 20u;
//    ring_status.ring_status.error_fifo_used_percent = 5u;

//    (void)m_systick.interface.get_time_ms(&m_systick.interface, &test_start_ms);
//    now_ms = test_start_ms;
//    last_step_ms = test_start_ms;
//    while(((now_ms - test_start_ms) < 3000u) && !skip_to_final_test)
//    {
//       (void)m_systick.interface.get_time_ms(&m_systick.interface, &now_ms);
//       if((now_ms - last_step_ms) >= loop_step_ms)
//       {
//          last_step_ms = now_ms;

//          sandbox_handle_nfc_step(&battery_status,
//                                  &ring_status,
//                                  &ring_status_time_s,
//                                  true,
//                                  &is_ring_present,
//                                  ring_uid,
//                                  medication_uid,
//                                  &medication_uid_stale);

//          if(NFC_POWER_STATE_LOWPOWER_ACTIVE == m_sandbox_last_nfc_state)
//          {
//             test3_saw_active = true;
//          }

//          if(test3_prev_state != m_sandbox_last_nfc_state)
//          {
//             SEGGER_RTT_printf(
//                0, "[NFC_SANDBOX][TEST3] state -> %s\n", nfc_power_state_to_str(m_sandbox_last_nfc_state));
//             test3_prev_state = m_sandbox_last_nfc_state;
//          }
//       }

//       feed_watchdog();
//       idle_state_handle();
//    }

//    // 3.4 Remove activity and verify ACTIVE hold timer expires back to OFF.
//    ring_status.ring_status.dose_fifo_used_percent = 0u;
//    ring_status.ring_status.battery_fifo_used_percent = 0u;
//    ring_status.ring_status.error_fifo_used_percent = 0u;

//    (void)m_systick.interface.get_time_ms(&m_systick.interface, &test_start_ms);
//    now_ms = test_start_ms;
//    last_step_ms = test_start_ms;
//    while(((now_ms - test_start_ms) < (NFC_LOWPOWER_ACTIVE_HOLD_MS + 1500u)) && !skip_to_final_test)
//    {
//       (void)m_systick.interface.get_time_ms(&m_systick.interface, &now_ms);
//       if((now_ms - last_step_ms) >= loop_step_ms)
//       {
//          last_step_ms = now_ms;

//          sandbox_handle_nfc_step(&battery_status,
//                                  &ring_status,
//                                  &ring_status_time_s,
//                                  true,
//                                  &is_ring_present,
//                                  ring_uid,
//                                  medication_uid,
//                                  &medication_uid_stale);

//          if(NFC_POWER_STATE_LOWPOWER_OFF == m_sandbox_last_nfc_state)
//          {
//             test3_saw_off = true;
//          }

//          if(test3_prev_state != m_sandbox_last_nfc_state)
//          {
//             SEGGER_RTT_printf(
//                0, "[NFC_SANDBOX][TEST3] state -> %s\n", nfc_power_state_to_str(m_sandbox_last_nfc_state));
//             test3_prev_state = m_sandbox_last_nfc_state;
//          }
//       }

//       feed_watchdog();
//       idle_state_handle();
//    }

//    // 3.5 Idle long enough to verify periodic OFF -> POLL wake behavior.
//    (void)m_systick.interface.get_time_ms(&m_systick.interface, &test_start_ms);
//    now_ms = test_start_ms;
//    last_step_ms = test_start_ms;
//    while(((now_ms - test_start_ms) < (NFC_LOWPOWER_POLL_INTERVAL_MS + NFC_LOWPOWER_POLL_ON_TIME_MS + 1500u))
//          && !skip_to_final_test)
//    {
//       (void)m_systick.interface.get_time_ms(&m_systick.interface, &now_ms);
//       if((now_ms - last_step_ms) >= loop_step_ms)
//       {
//          last_step_ms = now_ms;

//          sandbox_handle_nfc_step(&battery_status,
//                                  &ring_status,
//                                  &ring_status_time_s,
//                                  true,
//                                  &is_ring_present,
//                                  ring_uid,
//                                  medication_uid,
//                                  &medication_uid_stale);

//          if(NFC_POWER_STATE_LOWPOWER_POLL == m_sandbox_last_nfc_state)
//          {
//             test3_saw_interval_poll = true;
//          }

//          if(test3_prev_state != m_sandbox_last_nfc_state)
//          {
//             SEGGER_RTT_printf(
//                0, "[NFC_SANDBOX][TEST3] state -> %s\n", nfc_power_state_to_str(m_sandbox_last_nfc_state));
//             test3_prev_state = m_sandbox_last_nfc_state;
//          }
//       }

//       feed_watchdog();
//       idle_state_handle();
//    }

//    // 3.6 Charger connected + ring battery full should select CHARGER_LOW.
//    battery_status.charger_connected = true;
//    ring_status.ring_status.battery_charge_status = BATTERY_STATE_CHARGING_COMPLETED;
//    ring_status.ring_status.dose_fifo_used_percent = 0u;
//    ring_status.ring_status.battery_fifo_used_percent = 0u;
//    ring_status.ring_status.error_fifo_used_percent = 0u;

//    (void)m_systick.interface.get_time_ms(&m_systick.interface, &test_start_ms);
//    now_ms = test_start_ms;
//    last_step_ms = test_start_ms;
//    while(((now_ms - test_start_ms) < 1500u) && !skip_to_final_test)
//    {
//       (void)m_systick.interface.get_time_ms(&m_systick.interface, &now_ms);
//       if((now_ms - last_step_ms) >= loop_step_ms)
//       {
//          last_step_ms = now_ms;

//          sandbox_handle_nfc_step(&battery_status,
//                                  &ring_status,
//                                  &ring_status_time_s,
//                                  true,
//                                  &is_ring_present,
//                                  ring_uid,
//                                  medication_uid,
//                                  &medication_uid_stale);

//          if(NFC_POWER_STATE_CHARGER_LOW == m_sandbox_last_nfc_state)
//          {
//             test3_saw_charger_low = true;
//          }

//          if(test3_prev_state != m_sandbox_last_nfc_state)
//          {
//             SEGGER_RTT_printf(
//                0, "[NFC_SANDBOX][TEST3] state -> %s\n", nfc_power_state_to_str(m_sandbox_last_nfc_state));
//             test3_prev_state = m_sandbox_last_nfc_state;
//          }
//       }

//       feed_watchdog();
//       idle_state_handle();
//    }

//    bool test3_pass = test3_saw_charger_high && test3_saw_poll && test3_saw_active && test3_saw_off
//                      && test3_saw_interval_poll && test3_saw_charger_low;
//    SEGGER_RTT_printf(0,
//                      "[NFC_SANDBOX][TEST3] %s (high=%u poll=%u active=%u off=%u interval_poll=%u charger_low=%u)\n",
//                      test3_pass ? "PASS" : "FAIL",
//                      test3_saw_charger_high ? 1u : 0u,
//                      test3_saw_poll ? 1u : 0u,
//                      test3_saw_active ? 1u : 0u,
//                      test3_saw_off ? 1u : 0u,
//                      test3_saw_interval_poll ? 1u : 0u,
//                      test3_saw_charger_low ? 1u : 0u);

//    /**************************************************************************
//     * Final test - Continuous hardware visualization loop
//     *
//     * Intent:
//     * - Leave device in a perpetual handle_nfc() loop and visually expose
//     *   power-state behavior for manual observation/debug using live system data.
//     *
//     * Method:
//     * - Use the same input sources and sequencing as the normal application:
//     *   - handle_battery()
//     *   - handle_nfc()
//     *   - m_ring.interface.get_ring_status()
//     * - On each loop, call handle_nfc() and map observed state to LEDs:
//     *   BLUE  -> low-power NFC states (LOWPOWER_POLL/ACTIVE, CHARGER_LOW)
//     *   GREEN -> high-power charging state (CHARGER_HIGH)
//     *   OFF   -> NFC powered down (LOWPOWER_OFF)
//     **************************************************************************/
//    SEGGER_RTT_printf(0, "[NFC_SANDBOX][FINAL] Entering live NFC state loop (application-like inputs).\n");
//    SEGGER_RTT_printf(
//       0, "[NFC_SANDBOX][FINAL] LED mapping: BLUE=low power NFC, GREEN=high power NFC, OFF=NFC powered down.\n");

//    uint64_t final_loop_start_ms = 0u;
//    uint64_t final_last_step_ms = 0u;
//    (void)m_systick.interface.get_time_ms(&m_systick.interface, &final_loop_start_ms);
//    final_last_step_ms = final_loop_start_ms;
//    NFC_POWER_STATE final_prev_state = NFC_POWER_STATE_MAX;

//    while(true)
//    {
//       (void)m_systick.interface.get_time_ms(&m_systick.interface, &now_ms);
//       if((now_ms - final_last_step_ms) >= loop_step_ms)
//       {
//          final_last_step_ms = now_ms;

//          // Keep data sources/order aligned with implement_control_loop().
//          // handle_battery(&battery_status);
//          battery_status.charger_connected = false;

//          bool keep_nfc_responsive = m_system_sm_inputs.is_calibration_active ||
//          m_system_sm_inputs.is_baselining_active
//                                     || (STATEMACHINE_STATE_CALIBRATION == m_system_sm.current_state)
//                                     || (STATEMACHINE_STATE_BASELINING == m_system_sm.current_state);
//          bool allow_nfc_power_saving = !keep_nfc_responsive;

//          handle_nfc(&is_ring_present,
//                     ring_uid,
//                     medication_uid,
//                     &medication_uid_stale,
//                     battery_status,
//                     ring_status,
//                     allow_nfc_power_saving);

//          // result = m_ring.interface.get_ring_status(&m_ring.interface, &ring_status);
//          // ON_ERR_DEBUG_ERROR(
//          //    result, "Failed to get ring status. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

//          sandbox_set_nfc_power_led(m_sandbox_last_nfc_state);

//          if(final_prev_state != m_sandbox_last_nfc_state)
//          {
//             uint8_t tx_power = 0u;
//             uint32_t elapsed_ms = (uint32_t)(now_ms - final_loop_start_ms);
//             (void)m_nfc.interface.get_output_power(&m_nfc.interface, &tx_power);

//             SEGGER_RTT_printf(0,
//                               "[NFC_SANDBOX][FINAL] t=%lu ms state=%s charger=%u ring_present=%u tx_power=%u\n",
//                               (unsigned long)elapsed_ms,
//                               nfc_power_state_to_str(m_sandbox_last_nfc_state),
//                               battery_status.charger_connected ? 1u : 0u,
//                               is_ring_present ? 1u : 0u,
//                               tx_power);

//             final_prev_state = m_sandbox_last_nfc_state;
//          }
//       }

//       feed_watchdog();
//       idle_state_handle();
//    }
// }

// static void __attribute__((unused)) nfc_sandbox3(void)
// {
//    DEBUG_INFO("Starting sandbox3 - Type A or V detection");
//    uint64_t current_ms = 0;
//    uint64_t prev_ms = 0;
//    uint64_t prev_report_ms = 0;
//    uint64_t prev_switch_ms = 0;
//    NFC_TAG_TYPE tag_type = NFC_TAG_TYPE_V;
//    result_t result = RESULT_OK;

//    bool autotune_antenna = false;

//    // Init NFC reader
//    result = nfc_driver_init(&m_nfc, &m_i2c_driver_b.interface, &m_systick.interface);
//    if(IS_ERR(result))
//    {
//       DEBUG_ERROR("nfc init failed. Code %d", GET_ERR_CODE(result));
//    }

//    result = m_nfc.interface.set_detection_type(&m_nfc.interface, tag_type, NFC_POWER_LEVEL_LOW);

//    if(IS_ERR(result))
//    {
//       DEBUG_ERROR("Error: unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//    }

//    if(autotune_antenna)
//    {
//       result = m_nfc.interface.auto_tune_antenna(&m_nfc.interface);
//       ON_ERR_DEBUG_ERROR(result, "AAT Error: unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//    }

//    /* if(IS_ERR(result))
//    {
//       DEBUG_ERROR("Error: unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//    } */

//    while(true)
//    {
//       (void)m_systick.interface.get_time_ms(&m_systick.interface, &current_ms);
//       if(current_ms - prev_ms > 20u)
//       {
//          prev_ms = current_ms;

//          result = m_nfc.interface.process(&m_nfc.interface, true);
//          if(IS_ERR(result))
//          {
//             DEBUG_ERROR("Error: unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//          }
//       }

//       if(current_ms - prev_switch_ms > 5000u)
//       {
//          prev_switch_ms = current_ms;

//          // Toggle between tag types
//          tag_type = (NFC_TAG_TYPE_A == tag_type) ? NFC_TAG_TYPE_V : NFC_TAG_TYPE_A;
//          uint8_t power = (NFC_TAG_TYPE_V == tag_type) ? NFC_POWER_LEVEL_LOW : NFC_POWER_LEVEL_HIGH;

//          // Set tag type
//          result = m_nfc.interface.set_detection_type(&m_nfc.interface, tag_type, power);

//          if(IS_ERR(result))
//          {
//             DEBUG_ERROR("Error: unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//          }
//       }

//       // Report presence/UID every 1s for current tag type
//       if(current_ms - prev_report_ms > 1000u)
//       {
//          prev_report_ms = current_ms;
//          bool present = false;
//          result = m_nfc.interface.is_ring_present(&m_nfc.interface, &present);
//          if(IS_ERR(result))
//          {
//             DEBUG_ERROR("is_ring_present error: unit %d code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//          }
//          else if(!present)
//          {
//             DEBUG_INFO("Tag type %s not found", (tag_type == NFC_TAG_TYPE_A) ? "A" : "V");
//          }
//          else
//          {
//             uint8_t uid[16] = {0};
//             uint8_t uid_len = sizeof(uid);
//             result = m_nfc.interface.get_tag_uid(&m_nfc.interface, uid, &uid_len);
//             if(IS_OK(result))
//             {
//                SEGGER_RTT_printf(0, "Tag type %s UID (%uB): ", (tag_type == NFC_TAG_TYPE_A) ? "A" : "V", uid_len);
//                for(uint8_t i = 0; i < uid_len; i++)
//                {
//                   SEGGER_RTT_printf(0, "%02X", uid[i]);
//                }
//                SEGGER_RTT_printf(0, "\n");
//             }
//             else
//             {
//                DEBUG_ERROR("get_tag_uid error: unit %d code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//             }
//          }
//       }
//       feed_watchdog();
//       idle_state_handle();
//    }
// }

//**************************************************************************************************************************/
//                                     NFC sandboxes end - Copy the entire set
//***************************************************************************************************************************
//*/
// static void __attribute__((unused)) nfc_test_power_saving_sandbox(void)
// {
//    DEBUG_INFO("Starting NFC power saving sandbox");
//    uint64_t current_ms = 0;
//    uint64_t prev_ms = 0;
//    uint64_t prev_step_ms = 0;
//    uint8_t level = 3;
//    bool toggle = false;

//    result_t result = nfc_driver_init(&m_nfc, &m_i2c_driver_b.interface, &m_systick.interface);
//    if(IS_ERR(result))
//    {
//       SEGGER_RTT_printf(0, "nfc init failed. Code %d\n", GET_ERR_CODE(result));
//    }

//    result = m_nfc.interface.set_output_power(&m_nfc.interface, level);
//    if(IS_ERR(result))
//    {
//       DEBUG_ERROR("set_output_power failed: unit %d code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//    }

//    while(true)
//    {
//       (void)m_systick.interface.get_time_ms(&m_systick.interface, &current_ms);
//       if(current_ms - prev_ms > 20u)
//       {
//          prev_ms = current_ms;
//          (void)m_nfc.interface.process(&m_nfc.interface, true);
//       }

//       if(current_ms - prev_step_ms > 5000u)
//       {
//          prev_step_ms = current_ms;
//          if(toggle)
//          {
//             result = m_nfc.interface.field_off(&m_nfc.interface);
//             ON_ERR_DEBUG_ERROR(
//                result, "Failed to turn NFC field off. Unit %d, Error: %d", GET_ERR_UNIT(result),
//                GET_ERR_CODE(result));
//             // OR
//             result = m_nfc.interface.chip_power_down(&m_nfc.interface);
//             ON_ERR_DEBUG_ERROR(
//                result, "Failed to power down NFC chip. Unit %d, Error: %d", GET_ERR_UNIT(result),
//                GET_ERR_CODE(result));
//             // OR
//             result = m_nfc.interface.wakeup_mode_start(&m_nfc.interface);
//             ON_ERR_DEBUG_ERROR(result,
//                                "Failed to start wake-up mode NFC chip. Unit %d, Error: %d",
//                                GET_ERR_UNIT(result),
//                                GET_ERR_CODE(result));

//             toggle = false;
//          }
//          else
//          {
//             result = m_nfc.interface.field_on_and_start_gt(&m_nfc.interface);
//             ON_ERR_DEBUG_ERROR(
//                result, "Failed to turn NFC field on. Unit %d, Error: %d", GET_ERR_UNIT(result),
//                GET_ERR_CODE(result));
//             // OR
//             result = m_nfc.interface.chip_power_up(&m_nfc.interface);
//             ON_ERR_DEBUG_ERROR(
//                result, "Failed to power up NFC chip. Unit %d, Error: %d", GET_ERR_UNIT(result),
//                GET_ERR_CODE(result));
//             // OR
//             result = m_nfc.interface.wakeup_mode_stop(&m_nfc.interface);
//             ON_ERR_DEBUG_ERROR(result,
//                                "Failed to stop wake-up mode NFC chip. Unit %d, Error: %d",
//                                GET_ERR_UNIT(result),
//                                GET_ERR_CODE(result));

//             toggle = true;
//          }
//       }

//       feed_watchdog();
//       idle_state_handle();
//    }
// }

// void basic_ble_sandbox(void)
// {
//    SEGGER_RTT_printf(0, "\n\nStarting BLE sandbox test\n");

//    // Assume m_ble_control is a global or accessible instance
//    ble_control_interface_t *ble_ifc = &m_ble_control.interface;
//    comms_driver_interface_t *comms_ifc = &m_ble_control.data_ifc;

//    bool bond = true; // Set to true to test bonding

//    result_t result;
//    ble_control_status_t status = {0};

//    // Test: Get BLE status
//    result = ble_ifc->get_ble_status(ble_ifc, &status);
//    SEGGER_RTT_printf(0,
//                      "BLE status:\n bonded=%d\n connected=%d\n advertising=%d\n allow_new_bond=%d\n overflow    =
//                      %d\n", status.is_bonded, status.is_connected, status.is_advertising, status.allow_new_bond,
//                      status.received_data_overflow);

//    // Test: Start advertising (allow new bond)
//    if(bond)
//    {
//       uint8_t decrypted_passkey_text[64] = {0x00};
//       result = load_ble_passkey((char *)decrypted_passkey_text);

//       if(IS_ERR(result))
//       {
//          DEBUG_ERROR("Failed to load BLE passkey!");
//       }

//       // Start advertising
//       result = ble_ifc->start_advertising(ble_ifc, true, (char *)decrypted_passkey_text);
//       memset(decrypted_passkey_text, 0, 64);
//    }
//    else
//    {
//       result = ble_ifc->start_advertising(ble_ifc, false, NULL);
//    }

//    SEGGER_RTT_printf(0, "Start advertising result: unit: %d, err: %d\n", GET_ERR_UNIT(result), GET_ERR_CODE(result));

//    // Simulate connection event (would normally be handled by BLE stack)
//    // For sandbox, just check status again
//    result = ble_ifc->get_ble_status(ble_ifc, &status);
//    SEGGER_RTT_printf(0, "After advertising, BLE status: connected=%d\n", status.is_connected);

//    while(!status.is_connected && !status.is_bonded)
//    {
//       result = ble_ifc->get_ble_status(ble_ifc, &status);
//       feed_watchdog();
//       nrf_delay_ms(250);
//    }

//    SEGGER_RTT_printf(0, "Successfully connected via BLE!\n");

//    // Continuous loop: send incrementing data every 5s, print any received data
//    uint8_t tx_data[NRF_SDH_BLE_GATT_MAX_MTU_SIZE - 3u] = {0}; // Max size is MTU-3 (Opcode + Handle = 3 bytes) =

//    for(uint8_t i = 0; i < sizeof(tx_data); i++)
//    {
//       tx_data[i] = i;
//    }
//    uint8_t counter = 0;
//    uint64_t last_send_time = 0;
//    uint64_t current_time = 0;
//    uint8_t rx_data[BLE_RECEIVE_DATA_BUFFER_SIZE - 3u] = {0}; // Max size is MTU-3 (Opcode + Handle = 3 bytes)
//    uint8_t rx_len = 0;

//    (void)m_systick.interface.get_time_ms(&m_systick.interface, &last_send_time);

//    while(1)
//    {
//       //      (void)m_debug_uart_driver_interface->process(&m_debug_uart_driver.interface);
//       // Send data every T seconds
//       (void)m_systick.interface.get_time_ms(&m_systick.interface, &current_time);
//       if(current_time - last_send_time >= (15 * 1000))
//       {
//          // Update data with incrementing counter
//          for(uint8_t i = 0; i < sizeof(tx_data); i++)
//          {
//             tx_data[i] = counter + i;
//          }
//          counter++;
//          SEGGER_RTT_printf(0, "Sending data (counter=%d)...\n", counter);
//          result = comms_ifc->send_packet(comms_ifc, tx_data, sizeof(tx_data));
//          SEGGER_RTT_printf(0, "Send data result: %d\n", result);
//          last_send_time = current_time;
//       }
//       // Check for received data
//       rx_len = BLE_RX_PACKET_ELEMENT_SIZE;
//       result = comms_ifc->get_packet(comms_ifc, rx_data, sizeof(rx_data));

//       if(IS_ERR(result))
//       {
//          if(COMMS_DRIVER_ERROR_BUSY == GET_ERR_CODE(result))
//          {
//             SEGGER_RTT_printf(0, ".");
//          }
//          else
//          {
//             SEGGER_RTT_printf(0, "Error. Unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//          }
//       }
//       else
//       {
//          if((rx_len > 0) && IS_OK(result))
//          {
//             SEGGER_RTT_printf(0, "\nReceived data: ");
//             for(uint8_t i = 0; i < rx_len; i++)
//             {
//                SEGGER_RTT_printf(0, "%02X ", rx_data[i]);
//             }
//             SEGGER_RTT_printf(0, "\n");
//          }
//       }

//       feed_watchdog();
//       nrf_delay_ms(100);
//    }

//    // Test: Stop advertising (disconnect)
//    result = ble_ifc->stop_advertising(ble_ifc);
//    SEGGER_RTT_printf(0, "Stop advertising result: %d\n", result);

//    // Final status
//    result = ble_ifc->get_ble_status(ble_ifc, &status);
//    SEGGER_RTT_printf(0, "Final BLE status: connected=%d, advertising=%d\n", status.is_connected,
//    status.is_advertising);
// }

// #define NFC_MB_TEST_MAGIC0        (0xA5u)
// #define NFC_MB_TEST_MAGIC1        (0x5Au)
// #define NFC_MB_TEST_VERSION       (0x01u)
// #define NFC_MB_TEST_TYPE_PING     (0x01u)
// #define NFC_MB_TEST_TYPE_PONG     (0x02u)
// #define NFC_MB_TEST_HEADER_LEN    (6u)
// #define NFC_MB_TEST_MAX_PAYLOAD   (12u)
// #define NFC_MB_TEST_FRAME_MAX_LEN (NFC_MB_TEST_HEADER_LEN + NFC_MB_TEST_MAX_PAYLOAD + 1u)

// static uint8_t nfc_mb_test_checksum(const uint8_t *data, uint16_t len)
// {
//    uint8_t checksum = 0u;

//    for(uint16_t idx = 0u; idx < len; idx++)
//    {
//       checksum ^= data[idx];
//    }

//    return checksum;
// }

// static uint8_t nfc_mb_test_payload_seed(uint8_t msg_type)
// {
//    if(msg_type == NFC_MB_TEST_TYPE_PING)
//    {
//       return 0x11u;
//    }

//    if(msg_type == NFC_MB_TEST_TYPE_PONG)
//    {
//       return 0x77u;
//    }

//    return 0u;
// }

// static uint16_t nfc_mb_test_build_frame(uint8_t *buf, uint8_t msg_type, uint8_t seq, uint8_t payload_len)
// {
//    if(payload_len > NFC_MB_TEST_MAX_PAYLOAD)
//    {
//       payload_len = NFC_MB_TEST_MAX_PAYLOAD;
//    }

//    buf[0] = NFC_MB_TEST_MAGIC0;
//    buf[1] = NFC_MB_TEST_MAGIC1;
//    buf[2] = NFC_MB_TEST_VERSION;
//    buf[3] = msg_type;
//    buf[4] = seq;
//    buf[5] = payload_len;

//    const uint8_t seed = nfc_mb_test_payload_seed(msg_type);
//    for(uint8_t idx = 0u; idx < payload_len; idx++)
//    {
//       buf[NFC_MB_TEST_HEADER_LEN + idx] = (uint8_t)(seed + seq + (idx * 3u));
//    }

//    const uint16_t frame_len = (uint16_t)(NFC_MB_TEST_HEADER_LEN + payload_len + 1u);
//    buf[frame_len - 1u] = nfc_mb_test_checksum(buf, (uint16_t)(frame_len - 1u));

//    return frame_len;
// }

// static bool nfc_mb_test_parse_frame(const uint8_t *buf, uint8_t *msg_type, uint8_t *seq, uint8_t *payload_len)
// {
//    if((buf[0] != NFC_MB_TEST_MAGIC0) || (buf[1] != NFC_MB_TEST_MAGIC1) || (buf[2] != NFC_MB_TEST_VERSION))
//    {
//       return false;
//    }

//    const uint8_t type = buf[3];
//    if((type != NFC_MB_TEST_TYPE_PING) && (type != NFC_MB_TEST_TYPE_PONG))
//    {
//       return false;
//    }

//    const uint8_t length = buf[5];
//    if(length > NFC_MB_TEST_MAX_PAYLOAD)
//    {
//       return false;
//    }

//    const uint16_t frame_len = (uint16_t)(NFC_MB_TEST_HEADER_LEN + length + 1u);
//    const uint8_t expected_checksum = nfc_mb_test_checksum(buf, (uint16_t)(frame_len - 1u));
//    if(expected_checksum != buf[frame_len - 1u])
//    {
//       return false;
//    }

//    const uint8_t seed = nfc_mb_test_payload_seed(type);
//    for(uint8_t idx = 0u; idx < length; idx++)
//    {
//       const uint8_t expected = (uint8_t)(seed + buf[4] + (idx * 3u));
//       if(buf[NFC_MB_TEST_HEADER_LEN + idx] != expected)
//       {
//          return false;
//       }
//    }

//    if(msg_type != NULL)
//    {
//       *msg_type = type;
//    }

//    if(seq != NULL)
//    {
//       *seq = buf[4];
//    }

//    if(payload_len != NULL)
//    {
//       *payload_len = length;
//    }

//    return true;
// }

// static void __attribute__((unused)) nfc_sandbox_mb(void)
// {
//    feed_watchdog();
//    m_systick_period_ms = 20u;

//    result_t result = nfc_driver_init(&m_nfc, &m_i2c_driver_b.interface, &m_systick.interface);
//    if(IS_ERR(result))
//    {
//       SEGGER_RTT_printf(0, "nfc init failed. Code %d\n", GET_ERR_CODE(result));
//    }

//    result = m_nfc.interface.set_output_power(&m_nfc.interface, 12);
//    if(IS_ERR(result))
//    {
//       SEGGER_RTT_printf(0, "set_output_power failed. Unit %d, code %d\n", GET_ERR_UNIT(result),
//       GET_ERR_CODE(result));
//    }
//    result = m_nfc.interface.set_detection_type(&m_nfc.interface, NFC_TAG_TYPE_V);
//    if(IS_ERR(result))
//    {
//       SEGGER_RTT_printf(0, "set_detection_type failed. Unit %d, code %d\n", GET_ERR_UNIT(result),
//       GET_ERR_CODE(result));
//    }
//    result = m_nfc.interface.auto_tune_antenna(&m_nfc.interface);
//    if(IS_ERR(result))
//    {
//       SEGGER_RTT_printf(0, "auto_tune_antenna failed. Unit %d, code %d\n", GET_ERR_UNIT(result),
//       GET_ERR_CODE(result));
//    }

//    const uint32_t process_interval_ms = 20u;
//    const uint32_t poll_interval_ms = 50u;
//    const uint32_t send_interval_ms = 2000u;
//    const uint32_t busy_retry_ms = 200u;
//    const uint32_t response_timeout_ms = 2500u;
//    const uint32_t stats_interval_ms = 5000u;
//    const uint32_t heartbeat_ms = 1000u;

//    uint64_t now_ms = 0u;
//    uint64_t last_process_ms = 0u;
//    uint64_t last_poll_ms = 0u;
//    uint64_t next_send_ms = 0u;
//    uint64_t last_send_ms = 0u;
//    uint64_t last_stats_ms = 0u;
//    uint64_t last_heartbeat_ms = 0u;

//    uint8_t tx_seq = 0u;
//    uint8_t awaiting_seq = 0u;
//    bool awaiting_pong = false;

//    uint32_t tx_ok = 0u;
//    uint32_t tx_busy = 0u;
//    uint32_t tx_err = 0u;
//    uint32_t rx_ok = 0u;
//    uint32_t rx_invalid = 0u;
//    uint32_t ping_rx = 0u;
//    uint32_t pong_rx = 0u;
//    uint32_t pong_timeout = 0u;
//    uint32_t unexpected_pong = 0u;

//    uint8_t tx_buf[NFC_MB_TEST_FRAME_MAX_LEN] = {0};
//    uint8_t rx_buf[300] = {0};
//    uint8_t tag_uid[NFC_TAG_MAX_UID_SIZE] = {0};
//    uint8_t tag_uid_len = 0u;

//    bool is_ring_present = false;
//    bool last_ring_present = false;

//    nrf_gpio_pin_clear(LED_RED);
//    nrf_gpio_pin_clear(LED_GREEN);
//    nrf_gpio_pin_clear(LED_BLUE);

//    while(true)
//    {
//       (void)m_systick.interface.get_time_ms(&m_systick.interface, &now_ms);

//       if(now_ms - last_process_ms >= process_interval_ms)
//       {
//          last_process_ms = now_ms;
//          result = m_nfc.interface.process(&m_nfc.interface, true);
//          if(IS_ERR(result))
//          {
//             SEGGER_RTT_printf(0, "nfc process failed. Code %d\n", GET_ERR_CODE(result));
//          }

//          result = m_nfc.interface.is_ring_present(&m_nfc.interface, &is_ring_present);
//          if(IS_ERR(result))
//          {
//             SEGGER_RTT_printf(0, "nfc is_ring_present failed. Code %d\n", GET_ERR_CODE(result));
//          }
//       }

//       if(is_ring_present != last_ring_present)
//       {
//          last_ring_present = is_ring_present;
//          SEGGER_RTT_printf(0, "NFC ring present: %d\n", is_ring_present);
//          if(!is_ring_present)
//          {
//             awaiting_pong = false;
//             next_send_ms = now_ms + send_interval_ms;
//          }
//       }

//       if(is_ring_present && (now_ms - last_poll_ms >= poll_interval_ms))
//       {
//          last_poll_ms = now_ms;
//          memset(rx_buf, 0, sizeof(rx_buf));
//          result = m_nfc.data_ifc.get_packet(&m_nfc.data_ifc, rx_buf, sizeof(rx_buf));
//          if(IS_OK(result))
//          {
//             uint8_t rx_type = 0u;
//             uint8_t rx_seq = 0u;
//             uint8_t rx_payload_len = 0u;
//             if(nfc_mb_test_parse_frame(rx_buf, &rx_type, &rx_seq, &rx_payload_len))
//             {
//                rx_ok++;
//                if(rx_type == NFC_MB_TEST_TYPE_PING)
//                {
//                   ping_rx++;
//                   const uint16_t frame_len
//                      = nfc_mb_test_build_frame(tx_buf, NFC_MB_TEST_TYPE_PONG, rx_seq, rx_payload_len);
//                   result = m_nfc.data_ifc.send_packet(&m_nfc.data_ifc, tx_buf, frame_len);
//                   if(IS_OK(result))
//                   {
//                      tx_ok++;
//                   }
//                   else if(GET_ERR_CODE(result) == COMMS_DRIVER_ERROR_BUSY)
//                   {
//                      tx_busy++;
//                   }
//                   else
//                   {
//                      tx_err++;
//                      SEGGER_RTT_printf(
//                         0, "MB PONG send failed. Unit %d, code %d\n", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//                   }
//                }
//                else if(rx_type == NFC_MB_TEST_TYPE_PONG)
//                {
//                   pong_rx++;
//                   if(awaiting_pong && (rx_seq == awaiting_seq))
//                   {
//                      const uint32_t rtt_ms = (uint32_t)(now_ms - last_send_ms);
//                      SEGGER_RTT_printf(0, "MB PONG rx seq=%u rtt=%lu ms\n", rx_seq, (unsigned long)rtt_ms);
//                      awaiting_pong = false;
//                   }
//                   else
//                   {
//                      unexpected_pong++;
//                      SEGGER_RTT_printf(0, "MB unexpected PONG seq=%u (awaiting=%u)\n", rx_seq, awaiting_seq);
//                   }
//                }
//             }
//             else
//             {
//                rx_invalid++;
//             }
//          }
//          else if(GET_ERR_CODE(result) != COMMS_DRIVER_ERROR_BUSY)
//          {
//             SEGGER_RTT_printf(
//                0, "MB get_packet failed. Unit %d, code %d\n", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//          }
//       }

//       if(awaiting_pong && ((now_ms - last_send_ms) >= response_timeout_ms))
//       {
//          pong_timeout++;
//          SEGGER_RTT_printf(0, "MB PONG timeout seq=%u\n", awaiting_seq);
//          awaiting_pong = false;
//          next_send_ms = now_ms + busy_retry_ms;
//       }

//       if(is_ring_present && !awaiting_pong && (now_ms >= next_send_ms))
//       {
//          const uint8_t payload_len = (uint8_t)(8u + (tx_seq & 0x03u));
//          const uint16_t frame_len = nfc_mb_test_build_frame(tx_buf, NFC_MB_TEST_TYPE_PING, tx_seq, payload_len);
//          result = m_nfc.data_ifc.send_packet(&m_nfc.data_ifc, tx_buf, frame_len);
//          if(IS_OK(result))
//          {
//             SEGGER_RTT_printf(0, "MB PING tx seq=%u len=%u\n", tx_seq, payload_len);
//             awaiting_pong = true;
//             awaiting_seq = tx_seq;
//             last_send_ms = now_ms;
//             tx_seq = (uint8_t)(tx_seq + 1u);
//             tx_ok++;
//             next_send_ms = now_ms + send_interval_ms;
//          }
//          else if(GET_ERR_CODE(result) == COMMS_DRIVER_ERROR_BUSY)
//          {
//             tx_busy++;
//             next_send_ms = now_ms + busy_retry_ms;
//          }
//          else
//          {
//             tx_err++;
//             SEGGER_RTT_printf(0, "MB PING send failed. Unit %d, code %d\n", GET_ERR_UNIT(result),
//             GET_ERR_CODE(result)); next_send_ms = now_ms + send_interval_ms;
//          }
//       }

//       if(now_ms - last_heartbeat_ms >= heartbeat_ms)
//       {
//          last_heartbeat_ms = now_ms;
//          if(is_ring_present)
//          {
//             nrf_gpio_pin_toggle(LED_GREEN);
//             nrf_gpio_pin_clear(LED_RED);
//          }
//          else
//          {
//             nrf_gpio_pin_set(LED_RED);
//             nrf_gpio_pin_clear(LED_GREEN);
//          }
//       }

//       if(now_ms - last_stats_ms >= stats_interval_ms)
//       {
//          last_stats_ms = now_ms;
//          SEGGER_RTT_printf(0,
//                            "MB stats: present=%d tx_ok=%lu tx_busy=%lu tx_err=%lu rx_ok=%lu rx_invalid=%lu
//                            ping_rx=%lu " "pong_rx=%lu timeout=%lu\n", is_ring_present, (unsigned long)tx_ok,
//                            (unsigned long)tx_busy,
//                            (unsigned long)tx_err,
//                            (unsigned long)rx_ok,
//                            (unsigned long)rx_invalid,
//                            (unsigned long)ping_rx,
//                            (unsigned long)pong_rx,
//                            (unsigned long)pong_timeout);

//          if(is_ring_present)
//          {
//             memset(tag_uid, 0, sizeof(tag_uid));
//             tag_uid_len = 0u;
//             result = m_nfc.interface.get_tag_uid(&m_nfc.interface, tag_uid, &tag_uid_len);
//             if(IS_OK(result) && (tag_uid_len > 0u))
//             {
//                SEGGER_RTT_printf(0, "NFC UID (%uB): ", tag_uid_len);
//                for(uint8_t idx = 0u; idx < tag_uid_len; idx++)
//                {
//                   SEGGER_RTT_printf(0, "%02X", tag_uid[idx]);
//                }
//                SEGGER_RTT_printf(0, "\n");
//             }
//             else if(IS_ERR(result))
//             {
//                SEGGER_RTT_printf(
//                   0, "get_tag_uid failed. Unit %d, code %d\n", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//             }
//          }
//       }

//       feed_watchdog();
//       idle_state_handle();
//    }
// }

// // Test detection of different types of tags
// static void __attribute__((unused)) nfc_sandbox3(void)
// {
//    DEBUG_INFO("Starting sandbox3 - Type A or V detection");
//    uint64_t current_ms = 0;
//    uint64_t prev_ms = 0;
//    uint64_t prev_report_ms = 0;
//    NFC_TAG_TYPE tag_type = NFC_TAG_TYPE_V;
//    result_t result = RESULT_OK;

//    // Init NFC reader
//    result = nfc_driver_init(&m_nfc, &m_i2c_driver_b.interface, &m_systick.interface);
//    if(IS_ERR(result))
//    {
//       DEBUG_ERROR("nfc init failed. Code %d", GET_ERR_CODE(result));
//    }

//    (void)m_nfc.interface.set_detection_type(&m_nfc.interface, tag_type);

//    result = m_nfc.interface.set_detection_type(&m_nfc.interface, tag_type);

//    if(IS_ERR(result))
//    {
//       DEBUG_ERROR("Error: unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//    }

//    result = m_nfc.interface.set_output_power(&m_nfc.interface, 12);

//    result = m_nfc.interface.auto_tune_antenna(&m_nfc.interface);

//    /* if(IS_ERR(result))
//    {
//       DEBUG_ERROR("Error: unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//    } */

//    while(true)
//    {
//       (void)m_systick.interface.get_time_ms(&m_systick.interface, &current_ms);
//       if(current_ms - prev_ms > 20u)
//       {
//          prev_ms = current_ms;

//          result = m_nfc.interface.process(&m_nfc.interface, true);
//          if(IS_ERR(result))
//          {
//             DEBUG_ERROR("Error: unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//          }
//       }

//       /*  if(current_ms - prev_switch_ms > 5000u)
//        {
//           prev_switch_ms = current_ms;

//           // Toggle between tag types
//           if(NFC_TAG_TYPE_A == tag_type)
//           {
//              tag_type = NFC_TAG_TYPE_V;
//           }
//           else
//           {
//              tag_type = NFC_TAG_TYPE_A;
//           }

//           // Set tag type
//           result = m_nfc.interface.set_detection_type(&m_nfc.interface, tag_type);

//           if(IS_ERR(result))
//           {
//              DEBUG_ERROR("Error: unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//           }
//        } */

//       // Report presence/UID every 1s for current tag type
//       if(current_ms - prev_report_ms > 1000u)
//       {
//          prev_report_ms = current_ms;
//          bool present = false;
//          result = m_nfc.interface.is_ring_present(&m_nfc.interface, &present);
//          if(IS_ERR(result))
//          {
//             DEBUG_ERROR("is_ring_present error: unit %d code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//          }
//          else if(!present)
//          {
//             DEBUG_INFO("Tag type %s not found", (tag_type == NFC_TAG_TYPE_A) ? "A" : "V");
//          }
//          else
//          {
//             uint8_t uid[16] = {0};
//             uint8_t uid_len = sizeof(uid);
//             result = m_nfc.interface.get_tag_uid(&m_nfc.interface, uid, &uid_len);
//             if(IS_OK(result))
//             {
//                SEGGER_RTT_printf(0, "Tag type %s UID (%uB): ", (tag_type == NFC_TAG_TYPE_A) ? "A" : "V", uid_len);
//                for(uint8_t i = 0; i < uid_len; i++)
//                {
//                   SEGGER_RTT_printf(0, "%02X", uid[i]);
//                }
//                SEGGER_RTT_printf(0, "\n");
//             }
//             else
//             {
//                DEBUG_ERROR("get_tag_uid error: unit %d code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//             }
//          }
//       }
//       feed_watchdog();
//       idle_state_handle();
//    }
// }

// // HW test: Low power modes
// static void __attribute__((unused)) hw_test_low_power(void)
// {
//    DEBUG_INFO("Starting HW test: Low power");
//    uint64_t current_ms = 0;
//    uint64_t prev_ms = 0;
//    uint64_t prev_switch_ms = 0;
//    uint64_t prev_report_ms = 0;
//    NFC_TAG_TYPE tag_type = NFC_TAG_TYPE_A;
//    result_t result = RESULT_OK;
//    static battery_status_t battery_status = {0};
//    static pmic_status_t pmic_status = {0};
//    uint64_t current_systic_time_ms = 0;
//    static uint64_t prev_bat_blink_time_ms = 0;
//    static uint64_t prev_vbat_report_time_ms = 0;
//    static uint64_t prev_step_ms = 0;
//    bool toggle = false;

//    // Set system apptimer
//    m_systick_period_ms = 2000u;

//    // Turn off ADC
//    nrf_gpio_pin_clear(ADS1235_5V_EN_PIN); // Turn off by default
//    nrf_gpio_pin_clear(LEDDRV_SD1);        // Disable HMI
//    nrf_gpio_pin_clear(LEDDRV_SD2);        // Disable HMI

//    nrf_gpio_pin_clear(FLASH_NRST_PIN);
//    nrf_gpio_pin_clear(FLASH_WP_PIN);
//    // nrf_gpio_pin_clear(I2C_PULLUP); // Disable I2C Pullup
//    //   nrf_gpio_pin_clear(NFC_ANT_B_EN); // Disable NFC ANT_B select
//    //   nrf_gpio_pin_clear(NFC_BSS_EN);  // Disable NFC BSS pin

//    (void)m_battery.interface.power_flash_memory_and_load_cell(&m_battery.interface, false);
//    // (void)m_battery.interface.power_buzzer_leds_nfc(&m_battery.interface, false);
//    // m_i2c_driver_b.interface.disable_twi(&m_i2c_driver_b.interface);

//    result = nfc_driver_init(&m_nfc, &m_i2c_driver_b.interface, &m_systick.interface);
//    if(IS_ERR(result))
//    {
//       SEGGER_RTT_printf(0, "nfc init failed. Code %d\n", GET_ERR_CODE(result));
//    }

//    nrf_delay_ms(1000);

//    while(true)
//    {
//       (void)m_systick.interface.get_time_ms(&m_systick.interface, &current_systic_time_ms);

//       if(current_systic_time_ms - prev_step_ms > 5000u)
//       {
//          prev_step_ms = current_systic_time_ms;
//          if(toggle)
//          {
//             result = m_nfc.interface.chip_power_down(&m_nfc.interface);
//             ON_ERR_DEBUG_ERROR(
//                result, "Failed to power down NFC chip. Unit %d, Error: %d", GET_ERR_UNIT(result),
//                GET_ERR_CODE(result));
//             toggle = false;
//          }
//          else
//          {
//             result = m_nfc.interface.chip_power_up(&m_nfc.interface);
//             ON_ERR_DEBUG_ERROR(
//                result, "Failed to power up NFC chip. Unit %d, Error: %d", GET_ERR_UNIT(result),
//                GET_ERR_CODE(result));
//             result = m_nfc.interface.field_on_and_start_gt(&m_nfc.interface);
//             ON_ERR_DEBUG_ERROR(
//                result, "Failed to turn NFC field on. Unit %d, Error: %d", GET_ERR_UNIT(result),
//                GET_ERR_CODE(result));

//             result = m_nfc.interface.set_output_power(&m_nfc.interface, 10);
//             toggle = true;
//          }
//       }

//       if(current_systic_time_ms - prev_vbat_report_time_ms > 5000u)
//       {
//          prev_vbat_report_time_ms = current_systic_time_ms;
//          // Get battery data
//          result = m_battery.interface.enable_auto_measurements(&m_battery.interface);
//          result = m_battery.interface.get_battery_status(&m_battery.interface, &battery_status);
//          result = m_pmic.interface.get_pmic_status(&m_pmic.interface, &pmic_status);
//          result = m_battery.interface.disable_auto_measurements(&m_battery.interface);

//          DEBUG_INFO("Vbat = %d mV, Level = %d, Chgr = %d, ChrgOK = %d, Chrging = %d, ChrgDN = %d",
//                     battery_status.battery_voltage_mv,
//                     battery_status.battery_level,
//                     battery_status.charger_connected,
//                     pmic_status.charger_good,
//                     pmic_status.charging,
//                     pmic_status.charging_completed);

//          if(battery_status.charger_connected)
//          {
//             nrf_gpio_pin_set(LED_GREEN);
//             nrf_delay_ms(50);
//             nrf_gpio_pin_clear(LED_GREEN);
//          }
//          else
//          {
//             nrf_gpio_pin_set(LED_BLUE);
//             nrf_delay_ms(50);
//             nrf_gpio_pin_clear(LED_BLUE);
//          }
//       }
//       nrf_gpio_pin_set(LED_RED);
//       nrf_delay_ms(50);
//       nrf_gpio_pin_clear(LED_RED);
//       feed_watchdog();
//       idle_state_handle();
//    }
// }

// static void __attribute__((unused)) nfc_sandbox_aat(void)
// {
//    DEBUG_INFO("Starting sandbox - AAT");
//    uint64_t current_ms = 0;
//    uint64_t prev_ms = 0;

//    result_t result = RESULT_OK;

//    // Init NFC reader
//    result = nfc_driver_init(&m_nfc, &m_i2c_driver_b.interface, &m_systick.interface);
//    if(IS_ERR(result))
//    {
//       DEBUG_ERROR("nfc init failed. Code %d", GET_ERR_CODE(result));
//    }

//    if(IS_OK(result))
//    {
//       result = m_nfc.interface.auto_tune_antenna(&m_nfc.interface);
//    }

//    if(IS_ERR(result))
//    {
//       DEBUG_ERROR("Error: unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//    }

//    while(true)
//    {
//       nrf_delay_ms(1000);
//       nrf_gpio_pin_set(LED_GREEN);
//       nrf_delay_ms(50);
//       nrf_gpio_pin_clear(LED_GREEN);
//       feed_watchdog();
//       idle_state_handle();
//    }
// }

// static void test_data_manager_fds_sandbox(void)
// {
//    // WARNING NOTE: This sandbox is intentionally designed to minimize flash wear.
//    // It only writes when stored data differs from the expected test values.
//    // Do not add loops or repeated writes here; modify with caution to avoid
//    // exhausting the limited write-cycle endurance of internal flash.
//    SEGGER_RTT_printf(0, "\n[DM_FDS] Starting data manager FDS sandbox test.\n");

//    const dock_data_manager_interface_t *dm = &m_dock_data_manager.interface;

//    // Expected test data (used to verify persistence across reset).
//    const weight_stack_calibration_record_t expected_cal = {
//       .zero_offset = 1234,
//       .calibration_factor = 5678,
//       .full_assembly_weight_mg = 91011,
//    };

//    const dose_schedule_t expected_schedule = {
//       .dosage_amount = 2,
//       .events_per_day = 3,
//       .temperature_threshold_deg_c = 25,
//       .temperature_avg_time_window_minutes = 30,
//       .window = {
//          {.start_min = 60, .duration = 30},
//          {.start_min = 300, .duration = 45},
//          {.start_min = 900, .duration = 60},
//          {0},
//          {0},
//          {0},
//       },
//    };

//    const int32_t expected_total_weight = 123456;

//    bool needs_reset_check = false;

//    // --------------------------- Weight calibration record ---------------------------
//    weight_stack_calibration_record_t stored_cal = {0};
//    result_t result = dm->get_weight_calibration_record(dm, &stored_cal);
//    if(IS_OK(result))
//    {
//       if(0 == memcmp(&stored_cal, &expected_cal, sizeof(stored_cal)))
//       {
//          SEGGER_RTT_printf(0, "[DM_FDS] Weight calibration: PASS (persisted).\n");
//       }
//       else
//       {
//          SEGGER_RTT_printf(0, "[DM_FDS] Weight calibration: INFO (mismatch, writing test data).\n");
//          result = dm->set_weight_calibration_record(dm, (weight_stack_calibration_record_t *)&expected_cal);
//          if(IS_OK(result))
//          {
//             weight_stack_calibration_record_t verify_cal = {0};
//             result = dm->get_weight_calibration_record(dm, &verify_cal);
//             if(IS_OK(result) && (0 == memcmp(&verify_cal, &expected_cal, sizeof(verify_cal))))
//             {
//                SEGGER_RTT_printf(0, "[DM_FDS] Weight calibration: PASS (set/get).\n");
//                needs_reset_check = true;
//             }
//             else
//             {
//                SEGGER_RTT_printf(0, "[DM_FDS] Weight calibration: FAIL (verify).\n");
//             }
//          }
//          else
//          {
//             SEGGER_RTT_printf(0,
//                               "[DM_FDS] Weight calibration: FAIL (set). Unit %d, Error %d\n",
//                               GET_ERR_UNIT(result),
//                               GET_ERR_CODE(result));
//          }
//       }
//    }
//    else
//    {
//       SEGGER_RTT_printf(
//          0, "[DM_FDS] Weight calibration: FAIL (get). Unit %d, Error %d\n", GET_ERR_UNIT(result),
//          GET_ERR_CODE(result));
//    }

//    // ------------------------------- Dose schedule ----------------------------------
//    dose_schedule_t stored_schedule = {0};
//    result = dm->get_dose_schedule(dm, &stored_schedule);
//    if(IS_OK(result))
//    {
//       if(0 == memcmp(&stored_schedule, &expected_schedule, sizeof(stored_schedule)))
//       {
//          SEGGER_RTT_printf(0, "[DM_FDS] Dose schedule: PASS (persisted).\n");
//       }
//       else
//       {
//          SEGGER_RTT_printf(0, "[DM_FDS] Dose schedule: INFO (mismatch, writing test data).\n");
//          result = dm->set_dose_schedule(dm, (dose_schedule_t *)&expected_schedule);
//          if(IS_OK(result))
//          {
//             dose_schedule_t verify_schedule = {0};
//             result = dm->get_dose_schedule(dm, &verify_schedule);
//             if(IS_OK(result) && (0 == memcmp(&verify_schedule, &expected_schedule, sizeof(verify_schedule))))
//             {
//                SEGGER_RTT_printf(0, "[DM_FDS] Dose schedule: PASS (set/get).\n");
//                needs_reset_check = true;
//             }
//             else
//             {
//                SEGGER_RTT_printf(0, "[DM_FDS] Dose schedule: FAIL (verify).\n");
//             }
//          }
//          else
//          {
//             SEGGER_RTT_printf(0,
//                               "[DM_FDS] Dose schedule: FAIL (set). Unit %d, Error %d\n",
//                               GET_ERR_UNIT(result),
//                               GET_ERR_CODE(result));
//          }
//       }
//    }
//    else
//    {
//       SEGGER_RTT_printf(
//          0, "[DM_FDS] Dose schedule: FAIL (get). Unit %d, Error %d\n", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//    }

//    // ---------------------------- Total weight dispensed ----------------------------
//    int32_t stored_total_weight = 0;
//    result = dm->get_total_weight_dispensed_mg(dm, &stored_total_weight);
//    if(IS_OK(result))
//    {
//       if(stored_total_weight == expected_total_weight)
//       {
//          SEGGER_RTT_printf(0, "[DM_FDS] Total weight: PASS (persisted).\n");
//       }
//       else
//       {
//          SEGGER_RTT_printf(0, "[DM_FDS] Total weight: INFO (mismatch, writing test data).\n");
//          result = dm->set_total_weight_dispensed_mg(dm, expected_total_weight);
//          if(IS_OK(result))
//          {
//             int32_t verify_total_weight = 0;
//             result = dm->get_total_weight_dispensed_mg(dm, &verify_total_weight);
//             if(IS_OK(result) && (verify_total_weight == expected_total_weight))
//             {
//                SEGGER_RTT_printf(0, "[DM_FDS] Total weight: PASS (set/get).\n");
//                needs_reset_check = true;
//             }
//             else
//             {
//                SEGGER_RTT_printf(0, "[DM_FDS] Total weight: FAIL (verify).\n");
//             }
//          }
//          else
//          {
//             SEGGER_RTT_printf(
//                0, "[DM_FDS] Total weight: FAIL (set). Unit %d, Error %d\n", GET_ERR_UNIT(result),
//                GET_ERR_CODE(result));
//          }
//       }
//    }
//    else
//    {
//       SEGGER_RTT_printf(
//          0, "[DM_FDS] Total weight: FAIL (get). Unit %d, Error %d\n", GET_ERR_UNIT(result), GET_ERR_CODE(result));
//    }

//    if(needs_reset_check)
//    {
//       SEGGER_RTT_printf(0, "[DM_FDS] Reset to verify persistence of test data.\n");
//    }
//    else
//    {
//       SEGGER_RTT_printf(0, "[DM_FDS] Persistence already verified or no writes performed.\n");
//    }

//    while(true)
//    {
//       feed_watchdog();
//       idle_state_handle();
//    }
// }

// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // SANDBOX FUNCTIONS - For development and testing purposes only. Not used in production firmware. //
// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // static void __attribute__((unused)) nfc_sandbox_mb(void)
// // {
// //    feed_watchdog();
// //    m_systick_period_ms = 20u;

// // //    result_t result = nfc_driver_init(&m_nfc, &m_i2c_driver_b.interface, &m_systick.interface);
// // //    if(IS_ERR(result))
// // //    {
// // //       DEBUG_ERROR("nfc init failed. Code %d", GET_ERR_CODE(result));
// // //    }

// //    result = m_nfc.interface.set_output_power(&m_nfc.interface, 12);
// //    result = m_nfc.interface.set_detection_type(&m_nfc.interface, NFC_TAG_TYPE_V);
// //    // result = m_nfc.interface.auto_tune_antenna(&m_nfc.interface);

// //    uint64_t current_ms = 0;
// //    uint64_t prev_ms = 0;
// //    uint64_t prev_read_ms = 0;
// //    uint64_t prev_write_ms = 0;

// //    static uint8_t count = 0;
// //    static uint8_t demoMsg[6] = {0};
// //    uint8_t last_sent_count = 0;
// //    bool awaiting_response = false;
// //    uint64_t last_send_ms = 0;

// // //    uint8_t rx_data[300] = {0};
// // //    uint8_t rx_data_len = 0;

// // //    bool is_ring_present = false;

// //    bool led_state = false;
// //    nrf_gpio_pin_clear(LED_GREEN);

// //    while(true)
// //    {
// //       (void)m_systick.interface.get_time_ms(&m_systick.interface, &current_ms);
// //       if(current_ms - prev_ms > 20u)
// //       {
// //          prev_ms = current_ms;
// //          result = m_nfc.interface.process(&m_nfc.interface, true);
// //          if(IS_ERR(result))
// //          {
// //             DEBUG_ERROR("nfc process failed. Code %d", GET_ERR_CODE(result));
// //          }

// // //          result = m_nfc.interface.is_ring_present(&m_nfc.interface, &is_ring_present);
// // //          if(IS_ERR(result))
// // //          {
// // //             DEBUG_ERROR("nfc is_ring_present failed. Code %d", GET_ERR_CODE(result));
// // //          }
// // //       }

// //       // Test reading data
// //       if(current_ms - prev_read_ms > 1000u)
// //       {
// //          prev_read_ms = current_ms;
// //          bool read_ok = false;
// //          rx_data_len = 0;
// //          if(is_ring_present)
// //          {
// //             result = m_nfc.data_ifc.get_packet(&m_nfc.data_ifc, rx_data, sizeof(rx_data));
// //             if(IS_OK(result))
// //             {
// //                read_ok = true;
// //                SEGGER_RTT_printf(0, "MB READ (%uB): ", rx_data_len);
// //                for(uint8_t i = 0; i < rx_data_len; i++)
// //                   SEGGER_RTT_printf(0, "%02X", rx_data[i]);
// //                SEGGER_RTT_printf(0, "\n");
// //             }
// //             else if(NFC_R_ERROR_BUSY != GET_ERR_CODE(result))
// //             {
// //                SEGGER_RTT_printf(0, ".");
// //             }
// //             else if(NFC_R_ERROR_NO_TAG != GET_ERR_CODE(result))
// //             {
// //                DEBUG_ERROR("Failed to read ring data. Err %d", GET_ERR_CODE(result));
// //             }
// //          }
// //          // Toggle LED to indicate alive
// //          if(led_state)
// //          {
// //             nrf_gpio_pin_clear(LED_GREEN);
// //             led_state = false;
// //          }
// //          else
// //          {
// //             nrf_gpio_pin_set(LED_GREEN);
// //             led_state = true;
// //          }

// //          // Check for mailbox response and validate contents from this read only
// //          if(read_ok && (rx_data_len >= 3u) && (rx_data[0] == 0xAB))
// //          {
// //             const uint8_t echoed = rx_data[1];
// //             const uint8_t incremented = rx_data[2];
// //             const uint8_t expected_inc = (uint8_t)(echoed + 1u);

// //             if(!awaiting_response)
// //             {
// //                DEBUG_WARNING("Mailbox response received without pending send.");
// //             }
// //             else if((echoed == last_sent_count) && (incremented == expected_inc))
// //             {
// //                DEBUG_INFO("Mailbox loop PASS: echoed=%u incremented=%u", echoed, incremented);
// //                awaiting_response = false;
// //             }
// //             else
// //             {
// //                DEBUG_ERROR("Mailbox loop FAIL: expected echo=%u inc=%u got echo=%u inc=%u",
// //                            last_sent_count,
// //                            (uint8_t)(last_sent_count + 1u),
// //                            echoed,
// //                            incremented);
// //                awaiting_response = false;
// //             }
// //          }
// //       }

// //       // Test writing data
// //       if(!awaiting_response && (current_ms - prev_write_ms > 5000u))
// //       {
// //          prev_write_ms = current_ms;

// //          uint8_t next_count = (count == UINT8_MAX) ? 0u : (uint8_t)(count + 1u);
// //          demoMsg[0] = 0xAA;
// //          demoMsg[1] = next_count;
// //          demoMsg[2] = 7;
// //          demoMsg[3] = 8;
// //          demoMsg[4] = 9;
// //          demoMsg[5] = 10;

// //          result = m_nfc.data_ifc.send_packet(&m_nfc.data_ifc, demoMsg, sizeof(demoMsg));
// //          if(IS_OK(result))
// //          {
// //             count = next_count;
// //             last_sent_count = count;
// //             awaiting_response = true;
// //             last_send_ms = current_ms;
// //             DEBUG_INFO("Successful data write to NFC.");
// //          }
// //          else if(NFC_R_ERROR_BUSY == GET_ERR_CODE(result))
// //          {
// //             SEGGER_RTT_printf(0, ".");
// //          }
// //          else if(NFC_R_ERROR_NO_TAG != GET_ERR_CODE(result))
// //          {
// //             SEGGER_RTT_printf(0, "Failed to write data to NFC. Err %d", GET_ERR_CODE(result));
// //          }
// //       }

// //       // Timeout waiting for response
// //       if(awaiting_response && (current_ms - last_send_ms > 10000u))
// //       {
// //          DEBUG_WARNING("Mailbox response timeout for count %u", last_sent_count);
// //          awaiting_response = false;
// //       }

// //       feed_watchdog();
// //       idle_state_handle();
// //    }
// // }

// // void basic_ble_sandbox(void)
// // {
// //    SEGGER_RTT_printf(0, "\n\nStarting BLE sandbox test\n");

// //    // Assume m_ble_control is a global or accessible instance
// //    ble_control_interface_t *ble_ifc = &m_ble_control.interface;
// //    comms_driver_interface_t *comms_ifc = &m_ble_control.data_ifc;

// //    bool bond = true; // Set to true to test bonding

// //    result_t result;
// //    ble_control_status_t status = {0};

// //    // Test: Get BLE status
// //    result = ble_ifc->get_ble_status(ble_ifc, &status);
// //    SEGGER_RTT_printf(0,
// //                      "BLE status:\n bonded=%d\n connected=%d\n advertising=%d\n allow_new_bond=%d\n
// //                      overflow=%d\n", status.is_bonded, status.is_connected, status.is_advertising,
// //                      status.allow_new_bond,
// //                      status.received_data_overflow);

// //    // Test: Start advertising (allow new bond)
// //    if(bond)
// //    {
// //       uint8_t decrypted_passkey_text[64] = {0x00};
// //       result = load_ble_passkey((char *)decrypted_passkey_text);

// //       if(IS_ERR(result))
// //       {
// //          DEBUG_ERROR("Failed to load BLE passkey!");
// //       }

// //       // Start advertising
// //       result = ble_ifc->start_advertising(ble_ifc, true, (char *)decrypted_passkey_text);
// //       memset(decrypted_passkey_text, 0, 64);
// //    }
// //    else
// //    {
// //       result = ble_ifc->start_advertising(ble_ifc, false, NULL);
// //    }

// //    SEGGER_RTT_printf(0, "Start advertising result: unit: %d, err: %d\n", GET_ERR_UNIT(result),
// //    GET_ERR_CODE(result));

// //    // Simulate connection event (would normally be handled by BLE stack)
// //    // For sandbox, just check status again
// //    result = ble_ifc->get_ble_status(ble_ifc, &status);
// //    SEGGER_RTT_printf(0, "After advertising, BLE status: connected=%d\n", status.is_connected);

// //    while(!status.is_connected && !status.is_bonded)
// //    {
// //       result = ble_ifc->get_ble_status(ble_ifc, &status);
// //       feed_watchdog();
// //       nrf_delay_ms(250);
// //    }

// //    SEGGER_RTT_printf(0, "Successfully connected via BLE!\n");

// //    // Continuous loop: send incrementing data every 5s, print any received data
// //    uint8_t tx_data[NRF_SDH_BLE_GATT_MAX_MTU_SIZE - 3u] = {0}; // Max size is MTU-3 (Opcode + Handle = 3 bytes) =
// //    {0}; for(uint8_t i = 0; i < sizeof(tx_data); i++)
// //    {
// //       tx_data[i] = i;
// //    }
// //    uint8_t counter = 0;
// //    uint64_t last_send_time = 0;
// //    uint64_t current_time = 0;
// //    uint8_t rx_data[BLE_RECEIVE_DATA_BUFFER_SIZE - 3u] = {0}; // Max size is MTU-3 (Opcode + Handle = 3 bytes)
// //    uint8_t rx_len = 0;

// //    (void)m_systick.interface.get_time_ms(&m_systick.interface, &last_send_time);

// //    while(1)
// //    {
// //       (void)m_debug_uart_driver_interface->process(&m_debug_uart_driver.interface);
// //       // Send data every T seconds
// //       (void)m_systick.interface.get_time_ms(&m_systick.interface, &current_time);
// //       if(current_time - last_send_time >= (15 * 1000))
// //       {
// //          /* // Update data with incrementing counter
// //          for(uint8_t i = 0; i < sizeof(tx_data); i++)
// //          {
// //             tx_data[i] = counter + i;
// //          }
// //          counter++; */
// //          SEGGER_RTT_printf(0, "Sending data (counter=%d)...\n", counter);
// //          result = comms_ifc->send_packet(comms_ifc, tx_data, sizeof(tx_data));
// //          SEGGER_RTT_printf(0, "Send data result: %d\n", result);
// //          last_send_time = current_time;
// //          // Dequeue and print any dock debug logs
// //          raw_debug_log_t debug_log = {0};
// //          m_dock_data_manager.interface.dequeue_dock_debug_log(&debug_log);

// //          SEGGER_RTT_printf(0,
// //                            "Dock Debug Log: Level: %d timestamp: %u module id: %d line %d \n",
// //                            debug_log.level,
// //                            debug_log.timestamp,
// //                            debug_log.module_id,
// //                            debug_log.line);
// //       }

// //       // Check for received data
// //       rx_len = BLE_RX_PACKET_ELEMENT_SIZE;
// //       result = comms_ifc->get_packet(comms_ifc, rx_data, sizeof(rx_data));

// //       if(IS_ERR(result))
// //       {
// //          if(COMMS_DRIVER_ERROR_BUSY == GET_ERR_CODE(result))
// //          {
// //             SEGGER_RTT_printf(0, ".");
// //          }
// //          else
// //          {
// //             SEGGER_RTT_printf(0, "Error. Unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
// //          }
// //       }
// //       else
// //       {
// //          if((rx_len > 0) && IS_OK(result))
// //          {
// //             SEGGER_RTT_printf(0, "\nReceived data: ");
// //             for(uint8_t i = 0; i < rx_len; i++)
// //             {
// //                SEGGER_RTT_printf(0, "%02X ", rx_data[i]);
// //             }
// //             SEGGER_RTT_printf(0, "\n");
// //          }
// //       }

// //       feed_watchdog();
// //       nrf_delay_ms(100);
// //    }

// //    // Test: Stop advertising (disconnect)
// //    result = ble_ifc->stop_advertising(ble_ifc);
// //    SEGGER_RTT_printf(0, "Stop advertising result: %d\n", result);

// //    // Final status
// //    result = ble_ifc->get_ble_status(ble_ifc, &status);
// //    SEGGER_RTT_printf(0, "Final BLE status: connected=%d, advertising=%d\n", status.is_connected,
// //    status.is_advertising);
// // }

// // // Test detection of different types of tags
// // static void __attribute__((unused)) nfc_sandbox3(void)
// // {
// //    DEBUG_INFO("Starting sandbox3 - Type A or V detection");
// //    uint64_t current_ms = 0;
// //    uint64_t prev_ms = 0;
// //    uint64_t prev_switch_ms = 0;
// //    uint64_t prev_report_ms = 0;
// //    NFC_TAG_TYPE tag_type = NFC_TAG_TYPE_V;
// //    result_t result = RESULT_OK;

// //    // Init NFC reader
// //    result = nfc_driver_init(&m_nfc, &m_i2c_driver_b.interface, &m_systick.interface);
// //    if(IS_ERR(result))
// //    {
// //       DEBUG_ERROR("nfc init failed. Code %d", GET_ERR_CODE(result));
// //    }

// //    (void)m_nfc.interface.set_detection_type(&m_nfc.interface, tag_type);

// //    result = m_nfc.interface.set_detection_type(&m_nfc.interface, tag_type);

// //    if(IS_ERR(result))
// //    {
// //       DEBUG_ERROR("Error: unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
// //    }

// //    result = m_nfc.interface.set_output_power(&m_nfc.interface, 12);

// //    result = m_nfc.interface.auto_tune_antenna(&m_nfc.interface);

// //    /* if(IS_ERR(result))
// //    {
// //       DEBUG_ERROR("Error: unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
// //    } */

// //    while(true)
// //    {
// //       (void)m_systick.interface.get_time_ms(&m_systick.interface, &current_ms);
// //       if(current_ms - prev_ms > 20u)
// //       {
// //          prev_ms = current_ms;

// //          result = m_nfc.interface.process(&m_nfc.interface, true);
// //          if(IS_ERR(result))
// //          {
// //             DEBUG_ERROR("Error: unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
// //          }
// //       }

// //       /*  if(current_ms - prev_switch_ms > 5000u)
// //        {
// //           prev_switch_ms = current_ms;

// //           // Toggle between tag types
// //           if(NFC_TAG_TYPE_A == tag_type)
// //           {
// //              tag_type = NFC_TAG_TYPE_V;
// //           }
// //           else
// //           {
// //              tag_type = NFC_TAG_TYPE_A;
// //           }

// //           // Set tag type
// //           result = m_nfc.interface.set_detection_type(&m_nfc.interface, tag_type);

// //           if(IS_ERR(result))
// //           {
// //              DEBUG_ERROR("Error: unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
// //           }
// //        } */

// //       // Report presence/UID every 1s for current tag type
// //       if(current_ms - prev_report_ms > 1000u)
// //       {
// //          prev_report_ms = current_ms;
// //          bool present = false;
// //          result = m_nfc.interface.is_ring_present(&m_nfc.interface, &present);
// //          if(IS_ERR(result))
// //          {
// //             DEBUG_ERROR("is_ring_present error: unit %d code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
// //          }
// //          else if(!present)
// //          {
// //             DEBUG_INFO("Tag type %s not found", (tag_type == NFC_TAG_TYPE_A) ? "A" : "V");
// //          }
// //          else
// //          {
// //             uint8_t uid[16] = {0};
// //             uint8_t uid_len = sizeof(uid);
// //             result = m_nfc.interface.get_tag_uid(&m_nfc.interface, uid, &uid_len);
// //             if(IS_OK(result))
// //             {
// //                SEGGER_RTT_printf(0, "Tag type %s UID (%uB): ", (tag_type == NFC_TAG_TYPE_A) ? "A" : "V", uid_len);
// //                for(uint8_t i = 0; i < uid_len; i++)
// //                {
// //                   SEGGER_RTT_printf(0, "%02X", uid[i]);
// //                }
// //                SEGGER_RTT_printf(0, "\n");
// //             }
// //             else
// //             {
// //                DEBUG_ERROR("get_tag_uid error: unit %d code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
// //             }
// //          }
// //       }
// //       feed_watchdog();
// //       idle_state_handle();
// //    }
// // }

// // static void __attribute__((unused)) nfc_sandbox_aat(void)
// // {
// //    DEBUG_INFO("Starting sandbox - AAT");
// //    uint64_t current_ms = 0;
// //    uint64_t prev_ms = 0;

// //    result_t result = RESULT_OK;

// //    // Init NFC reader
// //    result = nfc_driver_init(&m_nfc, &m_i2c_driver_b.interface, &m_systick.interface);
// //    if(IS_ERR(result))
// //    {
// //       DEBUG_ERROR("nfc init failed. Code %d", GET_ERR_CODE(result));
// //    }

// //    if(IS_OK(result))
// //    {
// //       result = m_nfc.interface.auto_tune_antenna(&m_nfc.interface);
// //    }

// //    if(IS_ERR(result))
// //    {
// //       DEBUG_ERROR("Error: unit %d, code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
// //    }

// //    while(true)
// //    {
// //       nrf_delay_ms(1000);
// //       feed_watchdog();
// //       idle_state_handle();
// //    }
// // }

// // // Ramp NFC field power 0..15 in steps every 10s and repeat indefinitely.
// // static void __attribute__((unused)) nfc_power_ramp_sandbox(void)
// // {
// //    DEBUG_INFO("Starting NFC power ramp sandbox");
// //    uint64_t current_ms = 0;
// //    uint64_t prev_ms = 0;
// //    uint64_t prev_step_ms = 0;
// //    uint8_t level = 0;

// //    result_t result = nfc_driver_init(&m_nfc, &m_i2c_driver_b.interface, &m_systick.interface);
// //    if(IS_ERR(result))
// //    {
// //       DEBUG_ERROR("nfc init failed. Code %d", GET_ERR_CODE(result));
// //    }

// //    while(true)
// //    {
// //       (void)m_systick.interface.get_time_ms(&m_systick.interface, &current_ms);
// //       if(current_ms - prev_ms > 20u)
// //       {
// //          prev_ms = current_ms;
// //          (void)m_nfc.interface.process(&m_nfc.interface, true);
// //       }

// //       if(current_ms - prev_step_ms > 10000u)
// //       {
// //          prev_step_ms = current_ms;
// //          result = m_nfc.interface.set_output_power(&m_nfc.interface, level);
// //          if(IS_OK(result))
// //          {
// //             DEBUG_INFO("Set NFC power level to %u", level);
// //          }
// //          else
// //          {
// //             DEBUG_ERROR("set_output_power failed: unit %d code %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));
// //          }

// //          level++;
// //          if(level > 15u)
// //          {
// //             level = 0u;
// //          }
// //       }

// //       feed_watchdog();
// //       idle_state_handle();
// //    }
// // }

// // static void __attribute__((unused)) nfc_charge_ring(void)
// // {
// //    DEBUG_INFO("Starting NFC ring charge sandbox");
// //    uint64_t current_ms = 0;
// //    uint64_t prev_ms = 0;
// //    uint64_t prev_step_ms = 0;
// //    uint8_t level = 0;

// //    m_i2c_driver_b.interface.i2c_bus_scan(&m_i2c_driver_b.interface);

// //    result_t result = nfc_driver_init(&m_nfc, &m_i2c_driver_b.interface, &m_systick.interface);
// //    if(IS_ERR(result))
// //    {
// //       DEBUG_ERROR("nfc init failed. Code %d", GET_ERR_CODE(result));
// //    }

// //    result = m_nfc.interface.set_output_power(&m_nfc.interface, 12);

// //    while(true)
// //    {
// //       nrf_delay_ms(100u);
// //       nrf_gpio_pin_clear(LED_GREEN);
// //       nrf_delay_ms(100u);
// //       nrf_gpio_pin_set(LED_GREEN);

// //       feed_watchdog();
// //       idle_state_handle();
// //    }
// // }

// // static void flash_sandbox(void)
// // {
// //    DEBUG_INFO("Starting Flash Sandbox");
// //    uint64_t current_ms = 0;
// //    uint64_t prev_ms = 0;
// //    uint64_t prev_switch_ms = 0;
// //    result_t result = RESULT_OK;

// //    int err = lfs_format(&m_lfs, &(m_filesystem.cfg));
// //    err = lfs_mount(&m_lfs, &(m_filesystem.cfg));

// //    // reformat if we can't mount the filesystem
// //    // this should only happen on the first boot
// //    if(err)
// //    {
// //       err = lfs_format(&m_lfs, &(m_filesystem.cfg));
// //       err = lfs_mount(&m_lfs, &(m_filesystem.cfg));
// //    }

// //    // 4) Create + write a file
// //    const char message[] = "Hello flash!\n";

// //    lfs_file_t file;
// //    err = lfs_file_open(&m_lfs, &file, "test.txt", LFS_O_WRONLY | LFS_O_CREAT);

// //    err = lfs_file_write(&m_lfs, &file, message, sizeof(message));

// //    lfs_file_close(&m_lfs, &file);

// //    // 5) Read it back
// //    err = lfs_file_open(&m_lfs, &file, "test.txt", LFS_O_RDONLY);

// //    char buffer[56] = {0};
// //    int nread = lfs_file_read(&m_lfs, &file, buffer, sizeof(buffer) - 1);

// //    lfs_file_close(&m_lfs, &file);

// //    lfs_unmount(&m_lfs);

// //    while(true)
// //    {
// //       (void)m_systick.interface.get_time_ms(&m_systick.interface, &current_ms);

// //       if(current_ms - prev_switch_ms > 5000u)
// //       {
// //          prev_switch_ms = current_ms;

// //          DEBUG_INFO("Testing swop");
// //       }
// //       feed_watchdog();
// //       idle_state_handle();
// //    }
// // }

// /* static void temp_sensor_sandbox(void)
// {
//    DEBUG_INFO("Starting temp sensor sandbox");
//    uint64_t current_ms = 0;
//    uint64_t prev_ms = 0;

//    int16_t temperature_deciC = 0;
//    bool temp_is_stale = false;

//    result_t result
//       = sts30_dis_temp_sensor_init(&m_temp_sensor, &m_i2c_driver_b.interface, &m_systick.interface,
// STS30_I2C_ADDRESS);

//    while(true)
//    {
//       (void)m_systick.interface.get_time_ms(&m_systick.interface, &current_ms);
//       if(current_ms - prev_ms >= 50u)
//       {
//          prev_ms = current_ms;

//          IF_OK_RUN_AND_UPDATE(result,
//                               m_temp_sensor.interface.temp_sensor_process(&m_temp_sensor.interface,
// TEMP_READ_FREQ_MS));

//          // Read temperature
//          IF_OK_RUN_AND_UPDATE(result,
//                               m_temp_sensor.interface.get_temperature(
//                                  &m_temp_sensor.interface, true, &temperature_deciC, &temp_is_stale));
//          if(IS_OK(result))
//          {
//             DEBUG_INFO("Temperature: %d C, Stale: %d", temperature_deciC, temp_is_stale);
//          }
//       }

//       feed_watchdog();
//       idle_state_handle();
//    }
// } */

/**
 * Weight stack testing sandbox
 *
 * The code inside the #ifdef TESTING_WEIGHT_STACK_ENABLE guard is intended for testing the weight stack functionality
 * in isolation. It is meant to:
 * - Run the weight sensor
 * - Run the dose size detection
 * - Run the weight stack calibration routines
 * - Run the dose size detection baselining routines
 *
 * How to add to the firmware:
 * 1. Copy everything inside the #ifdef TESTING_WEIGHT_STACK_ENABLE block to the bottom of `general_control.c`
 * 2. Move the section between the `// START TOP OF FILE` and `// END TOP OF FILE` comments to the top of
 * `general_control.c` (after includes and defines)
 * 3. Call `weight_stack_sandbox()` from the end of `general_control_init()` to run the sandbox
 *
 *
 * How to use:
 * You will need to run the calibration and baselining sequences every time you reset the device, as currently
 * NVM storage is disabled.
 *
 * - When you want to trigger a sequence, add a breakpoint at the top of `simulate_sequence`.
 * - When you hit the breakpoint, set the trigger_sequence variable (and optionally the `calibration_weight_mg` or
 * `dose_repeat_count`).
 * - Remove the breakpoint and continue execution to see the sequence run.
 * - Ring removal and placement as well as relevant BLE packets will be simulated based on the sequence you triggered.
 */
// #define TESTING_WEIGHT_STACK_ENABLE
#ifdef TESTING_WEIGHT_STACK_ENABLE

// START TOP OF FILE - Move this section to the top of general_control.c (after includes and defines)

static void weight_stack_sandbox(void);
static void weight_stack_sandbox_control_step(void);

static result_t mock_queue_app_manager_tx_enqueue(const queue_interface_t *const interface, const void *const element);
static result_t mock_ble_get_status(const ble_control_interface_t *const p_interface, ble_control_status_t *p_status);

static void simulate_sequence(void);

static void sandbox_inject_start_baselining_packet(void);
static void sandbox_inject_start_calibration_packet(uint32_t weight_mg);
static void sandbox_inject_weight_present_packet(void);

static bool get_ring_presence(void);

static bool m_is_ring_present = true;

// END TOP OF FILE

/**
 * @brief Run the weight stack sandbox
 *
 * This function is called at the end of `general_control_init`, and replaces the "main loop".
 *
 * @todo Elaborate in this documentation
 */
static void weight_stack_sandbox(void)
{
   DEBUG_INFO("Starting weight stack sandbox...");

   result_t result = RESULT_OK;
   uint64_t prev_main_loop_timestamp_ms = 0;
   uint64_t current_systick_time_ms = 0;

   // Mock functions to isolate weight stack testing from other modules.
   m_queue_app_manger_tx_ifx->enqueue = mock_queue_app_manager_tx_enqueue;
   DEBUG_DEBUG("[testing] Mocking app manager TX queue to capture weight data packets being sent to the app.");

   DEBUG_DEBUG("[testing] BLE connection not required. Simulating a valid bonded connection.");
   m_ble_control.interface.get_ble_status = mock_ble_get_status;

   DEBUG_DEBUG("[testing] Dose schedule not required. Skipping dose schedule validation in state machine.");
   m_system_sm_inputs.is_valid_dose_schedule_detected = true;

   for(;;)
   {
      // State changed. Update systick period.
      if(m_system_sm_outputs.changed)
      {
         m_systick_period_ms = m_system_sm_outputs.current_state_systick_period_ms;
      }

      result = m_systick.interface.get_time_ms(&m_systick.interface, &current_systick_time_ms);
      BREAK_ON_ERR(result);

      if(current_systick_time_ms - prev_main_loop_timestamp_ms >= m_system_sm_outputs.current_state_loop_period_ms)
      {
         prev_main_loop_timestamp_ms = current_systick_time_ms;
         weight_stack_sandbox_control_step();
      }

      feed_watchdog();
      idle_state_handle();
   }

   DEBUG_ERROR("Exiting weight stack sandbox with result (%d, %d)", GET_ERR_UNIT(result), GET_ERR_CODE(result));
}

/**
 * @brief Per loop processing for the weight stack sandbox
 *
 * This function is what is called every loop when running the weight stack sandbox.
 */
static void weight_stack_sandbox_control_step(void)
{
   const weight_sensor_interface_t *p_weight_sensor = &m_weight_sensor.interface;
   const dose_size_detection_interface_t *p_dsd = &m_dsd.interface;

   result_t result = RESULT_OK;
   static battery_status_t battery_status = {0};
   static uint32_t rx_calibration_weight = 0; // Calibration weight size received for the calibration procedure
   static bool is_weight_present = false;     // Used during weight calibration process

   bool is_ring_present = false;
   static uint8_t medication_uid[NFC_TAG_MAX_UID_SIZE] = {0};
   bool medication_uid_stale = false;

   bool is_weight_valid = false;
   dock_weight_measurement_t weight_data = {0};

   simulate_sequence();

   update_ble_state();
   handle_battery(&battery_status);

   handle_dose_size_detection(&is_weight_valid, &weight_data);

   process_incoming_app_messages(&rx_calibration_weight, &is_weight_present);

   is_ring_present = get_ring_presence();

   m_system_sm_outputs.changed = false; // Clear output before updating state machine
   m_system_sm.update_statemachine(&m_system_sm, &m_system_sm_inputs, &m_system_sm_outputs);

   if(STATEMACHINE_STATE_PAIRING_BLE != m_system_sm.current_state)
   {
      result = m_hmi.interface.clear_notification_event(&m_hmi.interface, NOTIFICATION_EVENT_BLE_PAIRING);
      ON_ERR_DEBUG_ERROR(result,
                         "Failed to clear BLE pairing HMI notification. Unit %d, Error: %d",
                         GET_ERR_UNIT(result),
                         GET_ERR_CODE(result));
   }

   switch(m_system_sm.current_state)
   {
      case STATEMACHINE_STATE_UNBONDED:
         if(m_system_sm_outputs.changed)
         {
            DEBUG_INFO("GC SM current state: UNBONDED");
         }

         handle_sm_unbonded_state(&battery_status);
         break;

      case STATEMACHINE_STATE_IDLE:
         if(m_system_sm_outputs.changed)
         {
            DEBUG_INFO("GC SM current state: IDLE");
         }

         handle_sm_idle_state();
         break;

      case STATEMACHINE_STATE_ACTIVE:
         if(m_system_sm_outputs.changed)
         {
            DEBUG_INFO("GC SM current state: ACTIVE");
         }

         handle_sm_active_state();
         break;

      case STATEMACHINE_STATE_INVALID_DOSE_INFO:
         if(m_system_sm_outputs.changed)
         {
            DEBUG_INFO("GC SM current state: INVALID_DOSE_INFO");
         }

         handle_sm_invalid_dose_info_state();
         break;

      case STATEMACHINE_STATE_PAIRING_BLE:
         if(m_system_sm_outputs.changed)
         {
            DEBUG_INFO("GC SM current state: PAIRING_BLE");
         }

         handle_sm_pairing_state();
         break;

      case STATEMACHINE_STATE_CALIBRATION:
         if(m_system_sm_outputs.changed)
         {
            DEBUG_INFO("GC SM current state: CALIBRATION");
         }

         handle_sm_calibration_state(is_ring_present, rx_calibration_weight, is_weight_present);
         break;

      case STATEMACHINE_STATE_BASELINING:
         if(m_system_sm_outputs.changed)
         {
            DEBUG_INFO("GC SM current state: BASELINING");
         }

         handle_sm_baselining_state(is_ring_present, medication_uid, sizeof(medication_uid), medication_uid_stale);
         break;

      default:
         if(m_system_sm_outputs.changed)
         {
            DEBUG_INFO("GC SM current state: UNKNOWN");
         }
         break;
   }

   if(STATEMACHINE_STATE_BASELINING != m_system_sm.current_state)
   {
      // Ignore medication changes during the baselining state since it's goal is to lock in a new medication UID
      handle_medication_change_notification(medication_uid, sizeof(medication_uid));
   }

   if(STATEMACHINE_STATE_CALIBRATION != m_system_sm.current_state)
   {
      // Explicitly clear these if not in calibration state. This ensures fresh values when starting a new calibration
      rx_calibration_weight = false;
      is_weight_present = false;
   }

   // Handle weight related tasks
   result = p_weight_sensor->process(p_weight_sensor, is_ring_present);
   ON_ERR_DEBUG_ERROR(
      result, "Failed to process weight sensor. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

   result = p_dsd->process(p_dsd, is_ring_present);

   // Clear DSD errors related to calibration
   static bool reported_error_uncalibrated = false;
   static bool reported_error_not_baselined = false;
   if(IS_ERR(result) && (DOSE_SIZE_DETECTION_ERROR_SCALE_NOT_CALIBRATED == GET_ERR_CODE(result))
      && (SW_UNIT_ID_DOSE_SIZE_DETECTION == GET_ERR_UNIT(result)))
   {
      if(!reported_error_uncalibrated)
      {
         DEBUG_ERROR("DSD scale not calibrated. result(%d,%d)", GET_ERR_UNIT(result), GET_ERR_CODE(result));
         reported_error_uncalibrated = true;
      }
      CLEAR_ERR(result);
   }
   else if(IS_ERR(result) && (DOSE_SIZE_DETECTION_ERROR_NOT_BASELINED == GET_ERR_CODE(result))
           && (SW_UNIT_ID_DOSE_SIZE_DETECTION == GET_ERR_UNIT(result)))
   {
      if(!reported_error_not_baselined)
      {
         DEBUG_ERROR("DSD not baselined. result(%d,%d)", GET_ERR_UNIT(result), GET_ERR_CODE(result));
         reported_error_not_baselined = true;
      }
      CLEAR_ERR(result);
   }
   else
   {
      // For any other errors, log them as usual
      ON_ERR_DEBUG_ERROR(
         result, "Failed to process DSD. Unit %d, Error: %d", GET_ERR_UNIT(result), GET_ERR_CODE(result));

      // Clear the reported error flags if processing is successful, indicating that the issues have been resolved
      reported_error_uncalibrated = false;
      reported_error_not_baselined = false;
   }
}

static result_t mock_queue_app_manager_tx_enqueue(const queue_interface_t *const interface, const void *const element)
{
   (void)interface; // Unused parameter
   (void)element;   // Unused parameter

   return RESULT_OK;
}

static result_t mock_ble_get_status(const ble_control_interface_t *const p_interface, ble_control_status_t *p_status)
{
   (void)p_interface; // Unused parameter

   // Simulate a bonded and connected BLE status
   p_status->is_bonded = true;
   p_status->is_connected = true;
   p_status->allow_new_bond = false;

   return RESULT_OK;
}

typedef enum
{
   TEST_SEQUENCE_NONE = 0,
   TEST_SEQUENCE_BASELINING = 1,
   TEST_SEQUENCE_CALIBRATION = 2,
   TEST_SEQUENCE_DOSE = 3
} TEST_SEQUENCE;

/**
 * @brief Simulate sequences of events for testing purposes
 *
 * @note Use the debugger to modify @p trigger_sequence to start different test sequences. Once the BLE testing tool is
 * available and the NFC ring presence detection works, this sequence simulation will not be necessary.
 */
static void simulate_sequence(void)
{
   static TEST_SEQUENCE trigger_sequence = TEST_SEQUENCE_NONE;
   static TEST_SEQUENCE current_sequence = TEST_SEQUENCE_NONE;

   static uint32_t calibration_weight_mg = 20000; // 20g calibration weight for testing
   static uint32_t dose_repeat_count = 1u;        // Configurable in debugger
   static uint32_t dose_repeat_index = 0u;

   static uint32_t step = 0u;
   static uint64_t next_step_time_ms = 0u;

   uint64_t current_time_ms = 0u;

   bool step_executed = false;
   uint32_t step_wait_ms = 0u;

   (void)m_systick.interface.get_time_ms(&m_systick.interface, &current_time_ms);

   if(trigger_sequence != TEST_SEQUENCE_NONE && current_sequence == TEST_SEQUENCE_NONE)
   {
      current_sequence = trigger_sequence;
      trigger_sequence = TEST_SEQUENCE_NONE; // Reset trigger

      DEBUG_INFO("[testing] Starting test sequence: %d", current_sequence);

      step = 0u; // Reset step counter at the start of a sequence
      next_step_time_ms = current_time_ms;

      if(current_sequence == TEST_SEQUENCE_DOSE)
      {
         dose_repeat_index = 0u;
      }
   }

   if((current_sequence != TEST_SEQUENCE_NONE) && (current_time_ms < next_step_time_ms))
   {
      return;
   }

   switch(current_sequence)
   {
      case TEST_SEQUENCE_BASELINING:
         switch(step)
         {
            case 0:
               sandbox_inject_start_baselining_packet();
               step_executed = true;
               step_wait_ms = 1000u;
               break;
            case 1:
               step_executed = true;
               step_wait_ms = 5000u;

               DEBUG_DEBUG("[testing] Simulating ring removal in %d ms. ", step_wait_ms);
               break;
            case 2:
               m_is_ring_present = false; // Simulate ring removed
               step_executed = true;
               step_wait_ms = 1000u;
               break;
            case 3:
               step_executed = true;
               step_wait_ms = 5000u;

               DEBUG_DEBUG("[testing] Simulating ring replacement in %d ms. ", step_wait_ms);
               break;
            case 4:
               m_is_ring_present = true; // Simulate ring replaced
               step_executed = true;
               step_wait_ms = 3000u;
               break;

            case 5:
               // Skip backend validation for testing purposes to move the sequence along.
               step_wait_ms = 1000u;
               if(m_baseline_sm.current_state == BASELINING_STATE_WAIT_FOR_BACKEND_VALIDATION)
               {
                  DEBUG_DEBUG("[testing] Skipping backend validation for meds.");
                  m_baseline_sm_inputs.backend_validation_status = BACKEND_VAL_STATUS_SUCCESS;
                  m_baseline_sm_inputs.dose_schedule_status = DOSE_SCH_STATUS_VALID;
                  step_executed = true;
               }
               break;

            case 6:
               current_sequence = TEST_SEQUENCE_NONE; // End sequence after final step
               step = 0u;
               next_step_time_ms = current_time_ms;
               DEBUG_INFO("[testing] Baselining sequence complete");
               break;
            default:
               DEBUG_ERROR("[testing] Invalid step in TEST_SEQUENCE_BASELINING: %d", step);
               current_sequence = TEST_SEQUENCE_NONE;
               step = 0u;
               next_step_time_ms = current_time_ms;
               break;
         }

         break;

      case TEST_SEQUENCE_CALIBRATION:

         switch(step)
         {
            case 0:
               sandbox_inject_start_calibration_packet(calibration_weight_mg);
               step_executed = true;
               step_wait_ms = 1000u;
               break;
            case 1:
               step_executed = true;
               step_wait_ms = 5000u;
               DEBUG_DEBUG("[testing] Simulating ring removal in %d ms. ", step_wait_ms);
               break;
            case 2:
               m_is_ring_present = false; // Simulate ring removed
               step_executed = true;
               step_wait_ms = 1000u;
               break;
            case 3:
               step_executed = true;
               step_wait_ms = 10000u;
               DEBUG_DEBUG("[testing] Simulating weight present in %d ms. ", step_wait_ms);
               break;
            case 4:
               sandbox_inject_weight_present_packet(); // Simulate placing calibration weight on the dock
               step_executed = true;
               step_wait_ms = 1000u;
               break;
            case 5:
               step_executed = true;
               step_wait_ms = 10000u;
               DEBUG_DEBUG("[testing] Simulating ring replacement in %d ms. ", step_wait_ms);
               break;
            case 6:
               m_is_ring_present = true; // Simulate ring replaced
               step_executed = true;
               step_wait_ms = 3000u;
               break;
            case 7:
               current_sequence = TEST_SEQUENCE_NONE; // End sequence after final step
               step = 0u;
               next_step_time_ms = current_time_ms;
               DEBUG_INFO("[testing] Calibration sequence complete");
               break;
            default:
               DEBUG_ERROR("[testing] Invalid step in TEST_SEQUENCE_CALIBRATION: %d", step);
               current_sequence = TEST_SEQUENCE_NONE;
               step = 0u;
               next_step_time_ms = current_time_ms;
               break;
         }
         break;

      case TEST_SEQUENCE_DOSE:
      {
         uint32_t total_doses = (dose_repeat_count > 0u) ? dose_repeat_count : 1u;
         switch(step)
         {
            case 0:
               step_executed = true;
               step_wait_ms = 4000u;
               DEBUG_DEBUG("[testing] Dose repetition %u/%u", dose_repeat_index + 1u, total_doses);
               DEBUG_DEBUG("[testing] Removing ring to simulate dose administration in %d ms. ", step_wait_ms);
               break;
            case 1:
               m_is_ring_present = false; // Simulate ring removed
               step_executed = true;
               step_wait_ms = 10000u;
               break;
            case 2:
               step_executed = true;
               step_wait_ms = 5000u;
               DEBUG_DEBUG("[testing] Simulating ring replacement in %d ms. ", step_wait_ms);
               break;
            case 3:
               m_is_ring_present = true; // Simulate ring replaced after dose
               step_executed = true;
               step_wait_ms = 21000u;
               DEBUG_DEBUG("[testing] Settling time (%d ms)", step_wait_ms);
               break;
            case 4:
               dose_repeat_index++;
               if(dose_repeat_index < total_doses)
               {
                  step = 0u;
                  next_step_time_ms = current_time_ms;
                  DEBUG_INFO("[testing] Dose %d/%d complete", dose_repeat_index, total_doses);
               }
               else
               {
                  current_sequence = TEST_SEQUENCE_NONE; // End sequence after final step
                  step = 0u;
                  next_step_time_ms = current_time_ms;
                  DEBUG_INFO("[testing] Dose sequence complete (%u repetitions)", total_doses);
               }
               break;
            default:
               DEBUG_ERROR("[testing] Invalid step in TEST_SEQUENCE_DOSE: %d", step);
               current_sequence = TEST_SEQUENCE_NONE;
               step = 0u;
               next_step_time_ms = current_time_ms;
               break;
         }
         break;
      }

      case TEST_SEQUENCE_NONE:
      default:
         // No test sequence triggered
         break;
   }

   if(step_executed)
   {
      step++;
      next_step_time_ms = current_time_ms + (uint64_t)step_wait_ms;
   }
}

/**
 * @brief Sandbox-only one-shot injector for PPI_AD_START_BASELINING.
 */
static void sandbox_inject_start_baselining_packet(void)
{
   mp_packet_payload_t injected_msg = {0};
   injected_msg.type = (uint8_t)PPI_TYPE_RQ;
   injected_msg.ppi = (uint8_t)PPI_AD_START_BASELINING;
   injected_msg.payload[0u] = 1u;
   injected_msg.pkt_payload_len = sizeof(uint8_t);

   result_t result = m_queue_app_manager_rx.interface.enqueue(&m_queue_app_manager_rx.interface, &injected_msg);
   ON_ERR_DEBUG_ERROR(result,
                      "[sandbox] Failed to enqueue START_BASELINING packet. Unit %d, Error: %d",
                      GET_ERR_UNIT(result),
                      GET_ERR_CODE(result));
}

/**
 * @brief Sandbox-only one-shot injector for PPI_AD_START_CALIBRATION.
 *
 * @param weight_mg The calibration weight in milligrams to include in the injected packet
 */
static void sandbox_inject_start_calibration_packet(uint32_t weight_mg)
{
   mp_packet_payload_t injected_msg = {0};
   injected_msg.type = (uint8_t)PPI_TYPE_RQ;
   injected_msg.ppi = (uint8_t)PPI_AD_START_CALIBRATION;
   injected_msg.payload[0u] = 1u;
   injected_msg.payload[1u] = (uint8_t)(weight_mg & 0xFFu);
   injected_msg.payload[2u] = (uint8_t)((weight_mg >> 8u) & 0xFFu);
   injected_msg.payload[3u] = (uint8_t)((weight_mg >> 16u) & 0xFFu);
   injected_msg.payload[4u] = (uint8_t)((weight_mg >> 24u) & 0xFFu);
   injected_msg.pkt_payload_len = sizeof(start_calibration_param_t);

   result_t result = m_queue_app_manager_rx.interface.enqueue(&m_queue_app_manager_rx.interface, &injected_msg);
   ON_ERR_DEBUG_ERROR(result,
                      "[sandbox] Failed to enqueue START_CALIBRATION packet. Unit %d, Error: %d",
                      GET_ERR_UNIT(result),
                      GET_ERR_CODE(result));
}

/**
 * @brief Sandbox-only one-shot injector for PPI_AD_CALIBRATION_WEIGHT_PRESENT to simulate placing the calibration
 * weight on the dock during the calibration procedure.
 */
static void sandbox_inject_weight_present_packet(void)
{
   mp_packet_payload_t injected_msg = {0};
   injected_msg.type = (uint8_t)PPI_TYPE_PUSH;
   injected_msg.ppi = (uint8_t)PPI_AD_CALIBRATION_WEIGHT_PRESENT;
   injected_msg.payload[0u] = 1u; // is_weight_present = true
   injected_msg.pkt_payload_len = sizeof(uint8_t);

   result_t result = m_queue_app_manager_rx.interface.enqueue(&m_queue_app_manager_rx.interface, &injected_msg);
   ON_ERR_DEBUG_ERROR(result,
                      "[sandbox] Failed to enqueue WEIGHT_PRESENT packet. Unit %d, Error: %d",
                      GET_ERR_UNIT(result),
                      GET_ERR_CODE(result));
}

/**
 * Mock the ring presence while NFC is not functional
 */
static bool get_ring_presence(void)
{
   static bool previous_state = true; // Assume ring is present at startup

   if(m_is_ring_present != previous_state)
   {
      DEBUG_INFO("Ring presence changed: %s", m_is_ring_present ? "PRESENT" : "NOT PRESENT");
      previous_state = m_is_ring_present;
   }

   return m_is_ring_present;
}

#endif /* TESTING_WEIGHT_STACK_ENABLE */
