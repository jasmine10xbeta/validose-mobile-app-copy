#ifndef UNITY_CONFIG_H_
#define UNITY_CONFIG_H_

#include "SEGGER_RTT.h"
#include <stdint.h>
#include <stdio.h>

#ifdef TEST
#   define UNITYEX_READ_CHAR SEGGER_RTT_GetKey
#else
#   define UNITYEX_READ_CHAR() (0)
#endif
#define UNITYEX_RESET_WDG()
#define UNITY_OUTPUT_CHAR(x) SEGGER_RTT_PutChar(0, x)
#define UNITY_OUTPUT_START()
#define UNITY_OUTPUT_COLOR
#define UNITY_USE_FLUSH_STDOUT

#endif /* UNITY_CONFIG_H_ */
