#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include <string.h>

// A inclusão do driver do OLED dependerá do componente instalado via idf.py
#include "ssd1306.h"

static const char *TAG = "MAIN";

// Definições de Hardware (Apenas lado direito - 3V3)
#define PIN_LED          2   
#define PIN_BOTAO        15  
#define ADC_POT_CHANNEL  ADC_CHANNEL_0 // Refere-se à GPIO 4 (ADC2_CH0)
#define I2C_SDA_PIN      21
#define I2C_SCL_PIN      22

// Handlers do FreeRTOS
static QueueHandle_t xBotaoQueue = NULL;
static SemaphoreHandle_t xMutexDados = NULL;

// Variáveis Globais de Estado (Protegidas pelo Mutex)
static int estado_led_global = 0;
static int valor_potenciometro_global = 0;

// Rotina de Serviço de Interrupção (ISR)
static void IRAM_ATTR botao_isr_handler(void* arg) {
    uint32_t pino = (uint32_t) arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(xBotaoQueue, &pino, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// Task de Tratamento do Botão
void vTaskTrataBotao(void *pvParameters) {
    uint32_t pino_recebido;
    while (1) {
        if (xQueueReceive(xBotaoQueue, &pino_recebido, portMAX_DELAY) == pdTRUE) {
            
            // Requisita o Mutex para atualizar a variável global com segurança
            if (xSemaphoreTake(xMutexDados, portMAX_DELAY)) {
                estado_led_global = !estado_led_global;
                gpio_set_level(PIN_LED, estado_led_global);
                xSemaphoreGive(xMutexDados);
            }
            ESP_LOGI(TAG, "Botao pressionado. LED: %d", estado_led_global);
        }
    }
}

// Task de Leitura do Potenciômetro (ADC2)
void vTaskLeituraPotenciometro(void *pvParameters) {
    adc_oneshot_unit_handle_t adc2_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_2,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc2_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12, 
        .atten = ADC_ATTEN_DB_12,     
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, ADC_POT_CHANNEL, &config));

    int valor_lido = 0;
    while (1) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc2_handle, ADC_POT_CHANNEL, &valor_lido));
        
        // Protege a escrita da variável global
        if (xSemaphoreTake(xMutexDados, portMAX_DELAY)) {
            valor_potenciometro_global = valor_lido;
            xSemaphoreGive(xMutexDados);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Amostragem a cada 100ms
    }
}

// Task de Atualização do Display OLED via I2C
void vTaskAtualizaOLED(void *pvParameters) {
    SSD1306_t dev;

    // A lib pega SDA/SCL/RESET do menuconfig automaticamente.
    i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    ssd1306_init(&dev, 128, 64);

    ssd1306_clear_screen(&dev, false);
    ssd1306_contrast(&dev, 0xff);

    char buffer_led[32];
    char buffer_adc[32];
    int copia_led = 0, copia_pot = 0;

    while (1) {
        if (xSemaphoreTake(xMutexDados, portMAX_DELAY)) {
            copia_led = estado_led_global;
            copia_pot = valor_potenciometro_global;
            xSemaphoreGive(xMutexDados);
        }

        snprintf(buffer_led, sizeof(buffer_led), "LED: %s", copia_led ? "LIGADO" : "DESLIG");
        snprintf(buffer_adc, sizeof(buffer_adc), "ADC: %d", copia_pot);

        ssd1306_display_text(&dev, 0, buffer_led, strlen(buffer_led), false);
        ssd1306_display_text(&dev, 2, buffer_adc, strlen(buffer_adc), false);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void app_main(void) {
    // Inicialização do Mutex
    xMutexDados = xSemaphoreCreateMutex();

    // Configuração do LED
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << PIN_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_conf);

    // Configuração do Botão
    gpio_config_t botao_conf = {
        .pin_bit_mask = (1ULL << PIN_BOTAO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, 
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE 
    };
    gpio_config(&botao_conf);

    // Inicialização da Fila e Interrupções
    xBotaoQueue = xQueueCreate(5, sizeof(uint32_t));
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_BOTAO, botao_isr_handler, (void*) PIN_BOTAO);

    // Criação das Tasks
    xTaskCreate(vTaskTrataBotao, "Task_Botao", 2048, NULL, 10, NULL);
    xTaskCreate(vTaskLeituraPotenciometro, "Task_ADC", 2048, NULL, 5, NULL);
    xTaskCreate(vTaskAtualizaOLED, "Task_OLED", 4096, NULL, 3, NULL);
}