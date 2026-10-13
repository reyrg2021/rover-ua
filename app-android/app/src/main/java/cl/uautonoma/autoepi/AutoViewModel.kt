package cl.uautonoma.autoepi

import android.app.Application
import android.os.VibrationEffect
import android.os.Vibrator
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

data class UiState(
    val conectado: Boolean = false,
    val estado: Estado? = null,
    val corridas: List<CorridaGuardada> = emptyList(),
    val mensaje: String = ""
)

class AutoViewModel(app: Application) : AndroidViewModel(app) {
    private val repo = CorridasRepo(app)
    private val _ui = MutableStateFlow(UiState(corridas = repo.cargar()))
    val ui = _ui.asStateFlow()

    private var estadoAnterior: String? = null
    private var zonaAnterior: String = ""
    private var fallosSeguidos = 0

    init {
        // Sondeo continuo del auto, como el ciclo() de la página web
        viewModelScope.launch {
            while (isActive) {
                procesar(AutoApi.estado())
                delay(400)
            }
        }
    }

    private fun procesar(e: Estado?) {
        if (e == null) {
            fallosSeguidos++
            if (fallosSeguidos >= 3) _ui.update { it.copy(conectado = false) }
            return
        }
        fallosSeguidos = 0

        // Fin de corrida: pasó de "marcha" a "pausa" o "atrapado" con tiempo acumulado
        if (estadoAnterior == "marcha" && (e.estado == "pausa" || e.estado == "atrapado") && e.corrida.tMs > 0) {
            registrarCorrida(e)
        }
        // Aviso háptico al entrar en zona crítica
        if (e.zona == "CRITICA" && zonaAnterior != "CRITICA") vibrar(200)

        estadoAnterior = e.estado
        zonaAnterior = e.zona
        _ui.update { it.copy(conectado = true, estado = e) }
    }

    private fun registrarCorrida(e: Estado) {
        val c = CorridaGuardada(
            id = System.currentTimeMillis(),
            fecha = SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US).format(Date()),
            modo = e.modo,
            tiempoS = e.corrida.tMs / 1000.0,
            evasiones = e.corrida.evasiones,
            barridos = e.corrida.barridos,
            atrapado = e.estado == "atrapado",
            colisiones = 0,
            completada = e.estado != "atrapado",
            notas = ""
        )
        val lista = _ui.value.corridas + c
        repo.guardar(lista)
        _ui.update { it.copy(corridas = lista, mensaje = "Corrida guardada: ${"%.1f".format(c.tiempoS)} s") }
        vibrar(60)
    }

    private fun vibrar(ms: Long) {
        try {
            getApplication<Application>().getSystemService(Vibrator::class.java)
                ?.vibrate(VibrationEffect.createOneShot(ms, VibrationEffect.DEFAULT_AMPLITUDE))
        } catch (_: Exception) { }
    }

    // ---- comandos ----
    fun cmd(c: String) = viewModelScope.launch { AutoApi.cmd(c) }
    fun manual(v: Int, g: Int) = viewModelScope.launch { AutoApi.manual(v, g) }
    fun setDireccion(ip: String) { AutoApi.base = "http://$ip" }

    // ---- edición de corridas (lo que anota el observador) ----
    fun actualizarCorrida(id: Long, cambio: (CorridaGuardada) -> CorridaGuardada) {
        val lista = _ui.value.corridas.map { if (it.id == id) cambio(it) else it }
        repo.guardar(lista)
        _ui.update { it.copy(corridas = lista) }
    }

    fun borrarCorrida(id: Long) {
        val lista = _ui.value.corridas.filter { it.id != id }
        repo.guardar(lista)
        _ui.update { it.copy(corridas = lista) }
    }

    fun borrarTodas() {
        repo.guardar(emptyList())
        _ui.update { it.copy(corridas = emptyList()) }
    }

    fun exportarCsv(): File = repo.exportarCsv(_ui.value.corridas)

    fun limpiarMensaje() = _ui.update { it.copy(mensaje = "") }
}
