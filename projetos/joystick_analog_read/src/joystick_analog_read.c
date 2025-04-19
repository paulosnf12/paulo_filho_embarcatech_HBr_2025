#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

#define NUM_SAMPLES 10  // Número de amostras para média

// Função para obter média de N leituras do ADC de um canal específico
uint16_t read_adc_average(uint channel) {
    uint32_t sum = 0;
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        adc_select_input(channel);
        sum += adc_read();
        sleep_us(500); // Pequeno atraso para estabilidade
    }
    return sum / NUM_SAMPLES;
}

int main() {
    stdio_init_all();
    adc_init();

    // Inicializa GPIOs do ADC usados pelo joystick
    adc_gpio_init(26);  // Canal 0 (eixo Y)
    adc_gpio_init(27);  // Canal 1 (eixo X)

    while (1) {
        uint16_t adc_y = read_adc_average(0);  // Canal 0 → Y
        uint16_t adc_x = read_adc_average(1);  // Canal 1 → X

        // Exibe os valores convertidos com suavização
        printf("Joystick ADC - X: %4d  |  Y: %4d\n", adc_x, adc_y);

        sleep_ms(200);  // Atraso entre as leituras
    }

    return 0;
}