# MQTT e Telemetria na Nuvem

Etapa final do sistema IoT: o ESP32 passa a **publicar os dados dos sensores na nuvem** através do protocolo **MQTT sobre TLS**, conectando-se a um broker HiveMQ Cloud. Reúne tudo o que foi construído nos projetos anteriores — sensores, arquitetura multitarefa, display e Wi-Fi — e adiciona a camada de comunicação com a internet de forma segura.

Com este projeto, a telemetria de temperatura e umidade do dispositivo pode ser acompanhada em tempo real de qualquer lugar, através de qualquer cliente MQTT conectado ao mesmo broker.

---

## 🎯 Objetivos

* Publicar leituras dos sensores num broker MQTT na nuvem.
* Estabelecer uma conexão segura (TLS) validada por certificado.
* Estruturar os dados de telemetria em formato JSON.
* Garantir a ordem correta de inicialização: o cliente MQTT só é iniciado após a rede estar efetivamente disponível.

---

## 🧩 O que foi adicionado em relação ao projeto IoT

* **Cliente MQTT** conectado a um broker HiveMQ Cloud sobre TLS (porta 8883).
* **Certificado CA embutido** no firmware para validação da conexão segura.
* **Publicação de telemetria em JSON:** a tarefa central publica as leituras de temperatura e umidade no tópico `casa/sala/clima` a cada ciclo de leitura válido.
* **Inicialização orientada a evento:** o cliente MQTT é iniciado a partir do evento de obtenção de IP, garantindo que a rede já esteja pronta antes da tentativa de conexão (evitando falhas de resolução de nome).
* **Handler de eventos MQTT:** trata os estados de conexão, desconexão e erro do cliente.

---

## 🛠️ Tecnologias Utilizadas

* **Framework:** ESP-IDF
* **Linguagem:** C
* **Sistema de Tempo Real:** FreeRTOS (Tasks, Queue, Task Notification)
* **Conectividade:** Wi-Fi (station), MQTT sobre TLS, NVS
* **Broker:** HiveMQ Cloud (plano Serverless)
* **Segurança:** TLS com certificado CA (ISRG Root X1, Let's Encrypt)
* **Comunicação local:** I2C (display OLED)
* **Sensores:** DHT11 e HC-SR501 (PIR)
* **Display:** OLED SSD1306 128x64

---

## 📡 Fluxo de Dados

```
Sensores (DHT11/PIR) → Tarefas FreeRTOS → Tarefa Central
                                              ↓
                              Publicação MQTT (JSON, TLS)
                                              ↓
                                      Broker HiveMQ Cloud
                                              ↓
                              Qualquer cliente MQTT inscrito
```

Tópico de publicação: `casa/sala/clima`
Formato da mensagem: `{"temp": 30.0, "umid": 71.0}`

---

## ⚙️ Configuração

Antes de compilar, é necessário:

**1. Credenciais no menuconfig**

Seção *Configuracao WiFi do Projeto*:
* `WIFI_SSID` / `WIFI_PASSWORD`

Seção *Configuracao MQTT do Projeto*:
* `MQTT_BROKER_URI` — URI do broker no formato `mqtts://SEU_CLUSTER.s1.eu.hivemq.cloud:8883` (o prefixo `mqtts://` e a porta `8883` são obrigatórios para TLS)
* `MQTT_USERNAME` / `MQTT_PASSWORD` — credenciais de acesso criadas no HiveMQ

**2. Certificado CA**

O arquivo `hivemq_ca.pem` (certificado ISRG Root X1 da Let's Encrypt) deve estar na pasta `main/` e é embutido no firmware via `EMBED_TXTFILES` no `CMakeLists.txt`.

**3. Componente MQTT**

A partir do ESP-IDF v6.0, o cliente MQTT é um componente gerenciado. Instale com:

```
idf.py add-dependency "espressif/mqtt"
```

> O arquivo `sdkconfig` (que contém as senhas) deve estar no `.gitignore`.

---

## 🚀 Como Compilar e Executar

1. Instalar o componente MQTT (`idf.py add-dependency "espressif/mqtt"`).
2. Configurar credenciais de Wi-Fi e MQTT via menuconfig.
3. Garantir que o `hivemq_ca.pem` está na pasta `main/`.
4. Selecionar o target `esp32` e a porta serial.
5. Acionar **Build, Flash & Monitor**.

No monitor serial, a sequência esperada é: conexão Wi-Fi → obtenção de IP → conexão MQTT ao broker → publicações periódicas dos dados.

## 🔍 Visualizando os Dados

Para acompanhar as mensagens publicadas, conecte um cliente MQTT ao mesmo broker (por exemplo, o Web Client do HiveMQ ou o MQTT Explorer), usando as credenciais configuradas, e inscreva-se no tópico `casa/sala/clima`. As mensagens de telemetria aparecerão em tempo real.