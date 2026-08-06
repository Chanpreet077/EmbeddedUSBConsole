#include "bmp180.h"

extern I2C_HandleTypeDef hi2c1;

/* BMP180's fixed 7-bit I2C address is 0x77. HAL's I2C functions want
 * the address pre-shifted left by 1 (the 8th bit is the read/write
 * flag, which HAL sets automatically per call). */
#define BMP180_I2C_ADDR (0x77 << 1)

#define BMP180_REG_CALIB_START 0xAA
#define BMP180_REG_CONTROL     0xF4
#define BMP180_REG_RESULT      0xF6

#define BMP180_CMD_READ_TEMP     0x2E
#define BMP180_CMD_READ_PRESSURE 0x34

/* Oversampling setting: 0 = fastest/lowest power, up to 3 = most
 * accurate but slower. 0 is fine to start with. */
#define BMP180_OSS 0

/* Factory-programmed calibration constants, read once at Init() time
 * from the sensor's own EEPROM. Every BMP180 chip has different
 * values here -- this is why we can't hardcode them. */
typedef struct
{
    int16_t  AC1, AC2, AC3;
    uint16_t AC4, AC5, AC6;
    int16_t  B1, B2;
    int16_t  MB, MC, MD;
} BMP180_Calib_t;

static BMP180_Calib_t calib;

static HAL_StatusTypeDef ReadCalibration(void)
{
    uint8_t buf[22];

    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(
        &hi2c1, BMP180_I2C_ADDR, BMP180_REG_CALIB_START,
        I2C_MEMADD_SIZE_8BIT, buf, sizeof(buf), HAL_MAX_DELAY);

    if (status != HAL_OK) return status;

    calib.AC1 = (int16_t)((buf[0]  << 8) | buf[1]);
    calib.AC2 = (int16_t)((buf[2]  << 8) | buf[3]);
    calib.AC3 = (int16_t)((buf[4]  << 8) | buf[5]);
    calib.AC4 = (uint16_t)((buf[6]  << 8) | buf[7]);
    calib.AC5 = (uint16_t)((buf[8]  << 8) | buf[9]);
    calib.AC6 = (uint16_t)((buf[10] << 8) | buf[11]);
    calib.B1  = (int16_t)((buf[12] << 8) | buf[13]);
    calib.B2  = (int16_t)((buf[14] << 8) | buf[15]);
    calib.MB  = (int16_t)((buf[16] << 8) | buf[17]);
    calib.MC  = (int16_t)((buf[18] << 8) | buf[19]);
    calib.MD  = (int16_t)((buf[20] << 8) | buf[21]);

    return HAL_OK;
}

HAL_StatusTypeDef BMP180_Init(void)
{
    return ReadCalibration();
}

static HAL_StatusTypeDef ReadRawTemp(int32_t *rawOut)
{
    uint8_t cmd = BMP180_CMD_READ_TEMP;
    uint8_t data[2];

    HAL_StatusTypeDef status = HAL_I2C_Mem_Write(
        &hi2c1, BMP180_I2C_ADDR, BMP180_REG_CONTROL,
        I2C_MEMADD_SIZE_8BIT, &cmd, 1, HAL_MAX_DELAY);
    if (status != HAL_OK) return status;

    HAL_Delay(5); /* datasheet: temperature conversion takes ~4.5ms */

    status = HAL_I2C_Mem_Read(
        &hi2c1, BMP180_I2C_ADDR, BMP180_REG_RESULT,
        I2C_MEMADD_SIZE_8BIT, data, 2, HAL_MAX_DELAY);
    if (status != HAL_OK) return status;

    *rawOut = (int32_t)((data[0] << 8) | data[1]);
    return HAL_OK;
}

static HAL_StatusTypeDef ReadRawPressure(int32_t *rawOut)
{
    uint8_t cmd = BMP180_CMD_READ_PRESSURE + (BMP180_OSS << 6);
    uint8_t data[3];

    HAL_StatusTypeDef status = HAL_I2C_Mem_Write(
        &hi2c1, BMP180_I2C_ADDR, BMP180_REG_CONTROL,
        I2C_MEMADD_SIZE_8BIT, &cmd, 1, HAL_MAX_DELAY);
    if (status != HAL_OK) return status;

    HAL_Delay(8); /* conversion time grows with oversampling; safe at OSS=0 */

    status = HAL_I2C_Mem_Read(
        &hi2c1, BMP180_I2C_ADDR, BMP180_REG_RESULT,
        I2C_MEMADD_SIZE_8BIT, data, 3, HAL_MAX_DELAY);
    if (status != HAL_OK) return status;

    *rawOut = (int32_t)(((data[0] << 16) | (data[1] << 8) | data[2]) >> (8 - BMP180_OSS));
    return HAL_OK;
}

/* This is the Bosch BMP180 datasheet's published compensation
 * formula, implemented as-is -- every BMP180 driver (Adafruit's,
 * Bosch's own reference code, etc.) does the same math, since it's
 * how the sensor's raw output is defined to be interpreted. */
HAL_StatusTypeDef BMP180_ReadData(float *temperatureC, float *pressurePa)
{
    int32_t UT, UP;

    HAL_StatusTypeDef status = ReadRawTemp(&UT);
    if (status != HAL_OK) return status;

    status = ReadRawPressure(&UP);
    if (status != HAL_OK) return status;

    int32_t X1 = ((UT - calib.AC6) * calib.AC5) >> 15;
    int32_t X2 = (calib.MC << 11) / (X1 + calib.MD);
    int32_t B5 = X1 + X2;

    int32_t T = (B5 + 8) >> 4; /* in 0.1 deg C */
    *temperatureC = T / 10.0f;

    int32_t B6 = B5 - 4000;
    X1 = (calib.B2 * (B6 * B6 >> 12)) >> 11;
    X2 = (calib.AC2 * B6) >> 11;
    int32_t X3 = X1 + X2;
    int32_t B3 = (((calib.AC1 * 4 + X3) << BMP180_OSS) + 2) / 4;

    X1 = (calib.AC3 * B6) >> 13;
    X2 = (calib.B1 * (B6 * B6 >> 12)) >> 16;
    X3 = ((X1 + X2) + 2) >> 2;
    uint32_t B4 = (uint32_t)(calib.AC4) * (uint32_t)(X3 + 32768) >> 15;
    uint32_t B7 = ((uint32_t)UP - B3) * (50000 >> BMP180_OSS);

    int32_t p;
    if (B7 < 0x80000000)
    {
        p = (B7 * 2) / B4;
    }
    else
    {
        p = (B7 / B4) * 2;
    }

    X1 = (p >> 8) * (p >> 8);
    X1 = (X1 * 3038) >> 16;
    X2 = (-7357 * p) >> 16;
    p = p + ((X1 + X2 + 3791) >> 4);

    *pressurePa = (float)p;

    return HAL_OK;
}
