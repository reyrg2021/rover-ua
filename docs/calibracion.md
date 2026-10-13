# Calibración

Valores vigentes (11-oct-2026) en el encabezado de `firmware/rover_ua_v2_8`.

## Dirección (cuando se desmonte el brazo del servo)

1. Desacoplar el varillaje (sacar el tornillo del brazo). Cargar `herramientas/calibracion_servos`.
2. Monitor serie 115200, enviar `c`: ambos servos a 90° eléctrico.
3. Sin apagar, montar el brazo con las ruedas rectas.
4. `d85`, `d80`, … hasta que las ruedas dejen de moverse o el servo zumbe: ese es el tope.
   Retroceder 3° y anotar. Repetir hacia arriba (`d95`, `d100`, …).
5. En el firmware: `ANG_CENTRO`, `DIR_MIN`, `DIR_MAX`, y los ángulos suave/fuerte dentro del rango.
6. En el piso (`paso2_wifi_motores_direccion`): si avanza desviado, ajustar el centro de a 1°.

## Servo de barrido

1. `c` con el soporte del sensor desacoplado; montar el sensor mirando exactamente al frente.
2. `b30`: anotar si apunta a la derecha (→ `CERO_A_LA_DERECHA = true`).
3. `b45` y `b135` apuntando a espacio vacío deben dar > 100 cm; si dan 5–10 cm ven los soportes
   laterales: acotar a 50/130.
4. Residuo de centrado → `OFFSET_CENTRO_BARRIDO`.

## Motores

- `herramientas/paso1_motores`: `t` para sentido, `r` rampa para el PWM mínimo en el aire.
- `paso2_wifi_motores_direccion`: deslizador hasta que el auto cargado deje de moverse: ese valor
  + 20 es `VEL_PRECAUCION`.

## Controlador

- `KP_RUMBO` (0,35): zigzaguea en pasillos → bajar; se acerca demasiado antes de corregir → subir.
- `T_PIVOTE` (900 ms): debe dar 60–90° de giro en el piso de prueba.
- `FACTOR_INTERNA_FUERTE` (0,0): −0,4 para giros más cerrados en piso liso.
