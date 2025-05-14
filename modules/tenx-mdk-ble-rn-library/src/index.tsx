import { NativeModules, Platform } from 'react-native';

const LINKING_ERROR =
  `The package 'react-native-tenx-mdk-ble' doesn't seem to be linked. Make sure: \n\n` +
  Platform.select({ ios: "- You have run 'pod install'\n", default: '' }) +
  '- You rebuilt the app after installing the package\n' +
  '- You are not using Expo Go\n';

// @ts-expect-error
const isTurboModuleEnabled = global.__turboModuleProxy != null;

const TenxMdkTenxMdkBle = isTurboModuleEnabled
  ? require('./NativeTenxMdkBle').default
  : NativeModules.TenxMdkBle;

const TenxMdkBle = TenxMdkTenxMdkBle
  ? TenxMdkTenxMdkBle
  : new Proxy(
      {},
      {
        get() {
          throw new Error(LINKING_ERROR);
        },
      }
    );

/**
 * Scan for BLE devices.
 * @param scanInterval - The duration for scanning in seconds. Default is 10 secs.
 * @returns A promise resolving to an array of discovered devices.
 */
export function scanLeDevice(
  scanInterval?: number | undefined,
): Promise<any[]> {
  return TenxMdkBle.scanLeDevice(scanInterval);
}

/**
 * Stop scan for BLE devices.
 * @returns A promise resolving to an array of discovered devices till that point in time.
 */
export function stopLeScan(): Promise<any[]> {
  return TenxMdkBle.stopLeScan();
}

/**
 * Connect to a BLE device.
 * @param deviceIdentifier - Either device address or name of the BLE device to connect to.
 * @returns A promise resolving to a boolean indicating success.
 */
export function connect(deviceIdentifier: string): Promise<boolean> {
  return TenxMdkBle.connect(deviceIdentifier);
}

export function getPrevConnectedDevices(): Promise<any> {
  return TenxMdkBle.getPrevConnectedDevices();
}

export function getConnectedDevice(): Promise<any> {
  return TenxMdkBle.getConnectedDevice();
}

export function isDeviceConnected(): Promise<any> {
  return TenxMdkBle.isDeviceConnected();
}

/**
 * Disconnect from the currently connected BLE device.
 * @returns A promise resolving to a boolean indicating success.
 */
export function disconnect(): Promise<boolean> {
  return TenxMdkBle.disconnect();
}

/**
 * Bond with a BLE device.
 * @param deviceAddress - Address of the BLE device to bond with.
 * @returns A promise resolving when the operation is complete.
 */
export function bondDevice(deviceAddress: string): Promise<any> {
  return TenxMdkBle.bondDevice(deviceAddress);
}

/**
 * Discover services and characteristics of the connected BLE device.
 * @returns A promise resolving to an array of services and characteristics.
 */
export function discoverServicesAndCharacteristics(): Promise<any> {
  return TenxMdkBle.discoverServicesAndCharacteristics();
}

/**
 * Read the value of a specific characteristic.
 * @param characteristicUUID - UUID of the characteristic.
 * @returns A promise resolving to the characteristic value.
 */
export function readCharacteristic(characteristicUUID: string): Promise<any> {
  return TenxMdkBle.readCharacteristic(characteristicUUID);
}

/**
 * Write a value to a specific characteristic.
 * @param characteristicUUID - UUID of the characteristic.
 * @param value - Value to write.
 * @returns A promise resolving to a boolean indicating success.
 */
export function writeCharacteristic(
  characteristicUUID: string,
  value: any,
): Promise<boolean> {
  return TenxMdkBle.writeCharacteristic(characteristicUUID, value);
}

/**
 * Enable or disable notifications for a characteristic.
 * @param serviceUUID - UUID of the service.
 * @param characteristicUUID - UUID of the characteristic.
 * @param enabled - Whether to enable or disable notifications.
 * @returns A promise resolving to a boolean indicating success.
 */
export function setCharacteristicNotification(
  serviceUUID: string,
  characteristicUUID: string,
  enabled: boolean,
): Promise<boolean> {
  return TenxMdkBle.setCharacteristicNotification(
    serviceUUID,
    characteristicUUID,
    enabled,
  );
}

/**
 * Subscribe to updates from a characteristic.
 * @param callback - Function to handle updates.
 * @returns A function to unsubscribe from updates.
 */
export function subscribeToCharacteristic(
  callback: (data: any) => void,
): () => void {
  const subscription = TenxMdkBle.addListener(
    'onCharacteristicValueUpdate',
    callback,
  );
  return () => {
    subscription.remove();
  };
}
