#include <stdio.h>
#include "pico/stdlib.h"
#include "unity/src/unity.h"
#include "include/temperatura.h"

void setUp(void) {}
void tearDown(void) {}

void test_adc_to_temperature_should_return_27C_for_voltage_0_706V(void) {
    uint16_t adc_simulado = (uint16_t)((0.706f * 4095.0f) / 3.3f);  // ≈ 875

    float temperatura = adc_to_temperature(adc_simulado);
    
    // Verificamos se está entre 26.9 e 27.1 (margem de erro)
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 27.0f, temperatura);
}

int main(void) {
    stdio_init_all();  // <== necessário para comunicação USB funcionar
    sleep_ms(2000);    // Aguarda USB inicializar

    while (true) {
        UNITY_BEGIN();
        RUN_TEST(test_adc_to_temperature_should_return_27C_for_voltage_0_706V);
        UNITY_END();

        sleep_ms(2000); // Espera 2 segundos antes do próximo teste
    }

    return 0;
}