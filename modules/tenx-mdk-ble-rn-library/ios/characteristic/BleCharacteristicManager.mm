#import "BleCharacteristicManager.h"
#import <CoreBluetooth/CoreBluetooth.h>

@implementation BleCharacteristicManager

- (void)discoverServicesAndCharacteristicsWithResolve:(RCTPromiseResolveBlock)resolve
                                                reject:(RCTPromiseRejectBlock)reject {
    if (!self.connectedPeripheral) {
        reject(@"no_device_connected", @"No device is connected", nil);
        return;
    }

    self.connectResolve = resolve;
    self.connectReject = reject;
    self.discoveredServices = [NSMutableArray array];
    [self.connectedPeripheral discoverServices:nil];
}

- (void)writeCharacteristic:(NSString *)characteristicUUID
                      value:(id)value
                    resolve:(RCTPromiseResolveBlock)resolve
                     reject:(RCTPromiseRejectBlock)reject {
    if (!self.connectedPeripheral) {
        reject(@"peripheral_not_connected", @"No connected peripheral found", nil);
        return;
    }

    CBCharacteristic *characteristic = [self findCharacteristicWithUUID:characteristicUUID onPeripheral:self.connectedPeripheral];
    if (!characteristic) {
        reject(@"characteristic_not_found", @"Characteristic with the given UUID not found", nil);
        return;
    }

    NSData *dataToWrite = nil;

    if ([value isKindOfClass:[NSNumber class]]) {
        NSNumber *numberValue = (NSNumber *)value;
        if (numberValue.longLongValue <= UINT8_MAX) {
            uint8_t byte = numberValue.unsignedCharValue;
            dataToWrite = [NSData dataWithBytes:&byte length:sizeof(byte)];
        } else if (numberValue.longLongValue <= UINT16_MAX) {
            uint16_t shortVal = CFSwapInt16HostToLittle(numberValue.unsignedShortValue);
            dataToWrite = [NSData dataWithBytes:&shortVal length:sizeof(shortVal)];
        } else if (numberValue.longLongValue <= UINT32_MAX) {
            uint32_t intVal = CFSwapInt32HostToLittle(numberValue.unsignedIntValue);
            dataToWrite = [NSData dataWithBytes:&intVal length:sizeof(intVal)];
        } else if (numberValue.longLongValue <= UINT64_MAX) {
            uint64_t longVal = CFSwapInt64HostToLittle(numberValue.unsignedLongLongValue);
            dataToWrite = [NSData dataWithBytes:&longVal length:sizeof(longVal)];
        } else {
            reject(@"invalid_data", @"Number too large", nil);
            return;
        }
    } else if ([value isKindOfClass:[NSString class]]) {
        NSString *stringValue = (NSString *)value;
        NSCharacterSet *hexChars = [[NSCharacterSet characterSetWithCharactersInString:@"0123456789ABCDEFabcdef"] invertedSet];
        if ([stringValue rangeOfCharacterFromSet:hexChars].location == NSNotFound && stringValue.length % 2 == 0) {
            NSMutableData *hexData = [NSMutableData data];
            for (NSUInteger i = 0; i < stringValue.length; i += 2) {
                NSString *byteStr = [stringValue substringWithRange:NSMakeRange(i, 2)];
                unsigned char byte = strtol([byteStr UTF8String], NULL, 16);
                [hexData appendBytes:&byte length:1];
            }
            dataToWrite = hexData;
        } else {
            dataToWrite = [stringValue dataUsingEncoding:NSUTF8StringEncoding];
        }
    } else {
        reject(@"invalid_data", @"Unsupported value type", nil);
        return;
    }

    [self.connectedPeripheral writeValue:dataToWrite
                        forCharacteristic:characteristic
                                     type:CBCharacteristicWriteWithResponse];
    resolve(@(YES));
}

