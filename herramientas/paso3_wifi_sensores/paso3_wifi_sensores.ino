/*
 * =====================================================================
 *  PASO 3 (WiFi) - TRES HC-SR04 + SERVO DE BARRIDO, SIN MOTORES
 *  Lecturas en vivo y barrido desde el celular.
 * =====================================================================
 *
 *  USO: red "AutoEPI-Radar" (clave "epi2026auto"), http://192.168.4.1
 *
 *  La página muestra:
 *  - IZQ / CENT / DER en vivo (cada 300 ms) y la ZONA que calcularía el
 *    algoritmo con el sensor central (libre > 40, precaución 20-40,
 *    crítica <= 20, con histéresis de 5 cm).
 *  - Botones 45° / 90° / 135° para el servo de barrido. En 90° el sensor
 *    debe mirar exactamente al frente; si no, ajustar OFFSET_CENTRO_BARRIDO.
 *  - "Barrer": barrido 45..135 cada 15° (mediana de 3), fusión de los
 *    laterales y el hueco que elegiría la versión 2.
 *
 *  QUÉ VERIFICAR
 *  1. Cada sensor con la mano a 10, 20 y 40 cm: lecturas estables ±2 cm.
 *     "sin eco" permanente = cable Echo/GND suelto o VCC sin 5 V.
 *     "0-2 cm" permanente = Echo pegado o Trig/Echo cruzados.
 *  2. Sensor central en 45° y en 135° apuntando a una pieza vacía: debe
 *     dar > 100 cm. Si da 5-10 cm, está viendo los soportes laterales:
 *     usar 50/130.
 *  3. Barrido con una caja a 30 cm a la izquierda y nada a la derecha: el
 *     hueco debe salir a la derecha (centro negativo).
 */

#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_system.h>

#define PIN_SERVO_BARRIDO 23

#define Echo_1 5    // derecho
#define Trig_1 17
#define Echo_2 4    // central (sobre el servo)
#define Trig_2 16
#define Echo_3 22   // izquierdo
#define Trig_3 21

const char* AP_SSID  = "AutoEPI-Radar";
const char* AP_CLAVE = "epi2026auto";

// ---------- Umbrales (iguales al código final) ----------
const int DIST_CRITICA      = 20;
const int DIST_PRECAUCION   = 40;
const int MARGEN_HISTERESIS = 5;
const unsigned long T_ENTRE_SENSORES = 40;

// ---------- Barrido (iguales al código final) ----------
const int ANG_BARRIDO_MIN = 45;
const int ANG_BARRIDO_MAX = 135;
const int PASO_BARRIDO    = 15;
const int N_BARRIDO = (ANG_BARRIDO_MAX - ANG_BARRIDO_MIN) / PASO_BARRIDO + 1;
const int  OFFSET_CENTRO_BARRIDO = 0;     // ajustar si en 90° no mira al frente
const bool CERO_A_LA_DERECHA     = true;  // verificado 11-oct-2026
const int  LECTURAS_POR_ANGULO   = 3;
const unsigned long T_ASENTAR_SERVO  = 60;
const unsigned long T_ENTRE_LECTURAS = 25;
const bool FUSIONAR_LATERALES = true;
const int  ANG_SENSOR_LATERAL = 45;       // MEDIR: ángulo de montaje de los laterales
const int UMBRAL_HUECO_CM    = 50;
const int MIN_LECTURAS_HUECO = 2;

Servo servoBarrido;
WebServer server(80);

enum Zona { LIBRE, PRECAUCION, CRITICA };
Zona zonaAnterior = LIBRE;
struct Hueco { bool existe; int ini, fin, centro; };

long dIzq = 999, dCen = 999, dDer = 999;
unsigned long tUltimaLectura = 0;
bool leyendoEnVivo = true;
int angServo = 90;

long distBarrido[N_BARRIDO];
int  angBarrido[N_BARRIDO];
long latIzqBarrido = 999, latDerBarrido = 999;
Hueco ultimoHueco = { false, -1, -1, 0 };
unsigned long numBarrido = 0;
bool solicitudBarrido = false;

const int N_LOG = 10;
String logLineas[N_LOG];
int logIdx = 0;
void registrar(const String& s) {
  Serial.println(s);
  logLineas[logIdx] = s;
  logIdx = (logIdx + 1) % N_LOG;
}

// =====================================================
// Sensores (idéntico al código final)
// =====================================================

