PROJECT_NAME     := Validose_ophthalmic_ring
TARGETS          := nrf52840_xxaa
OUTPUT_DIRECTORY := bin

PRIVATE_KEY_FILE ?= $(PRIVATE_KEY_PATH)
APP_VERSION_HEADER_PATH := ./../common/common.h 
APP_VERSION_STRING := $(shell cpp -dM ./../common/version.h | grep 'RING_APP_VERSION' | cut -d ' ' -f 3 | tr -d '"')
HW_VERSION := 1

SDK_ROOT := /opt/nRF5_SDK_17.1.0_ddde560
PROJ_DIR := ./
SOFTDEVICE_BINARY := $(SDK_ROOT)/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex

$(OUTPUT_DIRECTORY)/nrf52840_xxaa.out: \
  LINKER_SCRIPT  := link.ld

# Source files common to all targets
EXT_SRC_FILES += \
  $(SDK_ROOT)/modules/nrfx/mdk/gcc_startup_nrf52.S \
  $(SDK_ROOT)/components/ble/ble_advertising/ble_advertising.c \
  $(SDK_ROOT)/components/ble/ble_db_discovery/ble_db_discovery.c \
  $(SDK_ROOT)/components/ble/ble_link_ctx_manager/ble_link_ctx_manager.c \
  $(SDK_ROOT)/components/ble/ble_services/ble_bas_c/ble_bas_c.c \
  $(SDK_ROOT)/components/ble/ble_services/ble_bas/ble_bas.c \
  $(SDK_ROOT)/components/ble/ble_services/ble_dfu/ble_dfu_bonded.c \
  $(SDK_ROOT)/components/ble/ble_services/ble_dfu/ble_dfu_unbonded.c \
  $(SDK_ROOT)/components/ble/ble_services/ble_dfu/ble_dfu.c \
  $(SDK_ROOT)/components/ble/ble_services/ble_dis_c/ble_dis_c.c \
  $(SDK_ROOT)/components/ble/ble_services/ble_dis/ble_dis.c \
  $(SDK_ROOT)/components/ble/ble_services/ble_nus/ble_nus.c \
  $(SDK_ROOT)/components/ble/common/ble_advdata.c \
  $(SDK_ROOT)/components/ble/common/ble_conn_params.c \
  $(SDK_ROOT)/components/ble/common/ble_conn_state.c \
  $(SDK_ROOT)/components/ble/common/ble_srv_common.c \
  $(SDK_ROOT)/components/ble/nrf_ble_gatt/nrf_ble_gatt.c \
  $(SDK_ROOT)/components/ble/nrf_ble_gq/nrf_ble_gq.c \
  $(SDK_ROOT)/components/ble/nrf_ble_qwr/nrf_ble_qwr.c \
  $(SDK_ROOT)/components/ble/peer_manager/auth_status_tracker.c \
  $(SDK_ROOT)/components/ble/peer_manager/gatt_cache_manager.c \
  $(SDK_ROOT)/components/ble/peer_manager/gatts_cache_manager.c \
  $(SDK_ROOT)/components/ble/peer_manager/id_manager.c \
  $(SDK_ROOT)/components/ble/peer_manager/peer_data_storage.c \
  $(SDK_ROOT)/components/ble/peer_manager/peer_database.c \
  $(SDK_ROOT)/components/ble/peer_manager/peer_id.c \
  $(SDK_ROOT)/components/ble/peer_manager/peer_manager_handler.c \
  $(SDK_ROOT)/components/ble/peer_manager/peer_manager.c \
  $(SDK_ROOT)/components/ble/peer_manager/pm_buffer.c \
  $(SDK_ROOT)/components/ble/peer_manager/security_dispatcher.c \
  $(SDK_ROOT)/components/ble/peer_manager/security_manager.c \
  $(SDK_ROOT)/components/boards/boards.c \
  $(SDK_ROOT)/components/libraries/atomic_fifo/nrf_atfifo.c \
  $(SDK_ROOT)/components/libraries/atomic_flags/nrf_atflags.c \
  $(SDK_ROOT)/components/libraries/atomic/nrf_atomic.c \
  $(SDK_ROOT)/components/libraries/balloc/nrf_balloc.c \
  $(SDK_ROOT)/components/libraries/bootloader/dfu/nrf_dfu_svci.c \
  $(SDK_ROOT)/components/libraries/bsp/bsp_btn_ble.c \
  $(SDK_ROOT)/components/libraries/bsp/bsp.c \
  $(SDK_ROOT)/components/libraries/button/app_button.c \
  $(SDK_ROOT)/components/libraries/crc16/crc16.c \
  $(SDK_ROOT)/components/libraries/crypto/backend/mbedtls/mbedtls_backend_aes_aead.c \
  $(SDK_ROOT)/components/libraries/crypto/backend/mbedtls/mbedtls_backend_aes.c \
  $(SDK_ROOT)/components/libraries/crypto/backend/mbedtls/mbedtls_backend_ecc.c \
  $(SDK_ROOT)/components/libraries/crypto/backend/mbedtls/mbedtls_backend_ecdh.c \
  $(SDK_ROOT)/components/libraries/crypto/backend/mbedtls/mbedtls_backend_ecdsa.c \
  $(SDK_ROOT)/components/libraries/crypto/backend/mbedtls/mbedtls_backend_hash.c \
  $(SDK_ROOT)/components/libraries/crypto/backend/mbedtls/mbedtls_backend_hmac.c \
  $(SDK_ROOT)/components/libraries/crypto/backend/mbedtls/mbedtls_backend_init.c \
  $(SDK_ROOT)/components/libraries/crypto/backend/nrf_hw/nrf_hw_backend_rng_mbedtls.c \
  $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_aead.c \
  $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_aes_shared.c \
  $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_aes.c \
  $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_ecc.c \
  $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_ecdh.c \
  $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_ecdsa.c \
  $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_eddsa.c \
  $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_error.c \
  $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_hash.c \
  $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_hkdf.c \
  $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_hmac.c \
  $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_init.c \
  $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_rng.c \
  $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_shared.c \
  $(SDK_ROOT)/components/libraries/experimental_section_vars/nrf_section_iter.c \
  $(SDK_ROOT)/components/libraries/fds/fds.c \
  $(SDK_ROOT)/components/libraries/fifo/app_fifo.c \
  $(SDK_ROOT)/components/libraries/fstorage/nrf_fstorage_sd.c \
  $(SDK_ROOT)/components/libraries/fstorage/nrf_fstorage.c \
  $(SDK_ROOT)/components/libraries/hardfault/hardfault_implementation.c \
  $(SDK_ROOT)/components/libraries/libuarte/nrf_libuarte_async.c \
  $(SDK_ROOT)/components/libraries/libuarte/nrf_libuarte_drv.c \
  $(SDK_ROOT)/components/libraries/log/src/nrf_log_backend_rtt.c \
  $(SDK_ROOT)/components/libraries/log/src/nrf_log_backend_serial.c \
  $(SDK_ROOT)/components/libraries/log/src/nrf_log_backend_uart.c \
  $(SDK_ROOT)/components/libraries/log/src/nrf_log_default_backends.c \
  $(SDK_ROOT)/components/libraries/log/src/nrf_log_frontend.c \
  $(SDK_ROOT)/components/libraries/log/src/nrf_log_str_formatter.c \
  $(SDK_ROOT)/components/libraries/memobj/nrf_memobj.c \
  $(SDK_ROOT)/components/libraries/pwr_mgmt/nrf_pwr_mgmt.c \
  $(SDK_ROOT)/components/libraries/queue/nrf_queue.c \
  $(SDK_ROOT)/components/libraries/ringbuf/nrf_ringbuf.c \
  $(SDK_ROOT)/components/libraries/scheduler/app_scheduler.c \
  $(SDK_ROOT)/components/libraries/sensorsim/sensorsim.c \
  $(SDK_ROOT)/components/libraries/sortlist/nrf_sortlist.c \
  $(SDK_ROOT)/components/libraries/strerror/nrf_strerror.c \
  $(SDK_ROOT)/components/libraries/timer/app_timer2.c \
  $(SDK_ROOT)/components/libraries/timer/drv_rtc.c \
  $(SDK_ROOT)/components/libraries/uart/app_uart_fifo.c \
  $(SDK_ROOT)/components/libraries/uart/retarget.c \
  $(SDK_ROOT)/components/libraries/util/app_error_handler_gcc.c \
  $(SDK_ROOT)/components/libraries/util/app_error_weak.c \
  $(SDK_ROOT)/components/libraries/util/app_error.c \
  $(SDK_ROOT)/components/libraries/util/app_util_platform.c \
  $(SDK_ROOT)/components/libraries/util/nrf_assert.c \
  $(SDK_ROOT)/components/softdevice/common/nrf_sdh_ble.c \
  $(SDK_ROOT)/components/softdevice/common/nrf_sdh_soc.c \
  $(SDK_ROOT)/components/softdevice/common/nrf_sdh.c \
  $(SDK_ROOT)/external/fprintf/nrf_fprintf_format.c \
  $(SDK_ROOT)/external/fprintf/nrf_fprintf.c \
  $(SDK_ROOT)/external/mbedtls/library/aes.c \
  $(SDK_ROOT)/external/mbedtls/library/aesni.c \
  $(SDK_ROOT)/external/mbedtls/library/arc4.c \
  $(SDK_ROOT)/external/mbedtls/library/aria.c \
  $(SDK_ROOT)/external/mbedtls/library/asn1parse.c \
  $(SDK_ROOT)/external/mbedtls/library/asn1write.c \
  $(SDK_ROOT)/external/mbedtls/library/base64.c \
  $(SDK_ROOT)/external/mbedtls/library/bignum.c \
  $(SDK_ROOT)/external/mbedtls/library/blowfish.c \
  $(SDK_ROOT)/external/mbedtls/library/camellia.c \
  $(SDK_ROOT)/external/mbedtls/library/ccm.c \
  $(SDK_ROOT)/external/mbedtls/library/certs.c \
  $(SDK_ROOT)/external/mbedtls/library/chacha20.c \
  $(SDK_ROOT)/external/mbedtls/library/chachapoly.c \
  $(SDK_ROOT)/external/mbedtls/library/cipher_wrap.c \
  $(SDK_ROOT)/external/mbedtls/library/cipher.c \
  $(SDK_ROOT)/external/mbedtls/library/cmac.c \
  $(SDK_ROOT)/external/mbedtls/library/ctr_drbg.c \
  $(SDK_ROOT)/external/mbedtls/library/debug.c \
  $(SDK_ROOT)/external/mbedtls/library/des.c \
  $(SDK_ROOT)/external/mbedtls/library/dhm.c \
  $(SDK_ROOT)/external/mbedtls/library/ecdh.c \
  $(SDK_ROOT)/external/mbedtls/library/ecdsa.c \
  $(SDK_ROOT)/external/mbedtls/library/ecjpake.c \
  $(SDK_ROOT)/external/mbedtls/library/ecp_curves.c \
  $(SDK_ROOT)/external/mbedtls/library/ecp.c \
  $(SDK_ROOT)/external/mbedtls/library/entropy_poll.c \
  $(SDK_ROOT)/external/mbedtls/library/entropy.c \
  $(SDK_ROOT)/external/mbedtls/library/error.c \
  $(SDK_ROOT)/external/mbedtls/library/gcm.c \
  $(SDK_ROOT)/external/mbedtls/library/havege.c \
  $(SDK_ROOT)/external/mbedtls/library/hmac_drbg.c \
  $(SDK_ROOT)/external/mbedtls/library/md_wrap.c \
  $(SDK_ROOT)/external/mbedtls/library/md.c \
  $(SDK_ROOT)/external/mbedtls/library/md2.c \
  $(SDK_ROOT)/external/mbedtls/library/md4.c \
  $(SDK_ROOT)/external/mbedtls/library/md5.c \
  $(SDK_ROOT)/external/mbedtls/library/net_sockets.c \
  $(SDK_ROOT)/external/mbedtls/library/nist_kw.c \
  $(SDK_ROOT)/external/mbedtls/library/oid.c \
  $(SDK_ROOT)/external/mbedtls/library/padlock.c \
  $(SDK_ROOT)/external/mbedtls/library/pem.c \
  $(SDK_ROOT)/external/mbedtls/library/pk_wrap.c \
  $(SDK_ROOT)/external/mbedtls/library/pk.c \
  $(SDK_ROOT)/external/mbedtls/library/pkcs11.c \
  $(SDK_ROOT)/external/mbedtls/library/pkcs12.c \
  $(SDK_ROOT)/external/mbedtls/library/pkcs5.c \
  $(SDK_ROOT)/external/mbedtls/library/pkparse.c \
  $(SDK_ROOT)/external/mbedtls/library/pkwrite.c \
  $(SDK_ROOT)/external/mbedtls/library/platform_util.c \
  $(SDK_ROOT)/external/mbedtls/library/platform.c \
  $(SDK_ROOT)/external/mbedtls/library/poly1305.c \
  $(SDK_ROOT)/external/mbedtls/library/ripemd160.c \
  $(SDK_ROOT)/external/mbedtls/library/rsa_internal.c \
  $(SDK_ROOT)/external/mbedtls/library/rsa.c \
  $(SDK_ROOT)/external/mbedtls/library/sha1.c \
  $(SDK_ROOT)/external/mbedtls/library/sha256.c \
  $(SDK_ROOT)/external/mbedtls/library/sha512.c \
  $(SDK_ROOT)/external/mbedtls/library/ssl_cache.c \
  $(SDK_ROOT)/external/mbedtls/library/ssl_ciphersuites.c \
  $(SDK_ROOT)/external/mbedtls/library/ssl_cli.c \
  $(SDK_ROOT)/external/mbedtls/library/ssl_cookie.c \
  $(SDK_ROOT)/external/mbedtls/library/ssl_srv.c \
  $(SDK_ROOT)/external/mbedtls/library/ssl_ticket.c \
  $(SDK_ROOT)/external/mbedtls/library/ssl_tls.c \
  $(SDK_ROOT)/external/mbedtls/library/threading.c \
  $(SDK_ROOT)/external/mbedtls/library/version_features.c \
  $(SDK_ROOT)/external/mbedtls/library/version.c \
  $(SDK_ROOT)/external/mbedtls/library/x509_create.c \
  $(SDK_ROOT)/external/mbedtls/library/x509_crl.c \
  $(SDK_ROOT)/external/mbedtls/library/x509_crt.c \
  $(SDK_ROOT)/external/mbedtls/library/x509_csr.c \
  $(SDK_ROOT)/external/mbedtls/library/x509.c \
  $(SDK_ROOT)/external/mbedtls/library/x509write_crt.c \
  $(SDK_ROOT)/external/mbedtls/library/x509write_csr.c \
  $(SDK_ROOT)/external/mbedtls/library/xtea.c \
  $(SDK_ROOT)/external/segger_rtt/SEGGER_RTT_printf.c \
  $(SDK_ROOT)/external/segger_rtt/SEGGER_RTT_Syscalls_GCC.c \
  $(SDK_ROOT)/external/segger_rtt/SEGGER_RTT.c \
  $(SDK_ROOT)/external/utf_converter/utf.c \
  $(SDK_ROOT)/integration/nrfx/legacy/nrf_drv_rng.c \
  $(SDK_ROOT)/integration/nrfx/legacy/nrf_drv_clock.c \
  $(SDK_ROOT)/integration/nrfx/legacy/nrf_drv_ppi.c \
  $(SDK_ROOT)/integration/nrfx/legacy/nrf_drv_twi.c \
  $(SDK_ROOT)/integration/nrfx/legacy/nrf_drv_uart.c \
  $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_clock.c \
  $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_gpiote.c \
  $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_ppi.c \
  $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_pwm.c \
  $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_saadc.c \
  $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_timer.c \
  $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_twi.c \
  $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_uart.c \
  $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_uarte.c \
  $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_wdt.c \
  $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_rng.c \
  $(SDK_ROOT)/modules/nrfx/drivers/src/prs/nrfx_prs.c \
  $(SDK_ROOT)/modules/nrfx/mdk/system_nrf52.c \
  $(SDK_ROOT)/modules/nrfx/soc/nrfx_atomic.c \
  $(PROJ_DIR)../external/st/ST25DV/st25dv_reg.c \
  $(PROJ_DIR)../external/st/ST25DV/st25dv.c \
  $(PROJ_DIR)/main.c \

