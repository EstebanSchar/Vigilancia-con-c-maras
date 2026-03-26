/*
 * Proyecto: Vigilancia con cámaras
 * Integrantes: Martines Franco, Julian
 *              Schar, Esteban
 */

#ifndef _STREAM_SERVER_H_
#define _STREAM_SERVER_H_

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

esp_err_t start_stream_server(const QueueHandle_t frame_i, const bool return_fb);

#endif