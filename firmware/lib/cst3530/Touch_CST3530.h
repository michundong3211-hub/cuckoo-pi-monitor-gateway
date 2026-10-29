#pragma once

#include <Arduino.h>

constexpr uint8_t CST3530_ADDR = 0x58;
constexpr uint8_t CST3530_SDA_PIN = 1;
constexpr uint8_t CST3530_SCL_PIN = 3;
constexpr uint8_t CST3530_INT_PIN = 4;
constexpr uint8_t CST3530_RST_PIN = 2;
constexpr uint32_t CST3530_I2C_FREQ_HZ = 400000;
constexpr uint8_t CST3530_LCD_TOUCH_MAX_POINTS = 5;

bool TOUCH2_Init();
bool Touch2_Read_Data();
bool Touch2_Get_XY(uint16_t* x, uint16_t* y, uint16_t* strength,
                   uint8_t* pointNum, uint8_t maxPointNum);
