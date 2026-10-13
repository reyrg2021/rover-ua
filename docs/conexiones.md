# Conexiones

## Mapa de pines del ESP32

| Elemento | Señal | GPIO |
|---|---|---|
| Motor A (rueda derecha) | ENA · IN1 · IN2 | 25 · 26 · 27 |
| Motor B (rueda izquierda) | ENB · IN3 · IN4 | 33 · 32 · 19 |
| Servo de dirección | señal | 18 |
| Servo de barrido | señal | 23 |
| HC-SR04 derecho | Trig · Echo | 17 · 12 |
| HC-SR04 central (sobre el servo) | Trig · Echo | 16 · 4 |
| HC-SR04 izquierdo | Trig · Echo | 21 · 22 |

GPIO 12 es pin de arranque (MTDI). El Echo en reposo está en bajo, así que normalmente no
molesta; si el ESP32 no arranca con el sensor conectado, mover ese Echo a 34, 35, 36 o 39
(solo entrada, ideales para Echo).

## Alimentación

- Pack 1 (2 × 18650, 7,4 V) → bornes 12V/GND del L298N. Jumper 5V-EN **puesto**.
- Pack 2 (2 × 18650, 7,4 V) → jack del shield del ESP32.
- **GND común** entre L298N y ESP32: obligatorio.
- Jumpers de ENA y ENB del L298N **quitados** (van cables a GPIO 25 y 33).
- Echo de cada HC-SR04 a través de divisor resistivo (p. ej. 1 kΩ / 2 kΩ): 5 V → 3,3 V.
- Servo de dirección: desde el 5 V del L298N (regulador 78M05 de 0,5 A, sin disipador: el del
  módulo enfría el puente). Funciona porque los topes por software (78°–117°) impiden que el servo
  se bloquee. Vigilar en el registro: si aparece "reset 9" (brownout), alimentar los servos aparte.
- Medir ambos packs antes de cada sesión: cambiar pilas bajo 7,0 V.

## Reglas mecánicas

- Cable del sensor central (el que gira) con holgura y asegurado con silicona caliente.
- Sensores laterales a ~45° hacia afuera; medir el ángulo real y ponerlo en `ANG_SENSOR_LATERAL`.
- Un interruptor por pack.
