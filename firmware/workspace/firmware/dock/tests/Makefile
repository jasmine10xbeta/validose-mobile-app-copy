# Compiler and flags
CC = gcc -fprofile-arcs -ftest-coverage -fno-inline -g
CXX = g++
CXXFLAGS = -std=c++14 -g
LDFLAGS = -lgtest -lgtest_main -lpthread -fprofile-arcs -fno-inline -ftest-coverage

# Source files
PROJ_GROUP_BASE_DIR := ../..
ROOT_DIR := ..
TEST_DIR := .
BIN_DIR := $(TEST_DIR)/bin
TEST_OUTPUT_DIR = ./test-reports
XML_OUTPUT_FILE = $(TEST_OUTPUT_DIR)/test_results.xml

C_SRC_DIRS := $(TEST_DIR)/mock/lib \
	$(PROJ_GROUP_BASE_DIR)/common/modules/debug \
	$(PROJ_GROUP_BASE_DIR)/common/modules/statemachine \
	$(PROJ_GROUP_BASE_DIR)/dock/modules/dose_scheduler \
	$(PROJ_GROUP_BASE_DIR)/dock/modules/general_control \
	$(PROJ_GROUP_BASE_DIR)/dock/modules/app_manager \
	$(PROJ_GROUP_BASE_DIR)/dock/modules/dock_data_manager \
	$(PROJ_GROUP_BASE_DIR)/common/utils \
	$(PROJ_GROUP_BASE_DIR)/common/utils/crc \
	$(PROJ_GROUP_BASE_DIR)/common/utils/message_protocol \
	$(PROJ_GROUP_BASE_DIR)/common/utils/queue \
	$(TEST_DIR)/mock/i2c_driver_mock \
	$(TEST_DIR)/mock/mock_serial_link_protocol \
	$(TEST_DIR)/mock/rtc_mock \
	$(TEST_DIR)/mock/spi_driver_mock \
	$(TEST_DIR)/mock/weight_sensor_mock \
	$(TEST_DIR)/mock/system_time_mock \
	$(TEST_DIR)/mock/generic_serial_link_mock \
	$(TEST_DIR)/mock \
	$(ROOT_DIR)/drivers/rtc_driver \
	$(ROOT_DIR)/bsp \
	$(PROJ_GROUP_BASE_DIR)/common/utils/serial_link_protocol \
	$(PROJ_GROUP_BASE_DIR)/dock/modules/dose_size_detection \

C_INC_DIRS := $(C_SRC_DIRS) \
	$(TEST_DIR)/mock/lib \
	$(TEST_DIR)/mock/i2c_driver_mock \
	$(TEST_DIR)/mock/rtc_mock \
	$(TEST_DIR)/mock/spi_driver_mock \
	$(TEST_DIR)/mock/system_time_mock \
	$(TEST_DIR)/mock/weight_sensor_mock \
	$(TEST_DIR)/mock/generic_serial_link_mock \
	$(TEST_DIR)/mock \
	$(TEST_DIR)/mock/mock_serial_link_protocol \
	$(ROOT_DIR)/drivers/rtc_driver \
	$(ROOT_DIR)/bsp \
	$(PROJ_GROUP_BASE_DIR)/common \
	$(PROJ_GROUP_BASE_DIR)/common/modules/statemachine \
	$(PROJ_GROUP_BASE_DIR)/dock/modules/weight_sensor \
	$(PROJ_GROUP_BASE_DIR)/dock/modules/dose_size_detection \
	$(PROJ_GROUP_BASE_DIR)/common/modules/system_time \
	$(PROJ_GROUP_BASE_DIR)/common/modules/debug \
	$(PROJ_GROUP_BASE_DIR)/common/modules/statemachine \
	$(PROJ_GROUP_BASE_DIR)/common/utils/crc \
	$(PROJ_GROUP_BASE_DIR)/dock/modules/dose_scheduler \
	$(PROJ_GROUP_BASE_DIR)/dock/modules/general_control \
	$(PROJ_GROUP_BASE_DIR)/dock/modules/app_manager \
	$(PROJ_GROUP_BASE_DIR)/dock/modules/dock_data_manager \
	$(PROJ_GROUP_BASE_DIR)/common/drivers/i2c \
	$(PROJ_GROUP_BASE_DIR)/common/drivers/spi \
	$(PROJ_GROUP_BASE_DIR)/common/utils \
	$(PROJ_GROUP_BASE_DIR)/common/drivers/comms_driver \
	$(PROJ_GROUP_BASE_DIR)/common/utils/message_protocol \
	$(PROJ_GROUP_BASE_DIR)/common/utils/queue \
	$(PROJ_GROUP_BASE_DIR)/common/utils/serial_link_protocol \
	$(PROJ_GROUP_BASE_DIR)/external/littlefs-2.11.2 \

