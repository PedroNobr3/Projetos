# Sensores e Arquitetura Multitarefa

Firmware para ESP32 que integra múltiplos sensores em uma arquitetura multitarefa baseada em **FreeRTOS**, exibindo os dados em tempo real num display OLED. Consolida os conceitos de leitura de sensores, comunicação I2C, interrupções e comunicação entre tarefas num único sistema coeso.

O projeto implementa o padrão **produtor-consumidor**: duas tarefas produtoras coletam dados de sensores de forma independente e uma tarefa consumidora central processa essas informações e atualiza o display.

---

## 🎯 Objetivos

* Ler sensores reais (temperatura/umidade e movimento) em tarefas FreeRTOS independentes.
* Comunicar dados entre tarefas usando os mecanismos corretos do FreeRTOS (fila e notificação de tarefa).
* Exibir informações dinâmicas num display OLED via I2C.
* Praticar a arquitetura multitarefa que serve de base para os projetos seguintes (Wi-Fi e MQTT).

---

## 🧩 Arquitetura de Tarefas

O firmware é dividido em três tarefas coordenadas:

* **Tarefa Produtora DHT11:** lê temperatura e umidade a cada 2 segundos e envia os dados por uma **fila (Queue)** para a tarefa central. Continua enviando mesmo em caso de falha de leitura, sinalizando o estado através de um campo de validade.
* **Tarefa Produtora PIR:** monitora o sensor de movimento e sinaliza a detecção através de **Task Notification**, usando detecção de borda de subida para evitar disparos repetidos.
* **Tarefa Central (Consumidora):** recebe os dados dos produtores, atualiza o display OLED com as leituras e exibe um alerta visual quando há movimento.

A comunicação usa dois mecanismos distintos de propósito: **fila** para transportar dados (telemetria do DHT11) e **notificação** para sinalizar eventos (movimento do PIR).

---

## 🛠️ Tecnologias Utilizadas

* **Framework:** ESP-IDF
* **Linguagem:** C
* **Sistema de Tempo Real:** FreeRTOS (Tasks, Queue, Task Notification)
* **Comunicação:** I2C (display OLED)
* **Sensores:** DHT11 (temperatura/umidade) e HC-SR501 (movimento PIR)
* **Display:** OLED SSD1306 128x64

---

## 🔌 Ligações de Hardware

| Componente        | Pino               | GPIO do ESP32 |
| ----------------- | ------------------ | ------------- |
| OLED SSD1306      | SDA                | GPIO 21       |
| OLED SSD1306      | SCL                | GPIO 22       |
| OLED SSD1306      | VCC / GND          | 3V3 / GND     |
| DHT11             | DADOS              | GPIO 4        |
| DHT11             | VCC / GND          | 3V3 / GND     |
| PIR HC-SR501      | OUT                | GPIO 34       |
| PIR HC-SR501      | VCC / GND          | 5V (VIN) / GND|

> **Observações:**
> - O GPIO 34 é entrada pura (sem pull-up/pull-down interno). O módulo PIR possui saída ativa, então não depende de resistor interno.
> - O sensor PIR HC-SR501 é especificado para 5V; recomenda-se alimentá-lo pelo pino VIN para operação estável.
> - O PIR possui dois potenciômetros de ajuste (sensibilidade e tempo de disparo) e um jumper de modo que devem ser configurados conforme o uso.

---

## ⚙️ Configuração

A biblioteca do display SSD1306 está incluída na pasta `components/` do projeto. Os parâmetros do display (interface I2C, resolução 128x64 e pinos) são definidos através do **SDK Configuration Editor (menuconfig)**, na seção *SSD1306 Configuration*.

---

## 🚀 Como Compilar e Executar

Recomenda-se o uso do **Visual Studio Code** com a extensão oficial do **ESP-IDF**.

1. Abrir a pasta do projeto no VS Code.
2. Selecionar o target: `esp32`.
3. Ajustar os parâmetros do display via menuconfig, se necessário.
4. Selecionar a porta serial da placa.
5. Acionar **Build, Flash & Monitor**.

Ao executar, o display exibe temperatura e umidade atualizadas a cada 2 segundos e um alerta visual sempre que movimento é detectado.