PROJ_SRC_FILES += \
  $(PROJ_DIR)../common/drivers/i2c/i2c_driver.c \
  $(PROJ_DIR)../common/drivers/imu/imu.c \
  $(PROJ_DIR)../common/drivers/uart/uart_driver.c \
  $(PROJ_DIR)../common/modules/debug/debug.c \
  $(PROJ_DIR)../common/modules/statemachine/statemachine.c \
  $(PROJ_DIR)../common/modules/system_time/system_time.c \
  $(PROJ_DIR)../common/utils/crc/crc16.c \
  $(PROJ_DIR)../common/utils/message_protocol/message_protocol.c \
  $(PROJ_DIR)../common/utils/queue/queue.c \
  $(PROJ_DIR)../common/utils/serial_link_protocol/serial_link_protocol.c \
  $(PROJ_DIR)../common/drivers/uart/uart_driver.c \
  $(PROJ_DIR)../common/drivers/i2c/i2c_driver.c \
  $(PROJ_DIR)../common/drivers/imu/imu.c \
  $(PROJ_DIR)/modules/general_control/general_control.c \
  $(PROJ_DIR)/modules/ble_control/ble_control_ring.c \
  $(PROJ_DIR)/modules/ble_control/ble_cs_ring.c \
  $(PROJ_DIR)/modules/dock_manager/dock_manager.c \
  $(PROJ_DIR)/drivers/pmic_driver/bq25150_driver.c \
  $(PROJ_DIR)/drivers/nfc_driver/st25dv_mb_driver.c \
  $(PROJ_DIR)/drivers/nfc_driver/nfc_tag_eeprom_queue.c \
  $(PROJ_DIR)/drivers/ir_proximity_sensor_driver/tmd2635_driver.c \
  $(PROJ_DIR)/modules/battery_manager/ring_battery_manager.c \
  $(PROJ_DIR)/modules/ble_control/ble_control_ring.c \
  $(PROJ_DIR)/modules/ble_control/ble_cs_ring.c \
  $(PROJ_DIR)/modules/cap_detection/cap_detection.c \
  $(PROJ_DIR)/modules/dock_manager/dock_manager.c \
  $(PROJ_DIR)/modules/ring_data_manager/ring_data_manager.c \
  $(PROJ_DIR)/modules/dose_detection/dose_detection.c \
  $(PROJ_DIR)/modules/dose_detection/dose_detection_fsm.c \
  $(PROJ_DIR)/modules/fds_manager/fds_manager.c \
  $(PROJ_DIR)/modules/general_control/general_control.c \
  $(PROJ_DIR)/modules/tilt_detection/tilt_detection.c \
  $(PROJ_DIR)/modules/ring_sources/ring_sources.c \
  
