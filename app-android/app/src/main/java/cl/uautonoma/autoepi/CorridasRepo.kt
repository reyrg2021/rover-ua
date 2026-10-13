package cl.uautonoma.autoepi

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/**
 * Guarda las corridas en un archivo JSON del teléfono y las exporta como CSV.
 * Sin base de datos: para 20 a 50 corridas, un archivo basta.
 */
class CorridasRepo(private val ctx: Context) {
    private val archivo get() = File(ctx.filesDir, "corridas.json")

    fun cargar(): List<CorridaGuardada> {
        if (!archivo.exists()) return emptyList()
        return try {
            val arr = JSONArray(archivo.readText())
            (0 until arr.length()).map { i ->
                val o = arr.getJSONObject(i)
                CorridaGuardada(
                    id = o.getLong("id"), fecha = o.getString("fecha"), modo = o.getString("modo"),
                    tiempoS = o.getDouble("tiempoS"), evasiones = o.getInt("evasiones"),
                    barridos = o.getInt("barridos"), atrapado = o.getBoolean("atrapado"),
                    colisiones = o.getInt("colisiones"), completada = o.getBoolean("completada"),
                    notas = o.optString("notas", "")
                )
            }
        } catch (_: Exception) { emptyList() }
    }

    fun guardar(lista: List<CorridaGuardada>) {
        val arr = JSONArray()
        lista.forEach { c ->
            arr.put(JSONObject().apply {
                put("id", c.id); put("fecha", c.fecha); put("modo", c.modo)
                put("tiempoS", c.tiempoS); put("evasiones", c.evasiones); put("barridos", c.barridos)
                put("atrapado", c.atrapado); put("colisiones", c.colisiones)
                put("completada", c.completada); put("notas", c.notas)
            })
        }
        archivo.writeText(arr.toString())
    }

    /** Escribe el CSV en caché (para compartir con FileProvider) y lo devuelve. */
    fun exportarCsv(lista: List<CorridaGuardada>): File {
        val f = File(ctx.cacheDir, "corridas_autoepi.csv")
        val sb = StringBuilder("n,fecha,version,tiempo_s,evasiones,barridos,atrapado,colisiones,completada,notas\n")
        lista.forEachIndexed { i, c ->
            sb.append(i + 1).append(',').append(c.fecha).append(',').append(c.modo).append(',')
                .append(String.format(java.util.Locale.US, "%.1f", c.tiempoS)).append(',')
                .append(c.evasiones).append(',').append(c.barridos).append(',')
                .append(if (c.atrapado) 1 else 0).append(',').append(c.colisiones).append(',')
                .append(if (c.completada) 1 else 0).append(',')
                .append('"').append(c.notas.replace("\"", "'")).append('"').append('\n')
        }
        f.writeText(sb.toString())
        return f
    }
}
