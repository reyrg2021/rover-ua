package cl.uautonoma.autoepi.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.Stroke
import cl.uautonoma.autoepi.Barrido
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.min
import kotlin.math.sin

/**
 * Mapa semicircular del último barrido: mismo dibujo que el canvas de la
 * página web. Ángulo + = izquierda. Azul = libre, rojo = ocupado, círculo
 * vacío = sin eco. El hueco elegido se sombrea en amarillo.
 */
@Composable
fun MapaBarrido(b: Barrido?, modifier: Modifier = Modifier, rangoCm: Int = 200) {
    Canvas(modifier.fillMaxWidth().aspectRatio(1.7f)) {
        val w = size.width; val h = size.height
        val cx = w / 2f; val cy = h - 12f
        val esc = min(cx - 10f, cy - 10f) / rangoCm

        fun punto(ang: Int, d: Long): Offset {
            val r = min(d, rangoCm.toLong()) * esc
            val a = ang * PI.toFloat() / 180f
            return Offset(cx - r * sin(a), cy - r * cos(a))
        }

        // Fondo y arcos de distancia cada 50 cm
        drawRect(Color(0xFFFBFCFD))
        for (r in 50..rangoCm step 50) {
            drawArc(
                color = Color(0xFF9FB6D3), startAngle = 180f, sweepAngle = 180f, useCenter = false,
                topLeft = Offset(cx - r * esc, cy - r * esc), size = Size(2 * r * esc, 2 * r * esc),
                style = Stroke(width = 1.5f)
            )
        }
        // Líneas de referencia cada 45°
        for (a in listOf(-90, -45, 0, 45, 90)) {
            drawLine(Color(0xFFD6E4F0), Offset(cx, cy), punto(a, rangoCm.toLong()), strokeWidth = 1f)
        }

        if (b != null && b.n > 0) {
            // Hueco elegido
            if (b.huecoExiste && b.huecoIni >= 0 && b.huecoFin < b.ang.size) {
                val a1 = b.ang[b.huecoIni]; val a2 = b.ang[b.huecoFin]
                val hi = min(90, maxOf(a1, a2) + b.paso / 2)
                val lo = maxOf(-90, minOf(a1, a2) - b.paso / 2)
                // En el canvas, el ángulo 0 está a la derecha; nuestro 0 está arriba (-90°)
                val start = -90f - hi
                val sweep = (hi - lo).toFloat()
                drawArc(
                    color = Color(0x8CFFE45C), startAngle = start, sweepAngle = sweep, useCenter = true,
                    topLeft = Offset(cx - rangoCm * esc, cy - rangoCm * esc),
                    size = Size(2 * rangoCm * esc, 2 * rangoCm * esc)
                )
            }
            // Lecturas
            for (i in 0 until b.n) {
                val d = b.d[i]; val sinEco = d >= 999; val libre = d > b.umbral
                val p = punto(b.ang[i], if (sinEco) rangoCm.toLong() else d)
                drawLine(
                    Color(0xFF8A94A6), Offset(cx, cy), p, strokeWidth = 1.5f,
                    pathEffect = if (sinEco) PathEffect.dashPathEffect(floatArrayOf(8f, 8f)) else null
                )
                if (sinEco) drawCircle(Color(0xFF8A94A6), radius = 7f, center = p, style = Stroke(2f))
                else drawCircle(if (libre) Color(0xFF1F5FAD) else Color(0xFFC62E2E), radius = 7f, center = p)
            }
            // Flecha de salida
            if (b.huecoExiste) {
                val p = punto(b.huecoCentro, (rangoCm * 0.9).toLong())
                drawLine(Color(0xFF1B2A41), Offset(cx, cy), p, strokeWidth = 4f)
                val ang = kotlin.math.atan2(p.y - cy, p.x - cx)
                val path = Path().apply {
                    moveTo(p.x, p.y)
                    lineTo(p.x - 18f * cos(ang - 0.4f), p.y - 18f * sin(ang - 0.4f))
                    lineTo(p.x - 18f * cos(ang + 0.4f), p.y - 18f * sin(ang + 0.4f))
                    close()
                }
                drawPath(path, Color(0xFF1B2A41))
            }
        }
        // El auto
        drawRect(Color(0xFF1B2A41), topLeft = Offset(cx - 10f, cy - 8f), size = Size(20f, 16f))
    }
}
