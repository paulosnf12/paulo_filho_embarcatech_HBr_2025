#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "pico/multicore.h" 

// Definição dos pinos e constantes
#define I2C_SDA 14        // Pino SDA do I2C
#define I2C_SCL 15        // Pino SCL do I2C
#define BUTTON_A 5        // Botão A (Reset)
#define BUTTON_B 6        // Botão B (Iniciar/Pausar)
#define JOYSTICK_X 27     // Eixo X do joystick
#define JOYSTICK_Y 26     // Eixo Y do joystick
#define BUZZER_PIN 21     // Pino do buzzer
#define BUZZER_FREQUENCY 500 // Frequência do som do buzzer (Hz)
#define WIFI_SSID "nome"     // Substitua pelo nome da sua rede
#define WIFI_PASS "senha"    // Substitua pela senha da sua rede

// Delay para evitar múltiplos acionamentos indesejados dos botões
#define DEBOUNCE_DELAY_US 150000 // 150ms

// Delay para evitar leituras muito rápidas do joystick
#define JOYSTICK_MOVE_DELAY_US 150000 // 150ms

// Limiares para detectar movimento no joystick (baseado em 12 bits de ADC)
#define JOYSTICK_THRESHOLD_HIGH 3072 // 75% do ADC
#define JOYSTICK_THRESHOLD_LOW 1024  // 25% do ADC

// Variáveis de controle
volatile bool timer_running = false;  // Indica se o timer está rodando
volatile bool time_up = false;        // Indica se o tempo chegou a 00:00
volatile int selected_digit = 0;      // Dígito selecionado no display (0: Dezena de minutos, 1: Unidade de minutos, 2: Dezena de segundos, 3: Unidade de segundos)
volatile int minutes = 25, seconds = 0; // Tempo inicial padrão (25:00)
volatile int study_default_minutes = 25; // Tempo customizado de estudo (minutos)
volatile int study_default_seconds = 0;   // Tempo customizado do estudo (segundos)
volatile int rest_default_minutes = 5;   // Tempo customizado de descanso (minutos)
volatile int rest_default_seconds = 0;    // Tempo customizado do descanso (segundos)

// Controle de debounce e joystick
volatile uint64_t last_button_time = 0, last_joystick_move = 0; 

// Armazena a última sessão (true = estudo, false = descanso)
volatile bool last_session_was_study = true;

// Buffer para o display OLED
uint8_t ssd1306_buffer[ssd1306_buffer_length];

// Buffer para resposta HTTP
char http_response[8192];

// ----- Início Módulo Wifi 

