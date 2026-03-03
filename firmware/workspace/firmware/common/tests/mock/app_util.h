#ifndef APP_UTIL_MOCK_H_
#define APP_UTIL_MOCK_H_

// Minimal app_util.h replacement for unit tests.
#ifdef __cplusplus
#define STATIC_ASSERT(expr, msg) static_assert((expr), msg)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define STATIC_ASSERT(expr, msg) _Static_assert((expr), msg)
#else
#define STATIC_ASSERT(expr, msg) typedef char static_assertion_##__LINE__[(expr) ? 1 : -1]
#endif

#endif // APP_UTIL_MOCK_H_
