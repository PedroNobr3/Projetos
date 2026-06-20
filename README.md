# Repositório de Projetos de Sistemas Embarcados

Este repositório centraliza o desenvolvimento de firmwares e projetos voltados ao universo dos sistemas embarcados. O objetivo principal é documentar a evolução prática no uso de microcontroladores modernos, sistemas operacionais de tempo real e protocolos de comunicação industriais.

A arquitetura deste repositório foi projetada para manter a organização individual de cada aplicação. Cada subpasta representa um laboratório ou projeto independente, contendo seu próprio código-fonte, arquivos de configuração de compilação e um arquivo de documentação específico detalhando os requisitos e os resultados alcançados.

---

## 🛠️ Tecnologias e Ferramentas Utilizadas

O ecossistema de desenvolvimento baseia-se fortemente no framework **ESP-IDF** (Espressif IoT Development Framework) na sua versão mais recente, utilizando a linguagem **C** como base para a escrita de firmwares otimizados. 

Para a automação de compilação e gerenciamento de build, são utilizadas as ferramentas **CMake** e **Ninja**. O gerenciamento do sistema operacional e o particionamento de tarefas de tempo real são implementados através das APIs nativas do **FreeRTOS**, garantindo determinismo e controle rígido sobre o hardware.

---

## 📂 Organização do Repositório

A estrutura de diretórios do repositório está organizada da seguinte forma:

* **`Blink_FreeRTOS/`**: Primeiro projeto prático focado na criação de Tasks explícitas no FreeRTOS para controle de GPIOs e alternância de estados de LEDs, com monitoramento de logs via UART.
* **`Botao_e_Potenciometro/`**: Implementação assíncrona utilizando Rotinas de Serviço de Interrupção (ISR) em memória RAM (IRAM_ATTR) integradas a Filas (Queue) do FreeRTOS para controle de LED, em conjunto com a leitura periódica de sinais analógicos via driver ADC One-Shot.
* *(Novos projetos serão adicionados nesta raiz seguindo o mesmo padrão de encapsulamento).*

---

## 🚀 Como Compilar os Projetos

Para rodar qualquer um dos subprojetos contidos neste repositório, recomenda-se a utilização do ambiente de desenvolvimento **Visual Studio Code** com a extensão oficial do **ESP-IDF** devidamente configurada.

O desenvolvedor deve abrir o VS Code focando diretamente na pasta do subprojeto desejado (ex: abrindo a pasta `Blink_FreeRTOS` individualmente). A partir disso, basta selecionar o target correto para a placa utilizada (como o chip `esp32`), definir a porta serial de comunicação ativa e acionar o comando de Flash (Build, Flash & Monitor) através da interface do VS Code para compilar e descarregar o binário diretamente no hardware.