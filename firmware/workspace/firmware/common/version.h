#ifndef VERSION_H
#define VERSION_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define DOCK_APP_VERSION "0.1.10"
#define RING_APP_VERSION "0.1.9"

/**
 * @brief Parse semantic version string "major.minor.patch" into uint8_t values.
 *
 * @param[in]  version_str Version string in "x.y.z" format.
 * @param[out] major       Parsed major version.
 * @param[out] minor       Parsed minor version.
 * @param[out] patch       Parsed patch version.
 *
 * @return true if parsing succeeds and each component is in [0, 255], false otherwise.
 */
static inline bool version_parse_triplet_u8(const char *version_str, uint8_t *major, uint8_t *minor, uint8_t *patch)
{
   if((NULL == version_str) || (NULL == major) || (NULL == minor) || (NULL == patch))
   {
      return false;
   }

   uint8_t *parts[3] = {major, minor, patch};

   for(uint8_t idx = 0u; idx < 3u; ++idx)
   {
      uint16_t value = 0u;
      uint8_t ch = (uint8_t)(*version_str);

      if((ch < (uint8_t)'0') || (ch > (uint8_t)'9'))
      {
         return false;
      }

      while((ch >= (uint8_t)'0') && (ch <= (uint8_t)'9'))
      {
         value = (uint16_t)((value * 10u) + (uint16_t)(ch - (uint8_t)'0'));
         if(value > UINT8_MAX)
         {
            return false;
         }

         ++version_str;
         ch = (uint8_t)(*version_str);
      }

      *parts[idx] = (uint8_t)value;

      if(idx < 2u)
      {
         if(ch != (uint8_t)'.')
         {
            return false;
         }
         ++version_str;
      }
   }

   return ('\0' == *version_str);
}

#endif // VERSION_H
