#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "dht.h" 
#include "ssd1306.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "mqtt_client.h"


static const char *TAG = "MAIN";

// ==========================================
// CONFIGURAÇÃO DOS PINOS (LADO DO 3V3 / GND)
// ==========================================
#define DHT_PIN          GPIO_NUM_4    // GPIO4 (DHT11)
#define PIR_PIN          GPIO_NUM_34   // GPIO34 (PIR) - somente entrada, nao e strapping
#define OLED_SDA_PIN     GPIO_NUM_21   // GPIO21 (OLED SDA)
#define OLED_SCL_PIN     GPIO_NUM_22   // GPIO22 (OLED SCL)

// ==========================================
// Referência ao certificado embutido e handle do MQTT 
// ==========================================
extern const uint8_t ca_cert_pem_start[] asm("_binary_hivemq_ca_pem_start");
extern const uint8_t ca_cert_pem_end[]   asm("_binary_hivemq_ca_pem_end");

esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_conectado = false;

// ==========================================
// ESTRUTURAS E MANIPULADORES DO FREERTOS
// ==========================================
typedef struct {
    float temperatura;
    float umidade;
    bool  valido;       // true = leitura ok; false = falha de comunicacao
} dht_data_t;

QueueHandle_t xDHTQueue = NULL;
TaskHandle_t xCentralTaskHandle = NULL;

// Instância global de controle do Display OLED
static SSD1306_t dev; 


//O handler de eventos do MQTT
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT conectado ao broker!");
            mqtt_conectado = true;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT desconectado.");
            mqtt_conectado = false;
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT erro.");
            break;
        default:
            break;
    }
}
// Função que inicializa o MQTT
void mqtt_init(void) {
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

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
    ESP_LOGI(TAG, "Cliente MQTT iniciado.");
}


// ==========================================
// PRODUTOR 1: TAREFA DO SENSOR DHT11
// ==========================================
void vDHTTask(void *pvParameters) {
    dht_data_t dados_envio;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xDelay2s = pdMS_TO_TICKS(2000); // Intervalo de 2s exigido pelo sensor

    ESP_LOGI(TAG, "Task Produtora DHT11 Iniciada.");

    for (;;) {
        int16_t temperature = 0, humidity = 0;
        
        // Leitura utilizando a API típica do componente DHT para ESP-IDF
        if (dht_read_data(DHT_TYPE_DHT11, DHT_PIN, &humidity, &temperature) == ESP_OK) {
            dados_envio.temperatura = temperature / 10.0;
            dados_envio.umidade = humidity / 10.0;
            dados_envio.valido = true;
        } else {
            dados_envio.valido = false;
            ESP_LOGW(TAG, "Falha de comunicação com o DHT11.");
        }

        // Envia sempre (mesmo em falha) para a Central manter a tela atualizada
        xQueueSend(xDHTQueue, &dados_envio, 0);

        vTaskDelayUntil(&xLastWakeTime, xDelay2s);
    }
}