# Include folders common to all targets
EXT_INC_FOLDERS += \
  $(SDK_ROOT)/components \
  $(SDK_ROOT)/components/ble/ble_advertising \
  $(SDK_ROOT)/components/ble/ble_db_discovery \
  $(SDK_ROOT)/components/ble/ble_dtm \
  $(SDK_ROOT)/components/ble/ble_link_ctx_manager \
  $(SDK_ROOT)/components/ble/ble_racp \
  $(SDK_ROOT)/components/ble/ble_services/ble_ancs_c \
  $(SDK_ROOT)/components/ble/ble_services/ble_ans_c \
  $(SDK_ROOT)/components/ble/ble_services/ble_bas \
  $(SDK_ROOT)/components/ble/ble_services/ble_bas_c \
  $(SDK_ROOT)/components/ble/ble_services/ble_cscs \
  $(SDK_ROOT)/components/ble/ble_services/ble_cts_c \
  $(SDK_ROOT)/components/ble/ble_services/ble_dfu \
  $(SDK_ROOT)/components/ble/ble_services/ble_dis \
  $(SDK_ROOT)/components/ble/ble_services/ble_gls \
  $(SDK_ROOT)/components/ble/ble_services/ble_hids \
  $(SDK_ROOT)/components/ble/ble_services/ble_hrs \
  $(SDK_ROOT)/components/ble/ble_services/ble_hrs_c \
  $(SDK_ROOT)/components/ble/ble_services/ble_hts \
  $(SDK_ROOT)/components/ble/ble_services/ble_ias \
  $(SDK_ROOT)/components/ble/ble_services/ble_ias_c \
  $(SDK_ROOT)/components/ble/ble_services/ble_lbs \
  $(SDK_ROOT)/components/ble/ble_services/ble_lbs_c \
  $(SDK_ROOT)/components/ble/ble_services/ble_lls \
  $(SDK_ROOT)/components/ble/ble_services/ble_nus \
  $(SDK_ROOT)/components/ble/ble_services/ble_nus_c \
  $(SDK_ROOT)/components/ble/ble_services/ble_rscs \
  $(SDK_ROOT)/components/ble/ble_services/ble_rscs_c \
  $(SDK_ROOT)/components/ble/ble_services/ble_tps \
  $(SDK_ROOT)/components/ble/common \
  $(SDK_ROOT)/components/ble/nrf_ble_gatt \
  $(SDK_ROOT)/components/ble/nrf_ble_gq \
  $(SDK_ROOT)/components/ble/nrf_ble_qwr \
  $(SDK_ROOT)/components/ble/peer_manager \
  $(SDK_ROOT)/components/boards \
  $(SDK_ROOT)/components/libraries/atomic \
  $(SDK_ROOT)/components/libraries/atomic_fifo \
  $(SDK_ROOT)/components/libraries/atomic_flags \
  $(SDK_ROOT)/components/libraries/balloc \
  $(SDK_ROOT)/components/libraries/bootloader/ \
  $(SDK_ROOT)/components/libraries/bootloader/ble_dfu \
  $(SDK_ROOT)/components/libraries/bootloader/ble_dfu/ \
  $(SDK_ROOT)/components/libraries/bootloader/dfu \
  $(SDK_ROOT)/components/libraries/bsp \
  $(SDK_ROOT)/components/libraries/button \
  $(SDK_ROOT)/components/libraries/cli \
  $(SDK_ROOT)/components/libraries/crc16 \
  $(SDK_ROOT)/components/libraries/crc32 \
  $(SDK_ROOT)/components/libraries/crypto \
  $(SDK_ROOT)/components/libraries/crypto/backend/cc310 \
  $(SDK_ROOT)/components/libraries/crypto/backend/cc310_bl \
  $(SDK_ROOT)/components/libraries/crypto/backend/cifra \
  $(SDK_ROOT)/components/libraries/crypto/backend/mbedtls \
  $(SDK_ROOT)/components/libraries/crypto/backend/micro_ecc \
  $(SDK_ROOT)/components/libraries/crypto/backend/nrf_hw \
  $(SDK_ROOT)/components/libraries/crypto/backend/nrf_sw \
  $(SDK_ROOT)/components/libraries/crypto/backend/oberon \
  $(SDK_ROOT)/components/libraries/crypto/backend/optiga \
  $(SDK_ROOT)/components/libraries/csense \
  $(SDK_ROOT)/components/libraries/csense_drv \
  $(SDK_ROOT)/components/libraries/delay \
  $(SDK_ROOT)/components/libraries/ecc \
  $(SDK_ROOT)/components/libraries/experimental_section_vars \
  $(SDK_ROOT)/components/libraries/experimental_task_manager \
  $(SDK_ROOT)/components/libraries/fds \
  $(SDK_ROOT)/components/libraries/fifo \
  $(SDK_ROOT)/components/libraries/fstorage \
  $(SDK_ROOT)/components/libraries/gfx \
  $(SDK_ROOT)/components/libraries/gpiote \
  $(SDK_ROOT)/components/libraries/hardfault \
  $(SDK_ROOT)/components/libraries/hci \
  $(SDK_ROOT)/components/libraries/led_softblink \
  $(SDK_ROOT)/components/libraries/libuarte \
  $(SDK_ROOT)/components/libraries/log \
  $(SDK_ROOT)/components/libraries/log/src \
  $(SDK_ROOT)/components/libraries/low_power_pwm \
  $(SDK_ROOT)/components/libraries/memobj \
  $(SDK_ROOT)/components/libraries/mpu \
  $(SDK_ROOT)/components/libraries/mutex \
  $(SDK_ROOT)/components/libraries/pwr_mgmt \
  $(SDK_ROOT)/components/libraries/queue \
  $(SDK_ROOT)/components/libraries/ringbuf \
  $(SDK_ROOT)/components/libraries/scheduler \
  $(SDK_ROOT)/components/libraries/sdcard \
  $(SDK_ROOT)/components/libraries/sensorsim \
  $(SDK_ROOT)/components/libraries/slip \
  $(SDK_ROOT)/components/libraries/sortlist \
  $(SDK_ROOT)/components/libraries/spi_mngr \
  $(SDK_ROOT)/components/libraries/stack_guard \
  $(SDK_ROOT)/components/libraries/stack_info \
  $(SDK_ROOT)/components/libraries/strerror \
  $(SDK_ROOT)/components/libraries/svc \
  $(SDK_ROOT)/components/libraries/timer \
  $(SDK_ROOT)/components/libraries/twi_mngr \
  $(SDK_ROOT)/components/libraries/twi_sensor \
  $(SDK_ROOT)/components/libraries/uart \
  $(SDK_ROOT)/components/libraries/usbd \
  $(SDK_ROOT)/components/libraries/usbd/class/audio \
  $(SDK_ROOT)/components/libraries/usbd/class/cdc \
  $(SDK_ROOT)/components/libraries/usbd/class/cdc/acm \
  $(SDK_ROOT)/components/libraries/usbd/class/hid \
  $(SDK_ROOT)/components/libraries/usbd/class/hid/generic \
  $(SDK_ROOT)/components/libraries/usbd/class/hid/kbd \
  $(SDK_ROOT)/components/libraries/usbd/class/hid/mouse \
  $(SDK_ROOT)/components/libraries/usbd/class/msc \
  $(SDK_ROOT)/components/libraries/util \
  $(SDK_ROOT)/components/nfc/ndef/conn_hand_parser \
  $(SDK_ROOT)/components/nfc/ndef/conn_hand_parser/ac_rec_parser \
  $(SDK_ROOT)/components/nfc/ndef/conn_hand_parser/ble_oob_advdata_parser \
  $(SDK_ROOT)/components/nfc/ndef/conn_hand_parser/le_oob_rec_parser \
  $(SDK_ROOT)/components/nfc/ndef/connection_handover/ac_rec \
  $(SDK_ROOT)/components/nfc/ndef/connection_handover/ble_oob_advdata \
  $(SDK_ROOT)/components/nfc/ndef/connection_handover/ble_pair_lib \
  $(SDK_ROOT)/components/nfc/ndef/connection_handover/ble_pair_msg \
  $(SDK_ROOT)/components/nfc/ndef/connection_handover/common \
  $(SDK_ROOT)/components/nfc/ndef/connection_handover/ep_oob_rec \
  $(SDK_ROOT)/components/nfc/ndef/connection_handover/hs_rec \
  $(SDK_ROOT)/components/nfc/ndef/connection_handover/le_oob_rec \
  $(SDK_ROOT)/components/nfc/ndef/generic/message \
  $(SDK_ROOT)/components/nfc/ndef/generic/record \
  $(SDK_ROOT)/components/nfc/ndef/launchapp \
  $(SDK_ROOT)/components/nfc/ndef/parser/message \
  $(SDK_ROOT)/components/nfc/ndef/parser/record \
  $(SDK_ROOT)/components/nfc/ndef/text \
  $(SDK_ROOT)/components/nfc/ndef/uri \
  $(SDK_ROOT)/components/nfc/platform \
  $(SDK_ROOT)/components/nfc/t2t_lib \
  $(SDK_ROOT)/components/nfc/t2t_parser \
  $(SDK_ROOT)/components/nfc/t4t_lib \
  $(SDK_ROOT)/components/nfc/t4t_parser/apdu \
  $(SDK_ROOT)/components/nfc/t4t_parser/cc_file \
  $(SDK_ROOT)/components/nfc/t4t_parser/hl_detection_procedure \
  $(SDK_ROOT)/components/nfc/t4t_parser/tlv \
  $(SDK_ROOT)/components/softdevice/common \
  $(SDK_ROOT)/components/softdevice/s140/headers \
  $(SDK_ROOT)/components/softdevice/s140/headers/nrf52 \
  $(SDK_ROOT)/components/toolchain/cmsis/include \
  $(SDK_ROOT)/external/fprintf \
  $(SDK_ROOT)/external/mbedtls/include \
  $(SDK_ROOT)/external/nrf_tls/mbedtls/nrf_crypto/config \
  $(SDK_ROOT)/external/segger_rtt \
  $(SDK_ROOT)/external/utf_converter \
  $(SDK_ROOT)/integration/nrfx \
  $(SDK_ROOT)/integration/nrfx/legacy \
  $(SDK_ROOT)/modules/nrfx \
  $(SDK_ROOT)/modules/nrfx/drivers/include \
  $(SDK_ROOT)/modules/nrfx/hal \
  $(SDK_ROOT)/modules/nrfx/mdk \
  $(PROJ_DIR)../external/st/ST25DV \

