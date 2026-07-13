#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "MAIN_APP";

// Definições de Hardware
#define PIN_LED          2   // D2 (GPIO 2)
#define PIN_BOTAO        15  // D15 (GPIO 15)
#define ADC_POT_CHANNEL  ADC_CHANNEL_0 // Refere-se à GPIO 4 (ADC2_CH0)

// Handlers do FreeRTOS
static QueueHandle_t xBotaoQueue = NULL;

// Variável estática para armazenar o estado do LED
static int estado_led = 0;

// 1. Rotina de Serviço de Interrupção (ISR)
static void IRAM_ATTR botao_isr_handler(void* arg) {
    uint32_t pino = (uint32_t) arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Envia o número do pino pela fila de forma não-bloqueante específica para ISR
    xQueueSendFromISR(xBotaoQueue, &pino, &xHigherPriorityTaskWoken);

    // Se necessário, força o escalonador a alternar para a task de maior prioridade
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// 2. Tasks do Sistema

// Task responsável por processar os eventos enviados pela ISR do botão
void vTaskTrataBotao(void *pvParameters) {
    uint32_t pino_recebido;
    ESP_LOGI(TAG, "Task de tratamento do botão iniciada.");

    while (1) {
        // Fica bloqueada aguardando indefinidamente (portMAX_DELAY) por um dado na fila
        if (xQueueReceive(xBotaoQueue, &pino_recebido, portMAX_DELAY) == pdTRUE) {
            // Inverte o estado do LED
            estado_led = !estado_led;
            gpio_set_level(PIN_LED, estado_led);

            // Loga o evento conforme solicitado
            ESP_LOGI(TAG, "Interrupcao disparada no pino GPIO %lu! LED -> %s", 
                     pino_recebido, estado_led ? "LIGADO" : "DESLIGADO");
        }
    }
}

// Task responsável por ler periodicamente o Potenciômetro via ADC
void vTaskLeituraPotenciometro(void *pvParameters) {
    // Configuração do ADC2 (Ajustado para sintaxe do ESP-IDF v6.x)
    adc_oneshot_unit_handle_t adc2_handle;
    adc_oneshot_unit_init_cfg_t init_config2 = {
        .unit_id = ADC_UNIT_2,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config2, &adc2_handle));

    // Configuração do canal específico do ADC (Ajustado para sintaxe do ESP-IDF v6.x)
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12, 
        .atten = ADC_ATTEN_DB_12,     
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, ADC_POT_CHANNEL, &config));

    int valor_adc = 0;
    ESP_LOGI(TAG, "Task do potenciometro iniciada.");

    while (1) {
        // Realiza a leitura crua (raw) do canal analógico
        ESP_ERROR_CHECK(adc_oneshot_read(adc2_handle, ADC_POT_CHANNEL, &valor_adc));
        
        // Exibe o valor no monitor serial
        ESP_LOGI(TAG, "Valor do Potenciometro (ADC Raw): %d", valor_adc);

        // Aguarda 1 segundo antes da próxima leitura
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// 3. Inicialização Principal (app_main)
void app_main(void) {
    ESP_LOGI(TAG, "Inicializando perifericos e tasks...");

    // 1. Configuração do LED como Saída Digital
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << PIN_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_conf);
    gpio_set_level(PIN_LED, estado_led);

    // 2. Configuração do Botão como Entrada com Interrupção na Borda de Descida (NEGEDGE)
    gpio_config_t botao_conf = {
        .pin_bit_mask = (1ULL << PIN_BOTAO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, 
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE 
    };
    gpio_config(&botao_conf);

    // 3. Criação da Fila (Queue) para armazenar até 5 eventos do botão
    xBotaoQueue = xQueueCreate(5, sizeof(uint32_t));
    if (xBotaoQueue == NULL) {
        ESP_LOGE(TAG, "Falha ao criar a fila do botao.");
        return;
    }

    // 4. Instalação do Serviço Global de Interrupção de GPIO e adição do Handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_BOTAO, botao_isr_handler, (void*) PIN_BOTAO);

    // 5. Criação das Tasks no FreeRTOS
    xTaskCreate(vTaskTrataBotao, "Task_Trata_Botao", 2048, NULL, 10, NULL);
    xTaskCreate(vTaskLeituraPotenciometro, "Task_Potenciometro", 2048, NULL, 5, NULL);

    ESP_LOGI(TAG, "Sistema pronto e rodando!");
}