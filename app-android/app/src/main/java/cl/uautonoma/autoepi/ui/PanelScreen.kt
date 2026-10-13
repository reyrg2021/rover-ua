package cl.uautonoma.autoepi.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import cl.uautonoma.autoepi.AutoViewModel
import cl.uautonoma.autoepi.Azul
import cl.uautonoma.autoepi.Tinta
import cl.uautonoma.autoepi.UiState

fun fmt(d: Long) = if (d >= 999) "sin eco" else "$d cm"

@Composable
fun PanelScreen(ui: UiState, vm: AutoViewModel) {
    val e = ui.estado
    Column(
        Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Text("Rover UA", fontSize = 24.sp, fontWeight = FontWeight.Bold, color = Tinta)

        // Estado de conexión
        val estadoTxt = when {
            !ui.conectado -> "Sin conexión. ¿Celular en la red RoverUA y datos móviles apagados?"
            e == null -> "Conectando…"
            else -> "${if (e.modo == "v2") "Versión 2 (barrido)" else "Versión 1 (reactiva)"} · ${
                when (e.estado) {
                    "marcha" -> "corrida en marcha"; "pausa" -> "en pausa"
                    "manual" -> "modo manual"; "atrapado" -> "ATRAPADO: despejar y reanudar"
                    else -> e.estado
                }
            }"
        }
        Text(estadoTxt, color = if (ui.conectado) Color(0xFF3D4E68) else Color(0xFFC62E2E))

        // Sensores en vivo
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Cifra("Izquierdo", e?.let { fmt(it.izq) } ?: "—", Modifier.weight(1f))
            Cifra("Central", e?.let { fmt(it.cen) } ?: "—", Modifier.weight(1f))
            Cifra("Derecho", e?.let { fmt(it.der) } ?: "—", Modifier.weight(1f))
        }
        val zona = e?.zona ?: "-"
        val zonaColor = when (zona) {
            "CRITICA" -> Color(0xFFFADADA); "precaucion" -> Color(0xFFFFF1C9); "libre" -> Color(0xFFDFF3E4)
            else -> Color(0xFFEEF1F5)
        }
        Text(
            "Zona: $zona · motores ${e?.vel ?: 0} · dirección ${e?.ang ?: 0}°",
            Modifier.fillMaxWidth().background(zonaColor, RoundedCornerShape(8.dp)).padding(10.dp),
            fontWeight = FontWeight.SemiBold
        )

        // Botones principales
        Button(
            onClick = { vm.cmd("pausa") }, Modifier.fillMaxWidth().height(60.dp),
            colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFC62E2E))
        ) { Text("PAUSAR", fontSize = 20.sp, fontWeight = FontWeight.Bold) }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = { vm.cmd("iniciar") }, Modifier.weight(1f).height(52.dp)) { Text("Iniciar corrida") }
            OutlinedButton(onClick = { vm.cmd("reanudar") }, Modifier.weight(1f).height(52.dp)) { Text("Reanudar") }
            OutlinedButton(onClick = { vm.cmd("barrer") }, Modifier.weight(1f).height(52.dp)) { Text("Barrer") }
        }

        // Corrida en curso
        val c = e?.corrida
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Cifra("Tiempo", c?.let { "%.1f s".format(it.tMs / 1000.0) } ?: "—", Modifier.weight(1f))
            Cifra("Evasiones", c?.evasiones?.toString() ?: "—", Modifier.weight(1f))
            Cifra("Barridos", c?.barridos?.toString() ?: "—", Modifier.weight(1f))
        }

        // Mapa del barrido
        Text("Mapa del último barrido", fontWeight = FontWeight.SemiBold, color = Azul)
        Card(colors = CardDefaults.cardColors(containerColor = Color.White)) {
            MapaBarrido(e?.barrido, Modifier.padding(8.dp))
        }
        val b = e?.barrido
        if (b != null && b.n > 0) {
            Text(
                if (b.huecoExiste) "Barrido ${b.id}: hueco centrado en ${
                    if (b.huecoCentro > 0) "${b.huecoCentro}° a la izquierda" else if (b.huecoCentro < 0) "${-b.huecoCentro}° a la derecha" else "el frente"
                }" else "Barrido ${b.id}: ningún hueco libre",
                color = Color(0xFF3D4E68)
            )
        }

        // Registro de eventos
        Text("Registro de eventos", fontWeight = FontWeight.SemiBold, color = Azul)
        Card(colors = CardDefaults.cardColors(containerColor = Color.White)) {
            Column(Modifier.padding(10.dp)) {
                (e?.log ?: listOf("—")).forEach { Text(it, fontSize = 13.sp, color = Color(0xFF3D4E68)) }
            }
        }

        // Calibración y dirección IP
        Text("Calibración (pone el auto en pausa)", fontWeight = FontWeight.SemiBold, color = Azul)
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = { vm.cmd("s0") }, Modifier.weight(1f)) { Text("Sensor 0°") }
            OutlinedButton(onClick = { vm.cmd("s90") }, Modifier.weight(1f)) { Text("90°") }
            OutlinedButton(onClick = { vm.cmd("s180") }, Modifier.weight(1f)) { Text("180°") }
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = { vm.cmd("dizq") }, Modifier.weight(1f)) { Text("Dir. izq") }
            OutlinedButton(onClick = { vm.cmd("dcen") }, Modifier.weight(1f)) { Text("Centro") }
            OutlinedButton(onClick = { vm.cmd("dder") }, Modifier.weight(1f)) { Text("Dir. der") }
        }
        var ip by rememberSaveable { mutableStateOf("192.168.4.1") }
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedTextField(value = ip, onValueChange = { ip = it }, label = { Text("IP del auto") }, modifier = Modifier.weight(1f), singleLine = true)
            OutlinedButton(onClick = { vm.setDireccion(ip.trim()) }) { Text("Usar") }
        }
        Spacer(Modifier.height(24.dp))
    }
}

@Composable
fun Cifra(titulo: String, valor: String, modifier: Modifier = Modifier) {
    Card(modifier, colors = CardDefaults.cardColors(containerColor = Color.White)) {
        Column(Modifier.padding(10.dp), horizontalAlignment = Alignment.CenterHorizontally) {
            Text(titulo, fontSize = 12.sp, color = Color(0xFF3D4E68))
            Text(valor, fontSize = 22.sp, fontWeight = FontWeight.Bold, color = Tinta)
        }
    }
}