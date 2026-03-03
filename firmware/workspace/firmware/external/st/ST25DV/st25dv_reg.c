/**
  ******************************************************************************
  * @file    st25dv_reg.c
  * @author  MMY Ecosystem Team
  * @brief   ST25DV register file
  ******************************************************************************
  * @attention
  *
  * COPYRIGHT 2021 STMicroelectronics, all rights reserved
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied,
  * AND SPECIFICALLY DISCLAIMING THE IMPLIED WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */
  
/* Includes ------------------------------------------------------------------*/
#include "st25dv_reg.h"

/** @addtogroup BSP
  * @{
  */

/** @addtogroup ST25DV
  * @{
  */
  
/* Private typedef -----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/
/* Private macros ------------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Public functions ---------------------------------------------------------*/
/**
 * @brief  Read register from component
 * @param[in] ctx structure containing context driver
 * @param[in] Reg register to read
 * @param[out] Data pointer to store register content
 * @param[out] len length of data
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_ReadReg(const ST25DV_Ctx_t *const ctx, uint16_t Reg, uint8_t* Data, uint16_t len)
{
  return ctx->ReadReg(ctx->handle, Reg, Data, len);
}

/**
 * @brief  Write register to component
 * @param[in] ctx structure containing context driver
 * @param[in] Reg register to write
 * @param[in] Data data pointer to write to register
 * @param[out] len length of data
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_WriteReg(const ST25DV_Ctx_t *const ctx, uint16_t Reg, uint8_t const *Data, uint16_t len)
{
  return ctx->WriteReg(ctx->handle, Reg, Data, len);
}

/**
 * @brief  Read IC Ref register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetICREF(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_ICREF_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read ENDA1 register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetENDA1(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_ENDA1_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Write ENDA1 register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetENDA1(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  if(ST25DV_WriteReg(ctx, (ST25DV_ENDA1_REG), value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read ENDA2 register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetENDA2(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_ENDA2_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Write ENDA2 register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetENDA2(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  if(ST25DV_WriteReg(ctx, (ST25DV_ENDA2_REG), value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read ENDA3 register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetENDA3(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_ENDA3_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Write ENDA3 register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetENDA3(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  if(ST25DV_WriteReg(ctx, (ST25DV_ENDA3_REG), value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read DSFID register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetDSFID(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_DSFID_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read AFI register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetAFI(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_AFI_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read MEM_SIZE_MSB register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetMEM_SIZE_MSB(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_MEM_SIZE_MSB_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read BLK_SIZE register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetBLK_SIZE(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_BLK_SIZE_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read MEM_SIZE_LSB register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetMEM_SIZE_LSB(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_MEM_SIZE_LSB_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read ICREV register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetICREV(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_ICREV_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read UID register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetUID(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_UID_REG), (uint8_t *)value, 8) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read I2CPASSWD register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetI2CPASSWD(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_I2CPASSWD_REG), (uint8_t *)value, 8) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Write I2CPASSWD register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetI2CPASSWD(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  if(ST25DV_WriteReg(ctx, (ST25DV_I2CPASSWD_REG), value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read LOCKDSFI register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetLOCKDSFID(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_LOCKDSFID_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read LOCKAFI register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetLOCKAFI(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_LOCKAFI_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read MB_MODE_RW register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetMB_MODE_RW(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_MB_MODE_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_MB_MODE_RW_MASK);
  *value = *value >> (ST25DV_MB_MODE_RW_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write MB_MODE_RW register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetMB_MODE_RW(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_MB_MODE_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ( (*value << (ST25DV_MB_MODE_RW_SHIFT)) & (ST25DV_MB_MODE_RW_MASK)) |
                (reg_value & ~(ST25DV_MB_MODE_RW_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_MB_MODE_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read MBLEN_DYN_MBLEN register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetMBLEN_DYN_MBLEN(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_MBLEN_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read MB_CTRL_DYN_MBEN register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetMB_CTRL_DYN_MBEN(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_MB_CTRL_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_MB_CTRL_DYN_MBEN_MASK);
  *value = *value >> (ST25DV_MB_CTRL_DYN_MBEN_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write MB_CTRL_DYN_MBEN register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetMB_CTRL_DYN_MBEN(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_MB_CTRL_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_MB_CTRL_DYN_MBEN_SHIFT)) & (ST25DV_MB_CTRL_DYN_MBEN_MASK)) |
                (reg_value & ~(ST25DV_MB_CTRL_DYN_MBEN_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_MB_CTRL_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read MB_CTRL_DYN_HOSTPUTMSG register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetMB_CTRL_DYN_HOSTPUTMSG(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_MB_CTRL_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_MB_CTRL_DYN_HOSTPUTMSG_MASK);
  *value = *value >> (ST25DV_MB_CTRL_DYN_HOSTPUTMSG_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Read MB_CTRL_DYN_RFPUTMSG register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetMB_CTRL_DYN_RFPUTMSG(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_MB_CTRL_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_MB_CTRL_DYN_RFPUTMSG_MASK);
  *value = *value >> (ST25DV_MB_CTRL_DYN_RFPUTMSG_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Read MB_CTRL_DYN_STRESERVED register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetMB_CTRL_DYN_STRESERVED(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_MB_CTRL_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_MB_CTRL_DYN_STRESERVED_MASK);
  *value = *value >> (ST25DV_MB_CTRL_DYN_STRESERVED_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Read MB_CTRL_DYN_HOSTMISSMSG register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetMB_CTRL_DYN_HOSTMISSMSG(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_MB_CTRL_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_MB_CTRL_DYN_HOSTMISSMSG_MASK);
  *value = *value >> (ST25DV_MB_CTRL_DYN_HOSTMISSMSG_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Read MB_CTRL_DYN_RFMISSMSG register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetMB_CTRL_DYN_RFMISSMSG(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_MB_CTRL_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_MB_CTRL_DYN_RFMISSMSG_MASK);
  *value = *value >> (ST25DV_MB_CTRL_DYN_RFMISSMSG_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Read MB_CTRL_DYN_CURRENTMSG register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetMB_CTRL_DYN_CURRENTMSG(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_MB_CTRL_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_MB_CTRL_DYN_CURRENTMSG_MASK);
  *value = *value >> (ST25DV_MB_CTRL_DYN_CURRENTMSG_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Read MB_CTRL_DYN_ALL register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetMB_CTRL_DYN_ALL(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_MB_CTRL_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read MB_WDG_DELAY register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetMB_WDG_DELAY(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_MB_WDG_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_MB_WDG_DELAY_MASK);
  *value = *value >> (ST25DV_MB_WDG_DELAY_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write MB_WDG_DELAY register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetMB_WDG_DELAY(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_MB_WDG_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_MB_WDG_DELAY_SHIFT)) & (ST25DV_MB_WDG_DELAY_MASK)) |
                (reg_value & ~(ST25DV_MB_WDG_DELAY_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_MB_WDG_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read GPO_RFUSERSTATE register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetGPO_RFUSERSTATE(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_GPO_RFUSERSTATE_MASK);
  *value = *value >> (ST25DV_GPO_RFUSERSTATE_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write GPO_RFUSERSTATE register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetGPO_RFUSERSTATE(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ( (*value << (ST25DV_GPO_RFUSERSTATE_SHIFT)) & (ST25DV_GPO_RFUSERSTATE_MASK)) |
                (reg_value & ~(ST25DV_GPO_RFUSERSTATE_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_GPO_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read GPO_RFACTIVITY register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetGPO_RFACTIVITY(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_GPO_RFACTIVITY_MASK);
  *value = *value >> (ST25DV_GPO_RFACTIVITY_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write GPO_RFACTIVITY register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetGPO_RFACTIVITY(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_GPO_RFACTIVITY_SHIFT)) & (ST25DV_GPO_RFACTIVITY_MASK)) |
                (reg_value & ~(ST25DV_GPO_RFACTIVITY_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_GPO_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read GPO_RFINTERRUPT register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetGPO_RFINTERRUPT(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_GPO_RFINTERRUPT_MASK);
  *value = *value >> (ST25DV_GPO_RFINTERRUPT_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write GPO_RFINTERRUPT register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetGPO_RFINTERRUPT(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ( (*value << (ST25DV_GPO_RFINTERRUPT_SHIFT)) & (ST25DV_GPO_RFINTERRUPT_MASK)) |
                (reg_value & ~(ST25DV_GPO_RFINTERRUPT_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_GPO_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read GPO_FIELDCHANGE register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetGPO_FIELDCHANGE(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_GPO_FIELDCHANGE_MASK);
  *value = *value >> (ST25DV_GPO_FIELDCHANGE_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write GPO_FIELDCHANGE register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetGPO_FIELDCHANGE(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_GPO_FIELDCHANGE_SHIFT)) & (ST25DV_GPO_FIELDCHANGE_MASK)) |
                (reg_value & ~(ST25DV_GPO_FIELDCHANGE_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_GPO_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read GPO_RFPUTMSG register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetGPO_RFPUTMSG(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_GPO_RFPUTMSG_MASK);
  *value = *value >> (ST25DV_GPO_RFPUTMSG_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write GPO_RFPUTMSG register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetGPO_RFPUTMSG(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_GPO_RFPUTMSG_SHIFT)) & (ST25DV_GPO_RFPUTMSG_MASK)) |
                (reg_value & ~(ST25DV_GPO_RFPUTMSG_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_GPO_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read GPO_RFGETMSG register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetGPO_RFGETMSG(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_GPO_RFGETMSG_MASK);
  *value = *value >> (ST25DV_GPO_RFGETMSG_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write GPO_RFGETMSG register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetGPO_RFGETMSG(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_GPO_RFGETMSG_SHIFT)) & (ST25DV_GPO_RFGETMSG_MASK)) |
                (reg_value & ~(ST25DV_GPO_RFGETMSG_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_GPO_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read GPO_RFWRITE register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetGPO1_RFWRITE(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_GPO_RFWRITE_MASK);
  *value = *value >> (ST25DV_GPO_RFWRITE_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write GPO_RFWRITE register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetGPO_RFWRITE(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ( (*value << (ST25DV_GPO_RFWRITE_SHIFT)) & (ST25DV_GPO_RFWRITE_MASK)) |
                (reg_value & ~(ST25DV_GPO_RFWRITE_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_GPO_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read GPO_ENABLE register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetGPO_ENABLE(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_GPO_ENABLE_MASK);
  *value = *value >> (ST25DV_GPO_ENABLE_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write GPO_ENABLE register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetGPO_ENABLE(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_GPO_ENABLE_SHIFT)) & (ST25DV_GPO_ENABLE_MASK)) |
                (reg_value & ~(ST25DV_GPO_ENABLE_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_GPO_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read GPO_ALL register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetGPO_ALL(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Write GPO_ALL register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetGPO_ALL(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  if(ST25DV_WriteReg(ctx, (ST25DV_GPO_REG), value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  return NFCTAG_OK;
}

/**
 * @brief  Read GPO_DYN_RFUSERSTATE register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetGPO_DYN_RFUSERSTATE(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_GPO_DYN_RFUSERSTATE_MASK);
  *value = *value >> (ST25DV_GPO_DYN_RFUSERSTATE_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write GPO_DYN_RFUSERSTATE register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetGPO_DYN_RFUSERSTATE(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_GPO_DYN_RFUSERSTATE_SHIFT)) & (ST25DV_GPO_DYN_RFUSERSTATE_MASK)) |
                (reg_value & ~(ST25DV_GPO_DYN_RFUSERSTATE_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_GPO_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read GPO_DYN_RFACTIVITY register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetGPO_DYN_RFACTIVITY(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_GPO_DYN_RFACTIVITY_MASK);
  *value = *value >> (ST25DV_GPO_DYN_RFACTIVITY_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write GPO_DYN_ALL register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetGPO_DYN_RFACTIVITY(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ( (*value << (ST25DV_GPO_DYN_RFACTIVITY_SHIFT)) & (ST25DV_GPO_DYN_RFACTIVITY_MASK)) |
                (reg_value & ~(ST25DV_GPO_DYN_RFACTIVITY_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_GPO_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read GPO_DYN_RFINTERRUPT register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetGPO_DYN_RFINTERRUPT(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_GPO_DYN_RFINTERRUPT_MASK);
  *value = *value >> (ST25DV_GPO_DYN_RFINTERRUPT_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write GPO_DYN_RFINTERRUPT register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetGPO_DYN_RFINTERRUPT(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;

  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
    }

  reg_value = ((*value << (ST25DV_GPO_DYN_RFINTERRUPT_SHIFT)) & (ST25DV_GPO_DYN_RFINTERRUPT_MASK)) |
                (reg_value & ~(ST25DV_GPO_DYN_RFINTERRUPT_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_GPO_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read GPO_DYN_FIELDCHANGE register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetGPO_DYN_FIELDCHANGE(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_GPO_DYN_FIELDCHANGE_MASK);
  *value = *value >> (ST25DV_GPO_DYN_FIELDCHANGE_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write GPO_DYN_FIELDCHANGE register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetGPO_DYN_FIELDCHANGE(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_GPO_DYN_FIELDCHANGE_SHIFT)) & (ST25DV_GPO_DYN_FIELDCHANGE_MASK)) |
                (reg_value & ~(ST25DV_GPO_DYN_FIELDCHANGE_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_GPO_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read GPO_DYN_RFPUTMSG register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetGPO_DYN_RFPUTMSG(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_GPO_DYN_RFPUTMSG_MASK);
  *value = *value >> (ST25DV_GPO_DYN_RFPUTMSG_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write GPO_DYN_RFPUTMSG register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetGPO_DYN_RFPUTMSG(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_GPO_DYN_RFPUTMSG_SHIFT)) & (ST25DV_GPO_DYN_RFPUTMSG_MASK)) |
                (reg_value & ~(ST25DV_GPO_DYN_RFPUTMSG_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_GPO_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read GPO_DYN_RFGETMSG register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetGPO_DYN_RFGETMSG(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_GPO_DYN_RFGETMSG_MASK);
  *value = *value >> (ST25DV_GPO_DYN_RFGETMSG_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write GPO_DYN_RFGETMSG register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetGPO_DYN_RFGETMSG(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_GPO_DYN_RFGETMSG_SHIFT)) & (ST25DV_GPO_DYN_RFGETMSG_MASK)) |
                (reg_value & ~(ST25DV_GPO_DYN_RFGETMSG_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_GPO_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read GPO_DYN_RFWRITE register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetGPO_DYN_RFWRITE(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_GPO_DYN_RFWRITE_MASK);
  *value = *value >> (ST25DV_GPO_DYN_RFWRITE_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write GPO_DYN_RFWRITE register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetGPO_DYN_RFWRITE(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_GPO_DYN_RFWRITE_SHIFT)) & (ST25DV_GPO_DYN_RFWRITE_MASK)) |
                (reg_value & ~(ST25DV_GPO_DYN_RFWRITE_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_GPO_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read GPO_DYN_ENABLE register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetGPO_DYN_ENABLE(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_GPO_DYN_ENABLE_MASK);
  *value = *value >> (ST25DV_GPO_DYN_ENABLE_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write GPO_DYN_ENABLE register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetGPO_DYN_ENABLE(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ( (*value << (ST25DV_GPO_DYN_ENABLE_SHIFT)) & (ST25DV_GPO_DYN_ENABLE_MASK)) |
                (reg_value & ~(ST25DV_GPO_DYN_ENABLE_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_GPO_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read GPO_DYN_ALL register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetGPO_DYN_ALL(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_GPO_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Write GPO_DYN_ALL register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetGPO_DYN_ALL(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  if(ST25DV_WriteReg(ctx, (ST25DV_GPO_DYN_REG), value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read ITTIME_DELAY register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetITTIME_DELAY(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_ITTIME_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_ITTIME_DELAY_MASK);
  *value = *value >> (ST25DV_ITTIME_DELAY_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write ITTIME_DELAY register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetITTIME_DELAY(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_ITTIME_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_ITTIME_DELAY_SHIFT)) & (ST25DV_ITTIME_DELAY_MASK)) |
                (reg_value & ~(ST25DV_ITTIME_DELAY_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_ITTIME_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read ITSTS_DYN_RFUSERSTATE register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetITSTS_DYN_RFUSERSTATE(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_ITSTS_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_ITSTS_DYN_RFUSERSTATE_MASK);
  *value = *value >> (ST25DV_ITSTS_DYN_RFUSERSTATE_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Read ITSTS_DYN_RFACTIVITY register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetITSTS_DYN_RFACTIVITY(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_ITSTS_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_ITSTS_DYN_RFACTIVITY_MASK);
  *value = *value >> (ST25DV_ITSTS_DYN_RFACTIVITY_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Read ITSTS_DYN_RFINTERRUPT register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetITSTS_DYN_RFINTERRUPT (const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_ITSTS_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_ITSTS_DYN_RFINTERRUPT_MASK);
  *value = *value >> (ST25DV_ITSTS_DYN_RFINTERRUPT_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Read ITSTS_DYN_FIELDFALLING register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetITSTS_DYN_FIELDFALLING(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_ITSTS_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_ITSTS_DYN_FIELDFALLING_MASK);
  *value = *value >> (ST25DV_ITSTS_DYN_FIELDFALLING_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Read ITSTS_DYN_FIELDRISING register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetITSTS_DYN_FIELDRISING(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_ITSTS_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_ITSTS_DYN_FIELDRISING_MASK);
  *value = *value >> (ST25DV_ITSTS_DYN_FIELDRISING_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Read ITSTS_DYN_RFPUTMSG register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetITSTS_DYN_RFPUTMSG(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_ITSTS_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_ITSTS_DYN_RFPUTMSG_MASK);
  *value = *value >> (ST25DV_ITSTS_DYN_RFPUTMSG_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Read ITSTS_DYN_RFGETMSG register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetITSTS_DYN_RFGETMSG(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_ITSTS_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_ITSTS_DYN_RFGETMSG_MASK);
  *value = *value >> (ST25DV_ITSTS_DYN_RFGETMSG_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Read ITSTS_DYN_RFWRITE register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetITSTS_DYN_RFWRITE(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_ITSTS_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_ITSTS_DYN_RFWRITE_MASK);
  *value = *value >> (ST25DV_ITSTS_DYN_RFWRITE_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Read ITSTS_DYN_ALL register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetITSTS_DYN_ALL(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_ITSTS_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read EH_MODE register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetEH_MODE(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_EH_MODE_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_EH_MODE_MASK);
  *value = *value >> (ST25DV_EH_MODE_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write EH_MODE register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetEH_MODE (const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_EH_MODE_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_EH_MODE_SHIFT)) & (ST25DV_EH_MODE_MASK)) |
                (reg_value & ~(ST25DV_EH_MODE_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_EH_MODE_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read EH_CTRL_DYN_EH_EN register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetEH_CTRL_DYN_EH_EN(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_EH_CTRL_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_EH_CTRL_DYN_EH_EN_MASK);
  *value = *value >> (ST25DV_EH_CTRL_DYN_EH_EN_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write EH_CTRL_DYN_EH_EN register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetEH_CTRL_DYN_EH_EN(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_EH_CTRL_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_EH_CTRL_DYN_EH_EN_SHIFT)) & (ST25DV_EH_CTRL_DYN_EH_EN_MASK)) |
                (reg_value & ~(ST25DV_EH_CTRL_DYN_EH_EN_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_EH_CTRL_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read EH_CTRL_DYN_EH_ON register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetEH_CTRL_DYN_EH_ON(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_EH_CTRL_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_EH_CTRL_DYN_EH_ON_MASK);
  *value = *value >> (ST25DV_EH_CTRL_DYN_EH_ON_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Read EH_CTRL_DYN_FIELD_ON register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetEH_CTRL_DYN_FIELD_ON(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_EH_CTRL_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_EH_CTRL_DYN_FIELD_ON_MASK);
  *value = *value >> (ST25DV_EH_CTRL_DYN_FIELD_ON_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Read EH_CTRL_DYN_VCC_ON register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetEH_CTRL_DYN_VCC_ON(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_EH_CTRL_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_EH_CTRL_DYN_VCC_ON_MASK);
  *value = *value >> (ST25DV_EH_CTRL_DYN_VCC_ON_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Read EH_CTRL_DYN_ALL register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetEH_CTRL_DYN_ALL(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_EH_CTRL_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_EH_CTRL_DYN_ALL_MASK);
  *value = *value >> (ST25DV_EH_CTRL_DYN_ALL_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Read RF_MNGT_RFDIS register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetRF_MNGT_RFDIS(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_RF_MNGT_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_RF_MNGT_RFDIS_MASK);
  *value = *value >> (ST25DV_RF_MNGT_RFDIS_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write RF_MNGT_RFDIS register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetRF_MNGT_RFDIS(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_RF_MNGT_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_RF_MNGT_RFDIS_SHIFT)) & (ST25DV_RF_MNGT_RFDIS_MASK)) |
                (reg_value & ~(ST25DV_RF_MNGT_RFDIS_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_RF_MNGT_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read RF_MNGT_RFSLEEP register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetRF_MNGT_RFSLEEP(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_RF_MNGT_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_RF_MNGT_RFSLEEP_MASK);
  *value = *value >> (ST25DV_RF_MNGT_RFSLEEP_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write RF_MNGT_RFSLEEP register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetRF_MNGT_RFSLEEP(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_RF_MNGT_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_RF_MNGT_RFSLEEP_SHIFT)) & (ST25DV_RF_MNGT_RFSLEEP_MASK)) |
                (reg_value & ~(ST25DV_RF_MNGT_RFSLEEP_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_RF_MNGT_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read RF_MNGT_ALL register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetRF_MNGT_ALL(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_RF_MNGT_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_RF_MNGT_ALL_MASK);
  *value = *value >> (ST25DV_RF_MNGT_ALL_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write RF_MNGT_ALL register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetRF_MNGT_ALL(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_RF_MNGT_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_RF_MNGT_ALL_SHIFT)) & (ST25DV_RF_MNGT_ALL_MASK)) |
                (reg_value & ~(ST25DV_RF_MNGT_ALL_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_RF_MNGT_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read RF_MNGT_DYN_RFDIS register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetRF_MNGT_DYN_RFDIS(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_RF_MNGT_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_RF_MNGT_DYN_RFDIS_MASK);
  *value = *value >> (ST25DV_RF_MNGT_DYN_RFDIS_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write RF_MNGT_DYN_RFDIS register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetRF_MNGT_DYN_RFDIS (const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if( ST25DV_ReadReg(ctx, (ST25DV_RF_MNGT_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_RF_MNGT_DYN_RFDIS_SHIFT)) & (ST25DV_RF_MNGT_DYN_RFDIS_MASK)) |
                (reg_value & ~(ST25DV_RF_MNGT_DYN_RFDIS_MASK));

  if( ST25DV_WriteReg(ctx, (ST25DV_RF_MNGT_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read RF_MNGT_DYN_RFSLEEP register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetRF_MNGT_DYN_RFSLEEP(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_RF_MNGT_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_RF_MNGT_DYN_RFSLEEP_MASK);
  *value = *value >> (ST25DV_RF_MNGT_DYN_RFSLEEP_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write RF_MNGT_DYN_RFSLEEP register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetRF_MNGT_DYN_RFSLEEP(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_RF_MNGT_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ( (*value << (ST25DV_RF_MNGT_DYN_RFSLEEP_SHIFT)) & (ST25DV_RF_MNGT_DYN_RFSLEEP_MASK)) |
                (reg_value & ~(ST25DV_RF_MNGT_DYN_RFSLEEP_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_RF_MNGT_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read RF_MNGT_DYN_ALL register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetRF_MNGT_DYN_ALL(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_RF_MNGT_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_RF_MNGT_DYN_ALL_MASK);
  *value = *value >> (ST25DV_RF_MNGT_DYN_ALL_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write RF_MNGT_DYN_ALL register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetRF_MNGT_DYN_ALL(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_RF_MNGT_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ( (*value << (ST25DV_RF_MNGT_DYN_ALL_SHIFT)) & (ST25DV_RF_MNGT_DYN_ALL_MASK)) |
                (reg_value & ~(ST25DV_RF_MNGT_DYN_ALL_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_RF_MNGT_DYN_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read RFA1SS_PWDCTRL register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetRFA1SS_PWDCTRL(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA1SS_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_RFA1SS_PWDCTRL_MASK);
  *value = *value >> (ST25DV_RFA1SS_PWDCTRL_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write RFA1SS_PWDCTRL register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetRFA1SS_PWDCTRL(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA1SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_RFA1SS_PWDCTRL_SHIFT)) & (ST25DV_RFA1SS_PWDCTRL_MASK)) |
                (reg_value & ~(ST25DV_RFA1SS_PWDCTRL_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_RFA1SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read RFA1SS_RWPROT register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetRFA1SS_RWPROT(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA1SS_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_RFA1SS_RWPROT_MASK);
  *value = *value >> (ST25DV_RFA1SS_RWPROT_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write RFA1SS_RWPROT register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetRFA1SS_RWPROT(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA1SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_RFA1SS_RWPROT_SHIFT)) & (ST25DV_RFA1SS_RWPROT_MASK)) |
                (reg_value & ~(ST25DV_RFA1SS_RWPROT_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_RFA1SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read RFA1SS_ALL register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetRFA1SS_ALL(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA1SS_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_RFA1SS_ALL_MASK);
  *value = *value >> (ST25DV_RFA1SS_ALL_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write RFA1SS_ALL register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetRFA1SS_ALL(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA1SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_RFA1SS_ALL_SHIFT)) & (ST25DV_RFA1SS_ALL_MASK)) |
                (reg_value & ~(ST25DV_RFA1SS_ALL_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_RFA1SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read RFA2SS_PWDCTRL register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetRFA2SS_PWDCTRL(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA2SS_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_RFA2SS_PWDCTRL_MASK);
  *value = *value >> (ST25DV_RFA2SS_PWDCTRL_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write RFA2SS_PWDCTRL register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetRFA2SS_PWDCTRL(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA2SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_RFA2SS_PWDCTRL_SHIFT)) & (ST25DV_RFA2SS_PWDCTRL_MASK)) |
                (reg_value & ~(ST25DV_RFA2SS_PWDCTRL_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_RFA2SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read RFA2SS_RWPROT register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetRFA2SS_RWPROT(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA2SS_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_RFA2SS_RWPROT_MASK);
  *value = *value >> (ST25DV_RFA2SS_RWPROT_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write RFA2SS_RWPROT register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetRFA2SS_RWPROT(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA2SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_RFA2SS_RWPROT_SHIFT)) & (ST25DV_RFA2SS_RWPROT_MASK)) |
                (reg_value & ~(ST25DV_RFA2SS_RWPROT_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_RFA2SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read RFA2SS_ALL register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetRFA2SS_ALL(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA2SS_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_RFA2SS_ALL_MASK);
  *value = *value >> (ST25DV_RFA2SS_ALL_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write RFA2SS_ALL register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetRFA2SS_ALL(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA2SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_RFA2SS_ALL_SHIFT)) & (ST25DV_RFA2SS_ALL_MASK)) |
                (reg_value & ~(ST25DV_RFA2SS_ALL_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_RFA2SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read RFA3SS_PWDCTRL register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetRFA3SS_PWDCTRL(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA3SS_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_RFA3SS_PWDCTRL_MASK);
  *value = *value >> (ST25DV_RFA3SS_PWDCTRL_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write RFA3SS_PWDCTRL register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetRFA3SS_PWDCTRL(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA3SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_RFA3SS_PWDCTRL_SHIFT)) & (ST25DV_RFA3SS_PWDCTRL_MASK)) |
                (reg_value & ~(ST25DV_RFA3SS_PWDCTRL_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_RFA3SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read RFA3SS_RWPROT register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetRFA3SS_RWPROT(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA3SS_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_RFA3SS_RWPROT_MASK);
  *value = *value >> (ST25DV_RFA3SS_RWPROT_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write RFA3SS_RWPROT register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetRFA3SS_RWPROT(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA3SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_RFA3SS_RWPROT_SHIFT)) & (ST25DV_RFA3SS_RWPROT_MASK)) |
                (reg_value & ~(ST25DV_RFA3SS_RWPROT_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_RFA3SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read RFA3SS_ALL register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetRFA3SS_ALL(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA3SS_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_RFA3SS_ALL_MASK);
  *value = *value >> (ST25DV_RFA3SS_ALL_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write RFA3SS_ALL register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetRFA3SS_ALL(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA3SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_RFA3SS_ALL_SHIFT)) & (ST25DV_RFA3SS_ALL_MASK)) |
                (reg_value & ~(ST25DV_RFA3SS_ALL_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_RFA3SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read RFA4SS_PWDCTRL register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetRFA4SS_PWDCTRL(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA4SS_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_RFA4SS_PWDCTRL_MASK);
  *value = *value >> (ST25DV_RFA4SS_PWDCTRL_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write RFA4SS_PWDCTRL register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetRFA4SS_PWDCTRL(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA4SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_RFA4SS_PWDCTRL_SHIFT)) & (ST25DV_RFA4SS_PWDCTRL_MASK)) |
                (reg_value & ~(ST25DV_RFA4SS_PWDCTRL_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_RFA4SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read RFA4SS_RWPROT register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetRFA4SS_RWPROT(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA4SS_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_RFA4SS_RWPROT_MASK);
  *value = *value >> (ST25DV_RFA4SS_RWPROT_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write RFA4SS_RWPROT register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetRFA4SS_RWPROT(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA4SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_RFA4SS_RWPROT_SHIFT)) & (ST25DV_RFA4SS_RWPROT_MASK)) |
                (reg_value & ~(ST25DV_RFA4SS_RWPROT_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_RFA4SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read RFA4SS_ALL register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetRFA4SS_ALL(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA4SS_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_RFA4SS_ALL_MASK);
  *value = *value >> (ST25DV_RFA4SS_ALL_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write RFA4SS_ALL register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetRFA4SS_ALL(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_RFA4SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_RFA4SS_ALL_SHIFT)) & (ST25DV_RFA4SS_ALL_MASK)) |
                (reg_value & ~(ST25DV_RFA4SS_ALL_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_RFA4SS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read I2CSS_PZ1 register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetI2CSS_PZ1(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_I2CSS_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_I2CSS_PZ1_MASK);
  *value = *value >> (ST25DV_I2CSS_PZ1_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write I2CSS_PZ1 register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetI2CSS_PZ1(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_I2CSS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_I2CSS_PZ1_SHIFT)) & (ST25DV_I2CSS_PZ1_MASK)) |
                (reg_value & ~(ST25DV_I2CSS_PZ1_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_I2CSS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read I2CSS_PZ2 register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetI2CSS_PZ2(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_I2CSS_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_I2CSS_PZ2_MASK);
  *value = *value >> (ST25DV_I2CSS_PZ2_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write I2CSS_PZ2 register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetI2CSS_PZ2(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_I2CSS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_I2CSS_PZ2_SHIFT)) & (ST25DV_I2CSS_PZ2_MASK)) |
                (reg_value & ~(ST25DV_I2CSS_PZ2_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_I2CSS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read I2CSS_PZ3 register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetI2CSS_PZ3(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_I2CSS_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_I2CSS_PZ3_MASK);
  *value = *value >> (ST25DV_I2CSS_PZ3_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write I2CSS_PZ3 register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetI2CSS_PZ3(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  if(ST25DV_ReadReg(ctx, (ST25DV_I2CSS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_I2CSS_PZ3_SHIFT)) & (ST25DV_I2CSS_PZ3_MASK)) |
                (reg_value & ~(ST25DV_I2CSS_PZ3_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_I2CSS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read I2CSS_PZ4 register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetI2CSS_PZ4(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_I2CSS_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_I2CSS_PZ4_MASK);
  *value = *value >> (ST25DV_I2CSS_PZ4_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write I2CSS_PZ4 register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetI2CSS_PZ4(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_I2CSS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_I2CSS_PZ4_SHIFT)) & (ST25DV_I2CSS_PZ4_MASK)) |
                (reg_value & ~(ST25DV_I2CSS_PZ4_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_I2CSS_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read I2CSS_ALL register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetI2CSS_ALL(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_I2CSS_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Write I2CSS_ALL register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetI2CSS_ALL(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  if(ST25DV_WriteReg(ctx, (ST25DV_I2CSS_REG), value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read LOCKCCFILE_BLCK0 register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetLOCKCCFILE_BLCK0(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_LOCKCCFILE_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_LOCKCCFILE_BLCK0_MASK);
  *value = *value >> (ST25DV_LOCKCCFILE_BLCK0_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write LOCKCCFILE_BLCK0 register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetLOCKCCFILE_BLCK0(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_LOCKCCFILE_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_LOCKCCFILE_BLCK0_SHIFT)) & (ST25DV_LOCKCCFILE_BLCK0_MASK)) |
                (reg_value & ~(ST25DV_LOCKCCFILE_BLCK0_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_LOCKCCFILE_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read LOCKCCFILE_BLCK1 register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetLOCKCCFILE_BLCK1(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_LOCKCCFILE_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_LOCKCCFILE_BLCK1_MASK);
  *value = *value >> (ST25DV_LOCKCCFILE_BLCK1_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write LOCKCCFILE_BLCK1 register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetLOCKCCFILE_BLCK1(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_LOCKCCFILE_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_LOCKCCFILE_BLCK1_SHIFT)) & (ST25DV_LOCKCCFILE_BLCK1_MASK)) |
                (reg_value & ~(ST25DV_LOCKCCFILE_BLCK1_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_LOCKCCFILE_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read LOCKCCFILE_ALL register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetLOCKCCFILE_ALL(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_LOCKCCFILE_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_LOCKCCFILE_ALL_MASK);
  *value = *value >> (ST25DV_LOCKCCFILE_ALL_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write LOCKCCFILE_ALL register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetLOCKCCFILE_ALL(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_LOCKCCFILE_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_LOCKCCFILE_ALL_SHIFT)) & (ST25DV_LOCKCCFILE_ALL_MASK)) |
                (reg_value & ~(ST25DV_LOCKCCFILE_ALL_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_LOCKCCFILE_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read LOCKCFG_B0 register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetLOCKCFG_B0(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_LOCKCFG_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_LOCKCFG_B0_MASK);
  *value = *value >> (ST25DV_LOCKCFG_B0_SHIFT);
  
  return NFCTAG_OK;
}

/**
 * @brief  Write LOCKCFG_B0 register
 * @param[in] ctx structure containing context driver
 * @param[in] value data pointer to write to register
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_SetLOCKCFG_B0(const ST25DV_Ctx_t *const ctx, const uint8_t *const value)
{
  uint8_t reg_value;
  
  if(ST25DV_ReadReg(ctx, (ST25DV_LOCKCFG_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }

  reg_value = ((*value << (ST25DV_LOCKCFG_B0_SHIFT)) & (ST25DV_LOCKCFG_B0_MASK)) |
                (reg_value & ~(ST25DV_LOCKCFG_B0_MASK));

  if(ST25DV_WriteReg(ctx, (ST25DV_LOCKCFG_REG), &reg_value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  return NFCTAG_OK;
}

/**
 * @brief  Read I2C_SSO_DYN_I2CSSO register
 * @param[in] ctx structure containing context driver
 * @param[out] value data pointer to store register content
 * @return 0 in case of success, an error code otherwise
 */
int32_t ST25DV_GetI2C_SSO_DYN_I2CSSO(const ST25DV_Ctx_t *const ctx, uint8_t *const value)
{
  if(ST25DV_ReadReg(ctx, (ST25DV_I2C_SSO_DYN_REG), (uint8_t *)value, 1) != 0)
  {
    return NFCTAG_ERROR;
  }
  
  *value &= (ST25DV_I2C_SSO_DYN_I2CSSO_MASK);
  *value = *value >> (ST25DV_I2C_SSO_DYN_I2CSSO_SHIFT);
  
  return NFCTAG_OK;
}

/**
  * @}
  */

/**
  * @}
  */ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