// Função para criar uma resposta HTTP simples
void create_http_response() {
    snprintf(http_response, sizeof(http_response),
             "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
             "<!DOCTYPE html>"
             "<html lang=\"pt-br\">"
             "<head>"
             "<meta charset=\"UTF-8\">"
             "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
             "<title>Pomodoro Wi-Fi</title>"

             "<style>"
             "body { font-family: Arial, sans-serif; text-align: center; background-color: #f4f4f4; margin: 0; padding: 0; height: 100vh; width: 100vw; display: flex; justify-content: center; align-items: center; }"
             "h1 { color: #333; }"
             "h2 { color: #555; }"
             "button { background-color: #007BFF; color: white; border: none; padding: 10px 20px; margin: 10px; font-size: 16px; cursor: pointer; }"
             "button:hover { background-color: #0056b3; }"
             "input { padding: 8px; margin: 5px; font-size: 16px; width: 80px; text-align: center; }"
             ".container { background: white; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); width: 100%%; max-width: 600px; box-sizing: border-box; }"

             "@media (max-width: 768px) {"
             ".container { width: 90%%; padding: 15px; }"
             "input, button { font-size: 14px; width: 70%%; }"
             "h1, h2 { font-size: 18px; }"
             "}"

             "@media (max-width: 480px) {"
             "h1, h2 { font-size: 16px; }"
             "input, button { width: 100%%; font-size: 16px; }"
             "button { padding: 12px 24px; }"
             "}"
             "</style>"

             "<script>"
             "function sendTime(type) {"
             "  var minutes = document.getElementById(type + '_minutes').value || 0;"
             "  var seconds = document.getElementById(type + '_seconds').value || 0;"
             "  fetch('/set_time', {"
             "    method: 'POST',"
             "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},"
             "    body: type + '_minutes=' + minutes + '&' + type + '_seconds=' + seconds"
             "  });"
             "}"

             "function toggleMode() {"
             "  fetch('/toggle_mode', { method: 'POST' })"
             "    .then(response => response.text())"
             "    .then(() => location.reload());"
             "}"

             "function toggleTimer() {"
             "  fetch('/toggle_timer', { method: 'POST' })"
             "    .then(response => response.text())"
             "    .then(status => {"
             "      document.getElementById('start_pause_btn').innerText = status === 'running' ? 'Pausar' : 'Iniciar';"
             "    });"
             "}"

             "function resetTimer() {"
             "  fetch('/reset_timer', { method: 'POST' })"
             "    .then(() => location.reload());"
             "}"
             "</script>"

             "</head>"
             "<body>"
             "<div class=\"container\">"
             "<h1>Configuração do Pomodoro</h1>"
             "<h2>Modo Atual: %s</h2>"

             "<h2>Tempo de Estudo</h2>"
             "<input type=\"number\" id=\"study_minutes\" placeholder=\"Min\" min=\"0\" max=\"99\">"
             "<input type=\"number\" id=\"study_seconds\" placeholder=\"Seg\" min=\"0\" max=\"59\">"
             "<button onclick=\"sendTime('study')\">Salvar</button>"

             "<h2>Tempo de Descanso</h2>"
             "<input type=\"number\" id=\"rest_minutes\" placeholder=\"Min\" min=\"0\" max=\"99\">"
             "<input type=\"number\" id=\"rest_seconds\" placeholder=\"Seg\" min=\"0\" max=\"59\">"
             "<button onclick=\"sendTime('rest')\">Salvar</button>"

             "<h2>Controles</h2>"
             "<button onclick=\"toggleMode()\">Alternar Estudo/Descanso</button>"
             "<button id=\"start_pause_btn\" onclick=\"toggleTimer()\">%s</button>"
             "<button onclick=\"resetTimer()\">Resetar</button>"

             "</div>"
             "</body>"
             "</html>\r\n",
             last_session_was_study ? "Estudo" : "Descanso",
             timer_running ? "Pausar" : "Iniciar");
}

static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *request = (char *)p->payload;

    if (strstr(request, "POST /toggle_timer")) {
        timer_running = !timer_running;

        const char *status = timer_running ? "running" : "paused";
        tcp_write(tpcb, status, strlen(status), TCP_WRITE_FLAG_COPY);
    } 
    else if (strstr(request, "POST /reset_timer")) {
        timer_running = false;
        time_up = false;

        if (last_session_was_study) {
            minutes = study_default_minutes;
            seconds = study_default_seconds;
        } else {
            minutes = rest_default_minutes;
            seconds = rest_default_seconds;
        }

        tcp_write(tpcb, "Timer resetado", 13, TCP_WRITE_FLAG_COPY);
    } 
    else if (strstr(request, "POST /set_time")) {
        char *body = strstr(request, "\r\n\r\n");
        if (body) {
            body += 4;

            int new_minutes = -1, new_seconds = -1;
            char type[16] = {0};

            sscanf(body, "%15[^_]%*[_]minutes=%d&%*[^_]%*[_]seconds=%d", type, &new_minutes, &new_seconds);

            if (strcmp(type, "study") == 0) {
                if (new_minutes >= 0 && new_minutes <= 99) study_default_minutes = new_minutes;
                if (new_seconds >= 0 && new_seconds <= 59) study_default_seconds = new_seconds;
            } else if (strcmp(type, "rest") == 0) {
                if (new_minutes >= 0 && new_minutes <= 99) rest_default_minutes = new_minutes;
                if (new_seconds >= 0 && new_seconds <= 59) rest_default_seconds = new_seconds;
            }

            if (last_session_was_study) {
                minutes = study_default_minutes;
                seconds = study_default_seconds;
            } else {
                minutes = rest_default_minutes;
                seconds = rest_default_seconds;
            }
        }
    } 
    else if (strstr(request, "POST /toggle_mode")) {
        last_session_was_study = !last_session_was_study;

        if (last_session_was_study) {
            minutes = study_default_minutes;
            seconds = study_default_seconds;
        } else {
            minutes = rest_default_minutes;
            seconds = rest_default_seconds;
        }

        tcp_write(tpcb, "Modo alterado", 13, TCP_WRITE_FLAG_COPY);
    } 
    else {
        create_http_response();
        tcp_write(tpcb, http_response, strlen(http_response), TCP_WRITE_FLAG_COPY);
    }

    tcp_recved(tpcb, p->len);
    tcp_output(tpcb);
    tcp_close(tpcb);
    pbuf_free(p);

    return ERR_OK;
}

