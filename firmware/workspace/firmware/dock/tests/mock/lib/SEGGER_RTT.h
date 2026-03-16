
#ifndef SEGGER_RTT_MOCK_H
#define SEGGER_RTT_MOCK_H

int SEGGER_RTT_printf(unsigned BufferIndex, const char *sFormat, ...);
void SEGGER_RTT_SetTerminal(int index);

#endif // SEGGER_RTT_MOCK_H
