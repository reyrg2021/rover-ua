# Protocolo de pruebas

Objetivo: llenar los corchetes del resumen con datos reales: [N] recorridos, circuito de [X] m con
[N] obstáculos, [X] % sin colisiones, tiempo [X ± Y] s, dónde chocó, costo [$X] CLP.

## Pista

- Rectángulo 4 × 2 m en piso liso, delimitado con cinta. Partida y meta en los lados cortos.
- Tres cajas de cartón 30 × 30 × 30 cm, separadas ≥ 1 m entre sí y ≥ 80 cm de los bordes.
  Una centrada, las otras dos desplazadas a lados opuestos. Marcar su posición con cinta.
- Sin rincones ni callejones (radio de giro ~60–80 cm). Fotografiar la pista.

## Por recorrido

Antes: packs ≥ 7,0 V; auto en la partida centrado; celular conectado y sensores en vivo con
valores razonables; cajas en sus marcas; anotar n°, versión y voltajes.

Durante: "Iniciar corrida". Un observador cuenta colisiones (contacto físico) y en qué obstáculo;
otro graba desde arriba. Nadie toca el auto. Si queda atrapado o sale de la pista: "Pausar" y anotar.

Después: al cruzar la meta, "Pausar". Copiar tiempo, evasiones, barridos, atrapamientos. Completar
la fila. Cada 5 corridas, volver a medir los packs.

## Planilla

`datos/planilla_corridas.csv`. Columnas: n, fecha, versión, v_pack1, v_pack2, tiempo_s, evasiones,
barridos, colisiones, completada (1/0), donde_choco, notas. La app Android genera el mismo CSV.

## Cálculos

- % sin colisiones = corridas con 0 colisiones ÷ total × 100 (incluir las no completadas).
- Tiempo = promedio ± desviación estándar, solo corridas completadas; decir cuántas.
- Tabla de colisiones por obstáculo y tipo (frontal, lateral, oblicuo).

## Diseño experimental

- ≥ 10 corridas por versión, alternando v1/v2 en bloques de 5, mismo día, misma pista, mismas pilas.
- Sesión de ajuste aparte (no cuenta). Congelar el código antes de las corridas que cuentan;
  si se cambia un parámetro, las corridas anteriores se descartan.
- Con n = 10 se presenta como tendencia; no afirmar significancia sin test.
