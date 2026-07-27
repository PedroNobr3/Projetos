# MQTT_Bidirecional — Comando Remoto via MQTT

Fechamento do ciclo de comunicação IoT. Enquanto o projeto anterior (`MQTT`) estabeleceu o fluxo de dados do dispositivo **para** a nuvem (telemetria), este projeto adiciona o sentido inverso: o ESP32 passa a **receber comandos** da nuvem, permitindo o controle remoto de um LED. O dispositivo deixa de apenas monitorar e passa também a executar ações à distância.

O comando é enviado por qualquer cliente MQTT publicando `ON` ou `OFF` num tópico de comando; o ESP32, inscrito nesse tópico, aciona o LED e confirma o novo estado publicando de volta numa mensagem *retained*.

---

## 🎯 Objetivos

* Assinar (subscribe) um tópico MQTT para receber comandos da nuvem.
* Tratar as mensagens recebidas e acionar uma saída física (LED) conforme o comando.
* Publicar o estado atual do dispositivo de forma persistente (mensagem retained), garantindo que qualquer cliente novo conheça o estado imediatamente.
* Demonstrar comunicação MQTT bidirecional completa: telemetria (dispositivo → nuvem) e comando (nuvem → dispositivo).

---

## 🧩 O que foi adicionado em relação ao projeto MQTT

* **Saída digital (LED):** um novo pino é configurado como saída para ser acionado remotamente.
* **Subscribe no evento de conexão:** ao conectar (ou reconectar) ao broker, o cliente assina automaticamente o tópico de comando.
* **Tratamento de mensagens recebidas:** um novo caso no handler de eventos (`MQTT_EVENT_DATA`) interpreta o conteúdo recebido (`ON`/`OFF`) e aciona o LED. Segue o mesmo padrão "callback sinaliza, ação é executada" já utilizado com as interrupções de sensores.
* **Confirmação de estado com mensagem retained:** após cada comando, o dispositivo publica o novo estado num tópico de estado com o flag *retained* ativo, de modo que o último estado fique disponível para qualquer cliente que se conecte posteriormente.

A telemetria dos sensores (temperatura, umidade e movimento) do projeto anterior permanece funcionando em paralelo.

---

## 📡 Tópicos MQTT

| Tópico                    | Sentido              | Conteúdo                      |
| ------------------------- | -------------------- | ----------------------------- |
| `casa/sala/clima`         | Dispositivo → Nuvem  | `{"temp": 30.0, "umid": 71.0}`|
| `casa/sala/led/set`       | Nuvem → Dispositivo  | `ON` ou `OFF` (comando)       |
| `casa/sala/led/estado`    | Dispositivo → Nuvem  | `ON` ou `OFF` (confirmação, retained) |

---

## 🛠️ Tecnologias Utilizadas

* **Framework:** ESP-IDF
* **Linguagem:** C
* **Sistema de Tempo Real:** FreeRTOS (Tasks, Queue, Task Notification)
* **Conectividade:** Wi-Fi (station), MQTT sobre TLS (publish e subscribe), NVS
* **Broker:** HiveMQ Cloud (plano Serverless)
* **Segurança:** TLS com certificado CA (ISRG Root X1, Let's Encrypt)
* **Comunicação local:** I2C (display OLED)
* **Sensores/Atuadores:** DHT11, HC-SR501 (PIR) e LED (saída controlada remotamente)
* **Display:** OLED SSD1306 128x64

---

## 🔌 Ligações de Hardware

Além das ligações dos projetos anteriores (OLED, DHT11 e PIR), acrescenta-se o LED:

| Componente | Pino          | GPIO do ESP32 |
| ---------- | ------------- | ------------- |
| LED        | Ânodo (via resistor ~220–330Ω) | GPIO 2 |
| LED        | Cátodo        | GND           |

> **Observação:** a maioria das placas ESP32 DevKit possui um LED embutido no GPIO 2, permitindo testar o controle remoto mesmo sem um LED externo. O GPIO 2 é um pino de strapping (boot), mas funciona normalmente como saída para esta aplicação.

---

## ⚙️ Configuração

As configurações são as mesmas do projeto `MQTT`:

**Credenciais no menuconfig:**
* Seção *Configuracao WiFi do Projeto*: `WIFI_SSID` e `WIFI_PASSWORD`
* Seção *Configuracao MQTT do Projeto*: `MQTT_BROKER_URI` (formato `mqtts://SEU_CLUSTER.s1.eu.hivemq.cloud:8883`), `MQTT_USERNAME` e `MQTT_PASSWORD`

**Certificado CA:** o arquivo `hivemq_ca.pem` deve estar na pasta `main/` e é embutido via `EMBED_TXTFILES`.

**Componente MQTT:** instalar com `idf.py add-dependency "espressif/mqtt"`.

> O arquivo `sdkconfig` (que contém as senhas) deve estar no `.gitignore`.

---

## 🚀 Como Compilar e Executar

1. Instalar o componente MQTT (`idf.py add-dependency "espressif/mqtt"`).
2. Configurar credenciais de Wi-Fi e MQTT via menuconfig.
3. Garantir que o `hivemq_ca.pem` está na pasta `main/`.
4. Selecionar o target `esp32` e a porta serial.
5. Acionar **Build, Flash & Monitor**.

No monitor serial, confirme a inscrição no tópico de comando após a conexão MQTT.

## 🔍 Testando o Controle Remoto

Usando um cliente MQTT (por exemplo, o Web Client do HiveMQ) conectado ao broker:

1. **Enviar comando:** publique `ON` (ou `OFF`) no tópico `casa/sala/led/set`. O LED deve acender ou apagar.
2. **Ver confirmação:** inscreva-se no tópico `casa/sala/led/estado` para receber a confirmação do estado atual. Inscrever-se em `casa/sala/#` permite acompanhar simultaneamente a telemetria e o estado do LED.