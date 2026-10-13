# Rover UA · Vehículo autónomo de bajo costo con evasión de obstáculos

Prototipo a escala desarrollado por estudiantes de primer año de la Facultad de Ingeniería,
Universidad Autónoma de Chile (Sede Santiago), presentado en el **IV Congreso de Ingeniería
Aplicada a la Industria Sustentable**, Sede Temuco, 21 y 22 de octubre de 2026.

ESP32 · puente H L298N · dos motores DC · servo de dirección · tres HC-SR04 (el central sobre
un servo de barrido) · telemetría y control por WiFi desde el celular.

## Estructura del repositorio

```
firmware/
  rover_ua_v2_8/           ← código que se carga en el auto (Arduino IDE, placa ESP32)
  historial/               versiones anteriores, de la v1 original a la 2.7
herramientas/              sketches de calibración y prueba por subsistemas
  calibracion_servos/      centrar los servos y medir topes
  diagnostico_vehiculo/    prueba de cada subsistema por monitor serie
  paso1_motores/           solo L298N y motores
  paso2_motores_direccion/ motores + dirección (serie)
  paso2_wifi_motores_direccion/  lo mismo, desde el celular
  paso3_wifi_sensores/     sensores y barrido, desde el celular
app-android/               app Kotlin + Jetpack Compose (panel, manual, corridas)
docs/
  resumen-congreso/        resumen enviado al congreso
  calibracion.md           valores medidos y cómo repetir la calibración
  conexiones.md            mapa de pines y reglas eléctricas
  protocolo-pruebas.md     pista, procedimiento, planilla y diseño experimental
  api.md                   endpoints HTTP del auto
datos/
  planilla_corridas.csv    plantilla de la planilla de corridas
  costos.csv               plantilla de la tabla de costos
```

## Empezar

1. Arduino IDE con soporte ESP32 y la librería **ESP32Servo**.
2. Abrir `firmware/rover_ua_v2_8/rover_ua_v2_8.ino`, placa *ESP32 Dev Module*, cargar.
3. Conectar el celular a la red **RoverUA** (clave `roverua2026`) y abrir `http://192.168.4.1`.
4. "Iniciar corrida". El auto parte en pausa para que nadie pierda un dedo.

Modo: `MODO_BARRIDO = false` es la versión reactiva (la del resumen); `true` agrega el
barrido ultrasónico y el mapa local. Todo lo demás es común.

## Cómo funciona, en tres líneas

- **Tres zonas por distancia frontal** (libre > 50 cm, precaución 25–50, crítica ≤ 25) con histéresis de 5 cm.
- **Rumbo proporcional** con los laterales: `desvío = 0,35 × (dDer − dIzq)`, recortado a ±17°, con la rueda trasera interior frenada en proporción (tracción diferencial).
- **Evasión en lazo cerrado**: retrocede hasta ver 45 cm libres, avanza en arco hasta ver 60; si no sale, pivota sobre el eje; tercer intento al otro lado; después, "atrapado".

Detalle en `docs/` y en la presentación interna del equipo.

## Valores calibrados (11-oct-2026)

| Parámetro | Valor | Cómo se midió |
|---|---|---|
| Centro de dirección | 100° | calibracion_servos + avance recto 2 m |
| Topes de dirección | 75° / 120° (software: 78–117) | calibracion_servos |
| PWM mínimo en piso | 125 | paso2_wifi, deslizador |
| VEL_PRECAUCION | 145 | PWM mínimo + 20 |
| Servo de barrido | 30° mira a la derecha → `CERO_A_LA_DERECHA = true` | paso3_wifi |
| Motor A / B | derecha / izquierda | paso1_motores |

## Historial de versiones

| Versión | Qué cambió |
|---|---|
| v1 | Código original: tres zonas, maniobra de evasión con tiempos fijos |
| 2.0 | Barrido ultrasónico 0–180°, mapa local, página WiFi |
| 2.1 | Dirección calibrada con tope por software; barrido 30–150°; PWM por ESP32Servo; "sin eco" ≠ libre |
| 2.2 | Barrido ±45°; laterales fusionados al mapa |
| 2.3 | Valores medidos en el piso; Echo derecho en GPIO 12 |
| 2.4 | Tracción diferencial; regla de alejarse de la pared |
| 2.5 | Telemetría en la página; márgenes 25/50; VEL_NORMAL 200 |
| 2.6 | Rumbo proporcional; evasión en lazo cerrado con pivote |
| 2.7 | Modo manual para la app Android |
| 2.8 | Nombre Rover UA; corrección del modo manual (motores zumbaban; Pausar no salía del manual) |

## Licencia

MIT. Ver `LICENSE`.
