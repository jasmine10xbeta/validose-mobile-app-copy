#ifndef FDS_MOCK_H
#define FDS_MOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef RET_CODE_T_DEFINED
#define RET_CODE_T_DEFINED
typedef uint32_t ret_code_t;
#endif

#ifndef NRF_SUCCESS
#define NRF_SUCCESS (0u)
#endif

// Minimal mock error set needed by dock_data_manager.c
#ifndef FDS_ERR_NOT_FOUND
#define FDS_ERR_NOT_FOUND (1u)
#endif

#ifndef FDS_ERR_INVALID_ARG
#define FDS_ERR_INVALID_ARG (2u)
#endif

#ifndef FDS_ERR_NO_SPACE_IN_FLASH
#define FDS_ERR_NO_SPACE_IN_FLASH (3u)
#endif

typedef enum
{
   FDS_EVT_INIT = 0u,
} fds_evt_id_t;

typedef struct
{
   fds_evt_id_t id;
   ret_code_t result;
} fds_evt_t;

typedef void (*fds_cb_t)(fds_evt_t const *p_evt);

typedef struct
{
   uint16_t record_index;
} fds_record_desc_t;

typedef struct
{
   uint16_t record_index;
} fds_find_token_t;

typedef struct
{
   uint16_t file_id;
   uint16_t key;
   struct
   {
      void const *p_data;
      uint16_t length_words;
   } data;
} fds_record_t;

typedef struct
{
   uint16_t file_id;
   uint16_t key;
   uint16_t length_words;
} fds_record_header_t;

typedef struct
{
   fds_record_header_t const *p_header;
   void const *p_data;
} fds_flash_record_t;

ret_code_t fds_register(fds_cb_t evt_handler);
ret_code_t fds_init(void);
ret_code_t fds_record_find(uint16_t file_id,
                           uint16_t record_key,
                           fds_record_desc_t *desc,
                           fds_find_token_t *tok);
ret_code_t fds_record_write(fds_record_desc_t *desc, fds_record_t const *record);
ret_code_t fds_record_open(fds_record_desc_t const *desc, fds_flash_record_t *flash_record);
ret_code_t fds_record_close(fds_record_desc_t const *desc);
ret_code_t fds_record_update(fds_record_desc_t const *desc, fds_record_t const *record);

// Optional utility for tests that want a clean FDS state.
void fds_mock_reset(void);

#endif // FDS_MOCK_H
