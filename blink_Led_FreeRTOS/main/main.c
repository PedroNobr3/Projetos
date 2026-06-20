#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "MAIN_APP";

// ====================================================================
// CONFIGURAÇÃO GENÉRICA: adicione ou remova quantos pinos quiser aqui
// ====================================================================
const int pinos_leds[] = {12, 14, 2}; 
#define NUM_LEDS (sizeof(pinos_leds) / sizeof(pinos_leds[0]))
#define PIN_LED 12
    
// Array global para armazenar o "controle remoto" (Handle) de cada task
TaskHandle_t xHandlesLeds[NUM_LEDS];

// Protótipo da Task Genérica
void vTaskLedSequencial(void *pvParameters);
void vTaskBlinkLED(void *pvParameters);

void app_main(void)
{
    //Blink de um LED
    ESP_LOGI(TAG, "Inicializando o firmware do entregavel...");

    // Criação da Task explícita usando as APIs do FreeRTOS
    xTaskCreate(
        vTaskBlinkLED,      // Função com a lógica do Blink
        "Blink_Task",       // Nome da task para rastreamento/debug
        2048,               // Tamanho da Stack em Bytes
        NULL,               // Sem parâmetros de entrada
        1,                  // Prioridade da tarefa
        NULL                // Sem necessidade de handle externo
    );

    ESP_LOGI(TAG, "Task criada com sucesso! Finalizando o escopo da app_main.");
    

   //Blink de vários LEDs em sequência
    // ESP_LOGI(TAG, "Inicializando sequenciador generico para %d LEDs...", NUM_LEDS);

    // // 1. Criamos todas as tasks na memória
    // for (int i = 0; i < NUM_LEDS; i++) {
    //     // Usamos o próprio índice 'i' mapeado como ponteiro para a task saber sua posição
    //     xTaskCreate(
    //         vTaskLedSequencial,
    //         "Task_Led_Seq",
    //         2048,
    //         (void *)i, // Passa a posição dela no array (0, 1, 2...)
    //         1,
    //         &xHandlesLeds[i] // Guarda o Handle dela para podermos notificá-la depois
    //     );
    // }

    // // 2. O sistema começa liberando a primeira task da fila (Índice 0)
    // xTaskNotifyGive(xHandlesLeds[0]);
}

// void vTaskLedSequencial(void *pvParameters)
// {
//     // Recupera qual o índice desta task específica
//     int meu_indice = (int)pvParameters;
//     int meu_pino = pinos_leds[meu_indice];

//     // Calcula automaticamente quem é a próxima task da fila (rotaciona ao chegar no fim)
//     int proximo_indice = (meu_indice + 1) % NUM_LEDS;

//     // Configura o pino correspondente
//     gpio_reset_pin(meu_pino);
//     gpio_set_direction(meu_pino, GPIO_MODE_OUTPUT);
//     gpio_set_level(meu_pino, 0); // Começa desligado

//     while (1) {
//         // A task fica Bloqueada (dormindo) aqui até receber o sinal de "sua vez"
//         // ulTaskNotifyTake pausa a task gastando 0% de CPU enquanto espera
//         ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

//         // --- SUA VEZ DE RODAR ---
//         ESP_LOGI(TAG, "LED no pino %d ligado.", meu_pino);
//         gpio_set_level(meu_pino, 1);
        
//         // Fica ligado por 1 segundo
//         vTaskDelay(pdMS_TO_TICKS(1000));

//         // Desliga
//         gpio_set_level(meu_pino, 0);
//         ESP_LOGI(TAG, "LED no pino %d desligado.", meu_pino);

//         // Passa o bastão para a próxima task da fila
//         xTaskNotifyGive(xHandlesLeds[proximo_indice]);
//     }
// }

void vTaskBlinkLED(void *pvParameters)
{
    // Configuração do pino 12 como saída digital
    gpio_reset_pin(PIN_LED);
    gpio_set_direction(PIN_LED, GPIO_MODE_OUTPUT);

    uint8_t estado_led = 0;

    ESP_LOGI(TAG, "Hardware do pino %d configurado. Iniciando loop infinito...", PIN_LED);

    while (1) {
        // Inversão do estado lógico
        estado_led = !estado_led;
        gpio_set_level(PIN_LED, estado_led);

        // Uso do ESP_LOGI para monitorar o comportamento do LED em tempo real
        if (estado_led) {
            ESP_LOGI(TAG, "LED no pino %d -> LIGADO", PIN_LED);
        } else {
            ESP_LOGI(TAG, "LED no pino %d -> DESLIGADO", PIN_LED);
        }

        // Bloqueio não-bloqueante de 1000ms (1 segundo) liberando a CPU
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
    