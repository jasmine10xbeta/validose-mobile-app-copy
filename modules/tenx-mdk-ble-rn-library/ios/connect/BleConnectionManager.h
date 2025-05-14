#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>
#import <React/RCTBridgeModule.h>

@interface BleConnectionManager : NSObject

@property (nonatomic, strong) CBCentralManager *centralManager;
@property (nonatomic, strong) NSMutableArray *discoveredPeripherals;
@property (nonatomic, strong) CBPeripheral *connectedPeripheral;
@property (nonatomic, strong) NSMutableArray *connectedPeripheralsHistory;
@property (nonatomic, copy) RCTPromiseResolveBlock connectResolve;
@property (nonatomic, copy) RCTPromiseRejectBlock connectReject;
@property (nonatomic, weak) id<CBPeripheralDelegate> delegate;

- (instancetype)initWithManager:(CBCentralManager *)manager
          discoveredPeripherals:(NSMutableArray *)peripherals
     connectedPeripheralsHistory:(NSMutableArray *)history;

- (void)connect:(NSString *)deviceIdentifier
        resolve:(RCTPromiseResolveBlock)resolve
         reject:(RCTPromiseRejectBlock)reject;

- (void)getPrevConnectedDevices:(RCTPromiseResolveBlock)resolve
                               reject:(RCTPromiseRejectBlock)reject;

- (void)getConnectedDevice:(RCTPromiseResolveBlock)resolve
                    reject:(RCTPromiseRejectBlock)reject;

- (void)isDeviceConnected:(RCTPromiseResolveBlock)resolve
                   reject:(RCTPromiseRejectBlock)reject;

- (void)disconnect:(RCTPromiseResolveBlock)resolve
            reject:(RCTPromiseRejectBlock)reject;

- (void)bondDevice:(NSString *)deviceIdentifier
           resolve:(RCTPromiseResolveBlock)resolve
            reject:(RCTPromiseRejectBlock)reject;
            
@end