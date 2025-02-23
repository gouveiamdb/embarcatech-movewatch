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

// Variáveis globais para controle de estado
typedef struct {
    bool sistemaAtivo;
    bool modoBusca;
    bool movimentoDetectado;
    bool confirmacaoMovimento;
    absolute_time_t tempoInicioAlarme;  // Usando o tipo correto do Pico SDK
} EstadoSistema;

// Variáveis globais
volatile EstadoSistema estado = {0};
volatile uint32_t last_button_time = 0;
ssd1306_t display;
uint pwm_slice_num;
static PIO ws2812_pio = pio0;
static uint ws2812_sm = 0;

// Padrões de alerta para matriz de LEDs
const uint8_t padrao_x[MATRIX_SIZE][MATRIX_SIZE] = {
    {1,0,0,0,1},
    {0,1,0,1,0},
    {0,0,1,0,0},
    {0,1,0,1,0},
    {1,0,0,0,1}
};

const uint8_t padrao_alerta[MATRIX_SIZE][MATRIX_SIZE] = {
    {0,1,1,1,0},
    {1,0,1,0,1},
    {1,1,1,1,1},
    {1,0,1,0,1},
    {0,1,1,1,0}
};

// Protótipos das funções
void init_gpio(void);
void init_adc(void);
void init_i2c(void);
void init_pwm(void);
void init_display(void);
void init_uart(void);
void ws2812_init(void);
void atualizarDisplay(const char *mensagem1, const char *mensagem2);
void controlarLEDs(uint8_t r, uint8_t g, uint8_t b);
void gpio_callback(uint gpio, uint32_t events);
void tocarBuzzer(uint16_t frequencia, uint16_t duracao);
void enviarLog(const char* mensagem);
void processarModoBusca(void);
void monitorarJoystick(void);
void display_pattern(const uint8_t pattern[MATRIX_SIZE][MATRIX_SIZE], uint8_t r, uint8_t g, uint8_t b);
void clear_matrix(void);
uint16_t calculate_pwm(uint16_t value);

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

void init_adc() {
    adc_init();
    adc_gpio_init(JOYSTICK_X);
    adc_gpio_init(JOYSTICK_Y);
}

void init_uart() {
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX, GPIO_FUNC_UART);
    gpio_set_function(UART_RX, GPIO_FUNC_UART);
}

void init_i2c() {
    i2c_init(I2C_PORT, 400 * 1000);  // 400kHz
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
}

void init_pwm() {
    gpio_set_function(BUZZER, GPIO_FUNC_PWM);
    pwm_slice_num = pwm_gpio_to_slice_num(BUZZER);
    pwm_set_wrap(pwm_slice_num, 4095);
    pwm_set_enabled(pwm_slice_num, true);
}

void init_display() {
    init_i2c();
    ssd1306_init(&display, 128, 64, false, DISPLAY_ADDR, I2C_PORT);
    ssd1306_config(&display);
    ssd1306_fill(&display, false);
    ssd1306_send_data(&display);

    printf("Display SSD1306 inicializado com sucesso!\n");
}

void ws2812_init() {
    uint offset = pio_add_program(ws2812_pio, &ws2812_program);
    ws2812_program_init(ws2812_pio, ws2812_sm, offset, WS2812_PIN, 800000, false);
}


// Funções de controle da matriz WS2812
void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(ws2812_pio, ws2812_sm, pixel_grb << 8u);
}

uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b) {
    return (g << 16) | (r << 8) | b;
}

void clear_matrix() {
    for(int i = 0; i < NUM_PIXELS; i++) {
        put_pixel(0);
    }
}

void display_pattern(const uint8_t pattern[MATRIX_SIZE][MATRIX_SIZE], uint8_t r, uint8_t g, uint8_t b) {
    uint32_t on_color = rgb_to_grb(r, g, b);
    uint32_t off_color = rgb_to_grb(0, 0, 0);
    
    for (int y = 0; y < MATRIX_SIZE; y++) {
        for (int x = 0; x < MATRIX_SIZE; x++) {
            int x_pos = (y % 2 == 0) ? x : (MATRIX_SIZE - 1 - x);
            int led_index = y * MATRIX_SIZE + x_pos;
            put_pixel(pattern[y][x] ? on_color : off_color);
        }
    }
}

void tocarBuzzer(uint16_t frequencia, uint16_t duracao) {
    uint32_t wrap = clock_get_hz(clk_sys) / frequencia;
    pwm_set_wrap(pwm_slice_num, wrap);
    pwm_set_chan_level(pwm_slice_num, PWM_CHAN_A, wrap / 2);
    sleep_ms(duracao);
    pwm_set_chan_level(pwm_slice_num, PWM_CHAN_A, 0);
}

// Função para verificar o timeout do alarme
bool verificarTimeoutAlarme() {
    if (!estado.movimentoDetectado || estado.confirmacaoMovimento) {
        return false;
    }
    
    // Verifica se passaram 10 segundos
    absolute_time_t tempoAtual = get_absolute_time();
    int64_t diferenca = absolute_time_diff_us(estado.tempoInicioAlarme, tempoAtual) / 1000;  // Converte para milissegundos
    
    return (diferenca >= 10000);  // 10 segundos em milissegundos
}

// Funções de interface
void atualizarDisplay(const char *mensagem1, const char *mensagem2) {
    ssd1306_fill(&display, false);
    ssd1306_draw_string(&display, mensagem1, 10, 25);
    ssd1306_draw_string(&display, mensagem2, 10, 25);
    ssd1306_send_data(&display);
}

