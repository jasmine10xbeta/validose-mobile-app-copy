PROJECT_NAME := sbl_ble
# The Bootloader DFU package version.
BL_PACKAGE_VERSION_INT := 1
HW_VERSION_STRING := 1
TARGETS := $(PROJECT_NAME)
OUTPUT_DIRECTORY := build/$(CONFIGURATION)
SDK_ROOT := /opt/nRF5_SDK_17.1.0_ddde560
PRIVATE_KEY_FILE := $(PRIVATE_KEY_PATH)
PROJ_DIR := .
TEMPLATE_PATH := $(SDK_ROOT)/components/toolchain/gcc
LINKER_SCRIPT := ./build/$(CONFIGURATION).ld
ENTRY_MODULE := $(PROJ_DIR)/src/main.c

HOST_IP := $(shell /workspace/tools/get_host_ip.sh)

# Note: In Makefile.common, line 279 must be changed to: '%.S.o %.s.o:'.
include $(TEMPLATE_PATH)/Makefile.common
include ./build/$(CONFIGURATION).mk

# Source files common to all targets
SRC_FILES += $(ENTRY_MODULE)
SRC_FILES += $(PROJ_DIR)/src/sbl.c
SRC_FILES += $(PROJ_DIR)/src/board.c
SRC_FILES += $(PROJ_DIR)/src/public_key.c
SRC_FILES += $(SDK_ROOT)/modules/nrfx/mdk/gcc_startup_nrf52840.S
SRC_FILES += $(SDK_ROOT)/modules/nrfx/mdk/system_nrf52840.c
SRC_FILES += $(SDK_ROOT)/modules/nrfx/soc/nrfx_atomic.c
SRC_FILES += $(SDK_ROOT)/modules/nrfx/hal/nrf_nvmc.c
SRC_FILES += $(SDK_ROOT)/external/nano-pb/pb_decode.c
SRC_FILES += $(SDK_ROOT)/external/nano-pb/pb_common.c
SRC_FILES += $(SDK_ROOT)/external/fprintf/nrf_fprintf_format.c
SRC_FILES += $(SDK_ROOT)/external/fprintf/nrf_fprintf.c
SRC_FILES += $(SDK_ROOT)/components/softdevice/common/nrf_sdh_soc.c
SRC_FILES += $(SDK_ROOT)/components/softdevice/common/nrf_sdh_ble.c
SRC_FILES += $(SDK_ROOT)/components/softdevice/common/nrf_sdh.c
SRC_FILES += $(SDK_ROOT)/components/libraries/util/nrf_assert.c
SRC_FILES += $(SDK_ROOT)/components/libraries/util/app_util_platform.c
SRC_FILES += $(SDK_ROOT)/components/libraries/util/app_error_weak.c
SRC_FILES += $(SDK_ROOT)/components/libraries/svc/nrf_svc_handler.c
SRC_FILES += $(SDK_ROOT)/components/libraries/strerror/nrf_strerror.c
SRC_FILES += $(SDK_ROOT)/components/libraries/sha256/sha256.c
SRC_FILES += $(SDK_ROOT)/components/libraries/scheduler/app_scheduler.c
SRC_FILES += $(SDK_ROOT)/components/libraries/ringbuf/nrf_ringbuf.c
SRC_FILES += $(SDK_ROOT)/components/libraries/queue/nrf_queue.c
SRC_FILES += $(SDK_ROOT)/components/libraries/memobj/nrf_memobj.c
SRC_FILES += $(SDK_ROOT)/components/libraries/mem_manager/mem_manager.c
SRC_FILES += $(SDK_ROOT)/components/libraries/log/src/nrf_log_str_formatter.c
SRC_FILES += $(SDK_ROOT)/components/libraries/log/src/nrf_log_frontend.c
SRC_FILES += $(SDK_ROOT)/components/libraries/fstorage/nrf_fstorage_sd.c
SRC_FILES += $(SDK_ROOT)/components/libraries/fstorage/nrf_fstorage_nvmc.c
SRC_FILES += $(SDK_ROOT)/components/libraries/fstorage/nrf_fstorage.c
SRC_FILES += $(SDK_ROOT)/components/libraries/experimental_section_vars/nrf_section_iter.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_shared.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_init.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_hash.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_ecdsa.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_ecc.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/backend/cc310_bl/cc310_bl_backend_ecc.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/backend/cc310_bl/cc310_bl_backend_ecdsa.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/backend/cc310_bl/cc310_bl_backend_hash.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/backend/cc310_bl/cc310_bl_backend_init.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/backend/cc310_bl/cc310_bl_backend_shared.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crc32/crc32.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/nrf_bootloader_wdt.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/nrf_bootloader_info.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/nrf_bootloader_fw_activation.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/nrf_bootloader_dfu_timers.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/nrf_bootloader_app_start_final.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/nrf_bootloader_app_start.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/nrf_bootloader.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/dfu/nrf_dfu_ver_validation.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/dfu/nrf_dfu_validation.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/dfu/nrf_dfu_utils.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/dfu/nrf_dfu_transport.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/dfu/nrf_dfu_svci_handler.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/dfu/nrf_dfu_svci.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/dfu/nrf_dfu_settings_svci.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/dfu/nrf_dfu_settings.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/dfu/nrf_dfu_req_handler.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/dfu/nrf_dfu_mbr.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/dfu/nrf_dfu_handling_error.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/dfu/nrf_dfu_flash.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/dfu/nrf_dfu.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/dfu/dfu-cc.pb.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/ble_dfu/nrf_dfu_ble.c
SRC_FILES += $(SDK_ROOT)/components/libraries/balloc/nrf_balloc.c
SRC_FILES += $(SDK_ROOT)/components/libraries/atomic_fifo/nrf_atfifo.c
SRC_FILES += $(SDK_ROOT)/components/libraries/atomic/nrf_atomic.c
SRC_FILES += $(SDK_ROOT)/components/boards/boards.c
SRC_FILES += $(SDK_ROOT)/components/ble/common/ble_srv_common.c

