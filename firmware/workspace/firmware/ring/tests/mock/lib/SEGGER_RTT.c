#include "SEGGER_RTT.h"

int SEGGER_RTT_printf(unsigned BufferIndex, const char *sFormat, ...) // NOSONAR - mocking external function prototype
{
   (void)BufferIndex;
   (void)sFormat;
   return 0;
}

void SEGGER_RTT_SetTerminal(int index) // NOSONAR - mocking external function prototype
{
   (void)index;
}