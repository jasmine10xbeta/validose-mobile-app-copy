/*
 * Copyright (C) 10XBETA - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @ingroup common
 * @brief Defines the result type and associated macros used for function status returns and the efficient handling
 * thereof.
 * @details
 *
 * @file result_macros.h
 * @version 1.0.5
 * @ingroup common
 * @brief
 */

#ifndef RESULT_MACROS_H_
#define RESULT_MACROS_H_
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <stdint.h>

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
// Magic number indicating initialization has ocurred.
// Specifically chosen not to be a hexword or ASCII that could appear randomly in uninitialized memory.
// Included here because a module's initializer will need to include result.h for the return value anyway
#define INITIALIZED (0x1A2B3C4Du)

#define RESULT_NO_ERROR (0u)

#define ERROR_VALUE_MASK (0xFF) // Error value is encoded in 8 bits

typedef uint16_t result_t;

/**
 * @brief Initialize a result_t with "okay" value
 *
 * @return A `result_t` value with no bits set.
 */
#define RESULT_OK ((result_t)RESULT_NO_ERROR)

/**
 * @brief Initialize a result_t with a specific error value
 *
 * @return A `result_t` value with specific error bits set.
 */
#define RESULT_THIS_UNIT_ERROR(err_val)                                                                                \
   (result_t)(((result_t)(THIS_UNIT_ID) << 8u) | ((result_t)((err_val)&ERROR_VALUE_MASK)))

/**
 * @brief This macro checks if the given result_t value IS_OK. If yes, run the given expression and update result.
 * @param[in] result The result value to check, and then update should the expression run
 * @param[in] expr The expression to run should the IS_OK(result) equate to true.
 *
 */
#define IF_OK_RUN_AND_UPDATE(result, expr)                                                                             \
   do                                                                                                                  \
   {                                                                                                                   \
      if(IS_OK(result))                                                                                                \
      {                                                                                                                \
         result = (expr);                                                                                              \
      }                                                                                                                \
   } while(0)

/**
 * @brief Set the status of a `result_t` to its specified error value and return
 *
 * @param[in] result The result_t to modify.
 * @param[in] err_val The new error value.
 */
#define RETURN_ERR(err_val)                                                                                            \
   do                                                                                                                  \
   {                                                                                                                   \
      return (result_t)((result_t)((result_t)(THIS_UNIT_ID) << 8u) | ((result_t)((err_val)&ERROR_VALUE_MASK)));        \
   } while(0)

/**
 * @brief Check if an interface or its parent is NULL and return an error result if true.
 *
 * This macro checks if the given `interface` or its `parent` is NULL. If either is NULL, it invokes `RETURN_ERR` with
 * the error set to the provided `null_err` value.
 *
 * @param[in] interface The interface to check.
 * @param[in] null_err The error value to return if the interface or its parent is NULL.
 */
#define RETURN_ERR_IF_INTERFACE_NULL(interface, null_err)                                                              \
   do                                                                                                                  \
   {                                                                                                                   \
      if(((interface) == NULL) || ((interface->parent) == NULL))                                                       \
      {                                                                                                                \
         RETURN_ERR(null_err);                                                                                         \
      }                                                                                                                \
   } while(0)

/**
 * @brief This macro checks if the given Nordic Semiconductor nRF SDK return code is not NRF_SUCCESS.
 *        If the return code is not NRF_SUCCESS, it invokes `RETURN_ERR` with the error set to the provided `err_val`
 * value.
 * @param[in] nrf_ret_code The Nordic Semiconductor nRF SDK return code to check.
 * @param[in] err_val The error value to return if the nrf_ret_code is not NRF_SUCCESS.
 *
 * @return If nrf_ret_code is not NRF_SUCCESS, it returns a `result_t` with `value` set to the provided `err_val`.
 *         If nrf_ret_code is NRF_SUCCESS, it does not return anything.
 */
#define RETURN_ERR_IF_NRF_ERR(nrf_ret_code, err_val)                                                                   \
   do                                                                                                                  \
   {                                                                                                                   \
      if((nrf_ret_code) != NRF_SUCCESS)                                                                                \
      {                                                                                                                \
         RETURN_ERR(err_val);                                                                                          \
      }                                                                                                                \
   } while(0)

/**
 * @brief Check if a given pointer variable is NULL and return an error result if true.
 *
 * This macro checks if the given `pointer_var` is NULL. If it is NULL, it invokes `RETURN_ERR` with the error set to
 * the provided `null_err` value.
 *
 * @param[in] pointer_var The pointer variable to check.
 * @param[in] null_err The error value to return if the pointer variable is NULL.
 */
