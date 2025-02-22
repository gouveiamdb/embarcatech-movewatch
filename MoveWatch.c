#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
#include "hardware/pwm.h"

// Definições dos pinos GPIO
#define BOTAO_A         5     // GPIO 5  - Liga/Desliga
#define BOTAO_B         6     // GPIO 6  - Reconhecimento
#define JOYSTICK_X      26    // ADC0    - Movimento X
#define JOYSTICK_Y      27    // ADC1    - Movimento Y
#define JOYSTICK_BTN    22    // GPIO 22 - Confirmação
#define LED_RED         13    // GPIO 13 - LED RGB (Vermelho)
#define LED_GREEN       11    // GPIO 11 - LED RGB (Verde)
#define LED_BLUE        12    // GPIO 12 - LED RGB (Azul)
#define BUZZER          10    // GPIO 10 - Buzzer PWM
#define I2C_SDA         14    // GPIO 14 - Display I2C (SDA)
#define I2C_SCL         15    // GPIO 15 - Display I2C (SCL)
#define I2C_PORT        i2c1  // Porta I2C utilizada
#define DISPLAY_ADDR    0x3C  // Endereço I2C do display OLED



int main()
{
    stdio_init_all();

    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}