long leerDistanciaCm(int pinTrig, int pinEcho) {
  digitalWrite(pinTrig, LOW);
  delayMicroseconds(2);
  digitalWrite(pinTrig, HIGH);
  delayMicroseconds(10);
  digitalWrite(pinTrig, LOW);
  long duracion = pulseIn(pinEcho, HIGH, 30000);
  if (duracion == 0) return 999;
  return duracion * 0.0343 / 2;
}

void esperar(unsigned long ms) {
  unsigned long t0 = millis();
  while (millis() - t0 < ms) { server.handleClient(); delay(2); }
}

long leerMediana(int pinTrig, int pinEcho, int n) {
  long v[5];
  n = constrain(n, 1, 5);
  for (int k = 0; k < n; k++) {
    v[k] = leerDistanciaCm(pinTrig, pinEcho);
    if (k < n - 1) esperar(T_ENTRE_LECTURAS);
  }
  for (int a = 0; a < n - 1; a++)
    for (int b = 0; b < n - 1 - a; b++)
      if (v[b] > v[b + 1]) { long t = v[b]; v[b] = v[b + 1]; v[b + 1] = t; }
  return v[n / 2];
}

Zona determinarZona(long distCent, Zona anterior) {
  switch (anterior) {
    case CRITICA:
      if (distCent > DIST_CRITICA + MARGEN_HISTERESIS) return PRECAUCION;
      return CRITICA;
    case PRECAUCION:
      if (distCent <= DIST_CRITICA) return CRITICA;
      if (distCent > DIST_PRECAUCION + MARGEN_HISTERESIS) return LIBRE;
      return PRECAUCION;
    default:
      if (distCent <= DIST_CRITICA) return CRITICA;
      if (distCent <= DIST_PRECAUCION) return PRECAUCION;
      return LIBRE;
  }
}

const char* nombreZona(Zona z) {
  return z == CRITICA ? "CRITICA" : (z == PRECAUCION ? "precaucion" : "libre");
}

// =====================================================
// Barrido (idéntico al código final v2.2)
// =====================================================

void moverSensor(int a) {
  angServo = constrain(a + OFFSET_CENTRO_BARRIDO, 0, 180);
  servoBarrido.write(angServo);
}

int anguloRespectoFrente(int a) {
  return CERO_A_LA_DERECHA ? (a - 90) : (90 - a);
}

int indiceMasCercano(int angFrente) {
  int mejor = 0;
  for (int i = 1; i < N_BARRIDO; i++)
    if (abs(angBarrido[i] - angFrente) < abs(angBarrido[mejor] - angFrente)) mejor = i;
  return mejor;
}

Hueco buscarHueco() {
  Hueco mejor = { false, -1, -1, 0 };
  int mejorAncho = 0, i = 0;
  while (i < N_BARRIDO) {
    if (distBarrido[i] <= UMBRAL_HUECO_CM) { i++; continue; }
    int ini = i;
    while (i < N_BARRIDO && distBarrido[i] > UMBRAL_HUECO_CM) i++;
    int fin = i - 1, ancho = fin - ini + 1;
    int centro = (angBarrido[ini] + angBarrido[fin]) / 2;
    bool esMejor = (ancho > mejorAncho) || (ancho == mejorAncho && abs(centro) < abs(mejor.centro));
    if (ancho >= MIN_LECTURAS_HUECO && esMejor) {
      mejor = { true, ini, fin, centro };
      mejorAncho = ancho;
    }
  }
  return mejor;
}

void barrer() {
  leyendoEnVivo = false;
  registrar("Barrido " + String(numBarrido + 1) + " iniciado");
  moverSensor(ANG_BARRIDO_MIN);
  esperar(250);
  for (int i = 0; i < N_BARRIDO; i++) {
    int a = ANG_BARRIDO_MIN + i * PASO_BARRIDO;
    moverSensor(a);
    esperar(T_ASENTAR_SERVO);
    distBarrido[i] = leerMediana(Trig_2, Echo_2, LECTURAS_POR_ANGULO);
    angBarrido[i]  = anguloRespectoFrente(a);
  }
  moverSensor(90);
  numBarrido++;
  if (FUSIONAR_LATERALES) {
    latIzqBarrido = leerMediana(Trig_3, Echo_3, LECTURAS_POR_ANGULO);
    esperar(T_ENTRE_SENSORES);
    latDerBarrido = leerMediana(Trig_1, Echo_1, LECTURAS_POR_ANGULO);
    int iI = indiceMasCercano(+ANG_SENSOR_LATERAL), iD = indiceMasCercano(-ANG_SENSOR_LATERAL);
    if (latIzqBarrido < distBarrido[iI]) distBarrido[iI] = latIzqBarrido;
    if (latDerBarrido < distBarrido[iD]) distBarrido[iD] = latDerBarrido;
  }
  ultimoHueco = buscarHueco();
  for (int i = 0; i < N_BARRIDO; i++) Serial.printf("B,%lu,%d,%ld\n", numBarrido, angBarrido[i], distBarrido[i]);
  if (ultimoHueco.existe) registrar("HUECO centro=" + String(ultimoHueco.centro) + "° (" + (ultimoHueco.centro > 0 ? "izquierda" : (ultimoHueco.centro < 0 ? "derecha" : "frente")) + "), lecturas=" + String(ultimoHueco.fin - ultimoHueco.ini + 1));
  else registrar("HUECO: ninguno");
  esperar(250);
  leyendoEnVivo = true;
}

