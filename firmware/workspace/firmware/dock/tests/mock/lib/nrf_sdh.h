#ifndef NRF_SDH_MOCK_H
#define NRF_SDH_MOCK_H

#include <stdbool.h>

bool nrf_sdh_is_enabled(void);

// Optional control hook for tests.
void nrf_sdh_mock_set_enabled(bool enabled);

#endif // NRF_SDH_MOCK_H