void controlarLEDs(uint8_t r, uint8_t g, uint8_t b) {
    gpio_put(LED_RED, r > 0);
    gpio_put(LED_GREEN, g > 0);
    gpio_put(LED_BLUE, b > 0);
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
        enviarLog("Reconhecimento iniciado");
    } else if (gpio == JOYSTICK_BTN && estado.movimentoDetectado) {
        if (!estado.confirmacaoMovimento) {
            estado.confirmacaoMovimento = true;
            controlarLEDs(255, 255, 0);
            atualizarDisplay("Confirmar", "movimentacao?");
            enviarLog("Aguardando confirmação");
        } else {
            estado.movimentoDetectado = false;
            estado.confirmacaoMovimento = false;
            controlarLEDs(0, 255, 0);
            atualizarDisplay("Monitoramento", "em Operacao");
            clear_matrix();
            enviarLog("Movimentação confirmada");
        }
    }
}

// Funções de processamento
uint16_t calculate_pwm(uint16_t value) {
    const uint16_t center = 2048;
    const uint16_t deadzone = 210;
    
    int32_t diff = abs((int32_t)value - center);
    
    if (diff < deadzone) {
        return 0;
    }
    
    diff -= deadzone;
    uint32_t pwm = (diff * 4095) / (2048 - deadzone);
    
    if (pwm > 4095) {
        pwm = 4095;
    }
    
    return (uint16_t)pwm;
}

// Função para exibir mensagens sequenciais com pausas
void exibirMensagensSequenciais(const char* mensagens[], int numMensagens, int tempoExibicao) {
    for (int i = 0; i < numMensagens; i += 2) {
        // Exibe par de mensagens (duas linhas)
        atualizarDisplay(mensagens[i], 
                        (i + 1 < numMensagens) ? mensagens[i + 1] : "");
        sleep_ms(tempoExibicao);
    }
}

void monitorarJoystick() {
    if (!estado.sistemaAtivo || estado.modoBusca) return;
    
    adc_select_input(0);
    uint16_t x = adc_read();
    adc_select_input(1);
    uint16_t y = adc_read();
    
    uint16_t pwm_x = calculate_pwm(x);
    uint16_t pwm_y = calculate_pwm(y);
    
    if (pwm_x > 0 || pwm_y > 0) {
        if (!estado.movimentoDetectado) {
            estado.movimentoDetectado = true;
            estado.tempoInicioAlarme = get_absolute_time();
            
            // Mensagens de alerta
            const char* mensagensAlerta[] = {
                "ALERTA!", 
                "Movimento",
                "Objeto em",
                "Movimento",
                "Aguardando",
                "Confirmacao"
            };
            
            exibirMensagensSequenciais(mensagensAlerta, 6, 800);  // 800ms por mensagem
            
            controlarLEDs(255, 0, 0);
            tocarBuzzer(2000, 500);
            display_pattern(padrao_x, 255, 0, 0);
            enviarLog("Movimentação detectada!");
            
            atualizarDisplay("ALERTA!", "Objeto em movimento");
        }
    }
    
    // Verifica o timeout do alarme
    if (verificarTimeoutAlarme()) {
        estado.movimentoDetectado = false;
        controlarLEDs(0, 255, 0);  // Verde
        clear_matrix();
        atualizarDisplay("Monitoramento", "em Operacao");
        enviarLog("Alarme desativado por timeout");
    }
}

// No processarModoBusca:
void processarModoBusca() {
    if (!estado.modoBusca) return;
    
    // Mensagens para o modo de busca
    const char* mensagensBusca[] = {
        "Iniciando", 
        "Reconhecimento",
        "Buscando", 
        "Padroes...",
        "Analisando", 
        "Ambiente"
    };
    
    // Exibe mensagens iniciais
    exibirMensagensSequenciais(mensagensBusca, 6, 1000);  // 1 segundo por mensagem
    
    for (int i = 0; i < 5; i++) {
        atualizarDisplay("Reconhecendo", "ambiente...");
        
        for (int j = 0; j < 10; j++) {
            controlarLEDs(0, 0, 255);
            display_pattern(padrao_alerta, 0, 0, 255);
            sleep_ms(100);
            controlarLEDs(0, 0, 0);
            clear_matrix();
            sleep_ms(100);
        }
        
        tocarBuzzer(1000, 200);
        sleep_ms(500);
    }
    
    // Mensagens de conclusão
    const char* mensagensConclusao[] = {
        "Busca", 
        "Concluida",
        "Iniciando",
        "Monitoramento",
        "Sistema",
        "Operacional"
    };
    
    exibirMensagensSequenciais(mensagensConclusao, 6, 1000);
    
    estado.modoBusca = false;
    controlarLEDs(0, 255, 0);
    atualizarDisplay("Monitoramento", "em Operacao");
    clear_matrix();
    enviarLog("Reconhecimento concluído");
}

int main()
{
    stdio_init_all();
    init_gpio();
    init_adc();
    init_i2c();
    init_uart();
    init_pwm();
    init_display();
    ws2812_init();

    const char* mensagensInicio[] = {
        "BitDogLab",
        "MoveWatch",
        "Iniciando",
        "Sistema",
        "Carregando",
        "Configuracoes",
        "Sistema",
        "Pronto!"
    };
    
    exibirMensagensSequenciais(mensagensInicio, 8, 1000);

    while (true) {
        if (estado.sistemaAtivo) {
            processarModoBusca();
            monitorarJoystick();
        }
        sleep_ms(20);  // Delay para não sobrecarregar o processador
    }

    return 0;
}
