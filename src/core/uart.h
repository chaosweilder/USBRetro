// uart.h
#ifndef UART_H
#define UART_H

#define UART_ID uart0
#define BAUD_RATE 115200

// UART pins — default to KB2040 Stemma connector (GPIO 12/13)
// Define NO_CUSTOM_UART_PINS to skip (e.g., Pico boards using default UART)
// Override per-app or per-board by defining before including this header
#if !defined(NO_CUSTOM_UART_PINS)
  #ifndef UART_TX_PIN
  #define UART_TX_PIN 12
  #endif

  #ifndef UART_RX_PIN
  #define UART_RX_PIN 13
  #endif
#endif

#endif // UART_H
