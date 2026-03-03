# Secure Bootloader - BLE (sbl_ble)

The `sbl_ble` project implements a secure bootloader and DFU function for an nRF52840-based device using a BLE transport. It is based on the Nordic Semiconductor [BLE Secure DFU Bootloader](https://docs.nordicsemi.com/bundle/sdk_nrf5_v17.1.0/page/ble_sdk_app_dfu_bootloader.html) example in the nRF SDK.

The bootloader will enter DFU mode if either of these conditions are true after a reset:
- there is no valid application.
- `GPREGRET` contains the value `0xB1`.

## Building

Three build configurations are defined:
- `release`: Excludes logging and tests. Optimized for size. The `main()` function is in `src/main.c`. 
- `debug`: Includes logging and is optimized for debugging. The `main()` function is in `src/main.c`. 
- `test`: Includes logging and tests. Optimized for debugging. The `main()` function is in `test/test.c`.

To build a configuration using `make` enter:
``` sh
make CONFIGURATION=config
```
where `sdk` is the path to your nRF5 SDK17.1.0 installation and `config` is one of `release`, `debug`, or `test`.  

## Flashing

To flash a configuration using `make` enter:
``` sh
make CONFIGURATION=config erase # ensure UICR, MBR, and MBR parameter storage are erased.
make CONFIGURATION=config flash # flash the bootloader and the bootloader settings.
make CONFIGURATION=config flash_softdevice # flash the softdevice.
```
where `sdk` is the path to your nRF5 SDK17.1.0 installation and `config` is one of `release`, `debug`, or `test`.  

After erasing and flashing, there will be no application on the device and the bootloader will enter DFU mode to receive an application or bootloader package with version > 0.

## Application Requirements

To prepare an application for use with `sbl_ble`:
- The application must not use the FLASH memory region above 0xF7000 with release BL or 0xF0000 with debug BL.
- The application must provide a means to enter DFU mode as follows:
``` C
void on_dfu_trigger()
{
    nrf_power_gpregret_set(0xB1);
    nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_RESET);
}
```
## Generating DFU Packages

### Keys

A private-public cryptographic key pair is required to sign and verify DFU packages.  See [Working with keys](https://docs.nordicsemi.com/bundle/sdk_nrf5_v17.1.0/page/lib_bootloader_dfu_keys.html) for instructions, and [Signature verification](https://docs.nordicsemi.com/bundle/sdk_nrf5_v17.1.0/page/lib_bootloader_dfu_validation.html#lib_bootloader_signatures) for more information about signatures.

This project expects the public key to be in the file `./src/public_key.c`.

### Application DFU Package
The secure bootloader will only accept application DFU packages that:
- have been correctly signed using the private key from which `./src/public_key.c` was generated;
- match the hardware version (e.g. 3);
- match the SoftDevice ID (0x101); and
- have an application version number greater than that of the application already on the device.

For example, to generate a signed DFU package for `application.hex', version 1.0.0:
``` sh
nrfutil pkg generate
    --app-boot-validation VALIDATE_ECDSA_P256_SHA256 # The method the SBL uses to validate the application.
    --hw-version 3 # The hardware version
    --sd-req 0x100 # The id for s140_nrf52_7.2.0_softdevice.
    --key-file private.pem # The private key.
    --application application.hex # The application binary.
    --application-version-string 1.0.0 # The application version. 
    application_v1.0.0.zip
```
### Bootloader + SoftDevice DFU Package
Similarly, the secure bootloader will only accept bootloader and SoftDevice DFU packages that:
- have been correctly signed using the private key from which `./src/public_key.c` was generated;
- match the hardware version (e.g. 3);
- match the SoftDevice ID (0x101); and
- have a bootloader version number greater than that of the bootloader already on the device.

For example, to generate a signed DFU package for `sbl_ble.hex' + `s140_nrf52_7.2.0_softdevice.hex`, bootloader version 1:

``` sh
nrfutil pkg generate
    --hw-version 3
    --sd-req 0x100
    --sd-boot-validation VALIDATE_ECDSA_P256_SHA256
    --key-file private.pem # The private key.
    --softdevice s140_nrf52_7.2.0_softdevice.hex
    --bootloader sbl_ble.hex
    --bootloader-version 1
    sbl_ble_v1.zip
```

See [here](https://docs.nordicsemi.com/bundle/nrfutil/page/guides-nrf5sdk/dfu_generating_packages.html) for more information on generating DFU packages.

## Loading DFU Packages

DFU packages can be sent to the SBL over BLE from a Windows/Linux/Mac desktop using the [Bluetooth Low Energy app](https://docs.nordicsemi.com/bundle/nrf-connect-ble/page/index.html) in [NRF Connect v5.1.0](https://github.com/NordicSemiconductor/pc-nrfconnect-launcher/releases) or from the Android-only [NRF Connect for Mobile app](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-mobile).

## Example DFU Packages

Example application and bootloader DFU packages are provided in `./test`. These packages have been signed with the private key corresponding to the public key in `./src/public_key.c`.

`inc_hrs` is based on the nRF5 SDK `ble_app_hrs` example, and simply calls `dfu_trigger()` (see [Application Requirements](#application-requirements) above) when `P1.6` goes low.

## Tools

The following tools were used to build and test this project:
- [xPack Windows Build Tools v4.4.0-1](https://xpack-dev-tools.github.io/windows-build-tools-xpack/) ([docs](https://xpack-dev-tools.github.io/windows-build-tools-xpack/docs/getting-started/))
- [GCC ARM Embedded 9.2020-q2.major](https://developer.arm.com/downloads/-/gnu-rm#panel4a)
- [nRF Util](https://www.nordicsemi.com/Products/Development-tools/nRF-Util) ([docs](https://docs.nordicsemi.com/bundle/nrfutil/page/README.html))
- [NRF Connect for Desktop v5.1.0](https://github.com/NordicSemiconductor/pc-nrfconnect-launcher/releases) ([docs](https://docs.nordicsemi.com/bundle/nrf-connect-desktop/page/index.html)) or [NRF Connect for Mobile](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-mobile)
- [Eclipse IDE for Embedded C/C++ Developers 2024-09](https://www.eclipse.org/downloads/packages/release/2024-09/r/eclipse-ide-embedded-cc-developers) (optional).
- [Segger J-Link RTT Viewer v8.10g](https://www.segger.com/products/debug-probes/j-link/tools/rtt-viewer/) - for viewing debug and test logging output.
- [Segger J-Link](https://www.segger.com/products/debug-probes/j-link/?utm_medium=top_menu&utm_source=www) - for debugging and flash programming.

## OTS Software Dependencies

The `sbl_ble` project depends on the following off-the-shelf software: 
- [nRF5 SDK 17.1.0][1] ([docs](https://docs.nordicsemi.com/bundle/sdk_nrf5_v17.1.0/page/index.html))
- [ARM libnrf_cc310_bl_0.9.13](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrfxlib/crypto/doc/nrf_cc310_bl.html) - included as a library in nRF5 SDK.
- [SoftDevice S140 v7.2.0][1] ([specification](https://docs.nordicsemi.com/bundle/sds_s140/page/SDS/s1xx/s140.html))

The `test` build additionally makes use of [Unity Test](https://github.com/ThrowTheSwitch/Unity/tree/master).

See [LICENSES](LICENSES.md) for license information. 

## Release History

### v0.1.0
- Initial release.

[1]:https://www.nordicsemi.com/Products/Development-software/nrf5-sdk/download