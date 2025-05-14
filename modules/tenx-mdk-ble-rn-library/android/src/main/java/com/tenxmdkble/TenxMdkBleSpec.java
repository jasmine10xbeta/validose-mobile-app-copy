package com.tenxmdkble;

import com.facebook.react.bridge.Promise;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.turbomodule.core.interfaces.TurboModule;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.bridge.NativeModule;

public interface TenxMdkBleSpec extends TurboModule, NativeModule {
  
  @ReactMethod void scanLeDevice(Promise promise);
  @ReactMethod void stopLeScan(Promise promise);
  @ReactMethod void getPrevConnectedDevices(Promise promise);
  @ReactMethod void connect(String deviceAddress, Promise promise);
  @ReactMethod void disconnect(Promise promise);
  @ReactMethod void getConnectedDevice(Promise promise);
  @ReactMethod void isDeviceConnected(Promise promise);
  @ReactMethod void bondDevice(String deviceAddress, Promise promise);
  @ReactMethod void discoverServicesAndCharacteristics(Promise promise);
}