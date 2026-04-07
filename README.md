# Juego de Reacción - Matriz LED Bicolor 4x4 con ESP32

Juego de tiempo de reacción implementado en ESP32 usando ESP-IDF puro, con una matriz LED bicolor 4x4 extraída de una matriz 8x8 física, un botón de reacción y un botón de pausa.

## ¿Cómo funciona el juego?

1. Aparece un LED **rojo** en una posición aleatoria de la matriz 4x4
2. El jugador debe presionar el botón de reacción lo más rápido posible
3. El LED cambia a **verde** en la misma posición durante 800ms
4. El tiempo de reacción se imprime en microsegundos y milisegundos por el puerto serial
5. La matriz se apaga y espera una pausa aleatoria entre 0.7s y 5.0s
6. El ciclo se repite infinitamente hasta que se pausa el juego por medio del boton.

## Hardware requerido

- ESP32 (cualquier variante con los GPIOs listados disponibles)
- Matriz LED bicolor 4x4 (extraída de una matriz HL-M2388BRG 8x8, usando las filas 3-6)
- 2 botones pulsadores
- Resistencias limitadoras de corriente para los LEDs (recomendado 220Ω por columna)

## Salida por puerto serial

Abre el monitor serial para ver los resultados en tiempo real:
