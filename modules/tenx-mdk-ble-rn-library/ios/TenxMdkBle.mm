#import "TenxMdkBle.h"
#import <objc/runtime.h>
#import <React/RCTEventEmitter.h>
#import <CoreBluetooth/CoreBluetooth.h>

#import "BleScanManager.h"
#import "BleConnectionManager.h"
#import "BleCharacteristicManager.h"

@interface TenxMdkBle () <RCTBridgeModule, CBCentralManagerDelegate, CBPeripheralDelegate>

@property (nonatomic, strong) BleScanManager *scanManager;
@property (nonatomic, strong) BleConnectionManager *connectionManager;
@property (nonatomic, strong) BleCharacteristicManager *characteristicManager;

@property (nonatomic, assign) BOOL isReadingCharacteristic;

@property (nonatomic, strong) NSString *targetCharacteristicUUID;

@property (nonatomic, strong) NSMutableArray *discoveredServices;
@property (nonatomic, strong) NSMutableArray *discoveredPeripherals;
@property (nonatomic, strong) NSMutableArray *connectedPeripheralsHistory;

@property (nonatomic, strong) CBCentralManager *centralManager;
@property (nonatomic, strong) CBPeripheral *connectedPeripheral;

@property (nonatomic, copy) RCTPromiseRejectBlock connectReject;
@property (nonatomic, copy) RCTPromiseResolveBlock connectResolve;

@end

@implementation TenxMdkBle
RCT_EXPORT_MODULE()

- (instancetype)init
{
    self = [super init];
    if (self) {
        self.centralManager = [[CBCentralManager alloc] initWithDelegate:self queue:nil];
        self.discoveredPeripherals = [NSMutableArray array];
        self.scanManager = [[BleScanManager alloc] initWithCentralManager:self.centralManager discoveredPeripherals:self.discoveredPeripherals];
        self.connectionManager = [[BleConnectionManager alloc] initWithManager:self.centralManager
                                                         discoveredPeripherals:self.discoveredPeripherals
                                                   connectedPeripheralsHistory:self.connectedPeripheralsHistory];
        self.connectionManager.delegate = self;
        self.characteristicManager = [[BleCharacteristicManager alloc] init];
        NSLog(@"TenxMdkBle initialized. Central Manager created.");
    }
    return self;
}

RCT_EXPORT_METHOD(scanLeDevice:(NSNumber *)interval resolve:(RCTPromiseResolveBlock)resolve reject:(RCTPromiseRejectBlock)reject) {
    [self.scanManager scanLeDevice:interval resolve:resolve reject:reject];
}

RCT_EXPORT_METHOD(stopLeScan:(RCTPromiseResolveBlock)resolve reject:(RCTPromiseRejectBlock)reject) {
    [self.scanManager stopLeScanWithResolve:resolve reject:reject];
}

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
    if (central.state == CBManagerStatePoweredOn) {
        NSLog(@"Bluetooth is powered on and ready.");
        // [self.centralManager scanForPeripheralsWithServices:nil options:nil];
    } else {
        NSLog(@"Bluetooth is not powered on or available.");
    }
}

- (void)centralManager:(CBCentralManager *)central didDiscoverPeripheral:(CBPeripheral *)peripheral advertisementData:(NSDictionary<NSString *,id> *)advertisementData RSSI:(NSNumber *)RSSI
{
    BOOL isNewPeripheral = YES;
    for (NSDictionary *info in self.discoveredPeripherals) {
        if ([info[@"identifier"] isEqualToString:peripheral.identifier.UUIDString]) {
            isNewPeripheral = NO;
            break;
        }
    }

    if (isNewPeripheral) {
        NSString *identifier = peripheral.identifier.UUIDString ?: @"Unknown Address";
        NSString *name = peripheral.name ?: @"Unknown Device";
        int rssiValue = RSSI.intValue;
        
        int txPower = [[advertisementData objectForKey:CBAdvertisementDataTxPowerLevelKey] intValue];
        BOOL isConnectable = [[advertisementData objectForKey:CBAdvertisementDataIsConnectable] boolValue];

        NSDictionary *peripheralInfo = @{
            @"deviceAddress": identifier,
            @"deviceName": name,
            @"rssi": @(rssiValue),
            @"txPower": @(txPower),
            @"isConnectable": @(isConnectable),
            @"peripheral": peripheral
        };

        [self.discoveredPeripherals addObject:peripheralInfo];
        NSLog(@"Discovered peripheral: %@", peripheralInfo);
    }
}

