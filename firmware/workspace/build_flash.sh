#!/bin/bash

DEVELOP_MODE=0
TEST_MODE=0
BUILD_ONLY_MODE=0

TARGET_NAME="dock"  # default target
JOBS=20
PUBLIC_KEY_DEST="./firmware/bootloader/src/public_key.c"

# Detect CI environment
IS_CI=0
if [ "$TF_BUILD" == "True" ] || [ ! -z "$CI" ]; then
    IS_CI=1
fi

# Parse first argument as target if valid
if [[ "$1" == "dock" || "$1" == "ring" || "$1" == "all" ]]; then
    TARGET_NAME=$1
    shift
fi

# Check flags
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -d|--dev) DEVELOP_MODE=1 ;;
        -t|--test) TEST_MODE=1 ;;
        -b|--build) BUILD_ONLY_MODE=1 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
    shift
done

# CI-safe: build all targets only
if [ "$TARGET_NAME" == "all" ]; then
    ./build_flash.sh dock --build
    ./build_flash.sh ring --build
    exit 0
fi

# BUILD ONLY MODE (skip flashing, key provisioning etc.)
if [ "$BUILD_ONLY_MODE" -eq 1 ]; then
    echo ">>>>>>>>>>>>>>>>>>>>>>Build App: $TARGET_NAME ..."
    cd ./firmware/${TARGET_NAME}/
    make -j"$JOBS" clean
    make -j"$JOBS"
    exit 0
fi

# FLASH MODE (not executed in CI)
HOST_IP=$(/workspace/tools/get_host_ip.sh)

set -e
set +x
echo off

echo ">>>>>>>>>>>>>>>>>>>>>>Regenerating bootloader public key..."
if [ -f "$PUBLIC_KEY_DEST" ]; then
    rm -f "$PUBLIC_KEY_DEST"
fi

nrfutil keys display --key pk --format code "$PRIVATE_KEY_PATH" --out_file $PUBLIC_KEY_DEST

echo ">>>>>>>>>>>>>>>>>>>>>>Erase target flash..."
echo "Using debugger IP: $HOST_IP"
nrfjprog --ip ${HOST_IP} --recover

echo ">>>>>>>>>>>>>>>>>>>>>>Build and flash bootloader & softdevice..."
cd ./firmware/bootloader/
make -j"$JOBS" CONFIGURATION=release erase
make -j"$JOBS" CONFIGURATION=release clean
make -j"$JOBS" CONFIGURATION=release flash
make -j"$JOBS" CONFIGURATION=release flash_softdevice

echo ">>>>>>>>>>>>>>>>>>>>>>Provision BLE pairing passkey..."
cd ../../tools/
dos2unix provision_pin.sh
if [ "$TEST_MODE" -eq 1 ]; then
    echo "Using test mode BLE pairing passkey..."
    ./provision_pin.sh -t
else
    echo "Using randomly generated BLE pairing passkey..."
    ./provision_pin.sh
fi

nrfjprog --ip ${HOST_IP} --reset

echo ">>>>>>>>>>>>>>>>>>>>>>Build and flash app: $TARGET_NAME ..."
cd ../firmware/${TARGET_NAME}/
make -j"$JOBS" clean
make -j"$JOBS" flash

if [ "$DEVELOP_MODE" -ne 1 ]; then
    echo "Enabling readback protection..."
    nrfjprog --ip ${HOST_IP} --rbp ALL
    echo "WARNING: readback protection is enabled! Use -d to disable."
fi