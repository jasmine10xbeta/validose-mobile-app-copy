/**
 * @file averaging_buffer.h
 * @brief Constant-time moving-average buffer for the last *N* samples.
 *
 * Usage example:
 *     #define AVG_BUF_LEN  16u      // any value ≥1 (power-of-two is cheapest)
 *     static avg_buf_t my_avg;
 *
 *     avg_buf_init(&my_avg, AVG_BUF_LEN);
 *     ...
 *     avg_buf_put(&my_avg, new_sample);
 *     uint32_t avg = avg_buf_get(&my_avg);
 *
 * @note All operations are O(1) and pointer-free – ideal for bare-metal
 *       Cortex-M4 using nRF5_SDK_17.1.0_ddde560.
 */

#ifndef AVERAGING_BUFFER_H_
#define AVERAGING_BUFFER_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint32_t *buf;      /**< Pointer to user-supplied storage                */
    uint32_t  len;      /**< Max number of samples (N)                       */
    uint32_t  idx;      /**< Ring index of next write                        */
    uint32_t  cnt;      /**< Number of samples accumulated so far            */
    uint64_t  sum;      /**< Running sum (64-bit avoids overflow for 32-bit) */
} avg_buf_t;

/**
 * @brief Initialise an averaging buffer.
 *
 * @param[in,out] ctx  Buffer context
 * @param[in]     len  Number of samples to average (≥1)
 * @param[in]     mem  Caller-allocated array of <len> uint32_t elements
 */
static inline void avg_buf_init(avg_buf_t *ctx,
                                uint32_t   len,
                                uint32_t  *mem)
{
    ctx->buf = mem;
    ctx->len = len;
    ctx->idx = 0u;
    ctx->cnt = 0u;
    ctx->sum = 0u;
}

/**
 * @brief Add a new sample.
 *
 * @param[in,out] ctx     Buffer context
 * @param[in]     sample  Sample value
 */
static inline void avg_buf_put(avg_buf_t *ctx, uint32_t sample)
{
    if (ctx->cnt < ctx->len)
    {
        /* Buffer not yet full */
        ctx->cnt++;
    }
    else
    {
        /* Subtract value that will be overwritten */
        ctx->sum -= ctx->buf[ctx->idx];
    }

    ctx->buf[ctx->idx] = sample;
    ctx->sum          += sample;

    /* Advance ring index */
    ctx->idx++;
    if (ctx->idx == ctx->len)
    {
        ctx->idx = 0u;
    }
}

/**
 * @brief Get the current average.
 *
 * @param[in] ctx Buffer context
 * @return Average of collected samples (0 if no samples yet)
 */
static inline uint32_t avg_buf_get(const avg_buf_t *ctx)
{
    return (ctx->cnt > 0u) ? (uint32_t)(ctx->sum / ctx->cnt) : 0u;
}

#endif /* AVERAGING_BUFFER_H_ */