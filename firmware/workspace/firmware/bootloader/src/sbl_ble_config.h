#ifndef SBL_BLE_CONFIG_H_
#define SBL_BLE_CONFIG_H_

#define NRF_BL_APP_CRC_CHECK_SKIPPED_ON_GPREGRET2       0
#define NRF_BL_APP_CRC_CHECK_SKIPPED_ON_SYSTEMOFF_RESET 0
#define NRF_BL_APP_SIGNATURE_CHECK_REQUIRED             1
#define NRF_BL_DFU_ALLOW_UPDATE_FROM_APP                0
#define NRF_BL_DFU_CONTINUATION_TIMEOUT_MS              10000
#define NRF_BL_DFU_ENTER_METHOD_BUTTON                  0
#define NRF_BL_DFU_ENTER_METHOD_BUTTONLESS              0
#define NRF_BL_DFU_ENTER_METHOD_GPREGRET                1
#define NRF_BL_DFU_ENTER_METHOD_PINRESET                0
#define NRF_BL_DFU_INACTIVITY_TIMEOUT_MS                30000
#define NRF_BL_FW_COPY_PROGRESS_STORE_STEP              8
#define NRF_BL_RESET_DELAY_MS                           0
#define NRF_BL_WDT_MAX_SCHEDULER_LATENCY_MS             10000
#define NRF_DFU_APP_DATA_AREA_SIZE                      (1 * 4096)
#define NRF_DFU_APP_DOWNGRADE_PREVENTION                1
#define NRF_DFU_BLE_ADV_INTERVAL                        40
#define NRF_DFU_BLE_ADV_NAME                            "VAlIDOSE DFU"
#define NRF_DFU_BLE_CONN_SUP_TIMEOUT_MS                 6000
#define NRF_DFU_BLE_MAX_CONN_INTERVAL                   12
#define NRF_DFU_BLE_MIN_CONN_INTERVAL                   12
#define NRF_DFU_BLE_REQUIRES_BONDS                      0
#define NRF_DFU_EXTERNAL_APP_VERSIONING                 0
#define NRF_DFU_FORCE_DUAL_BANK_APP_UPDATES             0
#define NRF_DFU_IN_APP                                  0
#define NRF_DFU_PROTOCOL_FW_VERSION_MSG                 0
#define NRF_DFU_PROTOCOL_REDUCED                        1
#define NRF_DFU_PROTOCOL_VERSION_MSG                    0
#define NRF_DFU_REQUIRE_SIGNED_APP_UPDATE               1
#define NRF_DFU_SAVE_PROGRESS_IN_FLASH                  0
#define NRF_DFU_SINGLE_BANK_APP_UPDATES                 0
#define NRF_DFU_SUPPORTS_EXTERNAL_APP                   0
#define NRF_DFU_TRANSPORT_BLE                           1
#define NRF_DFU_APP_ACCEPT_SAME_VERSION                 0

#ifndef DEBUG_NRF
/* Change this from `undef` to `define` to disable the debug AP in final production release builds. */
#   undef NRF_BL_DEBUG_PORT_DISABLE
#endif

#if DEBUG_NRF
#   define NRF_LOG_BACKEND_RTT_ENABLED           1
#   define NRF_LOG_BACKEND_RTT_TEMP_BUFFER_SIZE  64
#   define NRF_LOG_BACKEND_RTT_TX_RETRY_DELAY_MS 1
#   define NRF_LOG_BACKEND_RTT_TX_RETRY_CNT      3
#   define NRF_LOG_ENABLED                       1
/* Include errors, warnings, and info messages in log. */
#   define NRF_LOG_DEFAULT_LEVEL                  3
#   define SEGGER_RTT_CONFIG_BUFFER_SIZE_UP       1024
#   define SEGGER_RTT_CONFIG_MAX_NUM_UP_BUFFERS   2
#   define SEGGER_RTT_CONFIG_BUFFER_SIZE_DOWN     16
#   define SEGGER_RTT_CONFIG_MAX_NUM_DOWN_BUFFERS 2
#   define SEGGER_RTT_CONFIG_DEFAULT_MODE         0
#endif

#endif
