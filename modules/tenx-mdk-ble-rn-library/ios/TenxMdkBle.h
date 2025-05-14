#ifdef RCT_NEW_ARCH_ENABLED
#import "RNTenxMdkBleSpec.h"

@interface TenxMdkBle : NSObject <NativeTenxMdkBleSpec>
#else
#import <React/RCTBridgeModule.h>

@interface TenxMdkBle : NSObject <RCTBridgeModule>
#endif

@end