PROJ_INC_FOLDERS += \
  $(PROJ_DIR) \
  $(PROJ_DIR)/bsp \
  $(PROJ_DIR)/drivers/pmic_driver \
  $(PROJ_DIR)/drivers/ir_proximity_sensor_driver \
  $(PROJ_DIR)/drivers/nfc_driver \
  $(PROJ_DIR)/modules/general_control \
  $(PROJ_DIR)/modules/cap_detection \
  $(PROJ_DIR)/modules/dock_manager \
  $(PROJ_DIR)/modules/ring_sources \
  $(PROJ_DIR)/modules/ring_data_manager \
  $(PROJ_DIR)/modules/ble_control \
  $(PROJ_DIR)/modules/battery_manager \
  $(PROJ_DIR)/modules/tilt_detection \
  $(PROJ_DIR)/modules/dose_detection \
  $(PROJ_DIR)/modules/fds_manager \
  $(PROJ_DIR)/../common \
  $(PROJ_DIR)/../common/drivers/comms_driver \
  $(PROJ_DIR)/../common/drivers/i2c \
  $(PROJ_DIR)/../common/drivers/imu \
  $(PROJ_DIR)/../common/drivers/uart \
  $(PROJ_DIR)/../common/modules/debug \
  $(PROJ_DIR)/../common/modules/statemachine \
  $(PROJ_DIR)/../common/modules/system_time \
  $(PROJ_DIR)/../common/utils/crc \
  $(PROJ_DIR)/../common/utils/filter \
  $(PROJ_DIR)/../common/utils/message_protocol \
  $(PROJ_DIR)/../common/utils/queue \
  $(PROJ_DIR)/../common/utils/serial_link_protocol \
      
