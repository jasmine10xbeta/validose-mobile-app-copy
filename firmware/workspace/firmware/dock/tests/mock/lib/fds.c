#include "fds.h"

#include <string.h>

#define FDS_MOCK_MAX_RECORDS (16u)
#define FDS_MOCK_MAX_WORDS   (128u)

typedef struct
{
   bool used;
   fds_record_header_t header;
   uint32_t words[FDS_MOCK_MAX_WORDS];
} fds_mock_record_t;

static fds_cb_t m_evt_handler = NULL;
static fds_mock_record_t m_records[FDS_MOCK_MAX_RECORDS] = {0};

static int find_record_slot(uint16_t file_id, uint16_t key)
{
   for(size_t i = 0u; i < FDS_MOCK_MAX_RECORDS; i++)
   {
      if(m_records[i].used && (m_records[i].header.file_id == file_id) && (m_records[i].header.key == key))
      {
         return (int)i;
      }
   }
   return -1;
}

static int find_free_slot(void)
{
   for(size_t i = 0u; i < FDS_MOCK_MAX_RECORDS; i++)
   {
      if(!m_records[i].used)
      {
         return (int)i;
      }
   }
   return -1;
}

ret_code_t fds_register(fds_cb_t evt_handler)
{
   m_evt_handler = evt_handler;
   return NRF_SUCCESS;
}

ret_code_t fds_init(void)
{
   if(NULL != m_evt_handler)
   {
      fds_evt_t evt = {.id = FDS_EVT_INIT, .result = NRF_SUCCESS};
      m_evt_handler(&evt);
   }
   return NRF_SUCCESS;
}

ret_code_t fds_record_find(uint16_t file_id,
                           uint16_t record_key,
                           fds_record_desc_t *desc,
                           fds_find_token_t *tok)
{
   (void)tok;
   int slot = find_record_slot(file_id, record_key);
   if(slot < 0)
   {
      return FDS_ERR_NOT_FOUND;
   }

   if(NULL != desc)
   {
      desc->record_index = (uint16_t)slot;
   }
   return NRF_SUCCESS;
}

ret_code_t fds_record_write(fds_record_desc_t *desc, fds_record_t const *record)
{
   if((NULL == record) || (NULL == record->data.p_data))
   {
      return FDS_ERR_INVALID_ARG;
   }

   if(record->data.length_words > FDS_MOCK_MAX_WORDS)
   {
      return FDS_ERR_NO_SPACE_IN_FLASH;
   }

   int slot = find_record_slot(record->file_id, record->key);
   if(slot < 0)
   {
      slot = find_free_slot();
   }

   if(slot < 0)
   {
      return FDS_ERR_NO_SPACE_IN_FLASH;
   }

   m_records[slot].used = true;
   m_records[slot].header.file_id = record->file_id;
   m_records[slot].header.key = record->key;
   m_records[slot].header.length_words = record->data.length_words;
   memset(m_records[slot].words, 0, sizeof(m_records[slot].words));
   memcpy(m_records[slot].words, record->data.p_data, record->data.length_words * sizeof(uint32_t));

   if(NULL != desc)
   {
      desc->record_index = (uint16_t)slot;
   }
   return NRF_SUCCESS;
}

ret_code_t fds_record_open(fds_record_desc_t const *desc, fds_flash_record_t *flash_record)
{
   if((NULL == desc) || (NULL == flash_record))
   {
      return FDS_ERR_INVALID_ARG;
   }

   uint16_t slot = desc->record_index;
   if((slot >= FDS_MOCK_MAX_RECORDS) || !m_records[slot].used)
   {
      return FDS_ERR_NOT_FOUND;
   }

   flash_record->p_header = &m_records[slot].header;
   flash_record->p_data = m_records[slot].words;
   return NRF_SUCCESS;
}

ret_code_t fds_record_close(fds_record_desc_t const *desc)
{
   (void)desc;
   return NRF_SUCCESS;
}

ret_code_t fds_record_update(fds_record_desc_t const *desc, fds_record_t const *record)
{
   if((NULL == desc) || (NULL == record) || (NULL == record->data.p_data))
   {
      return FDS_ERR_INVALID_ARG;
   }

   uint16_t slot = desc->record_index;
   if((slot >= FDS_MOCK_MAX_RECORDS) || !m_records[slot].used)
   {
      return FDS_ERR_NOT_FOUND;
   }

   if(record->data.length_words > FDS_MOCK_MAX_WORDS)
   {
      return FDS_ERR_NO_SPACE_IN_FLASH;
   }

   m_records[slot].header.file_id = record->file_id;
   m_records[slot].header.key = record->key;
   m_records[slot].header.length_words = record->data.length_words;
   memset(m_records[slot].words, 0, sizeof(m_records[slot].words));
   memcpy(m_records[slot].words, record->data.p_data, record->data.length_words * sizeof(uint32_t));

   return NRF_SUCCESS;
}

void fds_mock_reset(void)
{
   memset(m_records, 0, sizeof(m_records));
}
