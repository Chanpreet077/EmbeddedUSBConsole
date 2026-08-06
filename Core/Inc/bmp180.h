#ifndef INC_BMP180_H_
#define INC_BMP180_H_

#include "main.h"

/* Returns HAL_OK if the sensor responded and calibration data was read
 * successfully. Call this once, after MX_I2C1_Init(). */
HAL_StatusTypeDef BMP180_Init(void);

/* Reads a fresh sample from the sensor and returns compensated values.
 * Blocking -- takes a few ms due to the sensor's own conversion delay.
 * Returns HAL_OK on success. */
HAL_StatusTypeDef BMP180_ReadData(float *temperatureC, float *pressurePa);

#endif