# Libraries common to all targets
LIB_FILES += 

# Optimization flags
OPT = -g3 #-Wno-comment -Wno-unused-variable #-Wno-unused-function#-w -Wl,-Map=output.map
# Uncomment the line below to enable link time optimization
#OPT += -flto

# C flags common to all targets
CFLAGS += $(OPT)
CFLAGS += -DNRF_DFU_HW_VERSION=$(HW_VERSION)
CFLAGS += -DHW_VERSION_STR=\"$(HW_VERSION)\"
CFLAGS += -DDEBUG_DISABLE_UART # Uncomment to disable UART debugging for production
CFLAGS += -DRING # Flag to to be used to distinguish between dock and ring when using common code
CFLAGS += -DDEBUG_INITIAL_LEVEL=DEBUG_LEVEL_DEBUG
CFLAGS += -DAPP_TIMER_V2
CFLAGS += -DAPP_TIMER_V2_RTC1_ENABLED
CFLAGS += -DBOARD_PCA10056		
CFLAGS += -DCONFIG_GPIO_AS_PINRESET
CFLAGS += -DFLOAT_ABI_HARD
CFLAGS += -DNRF52840_XXAA
CFLAGS += -DNRF52_PAN_74
CFLAGS += -DNRF_SD_BLE_API_VERSION=7
CFLAGS += -DS140
CFLAGS += -DSOFTDEVICE_PRESENT
CFLAGS += -mcpu=cortex-m4
CFLAGS += -mthumb -mabi=aapcs
CFLAGS += -std=c99
CFLAGS += -Wall # Enable most common warnings
CFLAGS += -Werror #Treat warnings as errors
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
# keep every function in a separate section, this allows linker to discard unused ones
CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
CFLAGS += -fno-builtin -fshort-enums
CFLAGS += -DCONFIG_NFCT_PINS_AS_GPIOS
CFLAGS += -DNRF_DFU_SVCI_ENABLED
CFLAGS += -DNRF_DFU_TRANSPORT_BLE=1
CFLAGS += -DDEBUG # Only for use during development

