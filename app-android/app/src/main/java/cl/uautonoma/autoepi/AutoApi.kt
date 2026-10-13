package cl.uautonoma.autoepi

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.net.HttpURLConnection
import java.net.URL

/**
 * Cliente HTTP mínimo contra el ESP32. Usa exactamente los mismos endpoints
 * que la página web: /estado (JSON) y /cmd?c=... (texto "ok").
 */
object AutoApi {
    @Volatile var base: String = "http://192.168.4.1"

    private suspend fun get(path: String, timeoutMs: Int): String? = withContext(Dispatchers.IO) {
        var conn: HttpURLConnection? = null
        try {
            conn = (URL(base + path).openConnection() as HttpURLConnection).apply {
                connectTimeout = timeoutMs
                readTimeout = timeoutMs
                useCaches = false
            }
            if (conn.responseCode != 200) null
            else conn.inputStream.bufferedReader().use { it.readText() }
        } catch (_: Exception) {
            null
        } finally {
            conn?.disconnect()
        }
    }

    suspend fun estado(): Estado? = get("/estado", 1500)?.let {
        try { Estado.parse(it) } catch (_: Exception) { null }
    }

    /** Comandos de la página: iniciar, pausa, reanudar, barrer, s0/s90/s180, dizq/dcen/dder, manual. */
    suspend fun cmd(c: String): Boolean = get("/cmd?c=$c", 800) != null

    /** Modo manual (firmware 2.7): v -255..255 (+ adelante), g -100..100 (+ derecha). */
    suspend fun manual(v: Int, g: Int): Boolean = get("/cmd?c=mv&v=$v&g=$g", 500) != null
}