CPP_INC_DIRS := /usr/include/gtest \
	/usr/include \
	$(TEST_DIR)/mock/lib \
	$(TEST_DIR)/mock/i2c_driver_mock \
	$(TEST_DIR)/mock/rtc_mock \
	$(TEST_DIR)/mock/spi_driver_mock \
	$(TEST_DIR)/mock/system_time_mock \
	$(TEST_DIR)/mock/weight_sensor_mock \
	$(TEST_DIR)/mock/generic_serial_link_mock \
	$(TEST_DIR)/mock/mock_serial_link_protocol \
	$(ROOT_DIR)/drivers/rtc_driver \
	$(ROOT_DIR)/bsp \
	$(PROJ_GROUP_BASE_DIR)/common/ \
	$(PROJ_GROUP_BASE_DIR)/common/modules/statemachine \
	$(PROJ_GROUP_BASE_DIR)/common/modules/system_time \
	$(PROJ_GROUP_BASE_DIR)/common/modules/debug \
	$(PROJ_GROUP_BASE_DIR)/dock/modules/weight_sensor \
	$(PROJ_GROUP_BASE_DIR)/dock/modules/dose_size_detection \
	$(PROJ_GROUP_BASE_DIR)/common/utils/crc \
	$(PROJ_GROUP_BASE_DIR)/dock/modules/dose_scheduler \
	$(PROJ_GROUP_BASE_DIR)/dock/modules/general_control \
	$(PROJ_GROUP_BASE_DIR)/dock/modules/app_manager \
	$(PROJ_GROUP_BASE_DIR)/dock/modules/dock_data_manager \
	$(PROJ_GROUP_BASE_DIR)/common/drivers/i2c \
	$(PROJ_GROUP_BASE_DIR)/common/drivers/spi \
	$(PROJ_GROUP_BASE_DIR)/dock/drivers/ads1235 \
	$(PROJ_GROUP_BASE_DIR)/common/utils \
	$(PROJ_GROUP_BASE_DIR)/common/utils/queue \
	$(PROJ_GROUP_BASE_DIR)/common/utils/message_protocol \
	$(PROJ_GROUP_BASE_DIR)/common/drivers/comms_driver \
	$(PROJ_GROUP_BASE_DIR)/common/utils/serial_link_protocol \
	$(PROJ_GROUP_BASE_DIR)/external/littlefs-2.11.2 \

C_SOURCES := $(sort $(foreach dir,$(C_SRC_DIRS),$(wildcard $(dir)/*.c)))
# Use the host-side system time mock in tests to avoid duplicate symbol definitions.
C_SOURCES := $(filter-out $(PROJ_GROUP_BASE_DIR)/common/modules/system_time/system_time.c,$(C_SOURCES))
# Exclude hardware-dependent module(s) that require Nordic headers not present in host tests.
C_SOURCES := $(filter-out $(PROJ_GROUP_BASE_DIR)/dock/modules/general_control/general_control.c,$(C_SOURCES))
# Exclude sandbox code
C_SOURCES := $(filter-out $(PROJ_GROUP_BASE_DIR)/dock/modules/general_control/sand_box_repo.c,$(C_SOURCES))
# Temporarily exclude system time test until it is updated in a future branch.
CPP_SOURCES = $(filter-out $(TEST_DIR)/test_filesystem.cpp $(TEST_DIR)/test_flash_driver_s25hl512t.cpp, $(wildcard $(TEST_DIR)/*.cpp))

# Object files
C_OBJS = $(C_SOURCES:.c=.o)
CPP_OBJS = $(CPP_SOURCES:.cpp=.o)

# Output executable
TARGET = $(BIN_DIR)/run_tests  

# Include directories
C_INCLUDES := $(addprefix -I,$(C_INC_DIRS))
CPP_INCLUDES := $(addprefix -I,$(CPP_INC_DIRS))

# Build rules
.PHONY: all run clean

# Default target
all: $(BIN_DIR) $(TARGET) run

# Build only target
buildonly: $(BIN_DIR) $(TARGET)

# Create the binary output directory
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Compile C sources
%.o: %.c
	$(CC) $(C_INCLUDES) -c $< -o $@

# Compile C++ sources
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPP_INCLUDES) -c $< -o $@

# Link the target executable
$(TARGET): $(C_OBJS) $(CPP_OBJS)
	$(CXX) $(CPP_OBJS) $(C_OBJS) -o $(TARGET) $(LDFLAGS)
	
# Run the executable after building
run: $(BIN_DIR) $(TARGET)
	./$(TARGET) --gtest_output=xml:$(XML_OUTPUT_FILE) 

print_inc_dirs:
	@for dir in $(CPP_INC_DIRS); do \
		printf "%s\n" "$$(realpath $$dir)"; \
	done

# Clean build files
clean:
	rm -f $(C_OBJS) $(CPP_OBJS) $(TARGET)
	rm -rf $(BIN_DIR)
	rm -rf $(TEST_OUTPUT_DIR)
	rm -rf $(ROOT_DIR)/tests/coverage_report
	find ../../common -type f -name '*.gcda' -delete
	find ../../common -type f -name '*.gcno' -delete
	find ../../dock -type f -name '*.gcda' -delete
	find ../../dock -type f -name '*.gcno' -delete
	find ../../external -type f -name '*.gcda' -delete
	find ../../external -type f -name '*.gcno' -delete
