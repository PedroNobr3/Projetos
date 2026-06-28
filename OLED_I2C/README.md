# Display OLED SSD1306 via I2C com FreeRTOS

Projeto prático focado na comunicação com um display **OLED SSD1306 (128x64)** através do barramento **I2C**, integrando a exibição de dados em tempo real à arquitetura multitarefa do **FreeRTOS**. Consolida os conceitos de interrupção, leitura analógica e comunicação serial síncrona num único firmware coeso.

O sistema lê o estado de um LED (alternado por botão via interrupção) e o valor bruto de um potenciômetro (lido via ADC), e exibe ambas as informações continuamente no display OLED, com todo o compartilhamento de dados entre tarefas protegido por mutex.

---

## 🎯 Objetivos

* Estabelecer comunicação com um periférico real através do protocolo **I2C**.
* Integrar uma biblioteca de terceiros como componente externo dentro do sistema de build do ESP-IDF.
* Exibir dados dinâmicos no display, atualizados a partir de variáveis compartilhadas entre múltiplas tasks.
* Reforçar o padrão de concorrência segura: produtores (sensores) e consumidor (display) comunicando-se com proteção de mutex.

---

## 🧩 Arquitetura de Tarefas

O firmware é dividido em três tasks independentes do FreeRTOS, coordenadas por um mutex e uma fila:

* **Task de Tratamento do Botão:** permanece bloqueada aguardando eventos numa fila. A Rotina de Serviço de Interrupção (ISR), residente em IRAM, apenas sinaliza o evento na fila; a task acordada executa a alternância do LED com segurança.
* **Task de Leitura do Potenciômetro:** realiza amostragem periódica do sinal analógico através do driver **ADC One-Shot**, atualizando a variável global protegida pelo mutex.
* **Task de Atualização do OLED:** consome as variáveis compartilhadas de forma segura e renderiza o estado do LED e o valor do ADC no display, atualizando a tela em intervalos regulares.

---

## 🛠️ Tecnologias Utilizadas

* **Framework:** ESP-IDF (Espressif IoT Development Framework)
* **Linguagem:** C
* **Sistema de Tempo Real:** FreeRTOS (Tasks, Queue, Mutex)
* **Protocolo de Comunicação:** I2C
* **Display:** OLED SSD1306 128x64
* **Driver Analógico:** ADC One-Shot
* **Biblioteca de Display:** componente SSD1306 (nopnop2002), integrado via pasta `components/`

---

## 🔌 Ligações de Hardware

| Componente        | Pino do Componente | GPIO do ESP32 |
| ----------------- | ------------------ | ------------- |
| OLED SSD1306      | SDA                | GPIO 21       |
| OLED SSD1306      | SCL                | GPIO 22       |
| OLED SSD1306      | VCC / GND          | 3V3 / GND     |
| LED               | Ânodo (via resistor) | GPIO 2      |
| Botão             | Entrada com pull-up interno | GPIO 15 |
| Potenciômetro     | Cursor (sinal)     | GPIO 4 (ADC)  |

> **Observação:** o pino de RESET do display foi configurado como `-1` no menuconfig, já que módulos OLED I2C genéricos não possuem pino de reset dedicado.

---

## ⚙️ Configuração do Componente do Display

A biblioteca do display não pertence ao registro oficial de componentes do ESP-IDF e foi adicionada manualmente na pasta `components/ssd1306/`. Os parâmetros do display (interface, resolução, pinos SDA/SCL/RESET) são definidos através do **SDK Configuration Editor (menuconfig)**, na seção **SSD1306 Configuration**, e lidos pelo firmware através das macros `CONFIG_*` geradas automaticamente.

---

## 🚀 Como Compilar e Executar

Recomenda-se o uso do **Visual Studio Code** com a extensão oficial do **ESP-IDF** configurada.

1. Abrir a pasta deste projeto diretamente no VS Code.
2. Selecionar o target do dispositivo: `esp32`.
3. Ajustar, se necessário, os pinos e a resolução do display via menuconfig (seção SSD1306 Configuration).
4. Selecionar a porta serial correspondente à placa conectada.
5. Acionar **Build, Flash & Monitor** para compilar, gravar e acompanhar a saída serial.

Ao executar, o display deve exibir o estado atual do LED e a leitura do potenciômetro, atualizando ambos em tempo real conforme o botão é pressionado e o potenciômetro é girado.