package com.tenxmdkble;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothProfile;
import android.content.Context;
import android.os.Handler;
import android.util.Log;

import com.facebook.react.bridge.Promise;
import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.WritableArray;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.modules.core.DeviceEventManagerModule;

import java.util.List;
import java.util.UUID;

public class BleConnectionManager {
    private static final String TAG = "BleConnectionManager";

    private final BluetoothAdapter bluetoothAdapter;
    private BluetoothGatt bluetoothGatt;
    private BluetoothDevice connectedDevice;
    private final Handler handler;
    private final Context context;
    private final ReactApplicationContext reactContext;
    private boolean isConnected;
    private Promise readPromise;

    public BleConnectionManager(Context context, ReactApplicationContext reactContext) {
        this.context = context;
        this.reactContext = reactContext;
        this.bluetoothAdapter = ((android.bluetooth.BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE)).getAdapter();
        this.handler = new Handler();
    }

    public void connect(String deviceAddress, Promise promise) {
        if (isConnected) {
            promise.reject("already_connected", "Already connected to a device");
            return;
        }

        if (bluetoothAdapter == null || deviceAddress == null) {
            promise.reject("adapter_unavailable", "BluetoothAdapter not initialized or unspecified address.");
            return;
        }

        try {
            final BluetoothDevice device = bluetoothAdapter.getRemoteDevice(deviceAddress);
            bluetoothGatt = device.connectGatt(context, false, bluetoothGattCallback);
            connectedDevice = device;
            promise.resolve(true);
        } catch (IllegalArgumentException e) {
            promise.reject("device_not_found", "Device not found. Unable to connect.");
        }
    }

    public void disconnect(Promise promise) {
        if (bluetoothGatt != null) {
            bluetoothGatt.disconnect();
            promise.resolve(true);
        } else {
            promise.reject("no_connection", "No active connection to disconnect");
        }
    }

    public void bondDevice(String deviceAddress, Promise promise) {
        if (bluetoothAdapter == null) {
            promise.reject("adapter_unavailable", "BluetoothAdapter not initialized");
            return;
        }

        BluetoothDevice device = bluetoothAdapter.getRemoteDevice(deviceAddress);
        if (device == null) {
            promise.reject("device_not_found", "Device not found");
            return;
        }

        boolean result = device.createBond();
        if (result) {
            promise.resolve("Bonding started");
        } else {
            promise.reject("bond_failed", "Failed to start bonding");
        }
    }

    public void discoverServicesAndCharacteristics(Promise promise) {
        if (bluetoothGatt == null) {
            promise.reject("no_connection", "No active connection");
            return;
        }

        bluetoothGatt.discoverServices();
        handler.postDelayed(() -> {
            if (bluetoothGatt.getServices().isEmpty()) {
                promise.reject("service_discovery_failed", "Service discovery failed");
            } else {
                WritableArray servicesArray = Arguments.createArray();
                for (BluetoothGattService service : bluetoothGatt.getServices()) {
                    WritableMap serviceMap = Arguments.createMap();
                    serviceMap.putString("uuid", service.getUuid().toString());

                    WritableArray characteristicsArray = Arguments.createArray();
                    List<BluetoothGattCharacteristic> characteristics = service.getCharacteristics();
                    for (BluetoothGattCharacteristic characteristic : characteristics) {
                        WritableMap charMap = Arguments.createMap();
                        charMap.putString("uuid", characteristic.getUuid().toString());
                        charMap.putString("properties", getPropertiesString(characteristic));
                        characteristicsArray.pushMap(charMap);
                    }

                    serviceMap.putArray("characteristics", characteristicsArray);
                    servicesArray.pushMap(serviceMap);
                }
                promise.resolve(servicesArray);
            }
        }, 2000);
    }

    public void getConnectedDevice(Promise promise) {
        if (connectedDevice != null) {
            WritableMap deviceMap = Arguments.createMap();
            deviceMap.putString("deviceAddress", connectedDevice.getAddress());
            deviceMap.putString("deviceName", connectedDevice.getName() != null ? connectedDevice.getName() : "Unknown Device");
            promise.resolve(deviceMap);
        } else {
            promise.resolve(null);
        }
    }

    public void isDeviceConnected(Promise promise) {
        promise.resolve(connectedDevice != null);
    }

    private String getPropertiesString(BluetoothGattCharacteristic characteristic) {
        StringBuilder properties = new StringBuilder();
        int props = characteristic.getProperties();

        if ((props & BluetoothGattCharacteristic.PROPERTY_READ) != 0) properties.append("READ ");
        if ((props & BluetoothGattCharacteristic.PROPERTY_WRITE) != 0) properties.append("WRITE ");
        if ((props & BluetoothGattCharacteristic.PROPERTY_NOTIFY) != 0) properties.append("NOTIFY ");
        if ((props & BluetoothGattCharacteristic.PROPERTY_INDICATE) != 0) properties.append("INDICATE ");

        return properties.toString().trim();
    }

