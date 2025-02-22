#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
#include "hardware/pwm.h"
#include "ws2812.pio.h"

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
#define WS2812_PIN      7     // GPIO 7  - Matriz WS2812
#define I2C_SDA         14    // GPIO 14 - Display I2C (SDA)
#define I2C_SCL         15    // GPIO 15 - Display I2C (SCL)
#define UART_TX         16    // GPIO 16 - UART TX
#define UART_RX         17    // GPIO 17 - UART RX
#define I2C_PORT        i2c1  // Porta I2C utilizada
#define UART_ID         uart0 // UART ID
#define DISPLAY_ADDR    0x3C  // Endereço I2C do display OLED
#define BAUD_RATE       115200// Taxa de transmissão UART
#define MATRIX_SIZE     5     // Tamanho da matriz de LEDs
#define NUM_PIXELS      25    // Total de LEDs na matriz (5x5)
#define DEBOUNCE_DELAY  200   // Tempo de debounce em ms

/**
 * Inicializa os pinos GPIO
 * Configura direção, pull-ups e interrupções para os pinos
 */
void init_gpio() {
    // 1. Inicialização dos pinos de entrada (Botões)
    gpio_init(BOTAO_A);        // GPIO 5  - Botão Liga/Desliga
    gpio_init(BOTAO_B);        // GPIO 6  - Botão Reconhecimento
    gpio_init(JOYSTICK_BTN);   // GPIO 22 - Botão do Joystick
    
    // Configura direção como entrada
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_set_dir(JOYSTICK_BTN, GPIO_IN);
    
    // Habilita resistores pull-up internos
    // Isso mantém os pinos em nível alto quando não pressionados
    gpio_pull_up(BOTAO_A);
    gpio_pull_up(BOTAO_B);
    gpio_pull_up(JOYSTICK_BTN);
    
    // 2. Inicialização dos pinos de saída (LEDs)
    gpio_init(LED_RED);        // GPIO 13 - LED Vermelho
    gpio_init(LED_GREEN);      // GPIO 11 - LED Verde
    gpio_init(LED_BLUE);       // GPIO 12 - LED Azul
    gpio_init(BUZZER);         // GPIO 10 - Buzzer
    
    // Configura direção como saída
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_set_dir(LED_BLUE, GPIO_OUT);
    gpio_set_dir(BUZZER, GPIO_OUT);
    
    // 3. Configuração das interrupções para os botões
    // Configura callback para borda de descida (quando o botão é pressionado)
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, true);  // Usa o mesmo callback
    gpio_set_irq_enabled(JOYSTICK_BTN, GPIO_IRQ_EDGE_FALL, true);  // Usa o mesmo callback
    
    // 4. Estado inicial dos LEDs (todos apagados)
    gpio_put(LED_RED, 0);
    gpio_put(LED_GREEN, 0);
    gpio_put(LED_BLUE, 0);
}

/**
 * Callback de interrupção para os botões
 * Atualiza o estado dos LEDs RGB
 */
void gpio_callback(uint gpio, uint32_t events) {
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    
    if (current_time - last_button_time < DEBOUNCE_DELAY) {
        return;
    }
    
    last_button_time = current_time;

    if (gpio == BOTAO_A) {
        estado.sistemaAtivo = !estado.sistemaAtivo;
        if (estado.sistemaAtivo) {
            controlarLEDs(0, 255, 0);
            atualizarDisplay("Sistema em", "Funcionamento");
            clear_matrix();
            printf("Sistema ativado\n");
        } else {
            controlarLEDs(0, 0, 0);
            atualizarDisplay("Sistema", "Desligado");
            clear_matrix();
            printf("Sistema desativado\n");
        }
    } else if (gpio == BOTAO_B && estado.sistemaAtivo) {
        estado.modoBusca = true;
        printf("Iniciando reconhecimento\n");
    }
}

int main()
{
    stdio_init_all();
    init_gpio();

    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}