#define RETURN_ERR_IF_NULL(pointer_var, null_err)                                                                      \
   do                                                                                                                  \
   {                                                                                                                   \
      if((pointer_var) == NULL)                                                                                        \
      {                                                                                                                \
         RETURN_ERR(null_err);                                                                                         \
      }                                                                                                                \
   } while(0)

/**
 * @brief Check if a given statement evaluates to true and return the given error result if does
 *
 * This macro checks if the given `statement` evaluates to true. If it does, it creates a `result_t` with
 * `value` set to the provided `err` value, along with the current unit's ID. The macro
 * then returns this `result_t` immediately.
 *
 * @param[in] statement The statement to evaluate for truthfulness
 * @param[in] err The error value to return if the pointer variable is NULL.
 */
#define RETURN_ERR_IF_TRUE(statement, err)                                                                             \
   do                                                                                                                  \
   {                                                                                                                   \
      if((statement))                                                                                                  \
      {                                                                                                                \
         RETURN_ERR(err);                                                                                              \
      }                                                                                                                \
   } while(0)

/**
 * @brief Return (void) immediately if specified pointer value is NULL.
 *
 * @param[in] pointer_var The pointer variable to check.
 */
#define RETURN_VOID_IF_NULL(pointer_var)                                                                               \
   do                                                                                                                  \
   {                                                                                                                   \
      if((pointer_var) == NULL)                                                                                        \
      {                                                                                                                \
         return;                                                                                                       \
      }                                                                                                                \
   } while(0)

/**
 * @brief Return a specific value immediately if specified pointer value is NULL.
 *
 * @param[in] pointer_var The pointer variable to check.
 * @param[in] return_val The value to return on NULL.
 */
#define RETURN_VALUE_IF_NULL(pointer_var, return_val)                                                                  \
   do                                                                                                                  \
   {                                                                                                                   \
      if((pointer_var) == NULL)                                                                                        \
      {                                                                                                                \
         return (return_val);                                                                                          \
      }                                                                                                                \
   } while(0)

/**
 * @brief Check if a `result_t` has a value other than `RESULT_NO_ERROR`.
 *
 * @param[in] result The result_t to check.
 */
#define IS_ERR(result) (RESULT_NO_ERROR != (result))

/**
 * @brief Check if a `result_t` has a value of `RESULT_NO_ERROR`.
 *
 * @param[in] result The result_t to check.
 */
#define IS_OK(result) (RESULT_NO_ERROR == (result))

/**
 * @brief Return immediately if a `result_t` has a value other than `RESULT_NO_ERROR`.
 *
 * @param[in] result The result_t to check.
 */
#define RETURN_ON_ERR(result)                                                                                          \
   do                                                                                                                  \
   {                                                                                                                   \
      if(RESULT_NO_ERROR != result)                                                                                    \
      {                                                                                                                \
         return (result);                                                                                              \
      }                                                                                                                \
   } while(0)

/**
 * @brief Break from loop immediately if a `result_t` has a value other than `RESULT_NO_ERROR`.
 *
 * @param[in] result The result_t to check.
 */
#define BREAK_ON_ERR(result)                                                                                           \
   do                                                                                                                  \
   {                                                                                                                   \
      if((result) != RESULT_NO_ERROR)                                                                                  \
      {                                                                                                                \
         break;                                                                                                        \
      }                                                                                                                \
   } while(0)

/**
 * @brief If a `result_t` has a value other than `RESULT_NO_ERROR`, update its error value to the specified
 * value.
 *
 * @param[in] result The result_t to check and modify.
 * @param[in] err_val The new error value.
 */
#define UPDATE_ERR(result, err_val)                                                                                    \
   do                                                                                                                  \
   {                                                                                                                   \
      if(RESULT_NO_ERROR != result)                                                                                    \
      {                                                                                                                \
         result = (result_t)((result_t)((result_t)(THIS_UNIT_ID) << 8u) | ((result_t)((err_val)&ERROR_VALUE_MASK)));   \
      }                                                                                                                \
   } while(0)

/**
 * @brief Set the status of a `result_t` to its specified error value
 *
 * @param[in] result The result_t to modify.
 * @param[in] err_val The new error value.
 */
