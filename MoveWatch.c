/*
 * MoveWatch - Sistema de monitoramento de movimentação
 * Implementação para BitDogLab
 * 
 * Sistema que monitora a movimentação de objetos usando joystick
 * e fornece alertas visuais e sonoros quando detecta movimento.
 * 
 * Autor: Matheus Gouveia
 */

// Inclusão de bibliotecas
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

//===========================================================================
// Definições e Constantes
//===========================================================================

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

// Configurações de hardware
#define I2C_PORT        i2c1  // Porta I2C utilizada
#define UART_ID         uart0 // UART ID
#define DISPLAY_ADDR    0x3C  // Endereço I2C do display OLED
#define BAUD_RATE       115200// Taxa de transmissão UART
#define MATRIX_SIZE     5     // Tamanho da matriz de LEDs
#define NUM_PIXELS      25    // Total de LEDs na matriz (5x5)
#define DEBOUNCE_DELAY  200   // Tempo de debounce em ms
#define CENTRO_JOYSTICK 2048  // Valor central do ADC (12 bits)
#define MARGEM_MOVIMENTO 2000 // percentual da escala ADC de 12 bits

//===========================================================================
// Estruturas e Variáveis Globais
//===========================================================================

// Estrutura de estado do sistema
typedef struct {
    bool sistemaAtivo;
    bool modoBusca;
    bool movimentoDetectado;
    bool confirmacaoMovimento;
    absolute_time_t tempoInicioAlarme;
    uint8_t tempoAlarme;
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

//===========================================================================
// Protótipos das funções
//===========================================================================

// Funções de inicialização
void init_gpio(void);
void init_adc(void);
void init_i2c(void);
void init_pwm(void);
void init_display(void);
void init_uart(void);
void ws2812_init(void);

// Funções de controle de hardware
void controlarLEDs(uint8_t r, uint8_t g, uint8_t b);
void tocarBuzzer(uint16_t frequencia, uint16_t duracao);
void pararBuzzer(void);
void atualizarDisplay(const char *mensagem1, const char *mensagem2);
void draw_double_rect(ssd1306_t *ssd, uint8_t x, uint8_t y, uint8_t width, uint8_t height);

// Funções para matriz de LEDs
void put_pixel(uint32_t pixel_grb);
uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b);
void clear_matrix(void);
void display_pattern(const uint8_t pattern[MATRIX_SIZE][MATRIX_SIZE], uint8_t r, uint8_t g, uint8_t b);

// Funções de processamento
uint16_t calculate_pwm(uint16_t value);
uint16_t lerJoystickSuavizado(uint adc);
void exibirMensagensSequenciais(const char* mensagens[], int numMensagens, int tempoExibicao);
bool verificarTimeoutAlarme(void);
void enviarLog(const char* mensagem);

// Funções de callback e tratamento de eventos
void gpio_callback(uint gpio, uint32_t events);
void joystick_callback(void);

// Funções de operação principal
void configurarTempoAlarme(void);
void processarModoBusca(void);
void monitorarJoystick(void);

//===========================================================================
// Funções de Inicialização
//===========================================================================

// Inicializa os pinos GPIO
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

// Inicializa o conversor ADC
void init_adc() {
    adc_init();
    adc_gpio_init(JOYSTICK_X);
    adc_gpio_init(JOYSTICK_Y);
}

// Inicializa a comunicação UART
void init_uart() {
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX, GPIO_FUNC_UART);
    gpio_set_function(UART_RX, GPIO_FUNC_UART);
}

// Inicializa a comunicação I2C
void init_i2c() {
    i2c_init(I2C_PORT, 400 * 1000);  // 400kHz
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
}

// Inicializa o PWM para o buzzer
void init_pwm() {
    gpio_set_function(BUZZER, GPIO_FUNC_PWM);
    pwm_slice_num = pwm_gpio_to_slice_num(BUZZER);
    pwm_set_wrap(pwm_slice_num, 4095);
    pwm_set_enabled(pwm_slice_num, true);
}

// Inicializa o display OLED
void init_display() {
    init_i2c();
    ssd1306_init(&display, 128, 64, false, DISPLAY_ADDR, I2C_PORT);
    ssd1306_config(&display);
    ssd1306_fill(&display, false);
    ssd1306_send_data(&display);

    printf("Display SSD1306 inicializado com sucesso!\n");
}

