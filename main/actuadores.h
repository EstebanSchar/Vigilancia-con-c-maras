/*
 * Proyecto: Vigilancia con cámaras
 * Integrantes: Martines Franco, Julian
 *              Schar, Esteban
 */

#ifndef _ACTUADORES_H_
#define _ACTUADORES_H_

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/Task.h"

void init_actuadores();

void set_led_brightness(int duty);
void set_servo_pan(int angle_deg);
void set_servo_tilt(int angle_deg);

#endif