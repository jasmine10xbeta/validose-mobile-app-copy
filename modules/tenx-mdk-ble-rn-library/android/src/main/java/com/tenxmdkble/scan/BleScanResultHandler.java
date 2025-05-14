package com.tenxmdkble;

import android.bluetooth.le.ScanResult;

import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.WritableArray;
import com.facebook.react.bridge.WritableMap;

import java.util.List;

public class BleScanResultHandler {

    public static WritableArray convertScanResultsToWritableArray(List<ScanResult> scanResults) {
        WritableArray resultsArray = Arguments.createArray();

        for (ScanResult result : scanResults) {
            WritableMap map = Arguments.createMap();
            if (result != null) {
                map.putString("deviceName", result.getDevice().getName() != null ? result.getDevice().getName() : "Unknown Device");
                map.putString("deviceAddress", result.getDevice().getAddress());
                map.putInt("rssi", result.getRssi());
                map.putBoolean("isConnectable", result.isConnectable());
            }
            resultsArray.pushMap(map);
        }

        return resultsArray;
    }
}