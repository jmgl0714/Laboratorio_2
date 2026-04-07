// Juego de reaccion

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_rom_sys.h"

// Dimensiones de la matriz
#define NUM_ROWS        4
#define NUM_COLS        4

// Polaridad de activacion 
#define COL_ACTIVE      1
#define COL_INACTIVE    0
#define ROW_ACTIVE      0
#define ROW_INACTIVE    1

// Botones
#define BUTTON_PIN          GPIO_NUM_0   // boton de reaccion
#define BUTTON_PAUSE_PIN    GPIO_NUM_18  // boton de pausa
#define BUTTON_ACTIVE       0            

// Tiempos en microsegundos
#define REFRESH_US      1000
#define DEBOUNCE_US     30000
#define GREEN_TIME_US   800000
#define WAIT_MIN_US     700000
#define WAIT_MAX_US     5000000

// Pines de columnas 
gpio_num_t col_pins[NUM_COLS] = {
    GPIO_NUM_13,
    GPIO_NUM_12,
    GPIO_NUM_14,
    GPIO_NUM_27
};

// Pines de filas ROJAS 
gpio_num_t red_row_pins[NUM_ROWS] = {
    GPIO_NUM_26,
    GPIO_NUM_25,
    GPIO_NUM_33,
    GPIO_NUM_32
};

// Pines de filas VERDES
gpio_num_t green_row_pins[NUM_ROWS] = {
    GPIO_NUM_23,
    GPIO_NUM_22,
    GPIO_NUM_21,
    GPIO_NUM_19
};

// Estados del juego
typedef enum {
    WAIT_TARGET,
    SHOW_RESULT,
    WAIT_DELAY,
    PAUSED
} game_state_t;

static uint8_t red_frame[NUM_ROWS];
static uint8_t green_frame[NUM_ROWS];

// Variables de estado global
static game_state_t state            = WAIT_TARGET;
static game_state_t state_before_pause = WAIT_TARGET;
static int          target_row       = 0;
static int          target_col       = 0;
static int64_t      start_time       = 0;
static int64_t      state_time       = 0;
static int64_t      wait_dur_us      = 0;

void    gpio_init_all(void);
void    clear_frames(void);
void    all_off(void);
void    refresh_display(void);
void    new_target(void);
static int64_t random_wait_us(void);

void gpio_init_all(void)
{
    gpio_config_t out_cfg = {
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
        .pin_bit_mask = 0
    };

    for (int i = 0; i < NUM_COLS; i++) {
        out_cfg.pin_bit_mask |= (1ULL << col_pins[i]);
    }

    for (int i = 0; i < NUM_ROWS; i++) {
        out_cfg.pin_bit_mask |= (1ULL << red_row_pins[i]);
        out_cfg.pin_bit_mask |= (1ULL << green_row_pins[i]);
    }

    gpio_config(&out_cfg);

    // boton de reaccion y boton de pausa ambos con pull-up interno
    gpio_config_t in_cfg = {
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
        .pin_bit_mask = (1ULL << BUTTON_PIN) | (1ULL << BUTTON_PAUSE_PIN)
    };

    gpio_config(&in_cfg);
    all_off();
}

void clear_frames(void)
{
    memset(red_frame,   0, sizeof(red_frame));
    memset(green_frame, 0, sizeof(green_frame));
}

void all_off(void)
{
    for (int i = 0; i < NUM_COLS; i++) {
        gpio_set_level(col_pins[i], COL_INACTIVE);
    }

    for (int i = 0; i < NUM_ROWS; i++) {
        gpio_set_level(red_row_pins[i],   ROW_INACTIVE);
        gpio_set_level(green_row_pins[i], ROW_INACTIVE);
    }
}

void refresh_display(void)
{
    for (int c = 0; c < NUM_COLS; c++) {
        all_off();

        for (int r = 0; r < NUM_ROWS; r++) {
            if (red_frame[r] & (1u << c)) {
                gpio_set_level(red_row_pins[r], ROW_ACTIVE);
            }

            if (green_frame[r] & (1u << c)) {
                gpio_set_level(green_row_pins[r], ROW_ACTIVE);
            }
        }

        gpio_set_level(col_pins[c], COL_ACTIVE);
        esp_rom_delay_us(REFRESH_US);
    }
}