// Callback de conexão: associa o http_callback à conexão
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_callback);
    return ERR_OK;
}

// Função para iniciar o servidor HTTP
static void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        printf("Erro ao criar PCB\n");
        return;
    }

    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }

    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);

    printf("Servidor HTTP rodando na porta 80...\n");
}


// Função para rodar o Wi-Fi em paralelo no Core 1
void wifi_task() {
    if (cyw43_arch_init()) {
        printf("Erro ao inicializar o Wi-Fi\n");
        return;
    }
    cyw43_arch_enable_sta_mode();

    while (true) {
        printf("Tentando conectar ao Wi-Fi...\n");
        if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 5000) == 0) {
            printf("Wi-Fi conectado!\n");
            
            uint8_t *ip = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
            printf("Endereço IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);

            start_http_server(); // *NOVO: Inicia o servidor HTTP assim que conectar

            break;
        }

        printf("Falha na conexão, tentando novamente...\n");
        sleep_ms(2000); // Pequena espera antes de tentar de novo
    }

    while (true) {
        cyw43_arch_poll();  // Mantém a conexão Wi-Fi ativa

        static uint64_t last_print_time = 0;
        uint64_t now = time_us_64();

        // Print do IP a cada 30 segundos
        if (now - last_print_time >= 5 * 1000000) { 
            uint8_t *ip = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
            printf("[Wi-Fi] Endereço IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
            last_print_time = now;
        }

        sleep_ms(100);      // Pequena espera para evitar uso excessivo da CPU
    }
}



// Inicializa o PWM para o buzzer
void pwm_init_buzzer(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (BUZZER_FREQUENCY * 4096)); 
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(pin, 0); // Começa desligado
}

// Função que emite um beep com duração especificada
void beep(uint pin, uint duration_ms) {
    uint slice_num = pwm_gpio_to_slice_num(pin);
    
    pwm_set_gpio_level(pin, 2048); // Ativa o buzzer
    sleep_ms(duration_ms);
    pwm_set_gpio_level(pin, 0); // Desativa o buzzer
}


// Função de callback do timer (executada a cada 1 segundo)
bool timer_callback(struct repeating_timer *t) {
    if (timer_running) {
        if (seconds == 0) {
            if (minutes == 0) {
                timer_running = false;
                time_up = true;  // Tempo acabou
                create_http_response();  // *NOVO: Atualiza a aba do navegador
                //return false;  //desligar o timer?
            } else {
                minutes--;
                seconds = 59;
            }
        } else {
            seconds--;
        }
    }
    return true; // Mantém o timer ativo
}

// Função para leitura suavizada do ADC
uint smooth_adc_read(uint input) {
    uint sum = 0;
    const int samples = 8; // Média de 8 leituras para suavizar
    for (int i = 0; i < samples; i++) {
        adc_select_input(input);
        sum += adc_read();
        sleep_us(50); // Pequeno atraso para estabilizar a leitura
    }
    uint avg = sum / samples;
    //printf("ADC[%d]: %d\n", input, avg); //debug
    return avg;
}

// Função para desenhar um pixel no buffer
void ssd1306_draw_pixel(uint8_t *buffer, int x, int y, bool color) {
    if (x < 0 || x >= ssd1306_width || y < 0 || y >= ssd1306_height) return;
    int index = x + (y / 8) * ssd1306_width;
    if (color) {
        buffer[index] |= (1 << (y % 8));
    } else {
        buffer[index] &= ~(1 << (y % 8));
    }
}