RCT_EXPORT_METHOD(connect:(NSString *)deviceId resolve:(RCTPromiseResolveBlock)resolve reject:(RCTPromiseRejectBlock)reject) {
    [self.connectionManager connect:deviceId resolve:resolve reject:reject];
}

RCT_EXPORT_METHOD(getPrevConnectedDevices:(RCTPromiseResolveBlock)resolve reject:(RCTPromiseRejectBlock)reject) {
    [self.connectionManager getPrevConnectedDevices:resolve reject:reject];
}

RCT_EXPORT_METHOD(getConnectedDevice:(RCTPromiseResolveBlock)resolve reject:(RCTPromiseRejectBlock)reject) {
    [self.connectionManager getConnectedDevice:resolve reject:reject];
}

RCT_EXPORT_METHOD(isDeviceConnected:(RCTPromiseResolveBlock)resolve reject:(RCTPromiseRejectBlock)reject) {
    [self.connectionManager isDeviceConnected:resolve reject:reject];
}

RCT_EXPORT_METHOD(disconnect:(RCTPromiseResolveBlock)resolve reject:(RCTPromiseRejectBlock)reject) {
    [self.connectionManager disconnect:resolve reject:reject];
}

RCT_EXPORT_METHOD(bondDevice:(NSString *)deviceId resolve:(RCTPromiseResolveBlock)resolve reject:(RCTPromiseRejectBlock)reject) {
    [self.connectionManager bondDevice:deviceId resolve:resolve reject:reject];
}

- (void)centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral {
    if ([self.centralManager isScanning]) {
        [self.centralManager stopScan];
        NSLog(@"Stopped scanning after successful connection.");
    }

    NSLog(@"Successfully connected to peripheral: %@", peripheral.identifier.UUIDString);
    self.connectedPeripheral = peripheral;
    self.connectedPeripheral.delegate = self;
    self.characteristicManager.connectedPeripheral = peripheral;
    
    if (!self.connectedPeripheralsHistory) {
        self.connectedPeripheralsHistory = [NSMutableArray array];
    }
    [self.connectedPeripheralsHistory addObject:peripheral];

    [peripheral discoverServices:nil];
    if (self.connectionManager.connectResolve) {
        self.connectionManager.connectResolve(@{ 
            @"deviceId": peripheral.identifier.UUIDString, 
            @"deviceName": peripheral.name ?: @"Unknown Device" 
        });
        self.connectionManager.connectResolve = nil;
        self.connectionManager.connectReject = nil;
    }
}

- (void)centralManager:(CBCentralManager *)central didFailToConnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {
    NSLog(@"Failed to connect to peripheral: %@, error: %@", peripheral.identifier.UUIDString, error.localizedDescription);

    if (self.connectionManager.connectReject) {
        self.connectionManager.connectReject(@"connect_error", @"Failed to connect to peripheral", error);
        self.connectionManager.connectResolve = nil;
        self.connectionManager.connectReject = nil;
    }
}

- (void)centralManager:(CBCentralManager *)central didDisconnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {
    NSLog(@"Disconnected from peripheral: %@", peripheral.identifier.UUIDString);

    if (error && self.connectionManager.connectReject) {
        self.connectionManager.connectReject(@"disconnected", @"Disconnected from peripheral", error);
    } else if (self.connectionManager.connectResolve) {
        self.connectionManager.connectResolve(@"Disconnected successfully");
    }

    self.connectionManager.connectResolve = nil;
    self.connectionManager.connectReject = nil;
    self.connectedPeripheral = nil;
}

#pragma GCC diagnostic ignored "-Wundeclared-selector"
- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(NSError *)error {
    [self.characteristicManager handleDidDiscoverServices:peripheral error:error];
}

- (BOOL)allCharacteristicsDiscovered {
    for (NSDictionary *serviceDict in self.discoveredServices) {
        if ([serviceDict[@"characteristics"] count] == 0) {
            return NO;
        }
    }
    return YES;
}

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverCharacteristicsForService:(CBService *)service error:(NSError *)error {
    [self.characteristicManager handleDidDiscoverCharacteristics:peripheral service:service error:error];
}

static NSString *const kNUSServiceUUID = @"6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static NSString *const kNUSRXCharacteristicUUID = @"6E400003-B5A3-F393-E0A9-E50E24DCCA9E"; // UUID for RX (Receive)

RCT_EXPORT_METHOD(discoverServicesAndCharacteristics:(RCTPromiseResolveBlock)resolve
                                              reject:(RCTPromiseRejectBlock)reject) {
    NSLog(@"discoverServicesAndCharacteristics called");
    [self.characteristicManager discoverServicesAndCharacteristicsWithResolve:resolve reject:reject];
}

