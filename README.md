# Repositório de Projetos de Sistemas Embarcados

Este repositório centraliza o desenvolvimento de firmwares e projetos voltados ao universo dos sistemas embarcados. O objetivo principal é documentar a evolução prática no uso de microcontroladores modernos, sistemas operacionais de tempo real e protocolos de comunicação

A arquitetura deste repositório foi projetada para manter a organização individual de cada aplicação. Cada subpasta representa um projeto, contendo seu próprio código-fonte, arquivos de configuração de compilação e um arquivo de documentação específico detalhando os requisitos e os resultados alcançados.

---

## 🛠️ Tecnologias e Ferramentas Utilizadas

O ecossistema de desenvolvimento baseia-se no framework **ESP-IDF** (Espressif IoT Development Framework), utilizando a linguagem **C** como base para a escrita de firmwares otimizados.

Para a automação de compilação e gerenciamento de build, são utilizadas as ferramentas **CMake** e **Ninja**. O gerenciamento do sistema operacional e o particionamento de tarefas de tempo real são implementados através das APIs nativas do **FreeRTOS**, garantindo determinismo e controle rígido sobre o hardware.

A comunicação com periféricos e serviços explora diversos protocolos ao longo dos projetos: **I2C** (displays e sensores), **Wi-Fi** (conectividade em modo station) e **MQTT sobre TLS** (telemetria e comando seguros na nuvem). A aquisição de sinais analógicos é feita através do driver **ADC One-Shot**, e a interação com sensores digitais explora interrupções e comunicação por pino único.

---

## 📂 Organização do Repositório

A estrutura de diretórios está organizada de forma progressiva, com cada projeto construindo sobre os conceitos do anterior:

* **`Blink_FreeRTOS/`**: Primeiro projeto prático focado na criação de Tasks explícitas no FreeRTOS para controle de GPIOs e alternância de estados de LEDs, com monitoramento de logs via UART.

* **`Botao_e_Potenciometro/`**: Implementação assíncrona utilizando Rotinas de Serviço de Interrupção (ISR) em memória RAM (IRAM_ATTR) integradas a Filas (Queue) do FreeRTOS para controle de LED, em conjunto com a leitura periódica de sinais analógicos via driver ADC One-Shot.

* **`OLED_I2C/`**: Comunicação com um display OLED SSD1306 (128x64) através do barramento I2C, integrando a leitura de botão (via interrupção) e potenciômetro (via ADC) à exibição de dados em tempo real. Demonstra a integração de uma biblioteca externa como componente e o uso de mutex para compartilhamento seguro de dados entre múltiplas tasks.

* **`Sensores_e_Multitarefa/`**: Sistema multitarefa que integra sensor de temperatura/umidade (DHT11) e sensor de movimento (PIR) numa arquitetura produtor-consumidor. Utiliza Fila (Queue) para telemetria e Task Notification para eventos, exibindo os dados num display OLED. Consolida os fundamentos de arquitetura de firmware com FreeRTOS.

* **`WiFi/`**: Adiciona conectividade Wi-Fi ao sistema de sensores, com o ESP32 operando em modo station e reconexão automática. Introduz o modelo de eventos de rede do ESP-IDF (`esp_event`) e o armazenamento seguro de credenciais via menuconfig.

* **`MQTT/`**: Publicação da telemetria dos sensores num broker HiveMQ Cloud através de MQTT sobre TLS, com certificado CA embutido. Reúne sensores, arquitetura multitarefa, display, Wi-Fi e comunicação segura com a nuvem, estabelecendo o fluxo de dados do dispositivo para a nuvem.

* **`MQTT_Bidirecional/`**: Fechamento do ciclo IoT. Além de publicar telemetria, o dispositivo assina (subscribe) um tópico de comando e passa a ser controlado remotamente, acionando um LED conforme mensagens ON/OFF recebidas da nuvem. Confirma o estado atual através de mensagens retained, demonstrando comunicação MQTT bidirecional completa.

Os projetos `Sensores`, `WiFi`, `MQTT` e `MQTT_Bidirecional` formam uma progressão contínua: partindo da leitura de sensores, passando pela conectividade e pela telemetria na nuvem, até o controle remoto bidirecional do dispositivo.

---

## 📌 Nota sobre a versão do ESP-IDF

Os projetos iniciais foram desenvolvidos na linha **v5.5.x** do ESP-IDF. Os projetos `Sensores`, `WiFi`, `MQTT` e `MQTT_Bidirecional` utilizam a linha **v6.0.x**, na qual alguns componentes (como o cliente MQTT) passaram a ser gerenciados via *Component Manager* — nesses casos, as dependências são declaradas em um arquivo `idf_component.yml` e instaladas automaticamente durante o build. Cada README específico detalha os requisitos do respectivo projeto.

---

## 🚀 Como Compilar os Projetos

Para rodar qualquer um dos subprojetos, recomenda-se a utilização do ambiente de desenvolvimento **Visual Studio Code** com a extensão oficial do **ESP-IDF** devidamente configurada.

O desenvolvedor deve abrir o VS Code focando diretamente na pasta do subprojeto desejado (ex.: abrindo a pasta `MQTT_Bidirecional` individualmente). A partir disso, basta selecionar o target correto para a placa utilizada (o chip `esp32`), definir a porta serial de comunicação ativa e acionar o comando **Build, Flash & Monitor** através da interface do VS Code para compilar e descarregar o binário diretamente no hardware.

Projetos que utilizam bibliotecas externas (como o `OLED_I2C/` e os projetos de IoT) incluem essas dependências numa pasta `components/` interna ou via Component Manager, e podem exigir o ajuste de parâmetros através do menuconfig antes da compilação (credenciais de rede, parâmetros do display, etc.), conforme detalhado no README de cada subprojeto.

> **Nota de segurança:** projetos que utilizam credenciais (Wi-Fi, MQTT) armazenam esses dados no arquivo `sdkconfig`, que é mantido fora do controle de versão através do `.gitignore`. Para reproduzir esses projetos, configure suas próprias credenciais via menuconfig.