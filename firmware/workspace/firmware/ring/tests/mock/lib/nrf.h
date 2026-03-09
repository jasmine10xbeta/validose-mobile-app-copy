#ifndef _NRF_H_
#define _NRF_H_

#include <stdint.h>

typedef struct
{
   uint32_t DEVICEID[2];
} NRF_FICR_Type;

NRF_FICR_Type nrf_ficr_instance = {
   .DEVICEID = {0x12345678, 0x9ABCDEF0} // Example device ID for testing
};

NRF_FICR_Type *NRF_FICR = &nrf_ficr_instance;

#endif // _NRF_H_