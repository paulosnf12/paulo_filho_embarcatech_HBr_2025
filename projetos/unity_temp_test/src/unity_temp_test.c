#include "include/temperatura.h"

float adc_to_temperature(uint16_t adc_value) {
    const float conversion_factor = 3.3f / 4096.0f;
    float voltage = adc_value * conversion_factor;
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}