# API HTTP del auto (firmware 2.8)

Red: `RoverUA` / `roverua2026`. Base: `http://192.168.4.1`.

| Método | Ruta | Descripción |
|---|---|---|
| GET | `/` | Página de control y mapa |
| GET | `/estado` | JSON con sensores, zona, corrida, barrido y registro (ver abajo) |
| GET | `/cmd?c=iniciar` | Inicia corrida (reinicia cronómetro y contadores) |
| GET | `/cmd?c=pausa` | Detiene de inmediato |
| GET | `/cmd?c=reanudar` | Continúa sin reiniciar contadores |
| GET | `/cmd?c=barrer` | Barrido con el auto detenido |
| GET | `/cmd?c=s0|s90|s180` | Posiciona el servo del sensor (pausa el auto) |
| GET | `/cmd?c=dizq|dcen|dder` | Posiciona la dirección (pausa el auto) |
| GET | `/cmd?c=manual` | Entra en modo manual |
| GET | `/cmd?c=mv&v=-255..255&g=-100..100` | Velocidad (+ adelante) y giro (+ derecha); se detiene si pasan 800 ms sin comando |

## `/estado`

```json
{
  "sens": {"izq": 120, "cen": 48, "der": 999, "zona": "precaucion", "vel": 160, "ang": 100},
  "log": ["zona precaucion (centro 48 cm)", "..."],
  "modo": "v2",
  "manual": false,
  "estado": "marcha",
  "barrido": {"id": 3, "n": 7, "paso": 15, "umbral": 50,
              "ang": [-45,-30,-15,0,15,30,45], "d": [30,25,999,999,120,110,40],
              "hueco": {"existe": true, "ini": 2, "fin": 5, "centro": 7}},
  "corrida": {"t": 18340, "evasiones": 2, "barridos": 2, "atrapamientos": 0}
}
```

`999` = sin eco. `ang` positivo = izquierda. `estado` ∈ marcha, pausa, atrapado, manual.
