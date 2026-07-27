#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// FreeRTOS core headers (tasks, queues, delays)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// Driver and system headers for GPIO, logs and network
#include "driver/gpio.h"
#include "esp_log.h"

// Periféricos e componentes usados pelo projeto
#include "dht.h" 
#include "ssd1306.h"

// Pilha de rede / WiFi / MQTT
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "mqtt_client.h"


// Tag usada pelo ESP_LOG* para identificar mensagens deste módulo
static const char *TAG = "MAIN";

// ==========================================
// CONFIGURAÇÃO DOS PINOS (LADO DO 3V3 / GND)
// ==========================================
#define DHT_PIN          GPIO_NUM_4    // GPIO4 (DHT11)
#define PIR_PIN          GPIO_NUM_34   // GPIO34 (PIR) - somente entrada, nao e strapping
#define OLED_SDA_PIN     GPIO_NUM_21   // GPIO21 (OLED SDA)
#define OLED_SCL_PIN     GPIO_NUM_22   // GPIO22 (OLED SCL)
#define LED_PIN          GPIO_NUM_2    // GPIO2 (LED controlado via MQTT)

#define TOPICO_COMANDO   "casa/sala/led/set"     // recebe ON/OFF
#define TOPICO_ESTADO    "casa/sala/led/estado"  // publica o estado atual

// ==========================================
// Referência ao certificado embutido e handle do MQTT 
// ==========================================
extern const uint8_t ca_cert_pem_start[] asm("_binary_hivemq_ca_pem_start");
extern const uint8_t ca_cert_pem_end[]   asm("_binary_hivemq_ca_pem_end");

// Handle do cliente MQTT e flag de estado da conexão
esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_conectado = false;

// ==========================================
// ESTRUTURAS E MANIPULADORES DO FREERTOS
// ==========================================
// Estrutura usada para transmitir leituras do DHT entre tarefas via Queue
typedef struct {
    float temperatura; 
    float umidade;    
    bool  valido;      // true = leitura OK; false = falha de comunicação
} dht_data_t;

QueueHandle_t xDHTQueue = NULL;
TaskHandle_t xCentralTaskHandle = NULL;

// Instância global de controle do Display OLED
static SSD1306_t dev; 


// Handler de eventos do MQTT: trata conexões, desconexões e erros
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT conectado ao broker!");
            mqtt_conectado = true;
            // Assina o tópico de comando para receber ON/OFF
            esp_mqtt_client_subscribe(mqtt_client, TOPICO_COMANDO, 1);
            ESP_LOGI(TAG, "Inscrito no topico: %s", TOPICO_COMANDO);
            break;
        case MQTT_EVENT_DISCONNECTED:
            // Perda de conexão com o broker
            ESP_LOGW(TAG, "MQTT desconectado.");
            mqtt_conectado = false;
            break;
        case MQTT_EVENT_ERROR:
            // Evento genérico de erro — investigar logs
            ESP_LOGE(TAG, "MQTT erro.");
            break;
        case MQTT_EVENT_DATA:
        // Chegou uma mensagem num tópico que assinamos.
        // event->topic e event->data NÃO são terminados em '\0',
        // por isso usamos os campos de tamanho (topic_len, data_len).
        ESP_LOGI(TAG, "Comando recebido: %.*s", event->data_len, event->data);

        // Compara o conteúdo recebido com "ON" ou "OFF"
        if (strncmp(event->data, "ON", event->data_len) == 0) {
            gpio_set_level(LED_PIN, 1);   // acende o LED
            ESP_LOGI(TAG, "LED ligado.");
            // Confirma o estado de volta (retained = 1)
            esp_mqtt_client_publish(mqtt_client, TOPICO_ESTADO, "ON", 0, 1, 1);
        }
        else if (strncmp(event->data, "OFF", event->data_len) == 0) {
            gpio_set_level(LED_PIN, 0);   // apaga o LED
            ESP_LOGI(TAG, "LED desligado.");
            esp_mqtt_client_publish(mqtt_client, TOPICO_ESTADO, "OFF", 0, 1, 1);
        }
        break;
        default:
            break;
    }
}
// Função que inicializa o MQTT
void mqtt_init(void) {
    // Configurações do cliente MQTT (valores vindos do menuconfig)
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = CONFIG_MQTT_BROKER_URI,
            .verification.certificate = (const char *)ca_cert_pem_start,
        },
        .credentials = {
            .username = CONFIG_MQTT_USERNAME,
            .authentication.password = CONFIG_MQTT_PASSWORD,
        },
    };

    // Cria, registra o handler e inicia o cliente MQTT
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
    ESP_LOGI(TAG, "Cliente MQTT iniciado.");
}