- (void)subscribeToCharacteristic:(NSString *)characteristicUUID
                      serviceUUID:(NSString *)serviceUUID
                         resolver:(RCTPromiseResolveBlock)resolve
                         rejecter:(RCTPromiseRejectBlock)reject {
    if (!self.connectedPeripheral) {
        reject(@"no_device_connected", @"No device is connected", nil);
        return;
    }

    self.targetCharacteristicUUID = characteristicUUID;
    self.connectResolve = resolve;
    self.connectReject = reject;

    for (CBService *service in self.connectedPeripheral.services) {
        if ([service.UUID.UUIDString isEqualToString:serviceUUID]) {
            [self.connectedPeripheral discoverCharacteristics:nil forService:service];
            return;
        }
    }

    reject(@"service_not_found", @"Service with the given UUID not found", nil);
}

- (void)readCharacteristic:(NSString *)characteristicUUID
                   resolve:(RCTPromiseResolveBlock)resolve
                    reject:(RCTPromiseRejectBlock)reject {
    if (!self.connectedPeripheral) {
        reject(@"no_device_connected", @"No device is connected", nil);
        return;
    }

    CBCharacteristic *characteristic = [self findCharacteristicWithUUID:characteristicUUID
                                                           onPeripheral:self.connectedPeripheral];
    if (!characteristic) {
        reject(@"characteristic_not_found", @"Characteristic not found", nil);
        return;
    }

    if (!(characteristic.properties & CBCharacteristicPropertyRead)) {
        reject(@"read_not_supported", @"Characteristic does not support read", nil);
        return;
    }

    self.isReadingCharacteristic = YES;
    self.connectResolve = resolve;
    self.connectReject = reject;

    [self.connectedPeripheral readValueForCharacteristic:characteristic];
}

// - (CBCharacteristic *)findCharacteristicWithUUID:(NSString *)uuid
//                                     onPeripheral:(CBPeripheral *)peripheral {
//     for (CBService *service in peripheral.services) {
//         for (CBCharacteristic *characteristic in service.characteristics) {
//             if ([characteristic.UUID.UUIDString isEqualToString:uuid]) {
//                 return characteristic;
//             }
//         }
//     }
//     return nil;
// }

- (CBCharacteristic *)findCharacteristicWithUUID:(NSString *)uuid
                                    onPeripheral:(CBPeripheral *)peripheral {
    for (CBService *service in peripheral.services) {
        for (CBCharacteristic *characteristic in service.characteristics) {
            NSLog(@"Comparing %@ with %@", characteristic.UUID.UUIDString, uuid);
            if ([characteristic.UUID.UUIDString caseInsensitiveCompare:uuid] == NSOrderedSame) {
                return characteristic;
            }
        }
    }
    return nil;
}

- (void)handleDidDiscoverServices:(CBPeripheral *)peripheral error:(NSError *)error {
    self.discoveredServices = [NSMutableArray array];

    if (error) {
        NSLog(@"Error discovering services: %@", error.localizedDescription);
        if (self.connectReject) {
            self.connectReject(@"service_discovery_error", @"Error discovering services", error);
            self.connectResolve = nil;
            self.connectReject = nil;
        }
        return;
    }

    NSLog(@"Services discovered: %@", peripheral.services);
    for (CBService *service in peripheral.services) {
        NSLog(@"Service UUID: %@", service.UUID.UUIDString);
        [peripheral discoverCharacteristics:nil forService:service];
    }
}

