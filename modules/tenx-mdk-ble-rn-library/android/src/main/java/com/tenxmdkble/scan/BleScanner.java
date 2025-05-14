package com.tenxmdkble;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.content.Context;
import android.os.Handler;

import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.bridge.Promise;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.WritableArray;
import com.facebook.react.modules.core.DeviceEventManagerModule;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class BleScanner {
    private static final long SCAN_PERIOD = 10000;

    private final BluetoothLeScanner bluetoothLeScanner;
    private final Handler handler;
    private final List<ScanResult> scanResults;
    private final Set<String> uniqueDeviceAddresses;
    private final ReactApplicationContext reactContext;
    private Promise scanPromise;
    private boolean scanning;

    public BleScanner(Context context, ReactApplicationContext reactContext) {
        BluetoothAdapter bluetoothAdapter = ((android.bluetooth.BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE)).getAdapter();
        this.bluetoothLeScanner = bluetoothAdapter.getBluetoothLeScanner();
        this.handler = new Handler();
        this.scanResults = new ArrayList<>();
        this.uniqueDeviceAddresses = new HashSet<>();
        this.reactContext = reactContext;
    }
    
    public void scanLeDevice(Double scanInterval, Promise promise) {
        if (!scanning) {
            this.scanPromise = promise;
            scanResults.clear();
            uniqueDeviceAddresses.clear();
    
            scanning = true;
            bluetoothLeScanner.startScan(leScanCallback);
    
            long intervalMillis = (scanInterval != null ? (long)(scanInterval * 1000) : SCAN_PERIOD);
    
            handler.postDelayed(this::stopScanning, intervalMillis);
        } else {
            promise.reject("already_scanning", "Scanning already in progress");
        }
    }

    public void stopLeScan(Promise promise) {
        if (!scanning) {
            promise.reject("not_scanning", "No scan is in progress");
            return;
        }
        stopScanning();
        WritableArray resultsArray = BleScanResultHandler.convertScanResultsToWritableArray(scanResults);
        promise.resolve(resultsArray);
    }

    public void getPrevConnectedDevices(Promise promise) {
        BluetoothAdapter bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        if (bluetoothAdapter == null) {
            promise.reject("bluetooth_unavailable", "Bluetooth Adapter not available");
            return;
        }
    
        Set<BluetoothDevice> bondedDevices = bluetoothAdapter.getBondedDevices();
        WritableArray devicesArray = Arguments.createArray();
    
        for (BluetoothDevice device : bondedDevices) {
            WritableMap deviceMap = Arguments.createMap();
            deviceMap.putString("deviceAddress", device.getAddress());
            deviceMap.putString("deviceName", device.getName() != null ? device.getName() : "Unknown Device");
            devicesArray.pushMap(deviceMap);
        }
    
        promise.resolve(devicesArray);
    }

    private void stopScanning() {
        scanning = false;
        bluetoothLeScanner.stopScan(leScanCallback);

        if (scanPromise != null) {
            WritableArray resultsArray = BleScanResultHandler.convertScanResultsToWritableArray(scanResults);
            scanPromise.resolve(resultsArray);
            scanPromise = null;
        }
    }

    private final ScanCallback leScanCallback = new ScanCallback() {
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            super.onScanResult(callbackType, result);
            if (!uniqueDeviceAddresses.contains(result.getDevice().getAddress())) {
                scanResults.add(result);
                uniqueDeviceAddresses.add(result.getDevice().getAddress());
            }
        }

        @Override
        public void onBatchScanResults(List<ScanResult> results) {
            super.onBatchScanResults(results);
            for (ScanResult result : results) {
                if (!uniqueDeviceAddresses.contains(result.getDevice().getAddress())) {
                    scanResults.add(result);
                    uniqueDeviceAddresses.add(result.getDevice().getAddress());
                }
            }
        }
    };
}