# Projeto: MoveWatch 🚨🎮

## **Descrição do Projeto**
O **MoveWatch** é um sistema de monitoramento de movimentação desenvolvido para a **BitDogLab**. Ele detecta alterações no posicionamento de um objeto dentro de uma área específica, utilizando **joystick** para simulação de sensores.

O projeto conta com:
- Exibição de status no **display OLED SSD1306**. 📟
- Alertas **visuais e sonoros** com LEDs RGB e buzzer. 🔊💡
- Reconhecimento de ambiente antes de iniciar a monitoração. 📡
- Matriz de LEDs WS2812 para animações visuais. ✨
- Registro de eventos via **UART**. 📝

---

## **Requisitos Atendidos**
### **1. Monitoramento de Movimento**
- O **joystick** simula um objeto, e qualquer movimentação além da margem definida dispara um alerta. ⚠️
- O sistema reconhece deslocamentos **nos eixos X e Y**.

### **2. Alertas**
- LED **verde** indica sistema operacional. 🟢
- LED **azul** pisca durante o reconhecimento do ambiente. 🔵
- LED **vermelho** e **buzzer** ativam caso o objeto seja movido. 🔴🔊
- A matriz de LEDs WS2812 exibe animações para diferentes estados do sistema. ✨
- O display exibe mensagens informando o status do sistema. 📟

### **3. Controles**
- **Botão A:** Liga/Desliga o sistema. 🔘
- **Botão B:** Inicia o reconhecimento do ambiente. 📡
- **Botão do Joystick:** Confirma ou cancela alertas de movimentação. ✅

---

## **Arquitetura do Projeto**

### **Funções Principais**
1. **`processarModoBusca`** – Simula o reconhecimento do ambiente antes da monitoração. 📡
2. **`monitorarJoystick`** – Verifica movimentações acima do limite permitido. 🎮
3. **`atualizarDisplay`** – Exibe mensagens informativas no SSD1306. 📟
4. **`tocarBuzzer`** – Ativa alerta sonoro ao detectar movimento. 🔊
5. **`controlarLEDs`** – Altera as cores dos LEDs conforme o estado do sistema. 💡
6. **`exibirAnimacaoWS2812`** – Controla a matriz de LEDs para alertas visuais. ✨
7. **`enviarLog`** – Registra eventos de movimentação via UART. 📝

### **Periféricos Utilizados**
- **GPIO**: Controle dos botões e LEDs.
- **ADC**: Leitura dos eixos do joystick.
- **PWM**: Controle do buzzer.
- **I2C**: Comunicação com o display OLED.
- **UART**: Registro de eventos no sistema.
- **PIO**: Controle da matriz de LEDs WS2812.

---

## **Como Usar**

### **1. Configuração do Hardware**
- **Joystick:**
  - **Eixo X:** ADC0 (GPIO 26).
  - **Eixo Y:** ADC1 (GPIO 27).
  - **Botão do Joystick:** GPIO 22.
- **LEDs RGB:**
  - **Vermelho:** GPIO 13.
  - **Verde:** GPIO 11.
  - **Azul:** GPIO 12.
- **Buzzer:** GPIO 10.
- **Display OLED SSD1306:**
  - **SDA:** GPIO 14.
  - **SCL:** GPIO 15.
- **Matriz de LEDs WS2812:**
  - **Pino de dados:** GPIO 7.

### **2. Dependências**
- SDK do Raspberry Pi Pico. 🔧
- Biblioteca SSD1306 (inclusa no projeto). 📚
- Biblioteca PIO para controle da matriz de LEDs WS2812. ✨

### **3. Compilação e Execução**
1. Configure o ambiente utilizando o **CMake** no VS Code. 🛠️
2. Compile o projeto e gere o arquivo `.uf2`.
3. Transfira o `.uf2` para a placa **Raspberry Pi Pico W**. 🚀

---

## **Testes a Realizar**
1. **Verifique se os LEDs indicam corretamente os estados do sistema.** 🟢🔴🔵
2. **Teste o botão A para ligar e desligar o sistema.** 🔘
3. **Pressione o botão B e veja se a busca do ambiente funciona.** 📡
4. **Movimente o joystick e observe se o alerta de movimento é acionado.** 🎮⚠️
5. **Aperte o botão do joystick para confirmar a movimentação e restaurar o monitoramento.** ✅
6. **Verifique se a matriz de LEDs WS2812 exibe corretamente as animações planejadas.** ✨

---

## **Estrutura do Projeto**
```
embarcatech-movewatch/
├── CMakeLists.txt
├── build/                 # Diretório de compilação
├── MoveWatch.c            # Arquivo principal do projeto
├── inc/
│   ├── ssd1306.c          # Implementação do driver do display
│   ├── ssd1306.h          # Cabeçalho do driver do display
│   ├── font.h             # Fonte para o display SSD1306
├── generated/             # Diretório contendo arquivos gerados
│   ├── ws2812.pio.h       # Arquivo gerado para controle dos LEDs WS2812
└── README.md              # Documentação do projeto
```
---

## 📹 Demonstração do Projeto

### Vídeo de Demonstração
O vídeo mostra:
- Funcionamento do MoveWatch.
- Alertas visuais e sonoros em ação.
- Monitoramento da movimentação.
- Animações da matriz de LEDs WS2812.

📌 **[Link para o vídeo](https://drive.google.com/file/d/1bIXVT5HDBMOJAykcu7k_OgcOvw1OGY11/view?usp=drive_link)

---

## **Autor**
**Matheus Gouveia de Deus Bastos**

---

## 📜 Licença
Este projeto é de uso acadêmico e segue as diretrizes da **Embarcatech**.
