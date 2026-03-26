/*
 * Proyecto: Vigilancia con cámaras
 * Integrantes: Martines Franco, Julian
 *              Schar, Esteban
 */

#include "esp_http_server.h"
#include "img_converters.h"
#include "esp_log.h"

#include "stream_server.h"
#include "actuadores.h"

#define PART_BOUNDARY "123456789000000000000987654321"  //Delimitador para el flujo de video.
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY; //Indica formato MJPEG.
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";    //Separador estándar de HTTP.
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n"; //Etiqueta de envio de datos.
static QueueHandle_t xQueueFrameI = NULL;   //Cola para los frames.
static bool gReturnFB = true;   //Hay que devolver frame buffer a la cámara
static httpd_handle_t stream_httpd = NULL;
static httpd_handle_t camera_httpd = NULL;

static const char *TAG = "stream_s";

/* ----Handler para los servos.---- */
static esp_err_t cmd_servo_handler(httpd_req_t *req) {
    char buf[20];
    char val_pan[5], val_tilt[5];
    
    // Esperamos: /servo?pan=90&tilt=45
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) { //Guardamos la cadena.
        
        // Procesar PAN
        if (httpd_query_key_value(buf, "pan", val_pan, sizeof(val_pan)) == ESP_OK) { //Guardamos el valor de pan.
            int angle = atoi(val_pan);  //Convertimos la cadena a entero.
            set_servo_pan(angle);
        }

        // Procesar TILT
        if (httpd_query_key_value(buf, "tilt", val_tilt, sizeof(val_tilt)) == ESP_OK) { //Guardamos el valor de tilt.
            int angle = atoi(val_tilt);
            set_servo_tilt(angle);
        }
    }
    httpd_resp_send(req, "OK", 2); //Respondemos a la petición.
    return ESP_OK;
}

/* ----Handler para el comando '/stream'---- */
static esp_err_t stream_handler(httpd_req_t *req){
    camera_fb_t *frame = NULL;
    struct timeval _timestamp;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char part_buf[128];

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);   //Establecemos el servidor como MJPEG
    if (res != ESP_OK) {
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*"); //Permite que el servidor central acepte stream de distintas IP.

    while (true) {
        if (xQueueReceive(xQueueFrameI, &frame, portMAX_DELAY)) {   //Se recibe un frame de la cámara.
            _timestamp.tv_sec = frame->timestamp.tv_sec;
            _timestamp.tv_usec = frame->timestamp.tv_usec;

            if (frame->format == PIXFORMAT_JPEG) {
                _jpg_buf = frame->buf;
                _jpg_buf_len = frame->len;
            } else if (!frame2jpg(frame, 60, &_jpg_buf, &_jpg_buf_len)) { //Si no se puede convertir el frame a JPEG
                ESP_LOGE(TAG, "JPEG compression failed");
                res = ESP_FAIL;
            }
        } else {
            res = ESP_FAIL;
            break;
        }

        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY)); //Envia el separador
            if (res == ESP_OK) {
                size_t hlen = snprintf((char *)part_buf, 128, _STREAM_PART, _jpg_buf_len, _timestamp.tv_sec, _timestamp.tv_usec); //Rellena la plantilla _STREAM_PART_ y la guarda en part_buf
                res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen); //Envia la plantilla al navegador
            }
            if (res == ESP_OK) {
                res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len); //Envía la imagen
            }

            if (frame->format != PIXFORMAT_JPEG) {  //Si la imagen no estaba en JPEG se libera la memoria que se tomo en la conversión
                free(_jpg_buf);
                _jpg_buf = NULL;
            }
        }

        if (gReturnFB) {        //Se devuelve el buffer a la cámara
            esp_camera_fb_return(frame);
        } else {
            free(frame->buf);
        }

        if (res != ESP_OK) {        //Si hubo algun problema corta el stream
            ESP_LOGE(TAG, "Break stream handler");
            break;
        }
    }
    return res;
}

/* ----Handler para el comando '/led'---- */
static esp_err_t cmd_led_handler(httpd_req_t *req) {
    char buf[10];
    char value[5];
    
    // Esperamos recibir algo como: /led?val=128
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) { //Guardamos la cadena.
        if (httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK) { //Guardamos el valor.
            int brillo = atoi(value); //Convertimos el valor a un entero.
            
            set_led_brightness(brillo);

            ESP_LOGI(TAG, "Brillo LED cambiado a %d/255", brillo);
        }
    }
    httpd_resp_send(req, "OK", 2); //Respondemos a la petición.
    return ESP_OK;
}

esp_err_t start_stream_server(const QueueHandle_t frame_i, const bool return_fb){
    xQueueFrameI = frame_i;
    gReturnFB = return_fb;

    init_actuadores(); //Inicializamos LED y servos

    httpd_config_t config = HTTPD_DEFAULT_CONFIG(); //Establecemos configuración HTTP por defecto.

    //SERVIDOR 1: CÁMARA (STREAM) EN PUERTO 81
    httpd_uri_t stream_uri = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };

    config.server_port = 81;       // Puerto para video
    config.ctrl_port = 32769;      // Puerto de control interno
    
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
        ESP_LOGI(TAG, "Iniciando Stream Server en puerto: '%d'", config.server_port);
    }

    //SERVIDOR 2: COMANDOS (LED y SERVOS) EN PUERTO 80
    httpd_uri_t cmd_led_uri = {
        .uri       = "/led",
        .method    = HTTP_GET,
        .handler   = cmd_led_handler,
        .user_ctx  = NULL
    };

    config.server_port = 80;       // Puerto estándar para comandos
    config.ctrl_port = 32768;      // Puerto de control por defecto
    
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &cmd_led_uri);
        ESP_LOGI(TAG, "Starting stream server on port: '%d'", config.server_port);
    }

    httpd_uri_t cmd_servo_uri = {
        .uri       = "/servo",
        .method    = HTTP_GET,
        .handler   = cmd_servo_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(camera_httpd, &cmd_servo_uri);

    return ESP_OK;
}