# Include folders common to all targets
INC_FOLDERS += $(SDK_ROOT)/modules/nrfx/mdk
INC_FOLDERS += $(SDK_ROOT)/modules/nrfx/hal
INC_FOLDERS += $(SDK_ROOT)/modules/nrfx
INC_FOLDERS += $(SDK_ROOT)/integration/nrfx
INC_FOLDERS += $(SDK_ROOT)/components/libraries/mutex
INC_FOLDERS += $(SDK_ROOT)/external/nano-pb
INC_FOLDERS += $(SDK_ROOT)/external/fprintf
INC_FOLDERS += $(SDK_ROOT)/external/nrf_cc310/include
INC_FOLDERS += $(SDK_ROOT)/external/nrf_cc310_bl/include
INC_FOLDERS += $(SDK_ROOT)/components/toolchain/cmsis/include
INC_FOLDERS += $(SDK_ROOT)/components/softdevice/s132/headers/nrf52
INC_FOLDERS += $(SDK_ROOT)/components/softdevice/s132/headers
INC_FOLDERS += $(SDK_ROOT)/components/softdevice/common
INC_FOLDERS += $(SDK_ROOT)/components/libraries/util
INC_FOLDERS += $(SDK_ROOT)/components/libraries/svc
INC_FOLDERS += $(SDK_ROOT)/components/libraries/strerror
INC_FOLDERS += $(SDK_ROOT)/components/libraries/stack_info
INC_FOLDERS += $(SDK_ROOT)/components/libraries/sha256
INC_FOLDERS += $(SDK_ROOT)/components/libraries/scheduler
INC_FOLDERS += $(SDK_ROOT)/components/libraries/queue
INC_FOLDERS += $(SDK_ROOT)/components/libraries/memobj
INC_FOLDERS += $(SDK_ROOT)/components/libraries/mem_manager
INC_FOLDERS += $(SDK_ROOT)/components/libraries/log/src
INC_FOLDERS += $(SDK_ROOT)/components/libraries/log
INC_FOLDERS += $(SDK_ROOT)/components/libraries/fstorage
INC_FOLDERS += $(SDK_ROOT)/components/libraries/experimental_section_vars
INC_FOLDERS += $(SDK_ROOT)/components/libraries/delay
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crypto/backend/optiga
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crypto/backend/oberon
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crypto/backend/nrf_sw
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crypto/backend/nrf_hw
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crypto/backend/micro_ecc
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crypto/backend/mbedtls
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crypto/backend/cifra
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crypto/backend/cc310_bl
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crypto/backend/cc310
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crypto
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crc32
INC_FOLDERS += $(SDK_ROOT)/components/libraries/bootloader/dfu
INC_FOLDERS += $(SDK_ROOT)/components/libraries/bootloader/ble_dfu
INC_FOLDERS += $(SDK_ROOT)/components/libraries/bootloader
INC_FOLDERS += $(SDK_ROOT)/components/libraries/balloc
INC_FOLDERS += $(SDK_ROOT)/components/libraries/atomic_fifo
INC_FOLDERS += $(SDK_ROOT)/components/libraries/atomic
INC_FOLDERS += $(SDK_ROOT)/components/boards
INC_FOLDERS += $(SDK_ROOT)/components/ble/common
INC_FOLDERS += $(PROJ_DIR)/src
  
