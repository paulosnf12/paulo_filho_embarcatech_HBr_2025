#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

#define ADC_TEMPERATURE_CHANNEL 4  // Canal ADC do sensor de temperatura interno

// Converte valor do ADC para temperatura em graus Celsius
float adc_to_temperature(uint16_t adc_value) {
    const float conversion_factor = 3.3f / (1 << 12);  // 3.3V / 4096
    float voltage = adc_value * conversion_factor;
    float temperature = 27.0f - (voltage - 0.706f) / 0.001721f;
    return temperature;
}

int main() {
    stdio_init_all();              // Inicializa a comunicação serial
    adc_init();                    // Inicializa o ADC
    adc_set_temp_sensor_enabled(true);       // Habilita o sensor de temperatura
    adc_select_input(ADC_TEMPERATURE_CHANNEL);  // Seleciona o canal 4

    while (true) {
        uint16_t adc_value = adc_read();  // Lê o valor ADC
        float temperature_celsius = adc_to_temperature(adc_value);  // Converte para °C

        printf("Temperatura: %.2f °C\n", temperature_celsius);  // Exibe somente em Celsius

        sleep_ms(1000);  // Aguarda 1 segundo
    }

    return 0;
}