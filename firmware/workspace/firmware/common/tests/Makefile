# Compiler and flags
CC = gcc -fprofile-arcs -ftest-coverage -fno-inline -g -DUNIT_TEST
CXX = g++
CXXFLAGS = -std=c++14 -g
LDFLAGS = -lgtest -lgtest_main -lpthread -fprofile-arcs -fno-inline -ftest-coverage

# Source files
ROOT_DIR := ../..
TEST_DIR := .
BIN_DIR := $(TEST_DIR)/bin
TEST_OUTPUT_DIR = ./test-reports
XML_OUTPUT_FILE = $(TEST_OUTPUT_DIR)/test_results.xml

# Source and include directories
MOCK_DIR := $(TEST_DIR)/mock
C_SRC_DIRS := $(MOCK_DIR) \
 	$(ROOT_DIR)/common \
	$(ROOT_DIR)/common/drivers/comms_driver \
	$(ROOT_DIR)/common/utils/queue \
	$(ROOT_DIR)/common/utils/message_protocol \
	$(ROOT_DIR)/common/modules/system_time \
	$(ROOT_DIR)/common/modules/debug \
	$(ROOT_DIR)/common/utils/crc \
	$(ROOT_DIR)/common/utils/queue \

C_INC_DIRS := $(MOCK_DIR) $(C_SRC_DIRS) \
	$(ROOT_DIR)/common \
	$(ROOT_DIR)/common/drivers/comms_driver \
	$(ROOT_DIR)/common/utils/queue \
	$(ROOT_DIR)/common/utils/message_protocol \
	$(ROOT_DIR)/common/modules/system_time \
	$(ROOT_DIR)/common/modules/debug \
	$(ROOT_DIR)/common/utils/crc \
	$(ROOT_DIR)/common/utils/queue \

CPP_INC_DIRS := /usr/include/gtest \
	/usr/include \
	$(MOCK_DIR) \
	$(ROOT_DIR)/common \
	$(ROOT_DIR)/common/drivers/comms_driver \
	$(ROOT_DIR)/common/utils/queue \
	$(ROOT_DIR)/common/utils/message_protocol \
	$(ROOT_DIR)/common/modules/system_time \
	$(ROOT_DIR)/common/modules/debug \
	$(ROOT_DIR)/common/utils/crc \
	$(ROOT_DIR)/common/utils/queue \

C_SOURCES := $(foreach dir,$(C_SRC_DIRS),$(wildcard $(dir)/*.c))
CPP_SOURCES = $(wildcard $(TEST_DIR)/*.cpp)

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

# Compile C source files
%.o: %.c
	$(CC) $(C_INCLUDES) -c $< -o $@

# Compile C++ source files
%.o: %.cpp
	$(CXX) $(CPP_INCLUDES) $(CXXFLAGS) -c $< -o $@

# Link object files
$(TARGET): $(C_OBJS) $(CPP_OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS)

run: $(TARGET)
	$(TARGET) --gtest_output=xml:$(XML_OUTPUT_FILE)

print_inc_dirs:
	@for dir in $(CPP_INC_DIRS); do \
		printf "%s\n" "$$(realpath $$dir)"; \
	done

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
