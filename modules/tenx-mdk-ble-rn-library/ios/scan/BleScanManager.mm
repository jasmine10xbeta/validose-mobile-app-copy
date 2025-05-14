#import "BleScanManager.h"

@interface BleScanManager ()

@property (nonatomic, assign) BOOL isScanCompleted;

@end

@implementation BleScanManager

- (instancetype)initWithCentralManager:(CBCentralManager *)manager discoveredPeripherals:(NSMutableArray *)peripherals {
    self = [super init];
    if (self) {
        self.centralManager = manager;
        self.discoveredPeripherals = peripherals;
        self.isScanCompleted = YES; // No scan running at start
    }
    return self;
}

- (void)scanLeDevice:(NSNumber *)scanInterval
             resolve:(RCTPromiseResolveBlock)resolve
              reject:(RCTPromiseRejectBlock)reject {
    NSLog(@"scanLeDevice method called.");

    if (self.centralManager.state != CBManagerStatePoweredOn) {
        NSLog(@"Bluetooth is not powered on or available.");
        reject(@"bluetooth_unavailable", @"Bluetooth is not available or powered on", nil);
        return;
    }

    if (!self.isScanCompleted) {
        NSLog(@"A scan is already in progress.");
        reject(@"scan_in_progress", @"A scan is already in progress.", nil);
        return;
    }

    [self.discoveredPeripherals removeAllObjects];
    self.scanResolve = resolve;
    self.scanReject = reject;
    self.isScanCompleted = NO;

    NSDictionary *scanOptions = @{ CBCentralManagerScanOptionAllowDuplicatesKey: @NO };
    [self.centralManager scanForPeripheralsWithServices:nil options:scanOptions];

    NSTimeInterval interval = scanInterval ? [scanInterval doubleValue] : 10.0;

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(interval * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [self.centralManager stopScan];
        NSLog(@"BLE scan stopped. Discovered peripherals: %@", self.discoveredPeripherals);

        NSMutableArray *resultArray = [NSMutableArray array];
        for (NSDictionary *peripheralInfo in self.discoveredPeripherals) {
            NSString *deviceName = peripheralInfo[@"deviceName"];
            NSLog(@"Device Name: %@", deviceName);
            if (![deviceName isEqualToString:@"Unknown Device"]) {
                [resultArray addObject:peripheralInfo];
            }
        }

        if (self.scanResolve) {
            self.scanResolve(resultArray);
        }
        self.scanResolve = nil;
        self.scanReject = nil;
        self.isScanCompleted = YES;
    });
}

- (void)stopLeScanWithResolve:(RCTPromiseResolveBlock)resolve
                       reject:(RCTPromiseRejectBlock)reject {
    if (self.isScanCompleted) {
        NSLog(@"No active scan to stop.");
        reject(@"no_active_scan", @"No scan is currently running", nil);
        return;
    }

    [self.centralManager stopScan];
    self.isScanCompleted = YES;
    NSLog(@"Stopped scanning manually.");

    if (resolve) {
         NSMutableArray *resultArray = [NSMutableArray array];
        for (NSDictionary *peripheralInfo in self.discoveredPeripherals) {
            NSString *deviceName = peripheralInfo[@"deviceName"];
            if (![deviceName isEqualToString:@"Unknown Device"]) {
                [resultArray addObject:peripheralInfo];
            }
        }
        resolve(resultArray);
    }

    // if (self.scanResolve) {
    //     self.scanResolve(resultArray);
    // }

    self.scanResolve = nil;
    self.scanReject = nil;
    // resolve(resultArray);
}

@end