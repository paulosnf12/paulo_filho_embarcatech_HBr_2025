#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"


const uint PWM_A = 8;            // Pino do PWM roda A
const uint PWM_B = 16;            // Pino do PWM roda B

const uint16_t PERIOD = 2000;   // Período do PWM (valor máximo do contador)
const float DIVIDER_PWM = 16.0; // Divisor fracional do clock para o PWM
uint16_t roda_ini = 0;       // Nível inicial do PWM (duty cycle)

void setup_pwm()
{
    uint slice_A;
    uint slice_B;

    gpio_set_function(PWM_A, GPIO_FUNC_PWM); // Configura o pino do driver para função PWM
    gpio_set_function(PWM_B, GPIO_FUNC_PWM); // Configura o pino do driver para função PWM

    slice_A = pwm_gpio_to_slice_num(PWM_A);    // Obtém o slice do PWM associado ao pino do driver
    slice_B = pwm_gpio_to_slice_num(PWM_B);    // Obtém o slice do PWM associado ao pino do driver

    pwm_set_clkdiv(slice_A, DIVIDER_PWM);    // Define o divisor de clock do PWM
    pwm_set_clkdiv(slice_B, DIVIDER_PWM);    // Define o divisor de clock do PWM

    pwm_set_wrap(slice_A, PERIOD);           // Configura o valor máximo do contador (período do PWM)
    pwm_set_wrap(slice_B, PERIOD);           // Configura o valor máximo do contador (período do PWM)

    pwm_set_gpio_level(PWM_A, roda_ini);    // Define o nível inicial do PWM para o pino da roda A
    pwm_set_gpio_level(PWM_B, roda_ini);    // Define o nível inicial do PWM para o pino da roda B

    pwm_set_enabled(slice_A, true);          // Habilita o PWM no slice correspondente
    pwm_set_enabled(slice_B, true);          // Habilita o PWM no slice correspondente
}

int main()
{


    // Configuração dos pinos dos botões como entrada com pull-up
    const uint BUTTON_A_PIN = 5;  // Botão A no GPIO 5
    const uint BUTTON_B_PIN = 6;  // Botão B no GPIO 6
    gpio_init(BUTTON_A_PIN);
    gpio_init(BUTTON_B_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
    gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_A_PIN);
    gpio_pull_up(BUTTON_B_PIN);

    // Configuração pinos Roda A

    const uint INA2_PIN = 9;  // Pino INA do motor A no GPIO 9
    const uint INA1_PIN = 4;  // Pino INB do motor A no GPIO 4
    const uint INB2_PIN = 19; // Pino INB do motor A no GPIO 19
    const uint INB1_PIN = 18;  // Pino INA do motor A no GPIO 18

    gpio_init(INA2_PIN);
    gpio_init(INA1_PIN);
    gpio_init(INB2_PIN);
    gpio_init(INB1_PIN);
    gpio_set_dir(INA2_PIN, GPIO_OUT);
    gpio_set_dir(INB2_PIN, GPIO_OUT);
    gpio_set_dir(INA1_PIN, GPIO_OUT);
    gpio_set_dir(INB1_PIN, GPIO_OUT);

    gpio_init(20);
    gpio_set_dir(20, GPIO_OUT); // Configura o pino 20 como saída (opcional, dependendo do uso)
    gpio_put(20, 1); // Inicializa o pino 20 com nível baixo (opcional, dependendo do uso)

    stdio_init_all(); // Inicializa o sistema padrão de I/O
    setup_pwm();      // Configura o PWM

    pwm_set_gpio_level(PWM_A, roda_ini); // Define o nível atual do PWM (duty cycle)
    pwm_set_gpio_level(PWM_B, roda_ini); // Define o nível atual do PWM (duty cycle)

    while (true)
    {

        // se o botão A for pressionado
        if (gpio_get(BUTTON_A_PIN) == 0)
        {
            printf("Botão A pressionado\n");
            gpio_put(INA1_PIN, 1); 
            gpio_put(INA2_PIN, 0);  
            gpio_put(INB1_PIN, 1);   
            gpio_put(INB2_PIN, 0);   
            roda_ini = 32000;         
            pwm_set_gpio_level(PWM_A, roda_ini); // Atualiza o nível do PWM para a roda A
        }
        // se o botão B for pressionado
        else if (gpio_get(BUTTON_B_PIN) == 0)
        {
            gpio_put(INA1_PIN, 0);   
            gpio_put(INA2_PIN, 0);  
            gpio_put(INB1_PIN, 0);   
            gpio_put(INB2_PIN, 0);   
            roda_ini = 0;         
            pwm_set_gpio_level(PWM_A, roda_ini); // Atualiza o nível do PWM para a roda A
        }
    }
}
