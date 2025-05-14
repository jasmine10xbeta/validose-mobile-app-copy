package com.tenxmdkble;

import androidx.annotation.NonNull;
import com.facebook.react.bridge.Promise;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.turbomodule.core.interfaces.TurboModule;
import com.facebook.react.bridge.NativeModule;
import com.facebook.react.bridge.ReactContextBaseJavaModule;

@ReactModule(name = TenxMdkBleModule.NAME)
public class TenxMdkBleModule extends NativeTenxMdkBleSpec {

    public static final String NAME = "TenxMdkBle";

    private final BleScanner bleScanner;
    private final BleConnectionManager bleConnectionManager;

    TenxMdkBleModule(ReactApplicationContext reactContext) {
        super(reactContext);
        bleScanner = new BleScanner(reactContext, reactContext);
        bleConnectionManager = new BleConnectionManager(reactContext, reactContext);
    }

    @Override
    public void scanLeDevice(Double scanInterval, Promise promise) {
        bleScanner.scanLeDevice(scanInterval, promise);
    }

    @Override
    public void stopLeScan(Promise promise) {
        bleScanner.stopLeScan(promise);
    }

    @Override
    public void connect(String deviceIdentifier, Promise promise) {
        bleConnectionManager.connect(deviceIdentifier, promise);
    }

    @Override
    public void getPrevConnectedDevices(Promise promise) {
        bleScanner.getPrevConnectedDevices(promise);
    }

    @Override
    public void getConnectedDevice(Promise promise) {
        bleConnectionManager.getConnectedDevice(promise);
    }

    @Override
    public void disconnect(Promise promise) {
        bleConnectionManager.disconnect(promise);
    }

    @Override
    public void discoverServicesAndCharacteristics(Promise promise) {
        bleConnectionManager.discoverServicesAndCharacteristics(promise);
    }

    @Override
    public void readCharacteristic(String characteristicUUID, Promise promise) {
        bleConnectionManager.readCharacteristic(characteristicUUID, promise);
    }

    @Override
    public void writeCharacteristic(String characteristicUUID, String value, Promise promise) {
        bleConnectionManager.writeCharacteristic(characteristicUUID, value, promise);
    }

    @Override
    public void setCharacteristicNotification(String serviceUUID, String characteristicUUID, boolean enabled, Promise promise) {
        bleConnectionManager.setCharacteristicNotification(serviceUUID, characteristicUUID, enabled, promise);
    }

    @Override
    public void bondDevice(String deviceAddress, Promise promise) {
        bleConnectionManager.bondDevice(deviceAddress, promise);
    }

    @Override
    public void addListener(String eventName) {
        // No-op for now (required method)
    }

    @Override
    public void removeListener(String eventName) {
        // No-op for now (required method)
    }
}