// Bitmaps para números grandes (32x24 pixels)
const uint8_t large_digits[10][96] = {

    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03, 0xff, 0xfe, 
        0x01, 0xff, 0xfc, 0x00, 0xff, 0xf8, 0x00, 0x7f, 0xf0, 0x78, 0x7f, 0xf0, 0xf8, 0x3f, 0xf0, 0xfc, 
        0x3f, 0xf0, 0xfc, 0x3f, 0xf0, 0xfc, 0x3f, 0xe0, 0xfc, 0x3f, 0xe0, 0xfc, 0x3f, 0xe0, 0xfc, 0x3f, 
        0xe0, 0xfc, 0x3f, 0xe0, 0xfc, 0x3f, 0xf0, 0xfc, 0x3f, 0xf0, 0xfc, 0x3f, 0xf0, 0xfc, 0x3f, 0xf0, 
        0xf8, 0x3f, 0xf0, 0x78, 0x7f, 0xf8, 0x00, 0x7f, 0xfc, 0x00, 0xff, 0xfe, 0x01, 0xff, 0xff, 0x03, 
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, // 0

    
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
        0xc3, 0xff, 0xff, 0xc3, 0xff, 0xff, 0x83, 0xff, 0xff, 0x03, 0xff, 0xfe, 0x03, 0xff, 0xf8, 0x03, 
        0xff, 0xf0, 0x03, 0xff, 0xf0, 0x43, 0xff, 0xf1, 0xc3, 0xff, 0xf7, 0xc3, 0xff, 0xff, 0xc3, 0xff, 
        0xff, 0xc3, 0xff, 0xff, 0xc3, 0xff, 0xff, 0xc3, 0xff, 0xff, 0xc3, 0xff, 0xff, 0xc3, 0xff, 0xff, 
        0xc3, 0xff, 0xff, 0xc3, 0xff, 0xff, 0xc3, 0xff, 0xff, 0xc3, 0xff, 0xff, 0xc3, 0xff, 0xff, 0xc3, 
        0xff, 0xff, 0xc3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, // 1


    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x03, 0xff, 0xfc, 
        0x00, 0xff, 0xf0, 0x00, 0x7f, 0xf0, 0x00, 0x7f, 0xe0, 0xf8, 0x3f, 0xe1, 0xfc, 0x3f, 0xe1, 0xfc, 
        0x3f, 0xff, 0xfc, 0x3f, 0xff, 0xfc, 0x3f, 0xff, 0xf8, 0x7f, 0xff, 0xf0, 0x7f, 0xff, 0xe0, 0xff, 
        0xff, 0xc0, 0xff, 0xff, 0x81, 0xff, 0xff, 0x03, 0xff, 0xfe, 0x0f, 0xff, 0xfc, 0x1f, 0xff, 0xf8, 
        0x3f, 0xff, 0xf8, 0x7f, 0xff, 0xf0, 0x00, 0x3f, 0xf0, 0x00, 0x3f, 0xe0, 0x00, 0x3f, 0xe0, 0x00, 
        0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, // 2


    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
        0x03, 0xff, 0xfc, 0x00, 0xff, 0xfc, 0x00, 0x7f, 0xf8, 0x00, 0x7f, 0xf0, 0x78, 0x3f, 0xf0, 0xfc, 
        0x3f, 0xf8, 0xfc, 0x3f, 0xff, 0xfc, 0x3f, 0xff, 0xf8, 0x7f, 0xff, 0x80, 0xff, 0xff, 0x81, 0xff, 
        0xff, 0x80, 0x7f, 0xff, 0xf8, 0x3f, 0xff, 0xfc, 0x1f, 0xff, 0xfe, 0x1f, 0xff, 0xfe, 0x1f, 0xf0, 
        0xfe, 0x1f, 0xf0, 0x7c, 0x1f, 0xf0, 0x38, 0x3f, 0xf8, 0x00, 0x3f, 0xfc, 0x00, 0x7f, 0xfe, 0x00, 
        0xff, 0xff, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, // 3
    

    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
        0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xc0, 0xff, 0xff, 0x80, 0xff, 0xff, 0x80, 
        0xff, 0xff, 0x00, 0xff, 0xfe, 0x10, 0xff, 0xfe, 0x30, 0xff, 0xfc, 0x30, 0xff, 0xf8, 0x70, 0xff, 
        0xf8, 0xf0, 0xff, 0xf1, 0xf0, 0xff, 0xe1, 0xf0, 0xff, 0xe0, 0x00, 0x1f, 0xe0, 0x00, 0x1f, 0xe0, 
        0x00, 0x1f, 0xe0, 0x00, 0x1f, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 
        0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, // 4
    

    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 
        0x00, 0x7f, 0xf8, 0x00, 0x7f, 0xf0, 0x00, 0x7f, 0xf0, 0x00, 0x7f, 0xf0, 0xff, 0xff, 0xf0, 0xff, 
        0xff, 0xe1, 0xff, 0xff, 0xe1, 0x07, 0xff, 0xe0, 0x01, 0xff, 0xe0, 0x00, 0xff, 0xc0, 0x00, 0x7f, 
        0xc0, 0xf0, 0x7f, 0xe3, 0xf8, 0x3f, 0xff, 0xfc, 0x3f, 0xff, 0xfc, 0x3f, 0xff, 0xfc, 0x3f, 0xc3, 
        0xfc, 0x3f, 0xc1, 0xf8, 0x3f, 0xe0, 0xf0, 0x7f, 0xe0, 0x00, 0x7f, 0xf0, 0x00, 0xff, 0xf8, 0x01, 
        0xff, 0xfe, 0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, // 5


    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
        0x83, 0xff, 0xfe, 0x00, 0xff, 0xfc, 0x00, 0x7f, 0xf8, 0x00, 0x7f, 0xf0, 0x78, 0x3f, 0xf0, 0xfc, 
        0x3f, 0xf0, 0xfc, 0x3f, 0xe1, 0xff, 0xff, 0xe1, 0xff, 0xff, 0xe1, 0x83, 0xff, 0xe0, 0x01, 0xff, 
        0xe0, 0x00, 0xff, 0xe0, 0x00, 0x7f, 0xe0, 0xf8, 0x3f, 0xe1, 0xfc, 0x3f, 0xe1, 0xfc, 0x3f, 0xe1, 
        0xfc, 0x3f, 0xf1, 0xfc, 0x3f, 0xf0, 0x78, 0x3f, 0xf8, 0x00, 0x7f, 0xf8, 0x00, 0xff, 0xfc, 0x01, 
        0xff, 0xff, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, // 6


    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
        0xff, 0xff, 0xf0, 0x00, 0x1f, 0xf0, 0x00, 0x1f, 0xf0, 0x00, 0x1f, 0xf0, 0x00, 0x1f, 0xff, 0xf8, 
        0x3f, 0xff, 0xf8, 0x7f, 0xff, 0xf0, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xe1, 0xff, 0xff, 0xc3, 0xff, 
        0xff, 0xc3, 0xff, 0xff, 0xc3, 0xff, 0xff, 0x87, 0xff, 0xff, 0x87, 0xff, 0xff, 0x0f, 0xff, 0xff, 
        0x0f, 0xff, 0xff, 0x0f, 0xff, 0xff, 0x0f, 0xff, 0xfe, 0x1f, 0xff, 0xfe, 0x1f, 0xff, 0xfe, 0x1f, 
        0xff, 0xfe, 0x1f, 0xff, 0xfe, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, // 7


    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 
        0x03, 0xff, 0xf8, 0x00, 0xff, 0xf0, 0x00, 0x7f, 0xf0, 0x70, 0x7f, 0xe0, 0xf8, 0x3f, 0xe1, 0xfc, 
        0x3f, 0xe1, 0xfc, 0x3f, 0xe1, 0xfc, 0x3f, 0xe0, 0xf8, 0x3f, 0xf0, 0x70, 0x7f, 0xf8, 0x00, 0xff, 
        0xfc, 0x01, 0xff, 0xf8, 0x00, 0xff, 0xf0, 0xf0, 0x7f, 0xe0, 0xf8, 0x3f, 0xe1, 0xfc, 0x3f, 0xe1, 
        0xfc, 0x3f, 0xe1, 0xfc, 0x3f, 0xe0, 0xf8, 0x3f, 0xf0, 0x70, 0x7f, 0xf0, 0x00, 0x7f, 0xf8, 0x00, 
        0xff, 0xfe, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, // 8


    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 
        0x07, 0xff, 0xfc, 0x01, 0xff, 0xf8, 0x00, 0xff, 0xf0, 0x00, 0xff, 0xe0, 0xf0, 0x7f, 0xe1, 0xfc, 
        0x7f, 0xe1, 0xfc, 0x3f, 0xe1, 0xfc, 0x3f, 0xe1, 0xfc, 0x3f, 0xe0, 0xf8, 0x3f, 0xf0, 0x00, 0x3f, 
        0xf8, 0x00, 0x3f, 0xfc, 0x00, 0x3f, 0xfe, 0x0c, 0x3f, 0xff, 0xfc, 0x3f, 0xff, 0xfc, 0x3f, 0xe1, 
        0xf8, 0x7f, 0xe1, 0xf8, 0x7f, 0xe0, 0xf0, 0x7f, 0xf0, 0x00, 0xff, 0xf0, 0x01, 0xff, 0xf8, 0x03, 
        0xff, 0xfe, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}  // 9
};

