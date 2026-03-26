/*
 * Proyecto: Vigilancia con cámaras
 * Integrantes: Martines Franco, Julian
 *              Schar, Esteban
 */

#include "actuadores.h"
#include "driver/ledc.h"

/* ----Definiciones para el LED---- */
#define LED_PIN 4                                    // GPIO del LED
#define LEDC_TIMER              LEDC_TIMER_1         
#define LEDC_MODE               LEDC_LOW_SPEED_MODE 
#define LEDC_CHANNEL            LEDC_CHANNEL_1 
#define LEDC_DUTY_RES           LEDC_TIMER_8_BIT
#define LEDC_FREQUENCY          5000                //Frecuencia de 5 kHz

/* ----Definiciones para los servomotores---- */
#define SERVO_PIN_PAN       13
#define SERVO_PIN_TILT      12
#define SERVO_TIMER         LEDC_TIMER_2
#define SERVO_MODE          LEDC_LOW_SPEED_MODE
#define SERVO_CHAN_PAN      LEDC_CHANNEL_2
#define SERVO_CHAN_TILT     LEDC_CHANNEL_3
#define SERVO_FREQ          50                      //50Hz es el estándar para servos

#define MIN_PULSE 500       //Para 0°
#define MAX_PULSE 2400      //Para 180°

// Función auxiliar para convertir grados (0-180) a Duty Cycle
// Para resolución de 10 bits (0-1023):
uint32_t angle_to_duty(int angle) {
    uint32_t pulse_us = MIN_PULSE + (angle * (MAX_PULSE-MIN_PULSE) / 180);
    return (pulse_us * 1023) / 20000;
}

 void init_led_pwm(){
    gpio_reset_pin(LED_PIN);
    //Configuramos el Timer del PWM
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    //Configuramos el Canal
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LED_PIN,
        .duty           = 0, // Empieza apagado (Duty cycle 0)
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
}

void init_servo_pwm() {
    //Configuramos el timer del PWM
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = SERVO_MODE,
        .timer_num        = SERVO_TIMER,
        .duty_resolution  = LEDC_TIMER_10_BIT, // 10 bits da más suavidad que 8
        .freq_hz          = SERVO_FREQ, 
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    //Configuramos el canal PAN
    ledc_channel_config_t servo_pan = {
        .speed_mode     = SERVO_MODE,
        .channel        = SERVO_CHAN_PAN,
        .timer_sel      = SERVO_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = SERVO_PIN_PAN,
        .duty           = angle_to_duty(90), // Empezar centrado
        .hpoint         = 0
    };
    ledc_channel_config(&servo_pan);

    //Configuramos el canal TILT
    ledc_channel_config_t servo_tilt = {
        .speed_mode     = SERVO_MODE,
        .channel        = SERVO_CHAN_TILT,
        .timer_sel      = SERVO_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = SERVO_PIN_TILT,
        .duty           = angle_to_duty(90), // Empezar centrado
        .hpoint         = 0
    };
    ledc_channel_config(&servo_tilt);
}

void init_actuadores(){
    init_led_pwm();
    init_servo_pwm();
}

void set_led_brightness(int duty){
    //Protección para asegurar que esté entre 0 y 255.
    if (duty < 0) duty = 0;
    if (duty > 255) duty = 255;

    //Actualizamos el PWM
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

void set_servo_pan(int angle_deg){
    //Limitamos los valores de 0 a 180°
    if(angle_deg < 0) angle_deg = 0; 
    if(angle_deg > 180) angle_deg = 180;

    //Actualizamos el duty.
    ledc_set_duty(SERVO_MODE, SERVO_CHAN_PAN, angle_to_duty(angle_deg));
    ledc_update_duty(SERVO_MODE, SERVO_CHAN_PAN);
}

void set_servo_tilt(int angle_deg){
    //Limitamos los valores de 0 a 180°
    if(angle_deg < 0) angle_deg = 0; 
    if(angle_deg > 180) angle_deg = 180;

    //Actualizamos los valores de duty.
    ledc_set_duty(SERVO_MODE, SERVO_CHAN_TILT, angle_to_duty(angle_deg));
    ledc_update_duty(SERVO_MODE, SERVO_CHAN_TILT);
}