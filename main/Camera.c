/*
 * Proyecto: Vigilancia con cámaras
 * Integrantes: Martines Franco, Julian
 *              Schar, Esteban
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "camera_pin.h"
#include "app_wifi.h"
#include "esp_camera.h"
#include "stream_server.h"

#define TEST_ESP_OK(ret) assert(ret == ESP_OK)
#define TEST_ASSERT_NOT_NULL(ret) assert(ret != NULL)

#define DEBUG_PIN GPIO_NUM_14   //Pin para el analizador lógico.
#define DEBUG_PIN_MASK (1ULL << DEBUG_PIN)

static QueueHandle_t xQueueIFrame = NULL;       //Creamos cola de FreeRTOS.

static const char *TAG = "video s_server";

static esp_err_t init_camera(uint32_t xclk_freq_hz, pixformat_t pixel_format, framesize_t frame_size, uint8_t fb_count){
    //CONFIGURACIÓN DEL ESP32-CAM
    camera_config_t camera_config = {
        .pin_pwdn = CAMERA_PIN_PWDN,
        .pin_reset = CAMERA_PIN_RESET,
        .pin_xclk = CAMERA_PIN_XCLK,
        .pin_sscb_sda = CAMERA_PIN_SIOD,
        .pin_sscb_scl = CAMERA_PIN_SIOC,

        .pin_d7 = CAMERA_PIN_D7,
        .pin_d6 = CAMERA_PIN_D6,
        .pin_d5 = CAMERA_PIN_D5,
        .pin_d4 = CAMERA_PIN_D4,
        .pin_d3 = CAMERA_PIN_D3,
        .pin_d2 = CAMERA_PIN_D2,
        .pin_d1 = CAMERA_PIN_D1,
        .pin_d0 = CAMERA_PIN_D0,
        .pin_vsync = CAMERA_PIN_VSYNC,
        .pin_href = CAMERA_PIN_HREF,
        .pin_pclk = CAMERA_PIN_PCLK,

        .xclk_freq_hz = xclk_freq_hz,
        .ledc_timer = LEDC_TIMER_0, 
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = pixel_format,       //YUV422,GRAYSCALE,RGB565,JPEG
        .frame_size = frame_size,           //QQVGA-UXGA, no se recomiendan vtamaños superiores a QVGA cuando el formato no es JPEG.

        .jpeg_quality = 12,                 //0-63
        .fb_count = fb_count,               //Con más de uno, I2S trabaja en modo continuo, se recomienta con formato JPEG.
        .grab_mode = CAMERA_GRAB_LATEST,    //Para no acumular frames viejos
        .fb_location = CAMERA_FB_IN_PSRAM,
    };

    //Inicializamos la cámara:
    esp_err_t ret = esp_camera_init(&camera_config); //Configura I2C, framebuffer, buffers DMA, entrada I2S paralela.
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed");
        return ret; 
    }

    return ret;
}

// Esta será la tarea que correrá exclusivamente en el NÚCLEO 1
void tarea_captura_imagen(void *pvParameters){

    camera_fb_t *frame = NULL;
    
    ESP_LOGI(TAG, "Tarea de Captura iniciada en Core: %d", xPortGetCoreID());

    while (true) {
        gpio_set_level(DEBUG_PIN, 1); //Ponemos en alto el pin debug para indicar inicio de captura

        frame = esp_camera_fb_get(); //Obtenemos un frame
        
        gpio_set_level(DEBUG_PIN, 0); //Ponemos en bajo el pin debug para indicar fin de captura
        if (frame) {
            //Enviamos el frame a la cola para ser consumido por el servidor
            //Esperamos portMAX_DELAY si no hay lugar en la cola
            if(xQueueSend(xQueueIFrame, &frame, portMAX_DELAY) != pdPASS){ 
                // Si falla el envío, liberamos memoria
                esp_camera_fb_return(frame);
            }
        } else {
             ESP_LOGE(TAG, "Fallo captura de camara");
        }

        //gpio_set_level(DEBUG_PIN, 0); //Ponemos en bajo el pin debug para indicar fin de captura

        //vTaskDelay(1); 
    }

    vTaskDelete(NULL); //Nunca debería llegar aca (es por seguridad)
}

void app_main(){
    
    app_wifi_main();

    //CONFIGURACIÓN DEL PIN DE DEBUG (ANALIZADOR LÓGICO)
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = DEBUG_PIN_MASK,
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);
    gpio_set_level(DEBUG_PIN, 0);

    xQueueIFrame = xQueueCreate(2, sizeof(camera_fb_t *)); //Creamos una cola de dos elementos para almacenar los frames.
    TEST_ASSERT_NOT_NULL(xQueueIFrame);

    TEST_ESP_OK(init_camera(20000000, PIXFORMAT_JPEG, FRAMESIZE_VGA, 3));   //inicializa la cámara con frecuencia, formato, resolcuión y cantidad de buffers
   
    TEST_ESP_OK(start_stream_server(xQueueIFrame, true)); //Se inicia el servidor

    ESP_LOGI(TAG, "Begin capture frame");

    //Establecemos la captura de frames en el CORE 1:
    xTaskCreatePinnedToCore(
        tarea_captura_imagen,   // Función
        "CapturaCam",           // Nombre
        4096,                   // Tamaño de Stack
        NULL,                   // Parámetros
        5,                      // Prioridad (5 es estándar/alta)
        NULL,                   // Handle de la tarea (opcional)
        1                       // CORE 1
    );
}
