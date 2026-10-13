package cl.uautonoma.autoepi.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import cl.uautonoma.autoepi.AutoViewModel
import cl.uautonoma.autoepi.Tinta
import cl.uautonoma.autoepi.UiState
import kotlinx.coroutines.delay
import kotlin.math.roundToInt

/**
 * Conducción manual: joystick (arriba = adelante, derecha = derecha) con un
 * límite de velocidad. Mientras el dedo está sobre el joystick se reenvía el
 * comando cada 250 ms (el firmware se detiene si no recibe nada en 800 ms).
 */
@Composable
fun ManualScreen(ui: UiState, vm: AutoViewModel) {
    val e = ui.estado
    var limite by remember { mutableFloatStateOf(180f) }
    var activo by remember { mutableStateOf(false) }
    var vel by remember { mutableStateOf(0) }
    var giro by remember { mutableStateOf(0) }

    // Reenvío periódico mientras el joystick está activo
    LaunchedEffect(activo) {
        while (activo) {
            vm.manual(vel, giro)
            delay(250)
        }
        vm.manual(0, 0)
    }

    Column(
        Modifier.fillMaxSize().padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        Text("Conducción manual", fontSize = 24.sp, fontWeight = FontWeight.Bold, color = Tinta)
        Text(
            if (e?.manual == true) "Modo manual activo. El auto no evade solo."
            else "Pulsa «Activar manual» para tomar el control (pausa la autonomía).",
            color = Color(0xFF3D4E68)
        )
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = { vm.cmd("manual") }, Modifier.weight(1f).height(52.dp),
                enabled = e?.manual != true) { Text("Activar manual") }
            OutlinedButton(onClick = { activo = false; vm.cmd("pausa") }, Modifier.weight(1f).height(52.dp)) { Text("Pausar (salir)") }
        }

        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Cifra("Izq", e?.let { fmt(it.izq) } ?: "—", Modifier.weight(1f))
            Cifra("Centro", e?.let { fmt(it.cen) } ?: "—", Modifier.weight(1f))
            Cifra("Der", e?.let { fmt(it.der) } ?: "—", Modifier.weight(1f))
        }

        Joystick(
            habilitado = e?.manual == true,
            onMover = { x, y ->
                // y negativo = arriba = adelante
                vel = (-y * limite).roundToInt().coerceIn(-255, 255)
                giro = (x * 100).roundToInt().coerceIn(-100, 100)
                activo = true
            },
            onSoltar = { vel = 0; giro = 0; activo = false }
        )
        Text("Velocidad $vel · giro $giro", color = Color(0xFF3D4E68))

        Text("Límite de velocidad: ${limite.roundToInt()}", fontWeight = FontWeight.SemiBold)
        Slider(value = limite, onValueChange = { limite = it }, valueRange = 125f..255f)

        Button(
            onClick = { activo = false; vm.cmd("pausa") }, Modifier.fillMaxWidth().height(60.dp),
            colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFC62E2E))
        ) { Text("PARAR", fontSize = 20.sp, fontWeight = FontWeight.Bold) }
    }
}

@Composable
fun Joystick(habilitado: Boolean, onMover: (Float, Float) -> Unit, onSoltar: () -> Unit) {
    var pos by remember { mutableStateOf(Offset.Zero) }
    Box(
        Modifier.size(260.dp).pointerInput(habilitado) {
            if (!habilitado) return@pointerInput
            val radio = size.width / 2f
            detectDragGestures(
                onDragEnd = { pos = Offset.Zero; onSoltar() },
                onDragCancel = { pos = Offset.Zero; onSoltar() }
            ) { change, arrastre ->
                change.consume()
                var n = pos + arrastre
                val len = n.getDistance()
                if (len > radio) n *= (radio / len)
                pos = n
                onMover(n.x / radio, n.y / radio)
            }
        },
        contentAlignment = Alignment.Center
    ) {
        Canvas(Modifier.fillMaxSize()) {
            val r = size.minDimension / 2f
            drawCircle(if (habilitado) Color(0xFFDDE3EA) else Color(0xFFEEEEEE), radius = r)
            drawCircle(Color(0xFFB8C4D4), radius = r, style = androidx.compose.ui.graphics.drawscope.Stroke(3f))
            drawLine(Color(0xFFB8C4D4), Offset(center.x, center.y - r), Offset(center.x, center.y + r), 2f)
            drawLine(Color(0xFFB8C4D4), Offset(center.x - r, center.y), Offset(center.x + r, center.y), 2f)
            drawCircle(if (habilitado) Tinta else Color(0xFF9AA3B0), radius = r * 0.28f, center = center + pos)
        }
    }
}
