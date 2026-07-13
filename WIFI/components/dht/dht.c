/*
 * Driver bit-bang para sensores DHT11 / DHT22 no ESP-IDF.
 *
 * O protocolo do DHT e sensivel a temporizacao (microssegundos), por isso a
 * leitura dos 40 bits e feita dentro de uma secao critica (interrupcoes
 * desabilitadas) para evitar jitter do escalonador do FreeRTOS.
 */
#include "dht.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

static const char *TAG = "dht";

// Passo do "polling" ao aguardar uma transicao de nivel (us)
#define DHT_TIMER_INTERVAL   2
// Timeout maximo por fase do protocolo (us)
#define DHT_DATA_BIT_TIMEOUT 90

// Portao para a secao critica (timing sensivel)
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

/**
 * Aguarda ate o pino atingir @p expected_pin_state, com timeout.
 * Em caso de sucesso opcionalmente devolve a duracao decorrida (us).
 */
static esp_err_t dht_await_pin_state(gpio_num_t pin, uint32_t timeout,
        int expected_pin_state, uint32_t *duration)
{
    for (uint32_t i = 0; i < timeout; i += DHT_TIMER_INTERVAL) {
        // Pequeno atraso ANTES da leitura estabiliza a amostragem
        esp_rom_delay_us(DHT_TIMER_INTERVAL);
        if (gpio_get_level(pin) == expected_pin_state) {
            if (duration) {
                *duration = i;
            }
            return ESP_OK;
        }
    }
    return ESP_ERR_TIMEOUT;
}

/**
 * Envia o pulso de start e captura os 40 bits do sensor em @p data (5 bytes).
 * Deve ser chamada dentro da secao critica.
 */
static esp_err_t dht_fetch_data(dht_sensor_type_t sensor_type, gpio_num_t pin,
        uint8_t data[5])
{
    uint32_t low_duration;
    uint32_t high_duration;

    // --- Pulso de start: MCU segura a linha em nivel baixo ---
    // Usamos INPUT_OUTPUT_OD (open-drain COM entrada habilitada). Isto e
    // essencial: em GPIO_MODE_OUTPUT_OD o buffer de entrada fica desligado e
    // gpio_get_level() nao consegue ler a resposta do sensor.
    gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_level(pin, 0);
    // DHT11 precisa de >= 18ms; AM2301 aceita ~1ms
    esp_rom_delay_us(sensor_type == DHT_TYPE_DHT11 ? 20000 : 1100);

    // Solta a linha (pull-up leva a nivel alto) e passa a escutar
    gpio_set_level(pin, 1);

    // ATENCAO: esta funcao roda dentro de uma secao critica (interrupcoes
    // desabilitadas). NAO chame ESP_LOG / printf aqui - isso tenta adquirir
    // um lock e causa abort(). Em caso de erro apenas retornamos o codigo; o
    // log e feito pelo chamador, fora da secao critica.

    // --- Resposta do sensor: ~80us baixo, depois ~80us alto ---
    if (dht_await_pin_state(pin, 40, 0, NULL) != ESP_OK) {
        return ESP_ERR_TIMEOUT;
    }
    if (dht_await_pin_state(pin, 88, 1, NULL) != ESP_OK) {
        return ESP_ERR_TIMEOUT;
    }
    if (dht_await_pin_state(pin, 88, 0, NULL) != ESP_OK) {
        return ESP_ERR_TIMEOUT;
    }

    // --- Leitura dos 40 bits ---
    for (int b = 0; b < 40; b++) {
        // Cada bit comeca com ~50us em nivel baixo
        if (dht_await_pin_state(pin, DHT_DATA_BIT_TIMEOUT, 1, &low_duration) != ESP_OK) {
            return ESP_ERR_TIMEOUT;
        }
        // ... seguido de nivel alto: curto (~26-28us) = 0, longo (~70us) = 1
        if (dht_await_pin_state(pin, DHT_DATA_BIT_TIMEOUT, 0, &high_duration) != ESP_OK) {
            return ESP_ERR_TIMEOUT;
        }

        uint8_t bit = high_duration > low_duration ? 1 : 0;
        data[b / 8] <<= 1;
        data[b / 8] |= bit;
    }

    return ESP_OK;
}

esp_err_t dht_read_data(dht_sensor_type_t sensor_type, gpio_num_t pin,
        int16_t *humidity, int16_t *temperature)
{
    uint8_t data[5] = { 0, 0, 0, 0, 0 };

    // Estado ocioso da linha: nivel alto (open-drain + pull-up).
    // O pull-up interno (~45k) ajuda quando nao ha resistor externo, mas o
    // ideal continua sendo um pull-up externo de 4.7k a 10k para o DHT11.
    gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
    gpio_set_level(pin, 1);
    // Estabiliza a linha antes de iniciar (sensor precisa estar em repouso alto)
    esp_rom_delay_us(50);

    portENTER_CRITICAL(&mux);
    esp_err_t result = dht_fetch_data(sensor_type, pin, data);
    // Reconfigura a linha como saida em nivel alto (ociosa)
    if (result == ESP_OK) {
        gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT_OD);
        gpio_set_level(pin, 1);
    }
    portEXIT_CRITICAL(&mux);

    if (result != ESP_OK) {
        return result;
    }

    // Verificacao de checksum
    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
        ESP_LOGE(TAG, "Falha de checksum na leitura do sensor");
        return ESP_ERR_INVALID_CRC;
    }

    // Conversao para decimos, conforme o tipo de sensor
    if (sensor_type == DHT_TYPE_DHT11) {
        // DHT11: byte inteiro (%RH e graus C), decimal desprezivel
        *humidity = data[0] * 10;
        *temperature = data[2] * 10;
    } else {
        // DHT22 / AM2301: 16 bits (decimos)
        *humidity = ((int16_t)data[0] << 8) | data[1];

        int16_t t = ((int16_t)(data[2] & 0x7F) << 8) | data[3];
        // Bit mais significativo de data[2] indica temperatura negativa
        *temperature = (data[2] & 0x80) ? -t : t;
    }

    return ESP_OK;
}

esp_err_t dht_read_float_data(dht_sensor_type_t sensor_type, gpio_num_t pin,
        float *humidity, float *temperature)
{
    int16_t i_humidity = 0;
    int16_t i_temperature = 0;

    esp_err_t result = dht_read_data(sensor_type, pin, &i_humidity, &i_temperature);
    if (result != ESP_OK) {
        return result;
    }

    *humidity = i_humidity / 10.0f;
    *temperature = i_temperature / 10.0f;

    return ESP_OK;
}
