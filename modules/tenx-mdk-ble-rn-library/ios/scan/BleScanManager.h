#import <Foundation/Foundation.h>
#import <React/RCTBridgeModule.h>
#import <CoreBluetooth/CoreBluetooth.h>

@interface BleScanManager : NSObject

@property (nonatomic, strong) CBCentralManager *centralManager;
@property (nonatomic, strong) NSMutableArray *discoveredPeripherals;
@property (nonatomic, copy) RCTPromiseResolveBlock scanResolve;
@property (nonatomic, copy) RCTPromiseRejectBlock scanReject;

- (instancetype)initWithCentralManager:(CBCentralManager *)manager discoveredPeripherals:(NSMutableArray *)peripherals;

- (void)scanLeDevice:(NSNumber *)scanInterval
             resolve:(RCTPromiseResolveBlock)resolve
              reject:(RCTPromiseRejectBlock)reject;

- (void)stopLeScanWithResolve:(RCTPromiseResolveBlock)resolve
                       reject:(RCTPromiseRejectBlock)reject;

@end