void new_target(void)
{
    target_row = (int)(esp_random() % NUM_ROWS);
    target_col = (int)(esp_random() % NUM_COLS);

    clear_frames();
    red_frame[target_row] |= (1u << target_col);

    start_time = esp_timer_get_time();
    printf("Objetivo rojo: fila %d, columna %d\n", target_row, target_col);
}

static int64_t random_wait_us(void)
{
    uint32_t range = WAIT_MAX_US - WAIT_MIN_US;
    return (int64_t)(WAIT_MIN_US + (esp_random() % range));
}

void app_main(void)
{
    printf("Juego de reaccion iniciado\n");

    gpio_init_all();
    new_target();
    state = WAIT_TARGET;

    // variables debounce boton reaccion
    bool    raw_prev        = false;
    bool    stable_state    = false;
    int64_t last_raw_change = esp_timer_get_time();
    bool    can_press       = true;

    // variables debounce boton pausa
    bool    pause_raw_prev        = false;
    bool    pause_stable          = false;
    int64_t pause_last_raw_change = esp_timer_get_time();
    bool    pause_can_press       = true;

    while (1) {
        int64_t now = esp_timer_get_time();

        // si esta pausado apaga la matriz, si no refresca normal
        if (state == PAUSED) {
            all_off();
        } else {
            refresh_display();
        }

        // --- debounce boton de reaccion ---
        bool raw_current = (gpio_get_level(BUTTON_PIN) == BUTTON_ACTIVE);

        if (raw_current != raw_prev) {
            raw_prev        = raw_current;
            last_raw_change = now;
        }

        bool new_stable = stable_state;
        if ((now - last_raw_change) >= DEBOUNCE_US) {
            new_stable = raw_current;
        }

        if (new_stable && !stable_state) {
            if (can_press && state == WAIT_TARGET) {
                int64_t reaction_us = now - start_time;
                printf("Tiempo reaccion: %lld us (%.1f ms)\n",
                       reaction_us, (float)(reaction_us / 1000.0f));

                clear_frames();
                green_frame[target_row] |= (1u << target_col);

                state      = SHOW_RESULT;
                state_time = now;
                can_press  = false;
            }
        }

        if (!new_stable && stable_state) {
            can_press = true;
        }

        stable_state = new_stable;

        // --- debounce boton de pausa ---
        bool pause_raw = (gpio_get_level(BUTTON_PAUSE_PIN) == BUTTON_ACTIVE);

        if (pause_raw != pause_raw_prev) {
            pause_raw_prev        = pause_raw;
            pause_last_raw_change = now;
        }

        bool pause_new_stable = pause_stable;
        if ((now - pause_last_raw_change) >= DEBOUNCE_US) {
            pause_new_stable = pause_raw;
        }

        // flanco de subida del boton pausa: alterna pausa/juego
        if (pause_new_stable && !pause_stable) {
            if (pause_can_press) {
                if (state != PAUSED) {
                    // pausa el juego
                    state_before_pause = state;
                    state = PAUSED;
                    printf("Juego pausado\n");
                } else {
                    // reanuda con objetivo nuevo
                    new_target();
                    state = WAIT_TARGET;
                    can_press = true;
                    printf("Juego reanudado\n");
                }
                pause_can_press = false;
            }
        }

        if (!pause_new_stable && pause_stable) {
            pause_can_press = true;
        }

        pause_stable = pause_new_stable;

        // --- maquina de estados del juego ---
        switch (state) {
            case WAIT_TARGET:
                break;

            case SHOW_RESULT:
                if ((now - state_time) >= GREEN_TIME_US) {
                    clear_frames();
                    wait_dur_us = random_wait_us();
                    state       = WAIT_DELAY;
                    state_time  = now;
                    printf("Pausa: %.1f ms\n", (float)(wait_dur_us / 1000.0f));
                }
                break;

            case WAIT_DELAY:
                if ((now - state_time) >= wait_dur_us) {
                    new_target();
                    state     = WAIT_TARGET;
                    can_press = true;
                }
                break;

            case PAUSED:
                // no hace nada, espera que presionen pausa de nuevo
                break;

            default:
                state = WAIT_TARGET;
                break;
        }

        // cede CPU a FreeRTOS para evitar watchdog
        vTaskDelay(1);
    }
}