// Função para desenhar um número grande (32x24 pixels)
void draw_large_digit(uint8_t *buffer, int x, int y, int digit) {
    if (digit < 0 || digit > 9) return;
    const uint8_t *bitmap = large_digits[digit]; // Obtém o bitmap do número

    for (int i = 0; i < 32; i++) {  // Percorre as linhas (altura do número)
        for (int j = 0; j < 24; j++) {  // Percorre as colunas (largura do número)
            int byte_index = (i * 3) + (j / 8);  // Calcula a posição correta no array
            int bit_pos = 7 - (j % 8);  // Obtém o bit correto dentro do byte

            if (bitmap[byte_index] & (1 << bit_pos)) {
                ssd1306_draw_pixel(buffer, x + j, y + i, true); // Desenha o pixel
            }
        }
    }
}

void gpio_callback(uint gpio, uint32_t events) {

    // Tempo para evitar múltiplos pressionamentos (debounce)
    uint64_t now = time_us_64();
    if (now - last_button_time > DEBOUNCE_DELAY_US) {
        last_button_time = now;

        if (gpio == BUTTON_A) {
            if (minutes == 0 && seconds == 0) {  
                // Alterna entre estudo e descanso com base na última sessão (se o tempo chegou a 00:00)
                if (last_session_was_study) {
                    minutes = rest_default_minutes;
                    seconds = rest_default_seconds;
                } else {
                    minutes = study_default_minutes;
                    seconds = study_default_seconds;
                }
                last_session_was_study = !last_session_was_study; // Alterna a sessão
            } else {  
                // Se o timer estiver rodando, reseta para a última sessão
                if (timer_running) {
                    if (last_session_was_study) {
                        minutes = study_default_minutes;
                        seconds = study_default_seconds;
                    } else {
                        minutes = rest_default_minutes;
                        seconds = rest_default_seconds;
                    }
                } else { // not running
                    if (minutes == study_default_minutes && seconds == study_default_seconds) { 
                        minutes = rest_default_minutes;
                        seconds = rest_default_seconds;
                        last_session_was_study = !last_session_was_study;
                    } else if (minutes == rest_default_minutes && seconds == rest_default_seconds) { 
                        minutes = study_default_minutes;
                        seconds = study_default_seconds;
                        last_session_was_study = !last_session_was_study;
                    } else if (last_session_was_study) {
                        minutes = study_default_minutes; 
                        seconds = study_default_seconds;
                    } else {
                        minutes = rest_default_minutes; 
                        seconds = rest_default_seconds;
                    }
                }
            }
            timer_running = false;
            time_up = false; 
            create_http_response();  // *NOVO: Atualiza a aba no navegador 
        } else if (gpio == BUTTON_B) {
            if (time_up) {
                time_up = false; 
                create_http_response();  // *NOVO: Atualiza a aba no navegador 
            } else {
                timer_running = !timer_running; 
            }
        }
    }
}