# Libraries common to all targets
LIB_FILES += $(SDK_ROOT)/external/nrf_cc310_bl/lib/cortex-m4/hard-float/libnrf_cc310_bl_0.9.13.a

# Defines
DEFS += -DBLE_STACK_SUPPORT_REQD
DEFS += -DBOARD_CUSTOM
DEFS += -DCONFIG_GPIO_AS_PINRESET
DEFS += -DFLOAT_ABI_HARD
DEFS += -DNRF52840_XXAA
DEFS += -DNRF_DFU_SETTINGS_VERSION=2
DEFS += -DNRF_DFU_SVCI_ENABLED
DEFS += -DNRF_SD_BLE_API_VERSION=7
DEFS += -DS140
DEFS += -DSOFTDEVICE_PRESENT
DEFS += -DSVC_INTERFACE_CALL_AS_NORMAL_FUNCTION
DEFS += -DUSE_APP_CONFIG

# C flags common to all targets
CFLAGS += $(OPT)
CFLAGS += -DNRF_DFU_HW_VERSION=$(HW_VERSION_STRING)
CFLAGS += -mcpu=cortex-m4
CFLAGS += -mthumb -mabi=aapcs
CFLAGS += -Wall -Werror -Wunused-but-set-parameter
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
CFLAGS += -fno-builtin -fshort-enums
CFLAGS += -D__HEAP_SIZE=0
CFLAGS += $(DEFS)

# Assembler flags common to all targets
ASMFLAGS += -g3
ASMFLAGS += -mcpu=cortex-m4
ASMFLAGS += -mthumb -mabi=aapcs
ASMFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
ASMFLAGS += -D__HEAP_SIZE=0
ASMFLAGS += $(DEFS)

# Linker flags
LDFLAGS += $(OPT)
LDFLAGS += -mthumb -mabi=aapcs -L$(SDK_ROOT)/modules/nrfx/mdk -T$(LINKER_SCRIPT)
LDFLAGS += -mcpu=cortex-m4
LDFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
LDFLAGS += -Wl,--gc-sections
LDFLAGS += --specs=nano.specs
LDFLAGS +=  -Wl,--print-memory-usage

# Add standard libraries at the very end of the linker input, after all objects
# that may need symbols provided by these libraries.
LIB_FILES += -lnosys -lm

.PHONY: default

BINARY := $(OUTPUT_DIRECTORY)/$(PROJECT_NAME).hex
MERGED_BINARY := $(OUTPUT_DIRECTORY)/$(PROJECT_NAME)_merged.hex
SOFTDEVICE_BINARY := $(SDK_ROOT)/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex

# Default target - first one defined
default: $(PROJECT_NAME) $(OUTPUT_DIRECTORY)/settings.hex
	mergehex -m $(BINARY) $(OUTPUT_DIRECTORY)/settings.hex -o $(MERGED_BINARY)
	
$(foreach target, $(TARGETS), $(call define_target, $(target)))

.PHONY: flash flash_softdevice erase

# Create the bootloader settings for bootloader version 0.
 $(OUTPUT_DIRECTORY)/settings.hex:
	nrfutil settings generate --family NRF52840 --bootloader-version 0 --bl-settings-version 2 --sd-boot-validation VALIDATE_ECDSA_P256_SHA256 --softdevice $(SOFTDEVICE_BINARY) --key-file $(PRIVATE_KEY_FILE) $(OUTPUT_DIRECTORY)/settings.hex
	