- (void)handleDidDiscoverCharacteristics:(CBPeripheral *)peripheral service:(CBService *)service error:(NSError *)error {
    if (error) {
        NSLog(@"Error discovering characteristics for service %@: %@", service.UUID.UUIDString, error.localizedDescription);
        if (self.connectReject) {
            self.connectReject(@"characteristic_discovery_error", @"Error discovering characteristics", error);
            self.connectResolve = nil;
            self.connectReject = nil;
        }
        return;
    }

    NSLog(@"Discovered characteristics for service: %@", service.UUID.UUIDString);
    NSMutableDictionary *serviceDict = [NSMutableDictionary dictionary];
    serviceDict[@"uuid"] = service.UUID.UUIDString;

    NSMutableArray *characteristicsArray = [NSMutableArray array];

    for (CBCharacteristic *characteristic in service.characteristics) {
        NSMutableDictionary *characteristicDict = [NSMutableDictionary dictionary];
        characteristicDict[@"uuid"] = characteristic.UUID.UUIDString;

        NSMutableArray *propertiesArray = [NSMutableArray array];
        CBCharacteristicProperties properties = characteristic.properties;
        if (properties & CBCharacteristicPropertyRead) [propertiesArray addObject:@"Read"];
        if (properties & CBCharacteristicPropertyWrite) [propertiesArray addObject:@"Write"];
        if (properties & CBCharacteristicPropertyNotify) [propertiesArray addObject:@"Notify"];
        if (properties & CBCharacteristicPropertyIndicate) [propertiesArray addObject:@"Indicate"];
        if (properties & CBCharacteristicPropertyWriteWithoutResponse) [propertiesArray addObject:@"WriteWithoutResponse"];

        characteristicDict[@"properties"] = propertiesArray;

        NSMutableArray *descriptorsArray = [NSMutableArray array];
        for (CBDescriptor *descriptor in characteristic.descriptors) {
            [descriptorsArray addObject:descriptor.UUID.UUIDString];
        }
        characteristicDict[@"descriptors"] = descriptorsArray;

        [characteristicsArray addObject:characteristicDict];
    }

    serviceDict[@"characteristics"] = characteristicsArray;
    [self.discoveredServices addObject:serviceDict];

    if (self.discoveredServices.count == peripheral.services.count) {
        NSLog(@"All services processed. Resolving promise.");
        if (self.connectResolve) {
            self.connectResolve(self.discoveredServices);
            self.connectResolve = nil;
            self.connectReject = nil;
        }
    } else {
        NSLog(@"Waiting for more services to complete. Current count: %lu, expected: %lu", 
              (unsigned long)self.discoveredServices.count, 
              (unsigned long)peripheral.services.count);
    }

    NSLog(@"Discovered Services: %@", self.discoveredServices);
}

- (void)handleDidUpdateValueForCharacteristic:(CBPeripheral *)peripheral
                               characteristic:(CBCharacteristic *)characteristic
                                        error:(NSError *)error
{
    if (error) {
        NSLog(@"[BleCharacteristicManager] Error reading characteristic: %@", error.localizedDescription);
        if (self.connectReject) {
            self.connectReject(@"read_error", @"Failed to read characteristic value", error);
            self.connectResolve = nil;
            self.connectReject = nil;
        }
        return;
    }

    NSLog(@"[BleCharacteristicManager] Characteristic value updated: %@", characteristic.value);

    if (self.isReadingCharacteristic) {
        NSString *hexString = [self hexadecimalString:characteristic.value];
        if (self.connectResolve) {
            self.connectResolve(hexString);
            self.connectResolve = nil;
            self.connectReject = nil;
        }
        self.isReadingCharacteristic = NO;
    } else {
        NSMutableArray *valueArray = [NSMutableArray array];
        const uint8_t *dataBytes = (const uint8_t *)characteristic.value.bytes;
        for (NSUInteger i = 0; i < characteristic.value.length; i++) {
            [valueArray addObject:@(dataBytes[i])];
        }

        if (self.connectResolve) {
            self.connectResolve(valueArray);
            self.connectResolve = nil;
            self.connectReject = nil;
        }
    }
}

- (NSString *)hexadecimalString:(NSData *)data {
    const unsigned char *dataBuffer = (const unsigned char *)[data bytes];
    if (!dataBuffer) return [NSString string];
    
    NSUInteger dataLength = [data length];
    NSMutableString *hexString = [NSMutableString stringWithCapacity:(dataLength * 2)];
    
    for (int i = 0; i < dataLength; ++i) {
        [hexString appendString:[NSString stringWithFormat:@"%02lx", (unsigned long)dataBuffer[i]]];
    }
    
    return [NSString stringWithString:hexString];
}

@end