#define SET_ERR(result, err_val)                                                                                       \
   do                                                                                                                  \
   {                                                                                                                   \
      result = (result_t)((result_t)((result_t)(THIS_UNIT_ID) << 8u) | ((result_t)((err_val)&ERROR_VALUE_MASK)));      \
   } while(0)

/**
 * @brief Update the error value of `result` if the given statement is true.
 *
 * This macro checks if the given `statement` evaluates to true. If it does, it sets the error value of `result`
 * to the specified `err_val` using the SET_ERR macro.
 *
 * @param[in] statement The statement to evaluate for truthfulness.
 * @param[in] err_val The error value to set if the statement is true.
 */
#define UPDATE_ERR_IF_TRUE(result, statement, err_val)                                                                 \
   do                                                                                                                  \
   {                                                                                                                   \
      if((statement))                                                                                                  \
      {                                                                                                                \
         SET_ERR(result, err_val);                                                                                     \
      }                                                                                                                \
   } while(0)

/**
 * @brief This macro checks if the given Nordic Semiconductor nRF SDK return code is not NRF_SUCCESS.
 *        If the return code is not NRF_SUCCESS, it invokes `SETT_ERR` with the error set to the provided `err_val`
 * value.
 * @param[in] nrf_ret_code The Nordic Semiconductor nRF SDK return code to check.
 * @param[in] result The result to modify in case of NRF ERROR.
 * @param[in] err_val The error value to which result must be set in case of NRF ERROR
 *
 */
#define UPDATE_IF_NRF_ERR(nrf_ret_code, result, err_val)                                                               \
   do                                                                                                                  \
   {                                                                                                                   \
      if((nrf_ret_code) != NRF_SUCCESS)                                                                                \
      {                                                                                                                \
         SET_ERR(result, err_val);                                                                                     \
      }                                                                                                                \
   } while(0)

/**
 * @brief Check if a given statement evaluates to true and returns RESULT_OK if it does
 *
 * This macro checks if the given `statement` evaluates to true. If it does, it creates a `result_t` with the value of
 * RESULT_OK. The macro then returns this `result_t` immediately.
 *
 * @param[in] statement The statement to evaluate for truthfulness
 */
#define RETURN_OK_IF_TRUE(statement)                                                                                   \
   do                                                                                                                  \
   {                                                                                                                   \
      if((statement))                                                                                                  \
      {                                                                                                                \
         return RESULT_OK;                                                                                             \
      }                                                                                                                \
   } while(0)

/**
 * @brief Check if an interface or its parent is NULL or unitialized and return an error result if true.
 *
 * This macro checks if the given `interface` or its `parent` is NULL. If either is NULL, it invokes `RETURN_ERR` with
 * the error set to the provided `null_err` value. It then checks if the parent is initialized and returns the error
 * if it is not
 *
 * @param[in] interface The interface to check.
 * @param[in] null_err The error value to return if the interface or its parent is NULL or uninitialized
 */
#define RETURN_ERR_IF_UNINITIALIZED(interface, null_err)                                                               \
   do                                                                                                                  \
   {                                                                                                                   \
      if(((interface) == NULL) || ((interface->parent) == NULL))                                                       \
      {                                                                                                                \
         RETURN_ERR(null_err);                                                                                         \
      }                                                                                                                \
      else if((interface->parent->_initialization_status) != INITIALIZED)                                              \
      {                                                                                                                \
         RETURN_ERR(null_err);                                                                                         \
      }                                                                                                                \
   } while(0)

/**
 * @brief Make a copy of a `result_t`.
 *
 * @param[in] dest_result The result_t to modify.
 * @param[in] src_result The result_t to copy from.
 */
#define CLONE_ERR(dest_result, src_result) (dest_result = src_result)

/**
 * @brief Reset a `result_t` to `RESULT_NO_ERROR`
 *
 * @param[in] result The result_t to modify.
 * @param[in] err_val The new error value.
 */
#define CLEAR_ERR(result) (result = RESULT_NO_ERROR)

/**
 * @brief Extract bits 8:15 (8bits) of a result_t value which correspond to the Unit ID.
 *
 * @param[in] result The result_t to check.
 */
#define GET_ERR_UNIT(result) ((result >> 8) & 0xFF)

/**
 * @brief Extract bits 0:7 bits (8bits) of a result_t value which correspond to error value within a given
 * Unit.
 *
 * @param[in] result The result_t to check.
 */
#define GET_ERR_CODE(result) (result & 0xFF)

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global functions
 **********************************************************************************************************************/
#endif // RESULT_MACROS_H_
