package cl.uautonoma.autoepi

import org.json.JSONObject

/** Último barrido publicado por el auto (igual que la página web). */
data class Barrido(
    val id: Long,
    val n: Int,
    val paso: Int,
    val umbral: Int,
    val ang: List<Int>,
    val d: List<Long>,
    val huecoExiste: Boolean,
    val huecoIni: Int,
    val huecoFin: Int,
    val huecoCentro: Int
)

/** Contadores de la corrida en curso. */
data class Corrida(val tMs: Long, val evasiones: Int, val barridos: Int, val atrapamientos: Int)

/** Todo lo que devuelve /estado. */
data class Estado(
    val modo: String,          // "v1" o "v2"
    val estado: String,        // "marcha" | "pausa" | "atrapado" | "manual"
    val manual: Boolean,
    val izq: Long, val cen: Long, val der: Long,
    val zona: String,          // "libre" | "precaucion" | "CRITICA"
    val vel: Int,
    val ang: Int,
    val log: List<String>,
    val barrido: Barrido,
    val corrida: Corrida
) {
    companion object {
        fun parse(json: String): Estado {
            val o = JSONObject(json)
            val sens = o.optJSONObject("sens")
            val b = o.getJSONObject("barrido")
            val h = b.getJSONObject("hueco")
            val c = o.getJSONObject("corrida")
            val angs = b.getJSONArray("ang"); val ds = b.getJSONArray("d")
            val logs = o.optJSONArray("log")
            return Estado(
                modo = o.optString("modo", "?"),
                estado = o.optString("estado", "?"),
                manual = o.optBoolean("manual", false),
                izq = sens?.optLong("izq", 999) ?: 999,
                cen = sens?.optLong("cen", 999) ?: 999,
                der = sens?.optLong("der", 999) ?: 999,
                zona = sens?.optString("zona", "-") ?: "-",
                vel = sens?.optInt("vel", 0) ?: 0,
                ang = sens?.optInt("ang", 0) ?: 0,
                log = (0 until (logs?.length() ?: 0)).map { logs!!.getString(it) },
                barrido = Barrido(
                    id = b.optLong("id", 0), n = b.optInt("n", 0), paso = b.optInt("paso", 15),
                    umbral = b.optInt("umbral", 50),
                    ang = (0 until angs.length()).map { angs.getInt(it) },
                    d = (0 until ds.length()).map { ds.getLong(it) },
                    huecoExiste = h.optBoolean("existe", false),
                    huecoIni = h.optInt("ini", -1), huecoFin = h.optInt("fin", -1),
                    huecoCentro = h.optInt("centro", 0)
                ),
                corrida = Corrida(
                    tMs = c.optLong("t", 0), evasiones = c.optInt("evasiones", 0),
                    barridos = c.optInt("barridos", 0), atrapamientos = c.optInt("atrapamientos", 0)
                )
            )
        }
    }
}

/** Una corrida guardada en el teléfono (fila de la planilla). */
data class CorridaGuardada(
    val id: Long,
    val fecha: String,
    val modo: String,
    val tiempoS: Double,
    val evasiones: Int,
    val barridos: Int,
    val atrapado: Boolean,
    val colisiones: Int,       // lo anota el observador en la app
    val completada: Boolean,   // cruzó la meta por sus propios medios
    val notas: String
)
