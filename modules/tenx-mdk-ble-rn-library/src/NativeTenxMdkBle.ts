import type { TurboModule } from 'react-native';
import { TurboModuleRegistry } from 'react-native';

export interface Spec extends TurboModule {
  /**
   * Scan for BLE devices for a specified interval.
   * @param scanInterval - Duration (in seconds) for the scan. Default is 10 seconds.
   * @returns A promise resolving to a list of discovered devices.
   */
  scanLeDevice(scanInterval: number | undefined): Promise<any>;

  stopLeScan(): Promise<any>;

  /**
   * Connect to a BLE device.
   * @param deviceIdentifier - Either the address or name of the BLE device to connect to.
   * @returns A promise resolving to a boolean indicating success.
   */
  connect(deviceIdentifier: string): Promise<boolean>;

  getPrevConnectedDevices(): Promise<any>;

  getConnectedDevice(): Promise<any>;

  isDeviceConnected(): Promise<any>;

  /**
   * Disconnect from the currently connected BLE device.
   * @returns A promise resolving when the operation is complete.
   */
  disconnect(): Promise<void>;

  /**
   * Discover services and characteristics of the connected BLE device.
   * @returns A promise resolving to the list of services and characteristics.
   */
  discoverServicesAndCharacteristics(): Promise<Array<any>>;

  /**
   * Read a characteristic's value.
   * @param characteristicUUID - UUID of the characteristic.
   * @returns A promise resolving to the value read.
   */
  readCharacteristic(characteristicUUID: string): Promise<any>;

  /**
   * Write a value to a characteristic.
   * @param characteristicUUID - UUID of the characteristic.
   * @param value - Value to write.
   * @returns A promise resolving when the operation is complete.
   */
  writeCharacteristic(characteristicUUID: string, value: string): Promise<void>;

  /**
   * Enable or disable notifications for a characteristic.
   * @param serviceUUID - UUID of the service.
   * @param characteristicUUID - UUID of the characteristic.
   * @param enabled - Whether notifications should be enabled.
   * @returns A promise resolving to a boolean indicating success.
   */
  setCharacteristicNotification(
    serviceUUID: string,
    characteristicUUID: string,
    enabled: boolean,
  ): Promise<boolean>;

  /**
   * Bond with a BLE device.
   * @param device - Address of the BLE device.
   * @returns A promise resolving when the operation is complete.
   */
  bondDevice(device: string): Promise<void>;

  /**
   * Add an event listener.
   * @param eventName - Name of the event to listen to.
   */
  addListener(eventName: string): void;

  /**
   * Remove an event listener.
   * @param eventName - Name of the event to stop listening to.
   */
  removeListener(eventName: string): void;
}

export default TurboModuleRegistry.getEnforcing<Spec>('TenxMdkBle');