# Merge and flash the bootloader and settings.
flash:  default $(OUTPUT_DIRECTORY)/settings.hex
	@echo Flashing: $(BINARY)
	nrfjprog --ip $(HOST_IP) -f nrf52 --program $(MERGED_BINARY) --sectorerase --verify
	nrfjprog --ip $(HOST_IP) -f nrf52 --reset

# Flash the SoftDevice
flash_softdevice:
	@echo Flashing: s140_nrf52_7.2.0_softdevice.hex
	nrfjprog --ip $(HOST_IP) -f nrf52 --program $(SOFTDEVICE_BINARY) --sectorerase --verify
	nrfjprog --ip $(HOST_IP) -f nrf52 --reset

# Erase all flash
erase:
	nrfjprog --ip $(HOST_IP) -f nrf52 --eraseall

.PHONY: package test_packages settings size

# Create bootloader + SoftDevice signed DFU package.
package: default
	nrfutil pkg generate --bootloader $(BINARY) --bootloader-version $(BL_PACKAGE_VERSION_INT) --sd-boot-validation VALIDATE_ECDSA_P256_SHA256 --softdevice $(SOFTDEVICE_BINARY) --hw-version $(HW_VERSION_STRING) --sd-req 0x100 --key-file $(PRIVATE_KEY_FILE) $(OUTPUT_DIRECTORY)/sbl_ble_v1.zip

test_packages: default
	nrfutil pkg generate --bootloader $(BINARY) --bootloader-version 1 --sd-boot-validation VALIDATE_ECDSA_P256_SHA256 --softdevice $(SOFTDEVICE_BINARY) --hw-version $(HW_VERSION_STRING) --sd-req 0x100 --key-file $(PRIVATE_KEY_FILE) test/packages/sbl_ble_v1.zip
	nrfutil pkg generate --bootloader $(BINARY) --bootloader-version 2 --sd-boot-validation VALIDATE_ECDSA_P256_SHA256 --softdevice $(SOFTDEVICE_BINARY) --hw-version $(HW_VERSION_STRING) --sd-req 0x100 --key-file $(PRIVATE_KEY_FILE) test/packages/sbl_ble_v2.zip
	nrfutil pkg generate --bootloader $(BINARY) --bootloader-version 2 --sd-boot-validation VALIDATE_ECDSA_P256_SHA256 --softdevice $(SOFTDEVICE_BINARY) --hw-version 53 --sd-req 0x100 --key-file $(PRIVATE_KEY_FILE) test/packages/sbl_ble_v2_wrong_hw.zip
	nrfutil pkg generate --bootloader $(BINARY) --bootloader-version 2 --sd-boot-validation VALIDATE_ECDSA_P256_SHA256 --softdevice $(SOFTDEVICE_BINARY) --hw-version $(HW_VERSION_STRING) --sd-req 0xCB --key-file $(PRIVATE_KEY_FILE) test/packages/sbl_ble_v2_wrong_sd.zip
	nrfutil pkg generate --bootloader $(BINARY) --bootloader-version 2 --sd-boot-validation VALIDATE_ECDSA_P256_SHA256 --softdevice $(SOFTDEVICE_BINARY) --hw-version $(HW_VERSION_STRING) --sd-req 0x100 --key-file $(PRIVATE_KEY_FILE).wrong test/packages/sbl_ble_v2_wrong_key.zip
	nrfutil pkg generate --bootloader $(BINARY) --bootloader-version 2 --sd-boot-validation VALIDATE_GENERATED_SHA256 --softdevice $(SOFTDEVICE_BINARY) --hw-version $(HW_VERSION_STRING) --sd-req 0x100 --key-file $(PRIVATE_KEY_FILE) test/packages/sbl_ble_v2_wrong_validation.zip
	
# Create the bootloader settings.
settings: $(OUTPUT_DIRECTORY)/settings.hex
	nrfutil settings display $(OUTPUT_DIRECTORY)/settings.hex

size:
	$(SIZE) $(OUTPUT_DIRECTORY)/*.out