// ==========================================
// PRODUTOR 1: TAREFA DO SENSOR DHT11
// ==========================================
// Task produtora responsável por ler o sensor DHT11 periodicamente
void vDHTTask(void *pvParameters) {
    dht_data_t dados_envio;
    // Usamos vTaskDelayUntil para manter intervalos regulares
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xDelay2s = pdMS_TO_TICKS(2000); // DHT11 exige ~2s entre leituras

    ESP_LOGI(TAG, "Task Produtora DHT11 Iniciada.");

    for (;;) {
        int16_t temperature = 0, humidity = 0;
        
        // Tenta ler o sensor; a API retorna valores inteiros (ex: 235 = 23.5°C)
        if (dht_read_data(DHT_TYPE_DHT11, DHT_PIN, &humidity, &temperature) == ESP_OK) {
            // Converte para float com uma casa decimal
            dados_envio.temperatura = temperature / 10.0;
            dados_envio.umidade = humidity / 10.0;
            dados_envio.valido = true;
        } else {
            // Em caso de falha, marca a leitura como inválida
            dados_envio.valido = false;
            ESP_LOGW(TAG, "Falha de comunicação com o DHT11.");
        }

        // Envia sempre (mesmo em falha) para que a task central atualize a tela
        xQueueSend(xDHTQueue, &dados_envio, 0);

        // Aguarda até o próximo ciclo de leitura
        vTaskDelayUntil(&xLastWakeTime, xDelay2s);
    }
}

// ==========================================
// PRODUTOR 2: TAREFA DO SENSOR PIR
// ==========================================
// Task produtora para o sensor PIR: detecta borda de subida (movimento)
void vPIRTask(void *pvParameters) {
    int ultimo_estado = 0;
    for (;;) {
        // Lê estado digital do pino do PIR
        int estado_atual = gpio_get_level(PIR_PIN);

        // Detecta transição 0 -> 1 e notifica a task central sem bloquear
        if (estado_atual == 1 && ultimo_estado == 0) {
            xTaskNotifyGive(xCentralTaskHandle);
        }
        ultimo_estado = estado_atual;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ==========================================
// CONSUMIDOR: TAREFA CENTRAL (CÉREBRO)
// ==========================================
// Escreve uma linha (page) do OLED sempre com 16 colunas, preenchendo com
// espacos para apagar qualquer resto da mensagem anterior (evita "lixo").
// Escreve uma linha no OLED com padding para garantir limpeza da linha
static void oled_linha(int page, const char *txt) {
    char linha[17];
    // Formata a string com largura fixa (16 colunas) para sobrescrever
    snprintf(linha, sizeof(linha), "%-16s", txt);
    ssd1306_display_text(&dev, page, linha, 16, false);
}

void vCentralTask(void *pvParameters) {
    dht_data_t dados_recebidos;
    uint32_t notificacao_pir;
    char txt_buffer[24];

    float ultima_temp = 0.0;
    float ultima_umid = 0.0;
    bool  tem_dado = false;   

    ESP_LOGI(TAG, "Task Consumidora Central Iniciada.");

    // Tela inicial exibida antes das primeiras leituras do DHT
    oled_linha(0, "-- TELEMETRIA --");
    oled_linha(2, "Temp: --.- C");
    oled_linha(4, "Umid: --.- %");
    oled_linha(7, "DHT: iniciando");

    for (;;) {
        // 1) Tenta receber novos dados do DHT (timeout curto para não bloquear)
        if (xQueueReceive(xDHTQueue, &dados_recebidos, pdMS_TO_TICKS(50)) == pdPASS) {
            if (dados_recebidos.valido) {
                // Atualiza valores válidos locais
                ultima_temp = dados_recebidos.temperatura;
                ultima_umid = dados_recebidos.umidade;
                tem_dado = true;
                // Se conectado ao MQTT, publica os dados em formato JSON
                if (mqtt_conectado) {
                    char payload[64];
                    snprintf(payload, sizeof(payload),
                            "{\"temp\": %.1f, \"umid\": %.1f}",
                            ultima_temp, ultima_umid);
                    esp_mqtt_client_publish(mqtt_client, "casa/sala/clima", payload, 0, 1, 0);
                    ESP_LOGI(TAG, "Publicado: %s", payload);
                }
            }

            // Atualiza a tela OLED com os últimos valores (ou placeholders)
            if (tem_dado) {
                snprintf(txt_buffer, sizeof(txt_buffer), "Temp: %.1f C", ultima_temp);
                oled_linha(2, txt_buffer);
                snprintf(txt_buffer, sizeof(txt_buffer), "Umid: %.1f %%", ultima_umid);
                oled_linha(4, txt_buffer);
            } else {
                oled_linha(2, "Temp: --.- C");
                oled_linha(4, "Umid: --.- %");
            }

            // Exibe status de leitura do sensor
            oled_linha(7, dados_recebidos.valido ? "DHT: OK" : "DHT: sem sinal");
        }

        // 2) Trata notificações do PIR (sem bloquear: timeout 0)
        notificacao_pir = ulTaskNotifyTake(pdTRUE, 0);

        if (notificacao_pir > 0) {
            ESP_LOGW(TAG, "Aviso: Movimento detectado!");
            // Publica evento de movimento no MQTT, se conectado
            if (mqtt_conectado) {
                esp_mqtt_client_publish(mqtt_client, "casa/sala/movimento", "detectado", 0, 1, 0);
                ESP_LOGI(TAG, "Movimento publicado no MQTT");
            }

            // Efeito visual simples: piscar mensagem de movimento na linha 6
            for (int i = 0; i < 3; i++) {
                oled_linha(6, ">> MOVIMENTO <<");
                vTaskDelay(pdMS_TO_TICKS(400));
                oled_linha(6, "");   // apaga (efeito pisca)
                vTaskDelay(pdMS_TO_TICKS(300));
            }
        }

        // Pausa curta para aliviar o escalonador e evitar busy-loop
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Reage aos eventos de Wi-Fi e de IP (o "despachante" chama isto)
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // O driver de WiFi foi inicializado: solicita conexão ao AP
        esp_wifi_connect();
        ESP_LOGI(TAG, "WiFi iniciado, conectando...");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // Tentativa de reconexão automática em caso de queda
        ESP_LOGW(TAG, "WiFi desconectado. Tentando reconectar...");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // Obtivemos um IP via DHCP: o sistema está online
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Conectado! IP obtido: " IPSTR, IP2STR(&event->ip_info.ip));

        // Ao ter IP, iniciamos o cliente MQTT (se ainda não iniciado)
        if (mqtt_client == NULL) {
            mqtt_init();
            ESP_LOGI(TAG, "Tentando conectar ao broker MQTT...");
        }
    }
}

void wifi_init_sta(void) {
    // Inicializa a pilha de rede e o event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Configuração padrão do driver de Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Registra nossa função para TODOS os eventos de WiFi e de IP
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));

    // Define as credenciais (vindas do menuconfig)
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };

    // Define modo estação (cliente), aplica configuração e inicia o driver
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Configuracao WiFi finalizada.");
}

