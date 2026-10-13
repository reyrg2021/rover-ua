/*
 * =====================================================================
 *  PASO 2 (WiFi) - MOTORES DC + SERVO DE DIRECCIÓN, CONTROL DESDE EL CELULAR
 *  Sin sensores, sin servo de barrido. Para probar y afinar en el piso.
 * =====================================================================
 *
 *  USO
 *  1. Conectar el celular a la red "AutoEPI-Radar" (clave "epi2026auto").
 *  2. Abrir http://192.168.4.1 en el navegador.
 *  3. La página tiene botones para dirección, motores, velocidad, las dos
 *     maniobras de evasión y la prueba de carga, y muestra un registro
 *     (lo mismo que saldría por el Monitor Serie).
 *
 *  AJUSTES DESDE LA PÁGINA (sin recompilar)
 *  - Centro de la dirección: botones "Centro -1°" / "Centro +1°". Mayor =
 *    más a la izquierda. Cuando avance recto 2 m, anotar el valor y
 *    ponerlo en ANG_CENTRO del código final.
 *  - Velocidad: deslizador. Bajar hasta que el auto cargado deje de
 *    moverse: ese valor + 20 es el VEL_PRECAUCION del código final.
 *  - T_RETROCESO y T_AVANCE_ARCO: campos numéricos (ms). Ajustar con las
 *    maniobras "Evasión izq / der" hasta que la trompa termine ~45° hacia
 *    el lado pedido.
 *
 *  SEGURIDAD: si la página deja de consultar el estado por más de 2 s
 *  (celular desconectado o pantalla apagada) y los motores están andando,
 *  el auto se detiene solo. No reemplaza un interruptor físico.
 *
 *  Calibración de la dirección (11-oct-2026): centro 100°, tope izquierdo
 *  120°, tope derecho 75°. Motor A = rueda derecha, motor B = rueda izquierda.
 */

#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_system.h>

// Motor A (rueda DERECHA)
#define ENA 25
#define IN1 26
#define IN2 27
// Motor B (rueda IZQUIERDA)
#define ENB 33
#define IN3 32
#define IN4 19
// Servo de dirección
#define PIN_SERVO_DIRECCION 18

const char* AP_SSID  = "AutoEPI-Radar";
const char* AP_CLAVE = "epi2026auto";

// ---------- Dirección (CALIBRADA) ----------
const int DIR_MIN = 78;    // tope derecho 75 + margen
const int DIR_MAX = 117;   // tope izquierdo 120 - margen
int ANG_CENTRO = 100;      // ajustable desde la página
const int DESVIO_SUAVE  = 10;   // suave  = centro ± 10
const int DESVIO_FUERTE = 17;   // fuerte = centro ± 17 (queda dentro de los topes)

// ---------- Velocidades ----------
int velocidad = 200;       // ajustable desde la página
const int VEL_EVASION = 180;

// ---------- Tiempos de maniobra (ms), ajustables desde la página ----------
unsigned long T_SERVO_DIRECCION = 150;
unsigned long T_RETROCESO       = 850;
unsigned long T_AVANCE_ARCO     = 500;

Servo servoDireccion;
ESP32PWM pwmA;
ESP32PWM pwmB;
WebServer server(80);

enum Lado { IZQUIERDA, DERECHA };

// ---------- Estado visible en la página ----------
int  angActual = 100;
const char* estadoMotores = "detenido";
bool motoresAndando = false;
unsigned long ultimoPing = 0;
bool ocupado = false;    // hay una maniobra en curso
bool abortar = false;    // PARAR pulsado durante una maniobra

// Registro: últimas N líneas (se ven en la página y en el Monitor Serie)
const int N_LOG = 14;
String logLineas[N_LOG];
int logIdx = 0;

void registrar(const String& s) {
  Serial.println(s);
  logLineas[logIdx] = s;
  logIdx = (logIdx + 1) % N_LOG;
}

// =====================================================
// Actuadores
// =====================================================

int angIzq(bool fuerte) { return ANG_CENTRO + (fuerte ? DESVIO_FUERTE : DESVIO_SUAVE); }
int angDer(bool fuerte) { return ANG_CENTRO - (fuerte ? DESVIO_FUERTE : DESVIO_SUAVE); }

void direccion(int ang) {
  angActual = constrain(ang, DIR_MIN, DIR_MAX);
  servoDireccion.write(angActual);
}