// Atualiza o display com os números grandes
void update_display(struct render_area *frame_area, volatile int minutes, volatile int seconds) {
    memset(ssd1306_buffer, 0, ssd1306_buffer_length);

    // Posições X dos dígitos
    int positions[4] = {8, 40, 75, 107};

    draw_large_digit(ssd1306_buffer, 8, 20, minutes / 10);
    draw_large_digit(ssd1306_buffer, 40, 20, minutes % 10);
    draw_large_digit(ssd1306_buffer, 75, 20, seconds / 10);
    draw_large_digit(ssd1306_buffer, 107, 20, seconds % 10);

     // Desenha um sublinhado abaixo do número selecionado
     int underline_x = positions[selected_digit]; // Posição do dígito selecionado
     int underline_y = 16; // Linha abaixo do número --- (56 em baixo e 16 em cima)
     for (int i = 0; i < 24; i++) { // Largura do traço
         ssd1306_draw_pixel(ssd1306_buffer, underline_x + i, underline_y, true);
     }   

    render_on_display(ssd1306_buffer, frame_area);
}

// Processa os comandos do joystick para ajustar o tempo
void process_joystick() {
    // Se o timer estiver rodando, evita leituras desnecessarias do joystick
    if (timer_running) return;
    
    uint64_t now = time_us_64();

    // Evita leituras muito rápidas do joystick (limite de 150ms entre movimentos)
    if (now - last_joystick_move < JOYSTICK_MOVE_DELAY_US) return;
    
    // Lê os valores analógicos do joystick (eixos X e Y)
    uint adc_x_raw = smooth_adc_read(1);
    uint adc_y_raw = smooth_adc_read(0);
    bool move_detected = false;

    // Movimentação no eixo X: Alterna entre os dígitos [prioridade alta]
    if (adc_x_raw > JOYSTICK_THRESHOLD_HIGH) {  
        selected_digit = (selected_digit + 1) % 4; // Avança para o próximo dígito
        move_detected = true;
    } else if (adc_x_raw < JOYSTICK_THRESHOLD_LOW) {  
        selected_digit = (selected_digit - 1 + 4) % 4; // Volta para o dígito anterior
        move_detected = true;
    } 
    
    // Movimentação no eixo Y: Aumenta ou diminui o valor do dígito selecionado [caso o eixo X não tenha sido acionado]
    else if (adc_y_raw > JOYSTICK_THRESHOLD_HIGH || adc_y_raw < JOYSTICK_THRESHOLD_LOW) {

        int delta = (adc_y_raw > JOYSTICK_THRESHOLD_HIGH) ? 1 : -1; // Se Y acima do limite, delta = 1, caso contrário delta = -1 (indica soma ou subtração dos dígitos)

        if (selected_digit == 0) { // Dezena minutos (0 a 9)
            minutes = (minutes + 10 * delta + 100) % 100;     

        } else if (selected_digit == 1) { // Unidade minutos (0 a 9)
            int tens = (minutes / 10) * 10;  
            int units = (minutes % 10 + delta + 10) % 10;  
            minutes = tens + units;                            

        } else if (selected_digit == 2) { // Dezena segundos (0 a 5)
            seconds = (seconds + 10 * delta + 60) % 60;       

        } else if (selected_digit == 3) { // Unidade segundos (0 a 9)
            int tens = (seconds / 10) * 10;  
            int units = (seconds % 10 + delta + 10) % 10;     
            seconds = tens + units;
        }
        
        move_detected = true; // Movimento detectado

        // Atualiza o tempo padrão dependendo da sessão
        if (!timer_running) {  
            if (last_session_was_study) {
                study_default_minutes = minutes;  
                study_default_seconds = seconds;
            } else {
                rest_default_minutes = minutes;  
                rest_default_seconds = seconds;
            }
        }

    }

    // Atualização do tempo de movimento para evitar leituras muito rápidas
    if (move_detected) last_joystick_move = now;
}