// ==========================================
// PRODUTOR 2: TAREFA DO SENSOR PIR
// ==========================================
void vPIRTask(void *pvParameters) {
    int ultimo_estado = 0;
    for (;;) {
        int estado_atual = gpio_get_level(PIR_PIN);

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
static void oled_linha(int page, const char *txt) {
    char linha[17];
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

    // Tela inicial (fica visivel antes da primeira leitura do DHT)
    oled_linha(0, "-- TELEMETRIA --");
    oled_linha(2, "Temp: --.- C");
    oled_linha(4, "Umid: --.- %");
    oled_linha(7, "DHT: iniciando");

    for (;;) {
        // 1. Novos dados de telemetria (timeout de 50ms)
        if (xQueueReceive(xDHTQueue, &dados_recebidos, pdMS_TO_TICKS(50)) == pdPASS) {
            if (dados_recebidos.valido) {
                ultima_temp = dados_recebidos.temperatura;
                ultima_umid = dados_recebidos.umidade;
                tem_dado = true;
                if (mqtt_conectado) {
                    char payload[64];
                    snprintf(payload, sizeof(payload),
                            "{\"temp\": %.1f, \"umid\": %.1f}",
                            ultima_temp, ultima_umid);
                    esp_mqtt_client_publish(mqtt_client, "casa/sala/clima", payload, 0, 1, 0);
                    ESP_LOGI(TAG, "Publicado: %s", payload);
                }
            }

            // Redesenha os valores (ultimos validos, ou "--.-" se nunca leu)
            if (tem_dado) {
                snprintf(txt_buffer, sizeof(txt_buffer), "Temp: %.1f C", ultima_temp);
                oled_linha(2, txt_buffer);
                snprintf(txt_buffer, sizeof(txt_buffer), "Umid: %.1f %%", ultima_umid);
                oled_linha(4, txt_buffer);
            } else {
                oled_linha(2, "Temp: --.- C");
                oled_linha(4, "Umid: --.- %");
            }

            // Linha de status do sensor
            oled_linha(7, dados_recebidos.valido ? "DHT: OK" : "DHT: sem sinal");
        }

        // 2. Notificacoes do PIR (sem bloquear, timeout 0)
        notificacao_pir = ulTaskNotifyTake(pdTRUE, 0);

        if (notificacao_pir > 0) {
            ESP_LOGW(TAG, "Aviso: Movimento detectado!");
            // Publica o evento de movimento na nuvem
            if (mqtt_conectado) {
                esp_mqtt_client_publish(mqtt_client, "casa/sala/movimento", "detectado", 0, 1, 0);
                ESP_LOGI(TAG, "Movimento publicado no MQTT");
            }

            // Efeito visual: alerta piscando na linha 6
            for (int i = 0; i < 3; i++) {
                oled_linha(6, ">> MOVIMENTO <<");
                vTaskDelay(pdMS_TO_TICKS(400));
                oled_linha(6, "");   // apaga (efeito pisca)
                vTaskDelay(pdMS_TO_TICKS(300));
            }
        }

        // Delay de alívio para o escalonador do RTOS
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Reage aos eventos de Wi-Fi e de IP (o "despachante" chama isto)
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // O driver ligou; hora de tentar conectar.
        esp_wifi_connect();
        ESP_LOGI(TAG, "WiFi iniciado, conectando...");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // Caiu (ou falhou). Reconecta automaticamente.
        ESP_LOGW(TAG, "WiFi desconectado. Tentando reconectar...");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // Conectou E ganhou um IP — agora sim estamos online.
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Conectado! IP obtido: " IPSTR, IP2STR(&event->ip_info.ip));

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

    // Modo station (cliente), aplica config e liga
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
    
    // 1.Inicializa o NVS (necessário para o WiFi funcionar)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Liga o WiFi (station + reconexão automática)
    wifi_init_sta();

    // 2. Configuração do pino GPIO para a entrada do PIR
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // 3. Inicialização do Barramento I2C e do display OLED
    // (Abaixo segue a estrutura padrão esperada pela maioria dos drivers SSD1306 no IDF)
#if CONFIG_I2C_INTERFACE
    i2c_master_init(&dev, OLED_SDA_PIN, OLED_SCL_PIN, -1);
    ssd1306_init(&dev, 128, 64);
    ssd1306_clear_screen(&dev, false);
#endif

    // 4. Inicialização dos mecanismos de comunicação do FreeRTOS
    xDHTQueue = xQueueCreate(3, sizeof(dht_data_t));

    if (xDHTQueue != NULL) {
        // Criando a tarefa consumidora (prioridade 2, mais alta)
        xTaskCreatePinnedToCore(vCentralTask, "CentralTask", 3072, NULL, 2, &xCentralTaskHandle, 1);
        
        // Criando as tarefas produtoras (prioridade 1)
        xTaskCreatePinnedToCore(vDHTTask, "DHTTask", 2048, NULL, 1, NULL, 1);
        xTaskCreatePinnedToCore(vPIRTask, "PIRTask", 2048, NULL, 1, NULL, 1);
        
        ESP_LOGI(TAG, "Arquitetura RTOS criada com sucesso no Core 1.");
    } else {
        ESP_LOGE(TAG, "Erro crítico ao tentar inicializar a Queue.");
    }
}