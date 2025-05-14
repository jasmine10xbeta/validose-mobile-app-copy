#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>
#import <React/RCTBridgeModule.h>

@interface BleCharacteristicManager : NSObject

@property (nonatomic, strong) CBPeripheral *connectedPeripheral;
@property (nonatomic, copy) NSString *targetCharacteristicUUID;
@property (nonatomic, copy) RCTPromiseResolveBlock connectResolve;
@property (nonatomic, copy) RCTPromiseRejectBlock connectReject;
@property (nonatomic, assign) BOOL isReadingCharacteristic;
@property (nonatomic, strong) NSMutableArray *discoveredServices;

// Public methods
- (void)discoverServicesAndCharacteristicsWithResolve:(RCTPromiseResolveBlock)resolve
                                                reject:(RCTPromiseRejectBlock)reject;

- (void)writeCharacteristic:(NSString *)characteristicUUID
                      value:(id)value
                    resolve:(RCTPromiseResolveBlock)resolve
                     reject:(RCTPromiseRejectBlock)reject;

- (void)subscribeToCharacteristic:(NSString *)characteristicUUID
                      serviceUUID:(NSString *)serviceUUID
                         resolver:(RCTPromiseResolveBlock)resolve
                         rejecter:(RCTPromiseRejectBlock)reject;

- (void)readCharacteristic:(NSString *)characteristicUUID
                   resolve:(RCTPromiseResolveBlock)resolve
                    reject:(RCTPromiseRejectBlock)reject;

// Forwarded delegate handlers
- (CBCharacteristic *)findCharacteristicWithUUID:(NSString *)uuid
                                    onPeripheral:(CBPeripheral *)peripheral;

- (void)handleDidDiscoverServices:(CBPeripheral *)peripheral error:(NSError *)error;

- (void)handleDidDiscoverCharacteristics:(CBPeripheral *)peripheral service:(CBService *)service error:(NSError *)error;

- (void)handleDidUpdateValueForCharacteristic:(CBPeripheral *)peripheral
                                characteristic:(CBCharacteristic *)characteristic
                                         error:(NSError *)error;

@end