// Inicializa a matriz de LEDs WS2812
void ws2812_init() {
    uint offset = pio_add_program(ws2812_pio, &ws2812_program);
    ws2812_program_init(ws2812_pio, ws2812_sm, offset, WS2812_PIN, 800000, false);
}

//===========================================================================
// Funções de Controle de Hardware
//===========================================================================

// Controla os LEDs RGB
void controlarLEDs(uint8_t r, uint8_t g, uint8_t b) {
    gpio_put(LED_RED, r > 0);
    gpio_put(LED_GREEN, g > 0);
    gpio_put(LED_BLUE, b > 0);
}

// Toca o buzzer com frequência e duração específicas
void tocarBuzzer(uint16_t frequencia, uint16_t duracao) {
    uint32_t wrap = clock_get_hz(clk_sys) / frequencia;
    pwm_set_wrap(pwm_slice_num, wrap);
    pwm_set_chan_level(pwm_slice_num, PWM_CHAN_A, wrap / 4);
    sleep_ms(duracao);
    pwm_set_chan_level(pwm_slice_num, PWM_CHAN_A, 2);
}

// Para o buzzer
void pararBuzzer(void) {
    pwm_set_chan_level(pwm_slice_num, PWM_CHAN_A, 0);
}

// Desenha um retângulo duplo no display
void draw_double_rect(ssd1306_t *ssd, uint8_t x, uint8_t y, uint8_t width, uint8_t height) {
    ssd1306_rect(ssd, x, y, width, height, true, false);
    if (width > 4 && height > 4) {
        // Opcional: desenhar um retângulo interno
    }
}

// Atualiza o display com duas linhas de texto
void atualizarDisplay(const char *mensagem1, const char *mensagem2) {
    ssd1306_fill(&display, false); //Limpa Tela
    draw_double_rect(&display, 0, 0, 128, 64);

    ssd1306_draw_string(&display, mensagem1, 10, 20);
    ssd1306_draw_string(&display, mensagem2, 10, 35);
    ssd1306_send_data(&display);
}

//===========================================================================
// Funções para Matriz de LEDs
//===========================================================================

// Envia um pixel para a matriz
void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(ws2812_pio, ws2812_sm, pixel_grb << 8u);
}

// Converte RGB para o formato GRB usado pelo WS2812
uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b) {
    return (g << 16) | (r << 8) | b;
}

// Limpa a matriz de LEDs
void clear_matrix() {
    for(int i = 0; i < NUM_PIXELS; i++) {
        put_pixel(0);
    }
}

// Exibe um padrão na matriz de LEDs
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

//===========================================================================
// Funções de Processamento
//===========================================================================