// ==========================================
// PONTO DE ENTRADA PADRÃO DO ESP-IDF
// ==========================================
void app_main(void) {
    ESP_LOGI(TAG, "Inicializando Firmware Simplificado - Versão v0.1");
    // Configura o pino do LED como saída digital
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_conf);
    gpio_set_level(LED_PIN, 0);   // começa apagado
    
    // 1. Inicializa o NVS (necessário para armazenar credenciais WiFi, entre outros)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Em caso de mudança de versão do NVS, apaga e re-inicializa
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Inicializa e conecta o WiFi (modo station com reconexão automática)
    wifi_init_sta();

    // 2. Configura o pino do PIR como entrada digital
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // 3. Inicializa o barramento I2C e o display OLED (se habilitado em menuconfig)
    // A maioria dos drivers SSD1306 no ESP-IDF espera esta inicialização
#if CONFIG_I2C_INTERFACE
    i2c_master_init(&dev, OLED_SDA_PIN, OLED_SCL_PIN, -1);
    ssd1306_init(&dev, 128, 64);
    ssd1306_clear_screen(&dev, false);
#endif

    // 4. Inicializa recursos do FreeRTOS: fila para leituras do DHT e tarefas
    xDHTQueue = xQueueCreate(3, sizeof(dht_data_t));

    if (xDHTQueue != NULL) {
        // Cria a task consumidora (responsável pela lógica central e display)
        xTaskCreatePinnedToCore(vCentralTask, "CentralTask", 3072, NULL, 2, &xCentralTaskHandle, 1);
        
        // Cria as task produtoras: leitura do DHT e monitoramento do PIR
        xTaskCreatePinnedToCore(vDHTTask, "DHTTask", 2048, NULL, 1, NULL, 1);
        xTaskCreatePinnedToCore(vPIRTask, "PIRTask", 2048, NULL, 1, NULL, 1);
        
        ESP_LOGI(TAG, "Arquitetura RTOS criada com sucesso no Core 1.");
    } else {
        ESP_LOGE(TAG, "Erro crítico ao tentar inicializar a Queue.");
    }
}