    private void sendConnectionStatusEvent(String status, String deviceId) {
        if (reactContext != null && reactContext.hasActiveCatalystInstance()) {
            WritableMap params = Arguments.createMap();
            params.putString("status", status);
            params.putString("deviceId", deviceId);

            reactContext
                .getJSModule(DeviceEventManagerModule.RCTDeviceEventEmitter.class)
                .emit("onConnectionStatusChange", params);
        }
    }

    private final BluetoothGattCallback bluetoothGattCallback = new BluetoothGattCallback() {
        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                Log.d(TAG, "Connected to GATT server.");
                isConnected = true;
                sendConnectionStatusEvent("connected", gatt.getDevice().getAddress());
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                Log.d(TAG, "Disconnected from GATT server.");
                isConnected = false;
                connectedDevice = null;
                sendConnectionStatusEvent("disconnected", gatt.getDevice().getAddress());
            }
        }

        @Override
        public void onCharacteristicRead(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                if (readPromise != null) {
                    byte[] value = characteristic.getValue();
                    String hexString = bytesToHex(value);
                    readPromise.resolve(hexString);
                    readPromise = null;
                }
            } else {
                if (readPromise != null) {
                    readPromise.reject("read_error", "Failed to read characteristic");
                    readPromise = null;
                }
            }
        }
    };

    public void readCharacteristic(String characteristicUUID, Promise promise) {
        if (bluetoothGatt == null) {
            promise.reject("no_connection", "No active connection");
            return;
        }
    
        for (BluetoothGattService service : bluetoothGatt.getServices()) {
            BluetoothGattCharacteristic characteristic = service.getCharacteristic(UUID.fromString(characteristicUUID));
            if (characteristic != null) {
                try {
                    readPromise = promise;
                    boolean success = bluetoothGatt.readCharacteristic(characteristic);
                    if (!success) {
                        promise.reject("read_failed", "Failed to initiate characteristic read");
                    }
                } catch (Exception e) {
                    promise.reject("error", e.getMessage());
                }
                return;
            }
        }
    
        promise.reject("characteristic_not_found", "Characteristic not found for UUID: " + characteristicUUID);
    }

    private byte[] hexStringToByteArray(String s) {
        int len = s.length();
        byte[] data = new byte[len / 2];
        for (int i = 0; i < len; i += 2) {
            data[i / 2] = (byte) ((Character.digit(s.charAt(i), 16) << 4)
                                 + Character.digit(s.charAt(i+1), 16));
        }
        return data;
    }

    private String bytesToHex(byte[] bytes) {
        StringBuilder hexString = new StringBuilder();
        for (byte b : bytes) {
            String hex = Integer.toHexString(0xFF & b);
            if (hex.length() == 1) {
                hexString.append('0');
            }
            hexString.append(hex);
        }
        return hexString.toString();
    }

    public void writeCharacteristic(String characteristicUUID, String value, Promise promise) {
        if (bluetoothGatt == null) {
            promise.reject("no_connection", "No active connection");
            return;
        }
    
        for (BluetoothGattService service : bluetoothGatt.getServices()) {
            BluetoothGattCharacteristic characteristic = service.getCharacteristic(UUID.fromString(characteristicUUID));
            if (characteristic != null) {
                try {
                    byte[] data = hexStringToByteArray(value);
                    characteristic.setValue(data);
    
                    boolean success = bluetoothGatt.writeCharacteristic(characteristic);
                    if (!success) {
                        promise.reject("write_failed", "Failed to write characteristic");
                    } else {
                        promise.resolve(true);
                    }
                    return;
                } catch (Exception e) {
                    promise.reject("error", e.getMessage());
                    return;
                }
            }
        }
    
        promise.reject("characteristic_not_found", "Characteristic with UUID not found: " + characteristicUUID);
    }

    public void setCharacteristicNotification(String serviceUUID, String characteristicUUID, boolean enabled, Promise promise) {
        if (bluetoothGatt == null) {
            promise.reject("no_connection", "No active connection");
            return;
        }
    
        BluetoothGattService service = bluetoothGatt.getService(UUID.fromString(serviceUUID));
        if (service == null) {
            promise.reject("service_not_found", "Service not found");
            return;
        }
    
        BluetoothGattCharacteristic characteristic = service.getCharacteristic(UUID.fromString(characteristicUUID));
        if (characteristic == null) {
            promise.reject("characteristic_not_found", "Characteristic not found");
            return;
        }
    
        bluetoothGatt.setCharacteristicNotification(characteristic, enabled);
        promise.resolve(true);
    }
}