// Calcula o valor PWM baseado no valor do ADC
uint16_t calculate_pwm(uint16_t value) {
    const uint16_t center = 2048;
    const uint16_t deadzone = 1000;
    
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

// Exibe uma sequência de mensagens no display
void exibirMensagensSequenciais(const char* mensagens[], int numMensagens, int tempoExibicao) {
    for (int i = 0; i < numMensagens; i += 2) {
        // Exibe par de mensagens (duas linhas)
        atualizarDisplay(mensagens[i], 
                        (i + 1 < numMensagens) ? mensagens[i + 1] : "");
        sleep_ms(tempoExibicao);
    }
}

// Verifica se o alarme deve ser desligado pelo timeout
bool verificarTimeoutAlarme() {
    if (!estado.movimentoDetectado || estado.confirmacaoMovimento) {
        return false;
    }
    
    // Verifica se passou o tempo configurado
    absolute_time_t tempoAtual = get_absolute_time();
    int64_t diferenca = absolute_time_diff_us(estado.tempoInicioAlarme, tempoAtual) / 1000000;  // Converte para segundos
    
    return (diferenca >= estado.tempoAlarme);  // Usa o tempo configurado pelo usuário
}

// Envia logs para a UART
void enviarLog(const char* mensagem) {
    printf("[%lu] %s\n", time_us_32() / 1000000, mensagem);
}

//===========================================================================
// Funções de Callback e Tratamento de Eventos
//===========================================================================

// Callback para interrupções GPIO
void gpio_callback(uint gpio, uint32_t events) {
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    
    if (current_time - last_button_time < DEBOUNCE_DELAY) {
        return;
    }
    
    last_button_time = current_time;

    if (gpio == BOTAO_A) {
        estado.sistemaAtivo = !estado.sistemaAtivo;
        if (estado.sistemaAtivo) {
            controlarLEDs(0, 64, 0);
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
            controlarLEDs(64, 64, 0);
            atualizarDisplay("Confirmar", "movimentacão?");
            enviarLog("Aguardando confirmação...");
        } else {
            estado.movimentoDetectado = false;
            estado.confirmacaoMovimento = false;
            controlarLEDs(0, 64, 0);
            atualizarDisplay("Monitoramento", "em Operacao");
            clear_matrix();
            enviarLog("Movimentação confirmada!");
        }
    }
}


// Callback específico para o botão do joystick
void joystick_callback() {
    static int confirmacoes = 0;

    if (estado.movimentoDetectado) {
        confirmacoes++;

        if (confirmacoes == 1) {
            atualizarDisplay("Confirmar?", "Pressione Novamente");
            controlarLEDs(64, 64, 0); // LED amarelo
            pararBuzzer();
        } 
        else if (confirmacoes == 2) {
            estado.movimentoDetectado = false;
            controlarLEDs(0, 64, 0);  // Volta para verde
            clear_matrix();
            atualizarDisplay("Monitoramento", "em Operacao");
            enviarLog("Movimentação confirmada pelo usuário");
            confirmacoes = 0;  // Reseta contador
        }
    }
}

//===========================================================================
// Funções de Operação Principal
//===========================================================================

// Processa o modo de busca/reconhecimento do ambiente
void processarModoBusca() {
    if (!estado.modoBusca) return;
    
    enviarLog("Iniciando reconhecimento de ambiente");
    
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
            controlarLEDs(0, 0, 64);
            display_pattern(padrao_alerta, 0, 0, 64);
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
        "Pronto"
    };
    
    exibirMensagensSequenciais(mensagensConclusao, 6, 1000);
    
    estado.modoBusca = false;
    controlarLEDs(0, 64, 0);  // Verde com intensidade menor
    atualizarDisplay("Monitoramento", "em Operacao");
    clear_matrix();
    enviarLog("Reconhecimento concluido");
}

// Monitora o joystick para detectar movimento
void monitorarJoystick() {
    if (!estado.sistemaAtivo || estado.modoBusca) return;
    
    // Ler valores do Joystick
    adc_select_input(0);
    uint16_t x = adc_read();
    adc_select_input(1);
    uint16_t y = adc_read();

    // Verifica se o movimento ultrapassa a zona morta
    bool movimentoX = (x < (CENTRO_JOYSTICK - MARGEM_MOVIMENTO)) || (x > (CENTRO_JOYSTICK + MARGEM_MOVIMENTO));
    bool movimentoY = (y < (CENTRO_JOYSTICK - MARGEM_MOVIMENTO)) || (y > (CENTRO_JOYSTICK + MARGEM_MOVIMENTO));

    if (movimentoX || movimentoY) {
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
            
            controlarLEDs(64, 0, 0);  // LED vermelho
            tocarBuzzer(2000, 500);
            display_pattern(padrao_x, 64, 64, 0);
            enviarLog("Movimentação detectada!");

            atualizarDisplay("ALERTA!", "Objeto em movimento");
        }
    }

    // Apenas desativa o buzzer após o timeout, mas mantém LEDs acesos
    if (verificarTimeoutAlarme()) {
        pararBuzzer();  // Apenas o buzzer é desativado
        enviarLog("Buzzer desativado por timeout");
    }
}

//===========================================================================
// Função Principal
//===========================================================================

int main() {
    stdio_init_all();
    
    // Inicialização de hardware
    init_gpio();  
    init_adc();
    init_i2c();
    init_uart();
    init_pwm();
    init_display();
    ws2812_init();
       
    // Mensagem de inicialização do sistema
    enviarLog("Sistema MoveWatch iniciando...");
    
    // Exibir mensagens de inicialização
    const char* mensagensInicio[] = {
        "BitDogLab",
        "MoveWatch",
        "Iniciando",
        "Sistema",
        "Carregando",
        "Configuracoes",
        "Sistema",
        "Pronto",
        "Botao A = Liga/Desl."
    };
    exibirMensagensSequenciais(mensagensInicio, 8, 1000);

    // Loop principal
    while (true) {
        if (estado.sistemaAtivo) {
            processarModoBusca();
            monitorarJoystick();
        }
        sleep_ms(20);  // Delay para não sobrecarregar o processador
    }

    return 0;
}