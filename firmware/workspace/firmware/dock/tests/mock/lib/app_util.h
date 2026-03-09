#ifndef APP_UTIL_MOCK_H
#define APP_UTIL_MOCK_H

#ifndef __LINT__

#   ifdef __GNUC__
#      ifdef __cplusplus
#         define STATIC_ASSERT_SIMPLE(EXPR)   extern char(*_do_assert(void))[sizeof(char[1 - 2 * !(EXPR)])]
#         define STATIC_ASSERT_MSG(EXPR, MSG) extern char(*_do_assert(void))[sizeof(char[1 - 2 * !(EXPR)])]
#      else
#         define STATIC_ASSERT_SIMPLE(EXPR)   _Static_assert(EXPR, "unspecified message")
#         define STATIC_ASSERT_MSG(EXPR, MSG) _Static_assert(EXPR, MSG)
#      endif
#   endif

#   ifdef __CC_ARM
#      define STATIC_ASSERT_SIMPLE(EXPR)   extern char(*_do_assert(void))[sizeof(char[1 - 2 * !(EXPR)])]
#      define STATIC_ASSERT_MSG(EXPR, MSG) extern char(*_do_assert(void))[sizeof(char[1 - 2 * !(EXPR)])]
#   endif

#   ifdef __ICCARM__
#      define STATIC_ASSERT_SIMPLE(EXPR)   static_assert(EXPR, "unspecified message")
#      define STATIC_ASSERT_MSG(EXPR, MSG) static_assert(EXPR, MSG)
#   endif

#else // __LINT__

#   define STATIC_ASSERT_SIMPLE(EXPR)   extern char(*_ignore(void))
#   define STATIC_ASSERT_MSG(EXPR, MSG) extern char(*_ignore(void))

#endif

#define _SELECT_ASSERT_FUNC(x, EXPR, MSG, ASSERT_MACRO, ...) ASSERT_MACRO

/**
 * @brief   Static (i.e. compile time) assert macro.
 *
 * @note The output of STATIC_ASSERT can be different across compilers.
 *
 * Usage:
 * STATIC_ASSERT(expression);
 * STATIC_ASSERT(expression, message);
 *
 * @hideinitializer
 */
// lint -save -esym(???, STATIC_ASSERT)
#define STATIC_ASSERT(...)                                                                                             \
   _SELECT_ASSERT_FUNC(x, ##__VA_ARGS__, STATIC_ASSERT_MSG(__VA_ARGS__), STATIC_ASSERT_SIMPLE(__VA_ARGS__))
// lint -restore

#endif /* APP_UTIL_MOCK_H */