int anguloDireccion(Lado lado, bool fuerte) {
  return (lado == IZQUIERDA) ? angIzq(fuerte) : angDer(fuerte);
}

Lado opuesto(Lado lado) { return (lado == IZQUIERDA) ? DERECHA : IZQUIERDA; }

void avanzar(int vel) {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  pwmA.write(vel); pwmB.write(vel);
  estadoMotores = "adelante"; motoresAndando = true;
}

void retroceder(int vel) {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  pwmA.write(vel); pwmB.write(vel);
  estadoMotores = "atras"; motoresAndando = true;
}

void detener() {
  pwmA.write(0); pwmB.write(0);
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  estadoMotores = "detenido"; motoresAndando = false;
}

// Espera atendiendo la página (así el registro se sigue actualizando)
void esperar(unsigned long ms) {
  unsigned long t0 = millis();
  while (millis() - t0 < ms) {
    server.handleClient();
    delay(2);
  }
}

// =====================================================
// Maniobra de evasión (idéntica a la del código final)
// =====================================================

// Espera dentro de una maniobra; devuelve false si se pulsó PARAR
bool paso(unsigned long ms) {
  esperar(ms);
  if (abortar) { detener(); direccion(ANG_CENTRO); registrar("  maniobra abortada"); return false; }
  return true;
}

void evadir(Lado lado) {
  ocupado = true; abortar = false;
  registrar(String("EVASION hacia la ") + (lado == IZQUIERDA ? "IZQUIERDA" : "DERECHA"));
  detener();
  if (!paso(100)) { ocupado = false; return; }

  direccion(anguloDireccion(opuesto(lado), false));
  registrar("  1) retrocede con ruedas a " + String(angActual) + "° por " + String(T_RETROCESO) + " ms");
  if (!paso(T_SERVO_DIRECCION)) { ocupado = false; return; }
  retroceder(VEL_EVASION);
  if (!paso(T_RETROCESO)) { ocupado = false; return; }
  detener();
  if (!paso(50)) { ocupado = false; return; }

  direccion(anguloDireccion(lado, true));
  registrar("  2) avanza en arco con ruedas a " + String(angActual) + "° por " + String(T_AVANCE_ARCO) + " ms");
  if (!paso(T_SERVO_DIRECCION)) { ocupado = false; return; }
  avanzar(VEL_EVASION);
  if (!paso(T_AVANCE_ARCO)) { ocupado = false; return; }
  detener();

  direccion(ANG_CENTRO);
  registrar("  fin. ¿Quedo la trompa apuntando hacia ese lado?");
  ocupado = false;
}

void pruebaCarga() {
  ocupado = true; abortar = false;
  registrar("CARGA: motores a fondo + direccion tope a tope x5. Si la pagina se desconecta, hubo reset.");
  avanzar(255);
  for (int i = 1; i <= 5; i++) {
    direccion(angIzq(true)); if (!paso(600)) { ocupado = false; return; }
    direccion(angDer(true)); if (!paso(600)) { ocupado = false; return; }
  }
  detener();
  direccion(ANG_CENTRO);
  registrar("fin carga: no hubo reset.");
  ocupado = false;
}

const char* motivoReset(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "encendido normal";
    case ESP_RST_BROWNOUT: return "BROWNOUT -> ALIMENTACION INSUFICIENTE";
    case ESP_RST_PANIC:    return "PANIC (error del programa)";
    case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: return "WATCHDOG";
    case ESP_RST_SW:       return "reset por software";
    case ESP_RST_EXT:      return "boton EN";
    default:               return "desconocido";
  }
}

// =====================================================
// Web
// =====================================================