CFLAGS += -DMBEDTLS_CONFIG_FILE=\"$(PROJ_DIR)/mbedtls_config.h\"

# C++ flags common to all targets
CXXFLAGS += $(OPT)
# Assembler flags common to all targets
ASMFLAGS += -g3
ASMFLAGS += -mcpu=cortex-m4
ASMFLAGS += -mthumb -mabi=aapcs
ASMFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
ASMFLAGS += -DAPP_TIMER_V2
ASMFLAGS += -DAPP_TIMER_V2_RTC1_ENABLED
ASMFLAGS += -DBOARD_PCA10056					  
ASMFLAGS += -DCONFIG_GPIO_AS_PINRESET
ASMFLAGS += -DFLOAT_ABI_HARD
ASMFLAGS += -DNRF52840_XXAA
ASMFLAGS += -DNRF52_PAN_74
ASMFLAGS += -DNRF_DFU_SVCI_ENABLED
ASMFLAGS += -DNRF_DFU_TRANSPORT_BLE=1
ASMFLAGS += -DNRF_SD_BLE_API_VERSION=7
ASMFLAGS += -DS140
ASMFLAGS += -DSOFTDEVICE_PRESENT

# Linker flags
LDFLAGS += $(OPT)
LDFLAGS += -mthumb -mabi=aapcs -L$(SDK_ROOT)/modules/nrfx/mdk -T$(LINKER_SCRIPT)
LDFLAGS += -mcpu=cortex-m4
LDFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
# let linker dump unused sections
LDFLAGS += -Wl,--gc-sections
# use newlib in nano version
LDFLAGS += --specs=nano.specs
LDFLAGS +=  -Wl,--print-memory-usage

