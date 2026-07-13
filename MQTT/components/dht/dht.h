/*
 * Driver para sensores DHT11 / DHT22 (AM2301) no ESP-IDF.
 *
 * API compativel com a utilizada em esp-idf-lib:
 *   dht_read_data(tipo, gpio, &umidade, &temperatura)
 * onde umidade e temperatura sao retornadas em decimos (x10),
 * ou seja, 235 -> 23.5 graus / 604 -> 60.4 %.
 */
#ifndef COMPONENTS_DHT_H_
#define COMPONENTS_DHT_H_

#include <stdint.h>
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Tipo do sensor.
 */
typedef enum {
    DHT_TYPE_DHT11 = 0,  //!< DHT11
    DHT_TYPE_AM2301,     //!< AM2301 / DHT21 / DHT22 / AM2302
    DHT_TYPE_SI7021      //!< Itead Si7021
} dht_sensor_type_t;

/**
 * @brief Le temperatura e umidade brutas (em decimos).
 *
 * @param sensor_type  Tipo do sensor.
 * @param pin          GPIO conectado ao pino de dados do sensor.
 * @param[out] humidity     Umidade em decimos de %RH (ex.: 604 => 60.4%).
 * @param[out] temperature  Temperatura em decimos de grau C (ex.: 235 => 23.5C).
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t dht_read_data(dht_sensor_type_t sensor_type, gpio_num_t pin,
        int16_t *humidity, int16_t *temperature);

/**
 * @brief Le temperatura e umidade ja convertidas para float.
 *
 * @param sensor_type  Tipo do sensor.
 * @param pin          GPIO conectado ao pino de dados do sensor.
 * @param[out] humidity     Umidade em %RH.
 * @param[out] temperature  Temperatura em graus C.
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t dht_read_float_data(dht_sensor_type_t sensor_type, gpio_num_t pin,
        float *humidity, float *temperature);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_DHT_H_ */
