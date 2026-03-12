#!/bin/bash

# Default test mode to off
TEST_MODE=0
SAVE_TO_FILE=0
HOST_IP=`/workspace/tools/get_host_ip.sh`

# Check for flags
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -t|--test) TEST_MODE=1 ;;  # Enable test mode
        -s|--save) SAVE_TO_FILE=1 ;;  # Enable save to file mode
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
    shift
done

write_value_to_flash() {
	local CURRENT_ADDR=$1
	local VALUE_TO_WRITE=$2
	local VALUE_TO_WRITE_HEX_LENGTH=${#VALUE_TO_WRITE}

	for ((i=0; i<VALUE_TO_WRITE_HEX_LENGTH; i+=8)); do
		# Extract each 8 hex digits (representing 4 bytes)
		WORD=${VALUE_TO_WRITE:i:8}  # Get the next 4-byte word in hex
		REVERSED_WORD=$(echo $WORD | sed 's/\(..\)/\1 /g' | awk '{for(i=NF;i>=1;i--) printf "%s", $i} END {print ""}' | tr -d ' ')
		nrfjprog --ip $HOST_IP --memwr $CURRENT_ADDR --val 0x$REVERSED_WORD
		CURRENT_ADDR=$((CURRENT_ADDR + 4))  # Move to the next 4-byte address
	done
}

# Set the file paths
ENCRYPTION_KEY_FILE="encryption_key.bin"
ENCRYPTED_PIN_FILE="encrypted_pin.bin"
PIN_FILE="bluetooth_pin.txt"

# Set the flash addresses
USER_DATA_FLASH_BASE_ADDR=0x000FD000
ENCRYPTION_KEY_FLASH_ADDR=0x000FD000
ENCRYPTED_PIN_FLASH_ADDR=0x000FD020

# Generate a random 256-bit encryption key (32 bytes)
ENCRYPTION_KEY=$(openssl rand -hex 32)
if [ $? -ne 0 ]; then
   echo "Error: Failed to generate encryption key."
   exit 1
fi

# Optionally write encryption key to local file
if [ "$SAVE_TO_FILE" -eq 1 ]; then
   echo -n "$ENCRYPTION_KEY" | xxd -r -p > "$ENCRYPTION_KEY_FILE"
fi

# Set PIN based on TEST_MODE
if [ "$TEST_MODE" -eq 1 ]; then
    PIN=825852  # Fixed value
else
    # Generate a random 6-digit PIN
    PIN=$(shuf -i 000000-999999 -n 1 | awk '{printf "%06d\n", $1}')
fi

echo ">>>>>>>>>>>>> DEVICE PIN IS $PIN"

# Optionally write PIN to local file
if [ "$SAVE_TO_FILE" -eq 1 ]; then
   echo "$PIN" > "$PIN_FILE"
fi

# Encrypt the Bluetooth PIN using AES-256-CBC (IV all zero)
ENCRYPTED_PIN=$(echo -n "$PIN" | openssl enc -aes-256-cbc -K "$ENCRYPTION_KEY" -iv "00000000000000000000000000000000" | xxd -p)

if [ $? -ne 0 ]; then
    echo "Error: Failed to encrypt Bluetooth PIN."
    exit 1
fi

# Optionally write encrypted PIN to local file
if [ "$SAVE_TO_FILE" -eq 1 ]; then
   echo -n "$ENCRYPTED_PIN" | xxd -r -p > "$ENCRYPTED_PIN_FILE"
fi

# Erase user data
echo ">>>>>>>>>>>>> Erasing user data page"
nrfjprog --ip $HOST_IP --erasepage $USER_DATA_FLASH_BASE_ADDR

# Write encryption key
echo ">>>>>>>>>>>>> Writing encryption key to flash starting at address $ENCRYPTION_KEY_FLASH_ADDR."
write_value_to_flash $ENCRYPTION_KEY_FLASH_ADDR $ENCRYPTION_KEY
echo "Done!"

# Write encrypted pin
echo ">>>>>>>>>>>>> Writing encrypted pin to flash starting at address $ENCRYPTED_PIN_FLASH_ADDR."
write_value_to_flash $ENCRYPTED_PIN_FLASH_ADDR $ENCRYPTED_PIN
echo "Done!"
