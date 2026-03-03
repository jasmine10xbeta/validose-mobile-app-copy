#include "nrf_sdh.h"

static bool m_nrf_sdh_enabled = true;

bool nrf_sdh_is_enabled(void)
{
   return m_nrf_sdh_enabled;
}

void nrf_sdh_mock_set_enabled(bool enabled)
{
   m_nrf_sdh_enabled = enabled;
}