nrf52840_xxaa: CFLAGS += -D__HEAP_SIZE=8192
nrf52840_xxaa: CFLAGS += -D__STACK_SIZE=8192
nrf52840_xxaa: ASMFLAGS += -D__HEAP_SIZE=8192
nrf52840_xxaa: ASMFLAGS += -D__STACK_SIZE=8192

# Add standard libraries at the very end of the linker input, after all objects
# that may need symbols provided by these libraries.
LIB_FILES += -lc -lnosys -lm

# Add additional flags to apply to project code which may be too strict for included third-party source
PROJECT_ADDITIONAL_CFLAGS := -Wextra # Enable additional warnings for project code only
PROJECT_ADDITIONAL_CFLAGS += -Wfloat-equal # Float equality checks
PROJECT_ADDITIONAL_CFLAGS += -Wundef # Uninitialized identifier used in if directive
PROJECT_ADDITIONAL_CFLAGS += -Wpointer-arith # Calculation depends on size of function or void
PROJECT_ADDITIONAL_CFLAGS += -Wcast-align # Pointer cast leads to alignment change
PROJECT_ADDITIONAL_CFLAGS += -Wstrict-prototypes # Function declared without argument types
PROJECT_ADDITIONAL_CFLAGS += -Wstrict-overflow=5 # Prevent compiler optimization based on assumption that arithmetic overflow does not occur
PROJECT_ADDITIONAL_CFLAGS += -Wwrite-strings # Enforce string length on copy
PROJECT_ADDITIONAL_CFLAGS += -Wcast-qual # Casting away pointer qualifiers
PROJECT_ADDITIONAL_CFLAGS += -Wswitch-default # Require default case on switches
PROJECT_ADDITIONAL_CFLAGS += -Wswitch-enum # Require cases for all values of enum used in a switch
PROJECT_ADDITIONAL_CFLAGS += -Wconversion # Implicit conversions
PROJECT_ADDITIONAL_CFLAGS += -Wunreachable-code # Unreachable code
PROJECT_ADDITIONAL_CFLAGS += -Wformat=2 # Additional checks on format strings
PROJECT_ADDITIONAL_CFLAGS += -Winit-self # Initalizing a variable with its own value
PROJECT_ADDITIONAL_CFLAGS += -Wpointer-arith # Additional checks for pointer arithmetic which may lead to undefined behaviour