int main() {

    stdio_init_all(); // Inicialização de entradas e saída padrão

    // *NOVO: Inicia o Wi-Fi em paralelo no Core 1
    multicore_launch_core1(wifi_task);

    pwm_init_buzzer(BUZZER_PIN); // Configuração PWM buzzer
    struct repeating_timer timer; // Estrutura do temporizador
    add_repeating_timer_ms(-1000, timer_callback, NULL, &timer); // Configuração de um timer que chamará a função timer_callback a cada 1 segundo por decremento de tempo (-1 segundo // -1000)

    // Inicializa o I2C e o display
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init();

     // Inicializa o ADC para o joystick
     adc_init();
     adc_gpio_init(JOYSTICK_X);
     adc_gpio_init(JOYSTICK_Y);   

    // Configura os botões como entrada com pull-up
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);

    // Registra a função de callback para ambos os botões
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_RISE, true, gpio_callback);
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_RISE, true, gpio_callback);

    // Inicializa a área do display
    struct render_area frame_area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&frame_area);

    while (true) {
        process_joystick(&minutes, &seconds);
        update_display(&frame_area, minutes, seconds);

        if (time_up) {  
            static uint64_t last_beep_time = 0;
            uint64_t now = time_us_64();
        
            if (now - last_beep_time >= 2000000) { // A cada 2 segundos (2.000.000 µs)
                beep(BUZZER_PIN, 1000); //1000ms de duração
                last_beep_time = now;
            }
        }
    }

    return 0;
}