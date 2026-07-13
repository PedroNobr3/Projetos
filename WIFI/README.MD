# Wi-Fi e Conectividade

Evolução do projeto da Sensores e Arquitetura Multitarefa, adicionando **conectividade Wi-Fi** ao sistema de sensores. O ESP32 passa a se conectar a uma rede local em modo *station* (cliente), com **reconexão automática** em caso de queda, mantendo em paralelo toda a arquitetura multitarefa de leitura de sensores e exibição no OLED.

Este projeto introduz o modelo de **eventos de rede** do ESP-IDF (`esp_event`), a base necessária para as etapas seguintes de comunicação com a nuvem (MQTT).

---

## 🎯 Objetivos

* Conectar o ESP32 a uma rede Wi-Fi 2.4GHz em modo station.
* Entender e aplicar o modelo de *event loop* do ESP-IDF para tratar eventos de conexão.
* Implementar reconexão automática robusta (o dispositivo se reconecta sozinho se a rede cair).
* Manter todo o sistema de sensores do projeto Sensores e Arquitetura Multitarefa funcionando simultaneamente à conectividade.

---

## 🧩 O que foi adicionado em relação à Sensores e Arquitetura Multitarefa

* **Inicialização da pilha de rede** e do *event loop* padrão do ESP-IDF.
* **Handler de eventos de Wi-Fi/IP**, que reage aos eventos de início, desconexão e obtenção de IP — seguindo o mesmo padrão "sinaliza → reage" usado com as interrupções de GPIO.
* **Reconexão automática:** ao detectar desconexão, o firmware tenta reconectar sozinho.
* **Armazenamento seguro de credenciais:** o SSID e a senha da rede são configurados via menuconfig (não ficam escritos no código-fonte), mantendo as credenciais fora do controle de versão.

O sistema só é considerado "online" quando obtém um endereço IP — momento sinalizado pelo evento `IP_EVENT_STA_GOT_IP`.

---

## 🛠️ Tecnologias Utilizadas

* **Framework:** ESP-IDF
* **Linguagem:** C
* **Sistema de Tempo Real:** FreeRTOS (Tasks, Queue, Task Notification)
* **Conectividade:** Wi-Fi (modo station), event loop (`esp_event`), NVS
* **Comunicação:** I2C (display OLED)
* **Sensores:** DHT11 e HC-SR501 (PIR)
* **Display:** OLED SSD1306 128x64

---

## 🔌 Ligações de Hardware

Idênticas às do Sensores e Arquitetura Multitarefa:

| Componente        | Pino    | GPIO do ESP32 |
| ----------------- | ------- | ------------- |
| OLED SSD1306      | SDA/SCL | GPIO 21 / 22  |
| DHT11             | DADOS   | GPIO 4        |
| PIR HC-SR501      | OUT     | GPIO 34       |

Alimentação: OLED e DHT11 em 3V3; PIR em 5V (VIN). GND comum.

---

## ⚙️ Configuração

Antes de compilar, configure as credenciais da rede no **menuconfig**, na seção *Configuracao WiFi do Projeto*:

* `WIFI_SSID` — nome da rede (2.4GHz)
* `WIFI_PASSWORD` — senha da rede

> As credenciais ficam salvas no arquivo `sdkconfig`, que deve ser incluído no `.gitignore` para não expor a senha no repositório.

A biblioteca do display SSD1306 está incluída na pasta `components/`.

---

## 🚀 Como Compilar e Executar

1. Abrir a pasta do projeto no VS Code com a extensão ESP-IDF.
2. Configurar SSID e senha via menuconfig.
3. Selecionar o target `esp32` e a porta serial.
4. Acionar **Build, Flash & Monitor**.

No monitor serial, a sequência esperada é: início da conexão, obtenção do IP e confirmação de que o dispositivo está online. Para testar a robustez, basta desligar e religar o roteador: o dispositivo deve reconectar automaticamente.