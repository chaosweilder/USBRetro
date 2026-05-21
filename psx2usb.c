#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "joypad.h"

// Definición de Pines GPIO asignados
#define PSX_DATA_PIN    22  // Entrada con Pull-Up obligatorio
#define PSX_CLOCK_PIN   20  // Salida (Reloj de transmisión)
#define PSX_LATCH_PIN   21  // Salida (Attention / Select)
// GP19 (COMMAND) se mantiene sin uso ni inicialización según requerimiento

// Tiempos del bus PSX (en microsegundos)
#define PSX_DELAY_CLK   5   // Medio ciclo de reloj (~100kHz)
#define PSX_DELAY_ATTN  20  // Espera inicial tras activar la línea de atención

// Estructura de estado interno para el controlador en Joypad OS
typedef struct {
    uint32_t current_buttons;
    uint32_t last_buttons;
    bool connected;
} psx2usb_input_t;

static psx2usb_input_t app_state;

// Lee un byte del bus bit a bit (Modo solo lectura sin pin COMMAND)
static uint8_t psx_read_byte(void) {
    uint8_t data_out = 0;

    for (int i = 0; i < 8; i++) {
        // Flanco de bajada: El mando prepara el bit en la línea DATA
        gpio_put(PSX_CLOCK_PIN, 0);
        sleep_us(PSX_DELAY_CLK);

        // Flanco de subida: Joypad OS lee el bit estable (LSB primero)
        gpio_put(PSX_CLOCK_PIN, 1);
        
        if (gpio_get(PSX_DATA_PIN)) {
            data_out |= (1 << i);
        }
        sleep_us(PSX_DELAY_CLK);
    }
    return data_out;
}

// Inicialización de la aplicación invocada por el core de Joypad OS
static void psx2usb_init(void) {
    // Configurar DATA como entrada con Pull-Up interno para evitar pines flotantes
    gpio_init(PSX_DATA_PIN);
    gpio_set_dir(PSX_DATA_PIN, GPIO_IN);
    gpio_pull_up(PSX_DATA_PIN);

    // Configurar CLOCK (Inicia en ALTO / Idle)
    gpio_init(PSX_CLOCK_PIN);
    gpio_set_dir(PSX_CLOCK_PIN, GPIO_OUT);
    gpio_put(PSX_CLOCK_PIN, 1);

    // Configurar LATCH / ATTENTION (Inicia en ALTO / Idle)
    gpio_init(PSX_LATCH_PIN);
    gpio_set_dir(PSX_LATCH_PIN, GPIO_OUT);
    gpio_put(PSX_LATCH_PIN, 1);

    // Inicializar estado de la estructura de la app
    app_state.current_buttons = 0;
    app_state.last_buttons = 0;
    app_state.connected = false;

    // Configurar el entorno de salida de Joypad OS por defecto a XInput
    joypad_set_output_mode(JP_OUTPUT_MODE_XINPUT);
}

// Tarea cíclica integrada en el planificador (Scheduler) de Joypad OS
static void psx2usb_task(void) {
    // 1. Iniciar trama bajando la línea ATTENTION (LATCH)
    gpio_put(PSX_LATCH_PIN, 0);
    sleep_us(PSX_DELAY_ATTN);

    // 2. Byte 1: Inicio de transmisión (El host manda 0x01, pero sin pin COMMAND solo leemos el bus)
    psx_read_byte(); 

    // 3. Byte 2: ID del periférico (El mando digital clásico de PS1 responde 0x41)
    uint8_t device_id = psx_read_byte();

    // 4. Byte 3: Byte de sincronización constante (Siempre responde 0x5A)
    uint8_t ready_signal = psx_read_byte();

    uint16_t raw_psx_buttons = 0xFFFF;

    // Verificar firmas del protocolo antes de procesar datos residuales
    if (device_id == 0x41 && ready_signal == 0x5A) {
        app_state.connected = true;
        
        // Byte 4: Select, L3, R3, Start, Joypad Arriba, Derecha, Abajo, Izquierda
        uint8_t byte_low = psx_read_byte();
        // Byte 5: L2, R2, L1, R1, Triángulo, Círculo, Equis, Cuadrado
        uint8_t byte_high = psx_read_byte();
        
        raw_psx_buttons = (byte_high << 8) | byte_low;
    } else {
        app_state.connected = false;
    }

    // 5. Finalizar trama subiendo la línea ATTENTION
    gpio_put(PSX_LATCH_PIN, 1);

    // Mapear los bits si el dispositivo está respondiendo correctamente
    if (app_state.connected) {
        uint32_t mapped_buttons = 0;

        // El bus de PS1 maneja lógica inversa (0 = Pulsado, 1 = Reposo)
        if (!(raw_psx_buttons & (1 << 0)))  mapped_buttons |= JP_BUTTON_BACK;
        if (!(raw_psx_buttons & (1 << 3)))  mapped_buttons |= JP_BUTTON_START;
        if (!(raw_psx_buttons & (1 << 4)))  mapped_buttons |= JP_BUTTON_UP;
        if (!(raw_psx_buttons & (1 << 5)))  mapped_buttons |= JP_BUTTON_RIGHT;
        if (!(raw_psx_buttons & (1 << 6)))  mapped_buttons |= JP_BUTTON_DOWN;
        if (!(raw_psx_buttons & (1 << 7)))  mapped_buttons |= JP_BUTTON_LEFT;
        
        if (!(raw_psx_buttons & (1 << 8)))  mapped_buttons |= JP_BUTTON_L2;
        if (!(raw_psx_buttons & (1 << 9)))  mapped_buttons |= JP_BUTTON_R2;
        if (!(raw_psx_buttons & (1 << 10))) mapped_buttons |= JP_BUTTON_L1;
        if (!(raw_psx_buttons & (1 << 11))) mapped_buttons |= JP_BUTTON_R1;
        
        if (!(raw_psx_buttons & (1 << 12))) mapped_buttons |= JP_BUTTON_Y;
        if (!(raw_psx_buttons & (1 << 13))) mapped_buttons |= JP_BUTTON_B;
        if (!(raw_psx_buttons & (1 << 14))) mapped_buttons |= JP_BUTTON_A;
        if (!(raw_psx_buttons & (1 << 15))) mapped_buttons |= JP_BUTTON_X;

        app_state.current_buttons = mapped_buttons;
    } else {
        app_state.current_buttons = 0;
    }

    // Inyectar el estado al motor de emulación si hay un cambio en los botones
    if (app_state.current_buttons != app_state.last_buttons) {
        joypad_update_buttons(app_state.current_buttons);
        app_state.last_buttons = app_state.current_buttons;
    }
}

// Macro de registro del Framework para indexar la app en el menú/núcleo de Joypad OS
JOYPAD_APP_REGISTER(
    "psx2usb",          // Identificador interno del módulo
    psx2usb_init,       // Puntero a la función de configuración de hardware
    psx2usb_task        // Puntero a la función cíclica del planificador general
);
