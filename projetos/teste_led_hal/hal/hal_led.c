#include "include/hal_led.h"
#include "include/led_embutido.h"

void hal_led_init(void) {
    led_embutido_init();
}

void hal_led_toggle(void) {
    static int estado = 0;
    estado = !estado;
    led_embutido_set(estado);
}