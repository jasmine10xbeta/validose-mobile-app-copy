#!/bin/bash

# Remove readback protection and erase the chip
nrfjprog --ip $(HOST_IP) --recover

# Read the two words from FICR registers
deviceaddr0=$(nrfjprog --ip $(HOST_IP) --memrd 0x100000A4 --n 4 | awk '{print $2}')
deviceaddr1=$(nrfjprog --ip $(HOST_IP) --memrd 0x100000A8 --n 4 | awk '{print $2}')

# Extract bytes using bitwise operations
byte0=${deviceaddr0:6:2}
byte1=${deviceaddr0:4:2}
byte2=${deviceaddr0:2:2}
byte3=${deviceaddr0:0:2}
byte4=${deviceaddr1:6:2}
byte5=${deviceaddr1:4:2}
byte5=$(printf "%02X" $((0x$byte5 | 0xC0)))

# Print MAC in standard format
echo "$byte5:$byte4:$byte3:$byte2:$byte1:$byte0"
