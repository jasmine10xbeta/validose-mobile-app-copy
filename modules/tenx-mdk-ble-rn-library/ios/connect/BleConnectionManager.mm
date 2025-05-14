#import "BleConnectionManager.h"

@implementation BleConnectionManager

- (instancetype)initWithManager:(CBCentralManager *)manager
          discoveredPeripherals:(NSMutableArray *)peripherals
     connectedPeripheralsHistory:(NSMutableArray *)history {
    self = [super init];
    if (self) {
        self.centralManager = manager;
        self.discoveredPeripherals = peripherals;
        self.connectedPeripheralsHistory = history;
    }
    return self;
}

- (void)connect:(NSString *)deviceIdentifier
        resolve:(RCTPromiseResolveBlock)resolve
         reject:(RCTPromiseRejectBlock)reject {
    NSLog(@"connectToDevice method called with identifier: %@", deviceIdentifier);
    NSLog(@"Discovered peripherals: %@", self.discoveredPeripherals);

    if (self.connectedPeripheral) {
        NSLog(@"Already connected to a device");
        reject(@"already_connected", @"Already connected to a device", nil);
        return;
    }

    if (!self.centralManager || !deviceIdentifier) {
        NSLog(@"Bluetooth manager not initialized or invalid address");
        reject(@"bluetooth_unavailable_or_invalid_address", @"Bluetooth manager not initialized or invalid address", nil);
        return;
    }

    BOOL foundPeripheral = NO;
    CBPeripheral *targetPeripheral = nil;

    for (NSDictionary *peripheralInfo in self.discoveredPeripherals) {
        NSString *deviceAddress = peripheralInfo[@"deviceAddress"];
        NSString *deviceName = peripheralInfo[@"deviceName"];

        NSLog(@"Checking peripheral: Address = %@, Name = %@", deviceAddress, deviceName);

        if ([deviceAddress isEqualToString:deviceIdentifier]) {
            targetPeripheral = peripheralInfo[@"peripheral"];
            NSLog(@"Peripheral found by address: %@", targetPeripheral);
            break;
        } else if ([deviceName isEqualToString:deviceIdentifier] && !targetPeripheral) {
            targetPeripheral = peripheralInfo[@"peripheral"];
            NSLog(@"Peripheral found by name: %@", targetPeripheral);
        }
    }

    if (targetPeripheral) {
        NSLog(@"Attempting to connect to peripheral: %@", targetPeripheral);
        self.connectResolve = resolve;
        self.connectReject = reject;
        self.connectedPeripheral = targetPeripheral;
        [self.centralManager connectPeripheral:targetPeripheral options:nil];
        foundPeripheral = YES;
    }

    if (!foundPeripheral) {
        NSLog(@"Peripheral not found with identifier: %@", deviceIdentifier);
        reject(@"device_not_found", @"Device not found", nil);
    }
}

- (void)getPrevConnectedDevices:(RCTPromiseResolveBlock)resolve
                               reject:(RCTPromiseRejectBlock)reject {
    NSMutableArray *resultArray = [NSMutableArray array];
    for (CBPeripheral *peripheral in self.connectedPeripheralsHistory) {
        [resultArray addObject:@{
            @"deviceId": peripheral.identifier.UUIDString,
            @"deviceName": peripheral.name ?: @"Unknown Device"
        }];
    }
    resolve(resultArray);
}

- (void)getConnectedDevice:(RCTPromiseResolveBlock)resolve
                    reject:(RCTPromiseRejectBlock)reject {
    if (self.connectedPeripheral) {
        resolve(@{
            @"deviceId": self.connectedPeripheral.identifier.UUIDString,
            @"deviceName": self.connectedPeripheral.name ?: @"Unknown Device"
        });
    } else {
        resolve(nil);
    }
}

- (void)isDeviceConnected:(RCTPromiseResolveBlock)resolve
                   reject:(RCTPromiseRejectBlock)reject {
    resolve(@(self.connectedPeripheral != nil));
}

- (void)disconnect:(RCTPromiseResolveBlock)resolve
            reject:(RCTPromiseRejectBlock)reject {
    if (!self.connectedPeripheral) {
        NSLog(@"No device connected to disconnect");
        reject(@"no_device_connected", @"No device connected to disconnect", nil);
        return;
    }

    [self.centralManager cancelPeripheralConnection:self.connectedPeripheral];
    self.connectedPeripheral = nil;

    if (resolve) {
        resolve(@"Disconnected successfully");
    }
}

- (void)bondDevice:(NSString *)deviceIdentifier
           resolve:(RCTPromiseResolveBlock)resolve
            reject:(RCTPromiseRejectBlock)reject {
    NSLog(@"bondDevice method called with identifier: %@", deviceIdentifier);
    CBPeripheral *peripheralToBond = nil;

    for (NSDictionary *peripheralInfo in self.discoveredPeripherals) {
        CBPeripheral *peripheral = peripheralInfo[@"peripheral"];
        if ([peripheral.identifier.UUIDString isEqualToString:deviceIdentifier]) {
            peripheralToBond = peripheral;
            break;
        }
    }

    if (!peripheralToBond) {
        NSLog(@"Peripheral not found with identifier: %@", deviceIdentifier);
        reject(@"device_not_found", @"Peripheral not found", nil);
        return;
    }

    peripheralToBond.delegate = self.delegate;
    self.connectResolve = resolve;
    self.connectReject = reject;

    if (peripheralToBond.state != CBPeripheralStateConnected) {
        [self.centralManager connectPeripheral:peripheralToBond options:@{CBConnectPeripheralOptionNotifyOnConnectionKey: @YES}];
    } else {
        // [peripheralToBond discoverServices:nil];
        if (self.connectResolve) {
            self.connectResolve(@{
                @"deviceId": peripheralToBond.identifier.UUIDString,
                @"deviceName": peripheralToBond.name ?: @"Unknown Device"
            });
            self.connectResolve = nil;
            self.connectReject = nil;
        }
    }
}

@end