#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/binary_info.h"
#include "inc/ssd1306.h"

const uint I2C_SDA = 14;
const uint I2C_SCL = 15;
const uint BUTTON_A = 5;
const uint BUTTON_B = 6;

volatile bool restart_requested = false;
volatile int button_b_clicks = 0;
volatile bool countdown_active = false;
volatile int countdown = 0;

struct repeating_timer timer; // Estrutura do temporizador

// Função de callback do temporizador
bool timer_callback(struct repeating_timer *t) {
    if (countdown_active && countdown > 0) {
        countdown--; // Decrementa o contador a cada 1 segundo
    }
    return true; // Retorna true para continuar a chamada do temporizador
}

void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == BUTTON_A) {
        // Resetando contador e cliques diretamente na interrupção do Botão A
        countdown_active = true;  // Inicia a contagem regressiva
        countdown = 9;
        button_b_clicks = 0;
    } else if (gpio == BUTTON_B) {
        if (countdown_active) {
            button_b_clicks++;  // Incrementa o contador de cliques do botão B
        }
    }
}

int main() {
    stdio_init_all();

    // Inicializa o I2C
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicializa o display OLED
    ssd1306_init();

    // Área de renderização
    struct render_area frame_area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1
    };
    calculate_render_area_buffer_length(&frame_area);

    // Buffer do display
    uint8_t ssd[ssd1306_buffer_length];
    
    // Exibe a mensagem inicial
    memset(ssd, 0, sizeof(ssd));             // Limpa o buffer
    ssd1306_draw_string(ssd, 0, 0, "Aguardando A...");
    render_on_display(ssd, &frame_area);     // Renderiza a tela inicial

    // Inicializa botões
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);

    // Configura interrupções
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, gpio_callback);
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, gpio_callback);

    // Configura o temporizador que chama o callback a cada 1 segundo
    add_repeating_timer_ms(-1000, timer_callback, NULL, &timer);

    while (true) {
        if (countdown_active && countdown >= 0) {
            // Limpa o buffer do display
            memset(ssd, 0, sizeof(ssd));

            // Exibe o contador e os cliques
            char countdown_text[16];
            snprintf(countdown_text, sizeof(countdown_text), "Contador: %d", countdown);

            char clicks_text[16];
            snprintf(clicks_text, sizeof(clicks_text), "Cliques: %d", button_b_clicks);

            ssd1306_draw_string(ssd, 0, 0, countdown_text);
            ssd1306_draw_string(ssd, 0, 16, clicks_text);

            render_on_display(ssd, &frame_area);
        }

        if (countdown == 0 && countdown_active) {
            // Após a contagem, o sistema deve congelar
            countdown_active = false;

            // Exibe a mensagem final (0 no contador e a quantidade de cliques)
            memset(ssd, 0, sizeof(ssd));
            ssd1306_draw_string(ssd, 0, 0, "Contador: 0");
            char final_clicks_text[16];
            snprintf(final_clicks_text, sizeof(final_clicks_text), "Cliques: %d", button_b_clicks);
            ssd1306_draw_string(ssd, 0, 16, final_clicks_text);
            render_on_display(ssd, &frame_area);
        }
    }

    return 0;
}