void procesarComando(const String& c, int n) {
  if (ocupado) {                       // durante una maniobra solo se acepta PARAR
    if (c == "x") { abortar = true; detener(); }
    return;
  }
  if      (c == "i")  { direccion(angIzq(false)); registrar("direccion izq suave -> " + String(angActual)); }
  else if (c == "I")  { direccion(angIzq(true));  registrar("direccion izq fuerte -> " + String(angActual)); }
  else if (c == "c")  { direccion(ANG_CENTRO);    registrar("direccion centro -> " + String(angActual)); }
  else if (c == "d")  { direccion(angDer(false)); registrar("direccion der suave -> " + String(angActual)); }
  else if (c == "D")  { direccion(angDer(true));  registrar("direccion der fuerte -> " + String(angActual)); }
  else if (c == "w")  { avanzar(velocidad);    registrar("adelante vel=" + String(velocidad)); }
  else if (c == "s")  { retroceder(velocidad); registrar("atras vel=" + String(velocidad)); }
  else if (c == "x")  { detener(); registrar("detenido"); }
  else if (c == "v")  { velocidad = constrain(n, 0, 255); if (motoresAndando) { if (estadoMotores[0]=='a' && estadoMotores[1]=='d') avanzar(velocidad); else retroceder(velocidad);} registrar("velocidad = " + String(velocidad)); }
  else if (c == "cp") { ANG_CENTRO = min(ANG_CENTRO + 1, DIR_MAX - DESVIO_FUERTE); direccion(ANG_CENTRO); registrar("ANG_CENTRO = " + String(ANG_CENTRO)); }
  else if (c == "cm") { ANG_CENTRO = max(ANG_CENTRO - 1, DIR_MIN + DESVIO_FUERTE); direccion(ANG_CENTRO); registrar("ANG_CENTRO = " + String(ANG_CENTRO)); }
  else if (c == "tr") { T_RETROCESO   = constrain(n, 100, 3000); registrar("T_RETROCESO = " + String(T_RETROCESO)); }
  else if (c == "ta") { T_AVANCE_ARCO = constrain(n, 100, 3000); registrar("T_AVANCE_ARCO = " + String(T_AVANCE_ARCO)); }
  else if (c == "l")  { evadir(IZQUIERDA); }
  else if (c == "r")  { evadir(DERECHA); }
  else if (c == "t")  { pruebaCarga(); }
}

String estadoJSON() {
  String s = "{\"vel\":" + String(velocidad) +
             ",\"ang\":" + String(angActual) +
             ",\"centro\":" + String(ANG_CENTRO) +
             ",\"tr\":" + String(T_RETROCESO) +
             ",\"ta\":" + String(T_AVANCE_ARCO) +
             ",\"motores\":\"" + String(estadoMotores) + "\",\"log\":[";
  for (int k = 0; k < N_LOG; k++) {
    int i = (logIdx + k) % N_LOG;
    if (logLineas[i].length() == 0) continue;
    String l = logLineas[i]; l.replace("\\", "\\\\"); l.replace("\"", "\\\"");
    s += "\"" + l + "\",";
  }
  if (s.endsWith(",")) s.remove(s.length() - 1);
  s += "]}";
  return s;
}

extern const char PAGINA[];

void configurarWeb() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_CLAVE);
  server.on("/", []() { server.send(200, "text/html; charset=utf-8", PAGINA); });
  server.on("/estado", []() {
    ultimoPing = millis();
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", estadoJSON());
  });
  server.on("/cmd", []() {
    ultimoPing = millis();
    server.send(200, "text/plain", "ok");   // responde antes de ejecutar maniobras largas
    procesarComando(server.arg("c"), server.arg("n").toInt());
  });
  server.begin();
  registrar("Red " + String(AP_SSID) + " lista. Abrir http://" + WiFi.softAPIP().toString());
}

// =====================================================

void setup() {
  Serial.begin(115200);
  delay(300);
  registrar("ARRANQUE paso 2 WiFi. Motivo del ultimo reset: " + String(motivoReset(esp_reset_reason())));

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  servoDireccion.setPeriodHertz(50);
  servoDireccion.attach(PIN_SERVO_DIRECCION, 500, 2400);
  direccion(ANG_CENTRO);

  pwmA.attachPin(ENA, 1000, 8);
  pwmB.attachPin(ENB, 1000, 8);
  detener();

  configurarWeb();
  ultimoPing = millis();
}

void loop() {
  server.handleClient();
  // Parada de seguridad: sin consultas del celular por 2 s con motores andando
  if (motoresAndando && millis() - ultimoPing > 2000) {
    detener();
    registrar("PARADA DE SEGURIDAD: el celular dejo de responder");
  }
  delay(2);
}

