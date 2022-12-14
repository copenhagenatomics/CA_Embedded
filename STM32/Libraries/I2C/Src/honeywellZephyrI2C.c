/* Description:
 * Provides an interface to communicate with the honeywell zephyr air flow sensor. */

#include "honeywellZephyrI2C.h"

#define ZephyrADDR 0x49
HAL_StatusTypeDef honeywellZephyrRead(I2C_HandleTypeDef *hi2c, float *flowData)
{
    uint8_t readSensorADDR = 0x00;

    uint8_t addata[2];
    HAL_StatusTypeDef ret;
    ret = HAL_I2C_Master_Transmit(hi2c, (uint16_t) (ZephyrADDR << 1), &readSensorADDR, 2, 50);
    if (ret != HAL_OK)
        return ret;

    ret = HAL_I2C_Master_Receive(hi2c, (uint16_t) (ZephyrADDR << 1) | 0x01, addata, 2, 50);
    if (ret != HAL_OK)
        return ret;

    const uint16_t lowLimit = 1638 + 16380/200; // 0 + 0.5% of Full Scale.
    uint16_t uFlow = ((uint16_t) addata[0] << 8) | addata[1];
    if (uFlow <= lowLimit)      *flowData = 0; // Lower limit 0%
    else if (uFlow >= 14746)    *flowData = 1; // Full scale output 100%
    else                        *flowData = ((uFlow / 16384.0) - 0.1) / 0.8;

    return HAL_OK;
}

HAL_StatusTypeDef honeywellZephyrSerial(I2C_HandleTypeDef *hi2c, uint32_t *serialNB)
{
    uint8_t readSerialNBADDR = 0x01;
    *serialNB = 0;

    // The sensor prints out 2 bytes on startup with its serial number
    uint8_t addata1[4];
    HAL_StatusTypeDef ret;
    ret = HAL_I2C_Master_Transmit(hi2c, (uint16_t) (ZephyrADDR << 1), &readSerialNBADDR, 1, 50);
    if (ret != HAL_OK)
        return ret;

    HAL_Delay(2);
    ret = HAL_I2C_Master_Receive(hi2c, (uint16_t) (ZephyrADDR << 1) | 0x01, &addata1[0], 2, 50);
    if (ret != HAL_OK)
        return ret;

    HAL_Delay(10);
    ret = HAL_I2C_Master_Receive(hi2c, (uint16_t) (ZephyrADDR << 1) | 0x01, &addata1[2], 2, 50);
    if (ret != HAL_OK)
        return ret;

    *serialNB = ((addata1[0] & 0xFF) << 24)
              | ((addata1[1] & 0xFF) << 16)
              | ((addata1[2] & 0xFF) << 8)
              |   addata1[3];

    return ret;
}
