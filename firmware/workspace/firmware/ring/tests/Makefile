# Compiler and flags
CC = gcc -fprofile-arcs -fno-inline -ftest-coverage
CXX = g++
CXXFLAGS = -std=c++14
LDFLAGS = -lgtest -lgtest_main -lpthread -fprofile-arcs -fno-inline -ftest-coverage

# Source files
ROOT_DIR := ../..
TEST_DIR := .
BIN_DIR := $(TEST_DIR)/bin
TEST_OUTPUT_DIR = ./test-reports
XML_OUTPUT_FILE = $(TEST_OUTPUT_DIR)/test_results.xml

C_SRC_DIRS := $(TEST_DIR) \
 	$(TEST_DIR)/mock \
	$(TEST_DIR)/mock/lib \
 	$(TEST_DIR)/mock/imu \
	$(TEST_DIR)/mock/cap_detection \
 	$(TEST_DIR)/mock/ir_proximity_sensor_driver \
 	$(TEST_DIR)/mock/system_time \
	${TEST_DIR}/mock/tilt_detection \
	$(TEST_DIR)/mock/fds_manager \
	$(TEST_DIR)/mock/nfc_driver \
	${TEST_DIR}/mock/battery_manager \
	${TEST_DIR}/mock/ring_sources \
	${TEST_DIR}/mock/dock_data_manager \
	${TEST_DIR}/mock/ring_data_manager \
	${TEST_DIR}/mock/rtc_mock \
	$(ROOT_DIR)/common \
	$(ROOT_DIR)/common/modules/debug \
	$(ROOT_DIR)/common/modules/statemachine \
	$(ROOT_DIR)/common/modules/system_time \
	$(ROOT_DIR)/common/utils/queue \
	$(ROOT_DIR)/common/utils/crc \
	$(ROOT_DIR)/common/utils/message_protocol \
	$(ROOT_DIR)/ring/modules/cap_detection \
	$(ROOT_DIR)/ring/modules/dose_detection \
	${ROOT_DIR}/ring/modules/tilt_detection \
	${ROOT_DIR}/ring/modules/ring_data_manager \
	${ROOT_DIR}/ring/modules/dock_manager \
	${ROOT_DIR}/dock/modules/ring_manager \

C_INC_DIRS := $(C_SRC_DIRS) \
	$(TEST_DIR)/mock \
	$(TEST_DIR)/mock/lib \
 	$(TEST_DIR)/mock/imu \
 	$(TEST_DIR)/mock/system_time \
	$(TEST_DIR)/mock/fds_manager \
	$(TEST_DIR)/mock/nfc_driver \
	${TEST_DIR}/mock/battery_manager \
	${TEST_DIR}/mock/ring_sources \
	${TEST_DIR}/mock/dock_data_manager \
	${TEST_DIR}/mock/ring_data_manager \
	${TEST_DIR}/mock/rtc_mock \
	$(ROOT_DIR)/common \
	$(ROOT_DIR)/common/drivers/i2c \
	$(ROOT_DIR)/common/drivers/imu \
	$(ROOT_DIR)/common/drivers/comms_driver \
	$(ROOT_DIR)/common/modules/statemachine \
	$(ROOT_DIR)/common/modules/system_time \
	$(ROOT_DIR)/common/modules/debug \
	$(ROOT_DIR)/common/utils/queue \
	$(ROOT_DIR)/common/utils/crc \
	$(ROOT_DIR)/common/utils/message_protocol \
	$(ROOT_DIR)/ring/modules/cap_detection \
	$(ROOT_DIR)/ring/modules/ring_sources \
	$(ROOT_DIR)/ring/modules/dose_detection \
	$(ROOT_DIR)/ring/modules/tilt_detection \
	${ROOT_DIR}/ring/modules/ring_data_manager \
	${ROOT_DIR}/ring/modules/fds_manager \
	${ROOT_DIR}/ring/modules/battery_manager \
	${ROOT_DIR}/ring/drivers/nfc_driver \
	$(ROOT_DIR)/ring/drivers/ir_proximity_sensor_driver \
	${ROOT_DIR}/ring/modules/dock_manager \
	${ROOT_DIR}/dock/modules/dock_data_manager \
	${ROOT_DIR}/dock/modules/ring_manager \
	${ROOT_DIR}/dock/drivers/rtc_driver \
	
CPP_INC_DIRS := /usr/include/gtest \
   	/usr/include \
   	$(TEST_DIR)/mock \
	$(TEST_DIR)/mock/lib \
	$(TEST_DIR)/mock/imu \
 	$(TEST_DIR)/mock/system_time \
	$(TEST_DIR)/mock/fds_manager \
	$(TEST_DIR)/mock/nfc_driver \
	${TEST_DIR}/mock/battery_manager \
	${TEST_DIR}/mock/ring_sources \
	${TEST_DIR}/mock/dock_data_manager \
	${TEST_DIR}/mock/rtc_mock \
	${TEST_DIR}/mock/ring_data_manager \
	$(ROOT_DIR)/common \
	$(ROOT_DIR)/common/drivers/i2c \
	$(ROOT_DIR)/common/drivers/imu \
	$(ROOT_DIR)/common/drivers/comms_driver \
	$(ROOT_DIR)/common/modules/statemachine \
	$(ROOT_DIR)/common/modules/system_time \
	$(ROOT_DIR)/common/utils/queue \
	$(ROOT_DIR)/common/modules/debug \
	$(ROOT_DIR)/common/utils/crc \
	$(ROOT_DIR)/common/utils/message_protocol \
	$(ROOT_DIR)/ring/modules/dose_detection \
	$(ROOT_DIR)/ring/modules/ring_sources \
	$(ROOT_DIR)/ring/modules/cap_detection \
	${ROOT_DIR}/ring/modules/tilt_detection \
	${ROOT_DIR}/ring/modules/ring_data_manager \
	${ROOT_DIR}/ring/modules/fds_manager \
	${ROOT_DIR}/ring/modules/battery_manager \
	${ROOT_DIR}/ring/drivers/nfc_driver \
	$(ROOT_DIR)/ring/drivers/ir_proximity_sensor_driver \
	${ROOT_DIR}/ring/modules/dock_manager \
	${ROOT_DIR}/dock/modules/dock_data_manager \
	${ROOT_DIR}/dock/modules/ring_manager \
	${ROOT_DIR}/dock/drivers/rtc_driver \

C_SOURCES := $(foreach dir,$(C_SRC_DIRS),$(wildcard $(dir)/*.c))
# Temporarily exclude system time test until it is updated in a future branch.
CPP_SOURCES = $(filter-out $(TEST_DIR)/test_system_time.cpp,$(wildcard $(TEST_DIR)/*.cpp))

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

# Clean build files
clean:
	rm -f $(C_OBJS) $(CPP_OBJS) $(TARGET)
	rm -rf $(BIN_DIR)
	rm -rf $(TEST_OUTPUT_DIR)
	find ../ -name "*.gcda" -delete
	find ../ -name "*.gcno" -delete
	find ../../common -name "*.gcda" -delete
	find ../../common -name "*.gcno" -delete
	find ../../dock -name "*.gcda" -delete
	find ../../dock -name "*.gcno" -delete
	find ../ -name "*.o" -delete
