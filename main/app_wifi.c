/*
 * Proyecto: Vigilancia con cámaras
 * Integrantes: Martines Franco, Julian
 *              Schar, Esteban
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#if (ESP_IDF_VERSION_MAJOR >= 5)
#include "esp_mac.h"
#include "lwip/ip_addr.h"
#endif

#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

#define STATIC_NETMASK  "255.255.255.0" //Mascara de subred
#define STATIC_IP_ADDR  "192.168.137.201" //IP de cada cámara
#define STATIC_GW_ADDR  "192.168.137.1"   //Gateway de la red

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "camera wifi";

static int s_retry_num = 0;     //Cuenta de reintento de conexión.

// Grupo de eventos de FreeRTOS para indicar cuando estamos conectados.
static EventGroupHandle_t s_wifi_event_group = NULL;

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){

    /* Sta mode */
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) { //Si el evento es inicio de la estación intentamos conectar al Wi-Fi.
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {   //Si se pierde la conexión intenta reconectar
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {        //Si se supera el número de reintentos de se activa WIFI_FAIL_BIT
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) { //Si se obtuvo una IP se imprime, se resetea el contador y se activa WIFI_CONNECTED_BIT
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }

    return;
}

static void wifi_init_sta(){

    //Creamos la interfaz de red por defecto.
    esp_netif_t *my_netif = esp_netif_create_default_wifi_sta();

    //Detenemos el servicio DHCP (necesario para poner IP estática).
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(my_netif));

    //Configuramos la IP estática.
    esp_netif_ip_info_t ip_info;
    esp_netif_str_to_ip4(STATIC_IP_ADDR, &ip_info.ip);          //Dirección IP.
    esp_netif_str_to_ip4(STATIC_GW_ADDR, &ip_info.gw);          //Dirección de Gateway
    esp_netif_str_to_ip4(STATIC_NETMASK, &ip_info.netmask);     //Mascara de red.
    
    ESP_ERROR_CHECK(esp_netif_set_ip_info(my_netif, &ip_info));

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t)); //Se inicializa en 0 un espacio de memoria asignado a wifi_config.
    
    snprintf((char *)wifi_config.sta.ssid, 32, "%s", EXAMPLE_ESP_WIFI_SSID);    //Se escribe el SSID.
    snprintf((char *)wifi_config.sta.password, 64, "%s", EXAMPLE_ESP_WIFI_PASS);    //Se escribe la contraseña.

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));    //Se configura el wifi con los datos.

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}

void app_wifi_main(){

    esp_err_t ret = nvs_flash_init();   //Inicializa el almacenamiento interno.
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) { //Si la NVS no contiene paginas vacias o contiene datos en un nuevo formato no reconocidos
        ESP_ERROR_CHECK(nvs_flash_erase()); //Se borran los datos
        ret = nvs_flash_init();     //se vuelve a inicializar
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());  //Inicializa el stack de red TCP/IP.
    ESP_ERROR_CHECK(esp_event_loop_create_default()); //Creamos el bucle de eventos por defecto.
    
    s_wifi_event_group = xEventGroupCreate();   //Creamos el evento de grupo para implementación de Wi-Fi en FreeRTOS.

    //esp_netif_create_default_wifi_sta();    //crea la STA Wi-Fi predeterminada.

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg)); //Se inicializa el wifi, con configuración por default.
   
    /* ----Callbacks manejadores de eventos---- */
    //Eventos de wifi:
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,                 //ID del evento.
                                                        ESP_EVENT_ANY_ID,           //ID específico del evento.
                                                        &wifi_event_handler,        //Handler del evento.
                                                        NULL,                       //Datos adicionales.
                                                        NULL));                     //Objeto de instancia.
    //Eventos de IP:
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_mode_t mode = WIFI_MODE_STA;
    ESP_ERROR_CHECK(esp_wifi_set_mode(mode)); //Se setea el modo del wifi en STA.
    wifi_init_sta();        //Carga las credenciales del wifi
    
    ESP_ERROR_CHECK(esp_wifi_start());  //Se inicializa el wifi
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE)); //Ahorro de enegía desactivado
    ESP_LOGI(TAG, "wifi init finished.");

    //Se congela el programa hasta que se conecte el wifi o falle
    xEventGroupWaitBits(s_wifi_event_group,
                        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                        pdFALSE,
                        pdFALSE,
                        portMAX_DELAY);

    //Una vez conectado o dada la codición de fallo se destruye el evento de grupo
    vEventGroupDelete(s_wifi_event_group);
    s_wifi_event_group = NULL;
}