// =====================================================
// Página
// =====================================================
const char PAGINA[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Paso 2: motores y dirección</title>
<style>
:root{--tinta:#1b2a41;--tenue:#3d4e68;--cuadro:#d6e4f0;--papel:#fbfcfd;--rojo:#c62e2e;--verde:#1f7a3a}
*{box-sizing:border-box}
body{margin:0;background:var(--papel);color:var(--tinta);font:16px/1.4 system-ui,sans-serif}
main{max-width:520px;margin:0 auto;padding:12px}
h1{font-size:22px;margin:4px 0 8px}
h2{font-size:14px;color:var(--tenue);margin:14px 0 6px;text-transform:uppercase;letter-spacing:.04em}
.estado{display:grid;grid-template-columns:repeat(3,1fr);border-top:2px solid var(--tinta);border-bottom:1px solid var(--cuadro);margin-bottom:6px}
.estado div{padding:6px 4px}.estado span{display:block;font-size:12px;color:var(--tenue)}
.estado strong{font-size:22px;font-variant-numeric:tabular-nums}
.fila{display:grid;gap:8px;margin:6px 0}.f3{grid-template-columns:repeat(3,1fr)}.f2{grid-template-columns:1fr 1fr}.f5{grid-template-columns:repeat(5,1fr)}
button{font:inherit;font-weight:600;min-height:52px;border-radius:6px;border:2px solid var(--tinta);background:var(--papel);color:var(--tinta)}
button.parar{background:var(--rojo);border-color:var(--rojo);color:#fff;min-height:64px;font-size:18px}
button.ir{background:var(--tinta);color:#fff}
input[type=range]{width:100%}
input[type=number]{font:inherit;width:100%;min-height:44px;padding:4px 8px;border:2px solid var(--cuadro);border-radius:6px}
label{font-size:13px;color:var(--tenue)}
pre{background:#fff;border:1px solid var(--cuadro);border-radius:6px;padding:8px;font-size:12px;white-space:pre-wrap;min-height:120px;margin:6px 0}
</style></head><body><main>
<h1>Paso 2: motores y dirección</h1>
<div class="estado">
<div><span>Motores</span><strong id="mot">—</strong></div>
<div><span>Dirección</span><strong id="ang">—</strong></div>
<div><span>Centro</span><strong id="cen">—</strong></div>
</div>
<button class="parar" style="width:100%" onclick="cmd('x')">PARAR</button>

<h2>Motores</h2>
<div class="fila f2"><button class="ir" onclick="cmd('w')">Adelante</button><button class="ir" onclick="cmd('s')">Atrás</button></div>
<label>Velocidad: <strong id="vel">—</strong></label>
<input type="range" id="sv" min="0" max="255" step="5" value="200" onchange="cmd('v',this.value)">

<h2>Dirección</h2>
<div class="fila f5">
<button onclick="cmd('I')">Izq fuerte</button><button onclick="cmd('i')">Izq suave</button>
<button onclick="cmd('c')">Centro</button>
<button onclick="cmd('d')">Der suave</button><button onclick="cmd('D')">Der fuerte</button></div>
<div class="fila f2"><button onclick="cmd('cm')">Centro −1°</button><button onclick="cmd('cp')">Centro +1°</button></div>

<h2>Maniobras de evasión</h2>
<div class="fila f2"><button class="ir" onclick="cmd('l')">Evasión → izquierda</button><button class="ir" onclick="cmd('r')">Evasión → derecha</button></div>
<div class="fila f2">
<div><label>T_RETROCESO (ms)</label><input type="number" id="tr" step="50" onchange="cmd('tr',this.value)"></div>
<div><label>T_AVANCE_ARCO (ms)</label><input type="number" id="ta" step="50" onchange="cmd('ta',this.value)"></div>
</div>

<h2>Prueba de carga</h2>
<button style="width:100%" onclick="cmd('t')">Motores a fondo + dirección tope a tope ×5</button>

<h2>Registro</h2>
<pre id="log">Conectando con el auto…</pre>
</main>
<script>
let editando=false;
function cmd(c,n){fetch('/cmd?c='+c+(n!==undefined?'&n='+n:'')).catch(()=>{});}
['tr','ta'].forEach(id=>{const e=document.getElementById(id);e.onfocus=()=>editando=true;e.onblur=()=>editando=false;});
function ciclo(){
  fetch('/estado',{cache:'no-store'}).then(r=>r.json()).then(e=>{
    document.getElementById('mot').textContent=e.motores;
    document.getElementById('ang').textContent=e.ang+'°';
    document.getElementById('cen').textContent=e.centro+'°';
    document.getElementById('vel').textContent=e.vel;
    if(!editando){document.getElementById('tr').value=e.tr;document.getElementById('ta').value=e.ta;}
    document.getElementById('log').textContent=e.log.join('\n');
  }).catch(()=>{document.getElementById('log').textContent='Sin conexión con el auto. Si los motores andaban, se detuvieron solos.';})
  .finally(()=>setTimeout(ciclo,500));
}
ciclo();
</script></body></html>)rawliteral";
