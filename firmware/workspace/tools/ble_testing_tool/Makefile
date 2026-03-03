# Compiler and Flags
CC = gcc -g -Wall
LDFLAGS ?= 

# Output
BUILD_DIR = build
TARGET = $(BUILD_DIR)/main
LIB_TARGET = $(BUILD_DIR)/message_protocol_wrapper.so

# Source and include directories
FW_PROJ_DIR := ../../firmware/
C_SRC_DIRS = \
	. \
	$(FW_PROJ_DIR)common/utils/message_protocol \
	$(FW_PROJ_DIR)common/utils/crc \
	$(FW_PROJ_DIR)common/utils/queue \
	$(FW_PROJ_DIR)common/modules/system_time \
	$(FW_PROJ_DIR)common/drivers/comms_driver \
	$(FW_PROJ_DIR)common/utils/serial_link_protocol \
	$(FW_PROJ_DIR)common \
	$(FW_PROJ_DIR)common/tests/mock \
	$(FW_PROJ_DIR)common/modules/debug

C_INC_DIRS = $(C_SRC_DIRS)

# Source and Object files
C_SOURCES = $(foreach dir,$(C_SRC_DIRS),$(wildcard $(dir)/*.c))
# Function to map source file to object file in build dir, mirroring structure
define map_src_to_obj
$(BUILD_DIR)/$(subst ../,,$(dir $(1)))$(notdir $(basename $(1))).o
endef

# All objects (for main executable)
OBJS = $(foreach src,$(C_SOURCES),$(call map_src_to_obj,$(src)))
# For library: exclude main.c from sources/objects
LIB_SOURCES = $(filter-out ./main.c,$(C_SOURCES))
LIB_OBJS = $(foreach src,$(LIB_SOURCES),$(call map_src_to_obj,$(src)))

# Output Executable
TARGET = $(BUILD_DIR)/main

# Include directories
C_INCLUDES := $(addprefix -I,$(C_INC_DIRS))

# Build rules
.PHONY: all clean


all: $(BUILD_DIR) $(TARGET)

# Build shared library (ignores main.c)
.PHONY: library
library: $(BUILD_DIR) $(LIB_TARGET)

$(LIB_TARGET): $(LIB_OBJS)
	$(CC) -shared -fPIC $(CFLAGS) $(C_INCLUDES) $^ -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)


# General pattern rule to build object files in build dir, mirroring source structure
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(C_INCLUDES) -fPIC -c $< -o $@

# Catch-all pattern rule for any .c file in any subdirectory
$(BUILD_DIR)/%.o: ../%.c | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(C_INCLUDES) -fPIC -c $< -o $@

$(BUILD_DIR)/%.o: ../../%.c | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(C_INCLUDES) -fPIC -c $< -o $@

# Link all object files into the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(C_INCLUDES) $^ -o $@

clean:
	rm -rf $(BUILD_DIR)
	find $(FW_PROJ_DIR) -type f -name '*.o' -delete
	find . -type f -name '*.o' -delete

.PHONY: all clean