RCT_EXPORT_METHOD(writeCharacteristic:(NSString *)uuid value:(id)value resolve:(RCTPromiseResolveBlock)resolve reject:(RCTPromiseRejectBlock)reject) {
    [self.characteristicManager writeCharacteristic:uuid value:value resolve:resolve reject:reject];
}

RCT_EXPORT_METHOD(readCharacteristic:(NSString *)uuid resolve:(RCTPromiseResolveBlock)resolve reject:(RCTPromiseRejectBlock)reject) {
    [self.characteristicManager readCharacteristic:uuid resolve:resolve reject:reject];
}

RCT_EXPORT_METHOD(subscribeToCharacteristic:(NSString *)uuid serviceUUID:(NSString *)serviceUUID resolver:(RCTPromiseResolveBlock)resolve rejecter:(RCTPromiseRejectBlock)reject) {
    [self.characteristicManager subscribeToCharacteristic:uuid serviceUUID:serviceUUID resolver:resolve rejecter:reject];
}

- (void)peripheral:(CBPeripheral *)peripheral didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
    if (error) {
        NSLog(@"Failed to set notify for characteristic %@: %@", characteristic.UUID.UUIDString, error.localizedDescription);
        if (self.connectReject) {
            self.connectReject(@"notify_error", @"Failed to set notify value", error);
            self.connectResolve = nil;
            self.connectReject = nil;
        }
        return;
    }

    if (characteristic.isNotifying) {
        NSLog(@"Notifications enabled for characteristic %@, bonding should start", characteristic.UUID.UUIDString);
        if (self.connectResolve) {
            self.connectResolve(@(YES));
            self.connectResolve = nil;
            self.connectReject = nil;
        }
    } else {
        NSLog(@"Notifications stopped for characteristic %@, bonding failed", characteristic.UUID.UUIDString);
        if (self.connectReject) {
            self.connectReject(@"notify_stopped", @"Notifications stopped unexpectedly", nil);
            self.connectResolve = nil;
            self.connectReject = nil;
        }
    }
}

- (void)peripheral:(CBPeripheral *)peripheral didWriteValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
    if (error) {
        NSLog(@"Failed to update value for characteristic: %@", error);
        if (self.connectReject) {
            self.connectReject(@"write_error", @"Failed to write value to characteristic", error);
            self.connectReject = nil;
        }
    } else {
        NSLog(@"Updated value for characteristic: %@", characteristic);
        if (self.connectResolve) {
            NSLog(@"It is coming here which means that resolve is not nil");
            self.connectResolve(nil);
            self.connectResolve = nil;
        }
    }
}

- (void)peripheral:(CBPeripheral *)peripheral 
didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic 
             error:(NSError *)error 
{
    [self.characteristicManager handleDidUpdateValueForCharacteristic:peripheral 
                                                       characteristic:characteristic 
                                                                error:error];
}

// - (void)peripheral:(CBPeripheral *)peripheral didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
//     if (error) {
//         NSLog(@"Error reading characteristic value: %@", error.localizedDescription);
//         if (self.connectReject) {
//             self.connectReject(@"read_error", @"Failed to read characteristic value", error);
//             self.connectResolve = nil;
//             self.connectReject = nil;
//         }
//         return;
//     }

//     NSLog(@"Characteristic value updated: %@", characteristic.value);

//     if (self.isReadingCharacteristic) {
//         NSLog(@"Characteristic value updated (read operation): %@", characteristic.value);
//         NSString *hexString = [self hexadecimalString:characteristic.value];
//         NSLog(@"Hexadecimal string: %@", hexString);

//         if (self.connectResolve) {
//             self.connectResolve(hexString);
//             self.connectResolve = nil;
//             self.connectReject = nil;
//         }
    
//         // Reset the flag after the read operation
//         self.isReadingCharacteristic = NO;
//     } else {
//         // Convert the characteristic value (NSData) to a hex string or array
//         NSMutableArray *valueArray = [NSMutableArray array];
//         const uint8_t *dataBytes = (const uint8_t *)characteristic.value.bytes; // Explicit cast to uint8_t *
//         for (NSUInteger i = 0; i < characteristic.value.length; i++) {
//             [valueArray addObject:@(dataBytes[i])];
//         }

//         if (self.connectResolve) {
//             self.connectResolve(valueArray);
//             self.connectResolve = nil;
//             self.connectReject = nil;
//         }
//     }
// }

#ifdef RCT_NEW_ARCH_ENABLED
- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:
    (const facebook::react::ObjCTurboModule::InitParams &)params
{
    return std::make_shared<facebook::react::NativeTenxMdkBleSpecJSI>(params);
}
#endif

@end