SRC_FILES:= $(EXT_SRC_FILES) $(PROJ_SRC_FILES) 

.PHONY: default help

# Default target - first one defined
default: nrf52840_xxaa

# Print all targets that can be built
help:
	@echo following targets are available:
	@echo		nrf52840_xxaa
	@echo		flash_softdevice
	@echo		sdk_config - starting external tool for editing sdk_config.h
	@echo		flash      - flashing binary
	@echo    package    - generate complete DFU package ZIP

TEMPLATE_PATH := $(SDK_ROOT)/components/toolchain/gcc

# Custom adaptation of makefile.common
include Makefile.common

$(foreach target, $(TARGETS), $(call define_target, $(target)))

.PHONY: flash flash_softdevice erase

# Generate application hex and corresponding settings for the bootloader, merge them into one image, and flash
flash: default
	@echo Flashing: $(OUTPUT_DIRECTORY)/nrf52840_xxaa.hex
	nrfutil settings generate --family NRF52840 --application $(OUTPUT_DIRECTORY)/nrf52840_xxaa.hex --application-version-string $(APP_VERSION_STRING) --bootloader-version 0 --bl-settings-version 2 --app-boot-validation VALIDATE_ECDSA_P256_SHA256 --sd-boot-validation VALIDATE_ECDSA_P256_SHA256 --softdevice $(SOFTDEVICE_BINARY) --key-file $(PRIVATE_KEY_FILE) $(OUTPUT_DIRECTORY)/settings.hex
	mergehex -m $(OUTPUT_DIRECTORY)/nrf52840_xxaa.hex $(OUTPUT_DIRECTORY)/settings.hex -o $(OUTPUT_DIRECTORY)/mergedbin.hex
	nrfjprog --ip $(HOST_IP) -f nrf52 --program $(OUTPUT_DIRECTORY)/mergedbin.hex --sectorerase --verify
	nrfjprog --ip $(HOST_IP) -f nrf52 --reset
	
# Flash softdevice
flash_softdevice:
	@echo Flashing: s140_nrf52_7.2.0_softdevice.hex
	nrfjprog --ip $(HOST_IP) -f nrf52 --program $(SOFTDEVICE_BINARY) --sectorerase
	nrfjprog --ip $(HOST_IP) -f nrf52 --reset

reset:
	nrfjprog --ip $(HOST_IP) -f nrf52 --reset

erase:
	nrfjprog --ip $(HOST_IP) -f nrf52 --eraseall --log

package: default
	nrfutil pkg generate --app-boot-validation VALIDATE_ECDSA_P256_SHA256 --application $(OUTPUT_DIRECTORY)/nrf52840_xxaa.hex --application-version-string $(APP_VERSION_STRING) --hw-version $(HW_VERSION) --sd-req 0x100 --key-file $(PRIVATE_KEY_FILE) $(OUTPUT_DIRECTORY)/validose_ophthalmic_hw_$(HW_VERSION)_app_$(APP_VERSION_STRING).zip	

SDK_CONFIG_FILE := bsp/sdk_config.h 
CMSIS_CONFIG_TOOL := $(SDK_ROOT)/external_tools/cmsisconfig/CMSIS_Configuration_Wizard.jar
sdk_config:
	java -jar $(CMSIS_CONFIG_TOOL) $(SDK_CONFIG_FILE)

new:  erase  flash_softdevice  flash

clean:
	find ../ -name "*.gcda" -delete
	find ../ -name "*.gcno" -delete
	find ../ -name "*.o" -delete