// =====================================================
// Web
// =====================================================

String estadoJSON() {
  String s = "{\"izq\":" + String(dIzq) + ",\"cen\":" + String(dCen) + ",\"der\":" + String(dDer) +
             ",\"zona\":\"" + nombreZona(zonaAnterior) + "\",\"servo\":" + String(angServo) +
             ",\"nb\":" + String(numBarrido) + ",\"latI\":" + String(latIzqBarrido) + ",\"latD\":" + String(latDerBarrido) + ",\"ang\":[";
  int n = numBarrido ? N_BARRIDO : 0;
  for (int i = 0; i < n; i++) { if (i) s += ","; s += angBarrido[i]; }
  s += "],\"d\":[";
  for (int i = 0; i < n; i++) { if (i) s += ","; s += distBarrido[i]; }
  s += "],\"hueco\":{\"existe\":" + String(ultimoHueco.existe ? "true" : "false") +
       ",\"ini\":" + String(ultimoHueco.ini) + ",\"fin\":" + String(ultimoHueco.fin) + ",\"centro\":" + String(ultimoHueco.centro) + "},\"log\":[";
  for (int k = 0; k < N_LOG; k++) {
    int i = (logIdx + k) % N_LOG;
    if (!logLineas[i].length()) continue;
    String l = logLineas[i]; l.replace("\"", "'");
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
  server.on("/estado", []() { server.sendHeader("Cache-Control", "no-store"); server.send(200, "application/json", estadoJSON()); });
  server.on("/cmd", []() {
    String c = server.arg("c");
    server.send(200, "text/plain", "ok");
    if      (c == "s45")  { moverSensor(45);  registrar("sensor -> 45 (debe mirar a la DERECHA)"); }
    else if (c == "s90")  { moverSensor(90);  registrar("sensor -> 90 (debe mirar al FRENTE)"); }
    else if (c == "s135") { moverSensor(135); registrar("sensor -> 135 (debe mirar a la IZQUIERDA)"); }
    else if (c == "barrer") solicitudBarrido = true;
  });
  server.begin();
  registrar("Red " + String(AP_SSID) + " lista. Abrir http://" + WiFi.softAPIP().toString());
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("ARRANQUE paso 3. Reset: %d\n", (int)esp_reset_reason());
  pinMode(Echo_1, INPUT); pinMode(Trig_1, OUTPUT);
  pinMode(Echo_2, INPUT); pinMode(Trig_2, OUTPUT);
  pinMode(Echo_3, INPUT); pinMode(Trig_3, OUTPUT);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  servoBarrido.setPeriodHertz(50);
  servoBarrido.attach(PIN_SERVO_BARRIDO, 500, 2400);
  moverSensor(90);
  configurarWeb();
}

void loop() {
  server.handleClient();
  if (solicitudBarrido) { solicitudBarrido = false; barrer(); }
  if (leyendoEnVivo && millis() - tUltimaLectura > 300) {
    dIzq = leerDistanciaCm(Trig_3, Echo_3);
    esperar(T_ENTRE_SENSORES);
    dCen = leerDistanciaCm(Trig_2, Echo_2);
    esperar(T_ENTRE_SENSORES);
    dDer = leerDistanciaCm(Trig_1, Echo_1);
    zonaAnterior = determinarZona(dCen, zonaAnterior);
    Serial.printf("IZQ: %ld | CENT: %ld | DER: %ld | zona %s\n", dIzq, dCen, dDer, nombreZona(zonaAnterior));
    tUltimaLectura = millis();
  }
  delay(2);
}

// =====================================================
const char PAGINA[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Paso 3: sensores</title>
<style>
:root{--tinta:#1b2a41;--tenue:#3d4e68;--cuadro:#d6e4f0;--papel:#fbfcfd;--rojo:#c62e2e;--ambar:#b8860b;--verde:#1f7a3a}
*{box-sizing:border-box}body{margin:0;background:var(--papel);color:var(--tinta);font:16px/1.4 system-ui,sans-serif}
main{max-width:520px;margin:0 auto;padding:12px}h1{font-size:22px;margin:4px 0 8px}
h2{font-size:14px;color:var(--tenue);margin:14px 0 6px;text-transform:uppercase;letter-spacing:.04em}
.estado{display:grid;grid-template-columns:repeat(3,1fr);border-top:2px solid var(--tinta);border-bottom:1px solid var(--cuadro)}
.estado div{padding:6px 4px;text-align:center}.estado span{display:block;font-size:12px;color:var(--tenue)}
.estado strong{font-size:30px;font-variant-numeric:tabular-nums}
.zona{text-align:center;font-weight:700;font-size:20px;padding:8px;border-radius:6px;margin:8px 0}
.libre{background:#dff3e4;color:var(--verde)}.precaucion{background:#fff1c9;color:var(--ambar)}.CRITICA{background:#fadada;color:var(--rojo)}
.fila{display:grid;gap:8px;margin:6px 0}.f3{grid-template-columns:repeat(3,1fr)}
button{font:inherit;font-weight:600;min-height:52px;border-radius:6px;border:2px solid var(--tinta);background:var(--papel);color:var(--tinta)}
button.ir{background:var(--tinta);color:#fff;width:100%}
table{width:100%;border-collapse:collapse;font-variant-numeric:tabular-nums}th,td{padding:5px 6px;border-bottom:1px solid var(--cuadro);text-align:right}
th:first-child,td:first-child{text-align:left}tr.libre td{color:var(--verde)}tr.ocupado td{color:var(--rojo)}tr.hueco td{background:#fff6c2}
pre{background:#fff;border:1px solid var(--cuadro);border-radius:6px;padding:8px;font-size:12px;white-space:pre-wrap;min-height:80px}
</style></head><body><main>
<h1>Paso 3: sensores</h1>
<div class="estado">
<div><span>Izquierdo</span><strong id="izq">—</strong></div>
<div><span>Central</span><strong id="cen">—</strong></div>
<div><span>Derecho</span><strong id="der">—</strong></div>
</div>
<div id="zona" class="zona">—</div>
<h2>Servo del sensor central (ahora: <span id="servo">—</span>°)</h2>
<div class="fila f3"><button onclick="cmd('s45')">45° (der.)</button><button onclick="cmd('s90')">90° (frente)</button><button onclick="cmd('s135')">135° (izq.)</button></div>
<h2>Barrido</h2>
<button class="ir" onclick="cmd('barrer')">Barrer ahora (45° → 135°)</button>
<p id="res" style="color:var(--tenue)">Todavía no hay barridos.</p>
<table><thead><tr><th>Ángulo</th><th>Distancia</th></tr></thead><tbody id="tabla"></tbody></table>
<p id="lat" style="color:var(--tenue);font-size:14px"></p>
<h2>Registro</h2><pre id="log">Conectando…</pre>
</main>
<script>
function cmd(c){fetch('/cmd?c='+c).catch(()=>{});}
function f(d){return d>=999?'sin eco':d+' cm';}
function ciclo(){
 fetch('/estado',{cache:'no-store'}).then(r=>r.json()).then(e=>{
  izq.textContent=f(e.izq);cen.textContent=f(e.cen);der.textContent=f(e.der);
  zona.textContent='Zona: '+e.zona;zona.className='zona '+e.zona;
  servo.textContent=e.servo;
  let filas='';
  for(let i=0;i<e.ang.length;i++){
   const a=e.ang[i],d=e.d[i];
   const enHueco=e.hueco.existe&&i>=e.hueco.ini&&i<=e.hueco.fin;
   filas+='<tr class="'+(enHueco?'hueco ':'')+(d>50?'libre':'ocupado')+'"><td>'+(a>0?a+'° izq.':a<0?(-a)+'° der.':'0° frente')+'</td><td>'+f(d)+'</td></tr>';
  }
  tabla.innerHTML=filas;
  if(e.nb>0){res.textContent='Barrido '+e.nb+': '+(e.hueco.existe?'hueco elegido centrado en '+(e.hueco.centro>0?e.hueco.centro+'° a la izquierda':e.hueco.centro<0?(-e.hueco.centro)+'° a la derecha':'el frente'):'ningún hueco libre');
   lat.textContent='Laterales fusionados: izq '+f(e.latI)+', der '+f(e.latD);}
  log.textContent=e.log.join('\n');
 }).catch(()=>{log.textContent='Sin conexión con el auto.';}).finally(()=>setTimeout(ciclo,400));
}
ciclo();
</script></body></html>)rawliteral";
