package cl.uautonoma.autoepi.ui

import android.content.Intent
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Checkbox
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.FileProvider
import cl.uautonoma.autoepi.AutoViewModel
import cl.uautonoma.autoepi.CorridaGuardada
import cl.uautonoma.autoepi.Tinta
import cl.uautonoma.autoepi.UiState

/**
 * Planilla de corridas. Cada corrida se guarda sola al pausar; aquí el
 * observador anota colisiones, si se completó y notas, y exporta el CSV.
 */
@Composable
fun CorridasScreen(ui: UiState, vm: AutoViewModel) {
    val ctx = LocalContext.current
    val lista = ui.corridas

    // Resumen por versión (lo que pide el resumen del congreso)
    fun resumen(modo: String): String {
        val l = lista.filter { it.modo == modo }
        if (l.isEmpty()) return "$modo: sin corridas"
        val sinCol = l.count { it.colisiones == 0 }
        val comp = l.filter { it.completada }
        val media = if (comp.isEmpty()) 0.0 else comp.map { it.tiempoS }.average()
        val sd = if (comp.size < 2) 0.0 else kotlin.math.sqrt(comp.map { (it.tiempoS - media) * (it.tiempoS - media) }.sum() / (comp.size - 1))
        return "$modo: n=${l.size} · sin colisiones ${100 * sinCol / l.size} % · completadas ${comp.size} · tiempo ${"%.1f".format(media)} ± ${"%.1f".format(sd)} s"
    }

    Column(Modifier.fillMaxSize().padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
        Text("Corridas registradas", fontSize = 24.sp, fontWeight = FontWeight.Bold, color = Tinta)
        Text(resumen("v1"), fontSize = 14.sp, color = Color(0xFF3D4E68))
        Text(resumen("v2"), fontSize = 14.sp, color = Color(0xFF3D4E68))
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = {
                val f = vm.exportarCsv()
                val uri = FileProvider.getUriForFile(ctx, "cl.uautonoma.autoepi.fileprovider", f)
                val i = Intent(Intent.ACTION_SEND).apply {
                    type = "text/csv"
                    putExtra(Intent.EXTRA_STREAM, uri)
                    putExtra(Intent.EXTRA_SUBJECT, "Corridas Rover UA")
                    addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                }
                ctx.startActivity(Intent.createChooser(i, "Exportar CSV"))
            }, Modifier.weight(1f)) { Text("Exportar CSV") }
            OutlinedButton(onClick = { vm.borrarTodas() }, Modifier.weight(1f)) { Text("Borrar todas") }
        }
        LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            items(lista.reversed(), key = { it.id }) { c -> FilaCorrida(c, lista.indexOf(c) + 1, vm) }
        }
    }
}

@Composable
private fun FilaCorrida(c: CorridaGuardada, n: Int, vm: AutoViewModel) {
    Card(colors = CardDefaults.cardColors(containerColor = Color.White)) {
        Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text("#$n · ${c.modo} · ${c.fecha}", fontWeight = FontWeight.SemiBold, color = Tinta)
            Text(
                "${"%.1f".format(c.tiempoS)} s · evasiones ${c.evasiones} · barridos ${c.barridos}" +
                        if (c.atrapado) " · ATRAPADO" else "",
                fontSize = 14.sp, color = Color(0xFF3D4E68)
            )
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                Text("Colisiones: ${c.colisiones}", fontSize = 14.sp)
                TextButton(onClick = { vm.actualizarCorrida(c.id) { it.copy(colisiones = maxOf(0, it.colisiones - 1)) } }) { Text("−") }
                TextButton(onClick = { vm.actualizarCorrida(c.id) { it.copy(colisiones = it.colisiones + 1) } }) { Text("+") }
                Checkbox(checked = c.completada, onCheckedChange = { v -> vm.actualizarCorrida(c.id) { it.copy(completada = v) } })
                Text("Completada", fontSize = 14.sp)
            }
            OutlinedTextField(
                value = c.notas,
                onValueChange = { v -> vm.actualizarCorrida(c.id) { it.copy(notas = v) } },
                label = { Text("Dónde chocó / notas") }, singleLine = true, modifier = Modifier.fillMaxWidth()
            )
            TextButton(onClick = { vm.borrarCorrida(c.id) }) { Text("Eliminar esta corrida", color = Color(0xFFC62E2E)) }
        }
    }
}

