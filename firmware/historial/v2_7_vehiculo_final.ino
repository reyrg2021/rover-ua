/*
 * =====================================================================
 *  VEHÍCULO AUTÓNOMO - VERSIÓN 2.7 (MODO MANUAL PARA LA APP, 12-oct-2026)
 *  Código base construido sobre el código de la EPI (versión 1)
 * =====================================================================
 *
 *  CAMBIOS 2.7
 *  - MODO MANUAL para la app Android (y para posicionar el auto en la pista):
 *      /cmd?c=manual            entra en manual (auto detenido, autonomía apagada)
 *      /cmd?c=mv&v=-255..255&g=-100..100   velocidad (+ adelante) y giro (+ derecha)
 *      /cmd?c=iniciar | reanudar  vuelve al modo autónomo
 *    En manual los sensores se siguen leyendo y publicando, pero no actúan.
 *    Si pasan más de 800 ms sin un "mv", el auto se detiene (pérdida de app).
 *  - /estado agrega "manual":true/false y el estado "manual".
 *
 *  CAMBIOS 2.6
 *  - RUMBO PROPORCIONAL (zona libre y precaución): el ángulo de dirección
 *    es KP_RUMBO x (distDer - distIzq), recortado a los topes, y la rueda
 *    trasera interior se frena en proporción. El auto se aleja de forma
 *    continua del lado más cercano, como un seguidor de pasillo, en vez de
 *    corregir a saltos. La velocidad baja linealmente entre DIST_PRECAUCION
 *    y DIST_CRITICA.
 *  - EVASIÓN EN LAZO CERRADO: ya no hay tiempos fijos. Retrocede HASTA que
 *    el sensor central vea >= DIST_SALIDA_RETROCESO (tope T_MAX_RETROCESO),
 *    avanza en arco HASTA ver >= DIST_SALIDA_ARCO (tope T_MAX_ARCO). Si
 *    sigue en crítica: 2º intento = PIVOTE sobre el eje (rueda interior en
 *    reversa) hacia el lado libre; 3º = retrocede recto y pivota al otro
 *    lado. Después, atrapado.
 *  - En modo barrido, el barrido solo elige el LADO (y si el hueco está al
 *    frente); la maniobra es la misma de lazo cerrado. En modo reactivo el
 *    lado lo dan los laterales. Así la comparación v1/v2 sigue siendo justa.
 *  - LÍMITE FÍSICO: con ±17° de servo el radio de giro es ~60-80 cm. En un
 *    rincón más chico que eso solo sale pivotando; pista con pasillos de
 *    >= 80 cm y obstáculos separados >= 1 m.
 *
 *  CAMBIOS 2.5
 *  - La página muestra en vivo IZQ / CENT / DER, la zona, la velocidad y el
 *    ángulo de dirección, y un registro de eventos (zona crítica, evasiones,
 *    huecos, atrapado, motivo del último reset). Sirve para diagnosticar
 *    en el piso sin el cable del PC.
 *  - Márgenes ampliados: DIST_CRITICA 25, DIST_PRECAUCION 50, VEL_NORMAL 200.
 *    Cada vuelta del loop tarda hasta ~250 ms (tres sensores); a 220 el auto
 *    avanzaba ~10 cm entre lecturas y llegaba a la pared antes de reaccionar.
 *  - MODO_BARRIDO = true por defecto para ver el barrido automático. Para
 *    los recorridos de la versión 1 (los del resumen), ponerlo en false.
 *
 *  CAMBIOS 2.4
 *  - TRACCIÓN DIFERENCIAL: el servo solo gira ±17°, así que en cada giro
 *    (correcciones y maniobras) la rueda trasera INTERIOR se frena o se
 *    invierte mientras la exterior empuja. El radio de giro baja mucho.
 *    Se ajusta con FACTOR_INTERNA_SUAVE y FACTOR_INTERNA_FUERTE.
 *  - PWM_MINIMO = 125: una rueda "lenta" por debajo de eso no gira; las
 *    velocidades reducidas se escalan entre PWM_MINIMO y la velocidad pedida.
 *  - ALEJARSE DE LA PARED: en zona libre, si un lateral está a menos de
 *    DIST_LATERAL_MIN, corrige el rumbo hacia el otro lado (antes el auto
 *    quedaba pegado a la pared porque el sensor central veía libre). Aplica
 *    por igual a v1 y v2, así que la comparación sigue siendo justa.
 *
 *  VALORES MEDIDOS EL 11-oct-2026 CON LOS SKETCHES PASO 1, 2 Y 3
 *  - Motor A = rueda DERECHA, motor B = rueda IZQUIERDA (cables del motor
 *    izquierdo invertidos en el borne del L298N para que giren parejos).
 *  - PWM mínimo que mueve el auto cargado en el piso: 125 -> VEL_PRECAUCION 145.
 *  - Dirección: centro 100°, topes 75°/120°, auto avanza recto con 100.
 *  - Tiempos de maniobra en el piso: T_RETROCESO 500, T_AVANCE_ARCO 500.
 *  - Echo del sensor derecho movido a GPIO 12.
 *
 *  CAMBIOS 2.2 (11-oct-2026, tarde)
 *  - Barrido acotado a 45°..135° (±45° del frente, 7 lecturas): más allá
 *    el sensor central apunta en paralelo a los laterales y roza sus
 *    soportes. Verificado: 30° mira a la derecha -> CERO_A_LA_DERECHA = true.
 *  - Los sensores laterales entran al mapa: tras el giro se leen y se
 *    combinan (mínimo) con la celda más cercana a su ángulo de montaje
 *    (ANG_SENSOR_LATERAL). Los laterales están en las esquinas y ven lo
 *    que el sensor central, en el medio, no alcanza a 20 cm.
 *  - LIMITE_FRONTAL_GRADOS baja a 35°: con el barrido en ±45° la maniobra
 *    de estacionamiento no se usaría nunca; ahora un hueco a 40-45° la usa.
 *
 *  CAMBIOS 2.1 (11-oct-2026)
 *  - Dirección calibrada: centro 100°, tope izquierdo 120°, tope derecho
 *    75° (medidos con calibracion_servos.ino). Todos los ángulos pasan por
 *    direccion(), que recorta a DIR_MIN..DIR_MAX: el servo nunca puede
 *    forzar un tope aunque la lógica lo pida.
 *  - Barrido acotado a 30°..150° (±60° del frente): a 0° y 180° el sensor
 *    mira los soportes de los sensores laterales, y el auto no puede salir
 *    hacia esos ángulos sin maniobra de todas formas. 9 lecturas en vez de 13.
 *  - PWM de motores con ESP32PWM (misma librería que los servos) en vez de
 *    analogWrite, para que no compitan por los timers del ESP32.
 *  - "Sin eco" (999) en el sensor central ya no se trata como vía libre a
 *    velocidad máxima: avanza recto a VEL_PRECAUCION. Afecta por igual a
 *    ambos modos, así que la comparación v1/v2 sigue siendo justa.
 *  - Pausa entre sensores de 15 a 40 ms (menos ecos cruzados).
 *
 *  QUÉ CAMBIA RESPECTO DE LA VERSIÓN 1
 *  - El sensor central va montado sobre un micro servo (SG90). Al entrar
 *    en zona crítica, el auto se detiene, barre de 0° a 180° y arma un
 *    mapa local de lo que tiene adelante.
 *  - La salida se elige con el hueco libre más ancho del barrido, en vez
 *    de comparar solo los dos sensores laterales.
 *  - La maniobra de estacionamiento está corregida: para salir hacia un
 *    lado, retrocede con las ruedas giradas hacia el lado CONTRARIO (así
 *    la trompa gira hacia el lado elegido) y luego avanza girando hacia
 *    ese lado.
 *  - El ESP32 crea su propia red WiFi y muestra el mapa en el celular.
 *  - Con MODO_BARRIDO = false se comporta como la versión 1 (con la
 *    maniobra corregida). Así se comparan ambas versiones con el mismo
 *    programa y en las mismas condiciones.
 *
 *  CONEXIONES NUEVAS
 *  - Señal del servo de barrido: GPIO 23.
 *  - Los dos servos se alimentan con 5 V estables (no desde el ESP32),
 *    con tierra común y un condensador de 470 a 1000 µF junto a ellos:
 *    sus picos de corriente pueden reiniciar el ESP32.
 *  - El sensor central ahora gira: usen cable flexible con holgura y
 *    aseguren bien su GND.
 *
 *  USO
 *  1. Conecten el celular a la red "AutoEPI-Radar" (clave "epi2026auto")
 *     y abran http://192.168.4.1 en el navegador.
 *  2. "Iniciar corrida" pone en marcha el auto y reinicia el cronómetro
 *     y los contadores. "Pausar" lo detiene de inmediato, incluso en
 *     medio de una maniobra (no reemplaza un interruptor físico).
 *  3. "Barrer ahora" hace un barrido con el auto detenido: sirve para la
 *     prueba de precisión con objetos en posiciones conocidas.
 *  4. El Monitor Serie (115200 baudios) registra cada barrido en líneas
 *     "B,barrido,ángulo,distancia" y el resumen de cada corrida.
 *
 *  CALIBRACIÓN (auto en pausa y sobre un soporte, con los botones de la
 *  sección "Calibración" de la página)
 *  a) Sensor 90°: debe apuntar exactamente al frente. Si no, ajusten
 *     OFFSET_CENTRO_BARRIDO.
 *  b) Sensor 0°: si apunta a la derecha, dejen CERO_A_LA_DERECHA en true;
 *     si apunta a la izquierda, cámbienlo a false.
 *  c) Dirección "Izquierda": las ruedas delanteras deben quedar giradas
 *     hacia la izquierda del auto. Si quedan hacia la derecha,
 *     intercambien los valores de ANG_IZQ_* con los de ANG_DER_*.
 *     (Ya calibrado el 11-oct-2026; repetir si se desmonta el brazo.)
 *  d) Luego, en el piso, ajusten UMBRAL_HUECO_CM y los tiempos de
 *     maniobra hasta que las salidas sean limpias.
 * =====================================================================
 */

// Librerías
#include <ESP32Servo.h>
#include <WiFi.h>       // NUEVO: red WiFi propia del auto
#include <WebServer.h>  // NUEVO: página con el mapa
#include <esp_system.h> // 2.5: motivo del reset

// =============================
// L298N - ESP32 (igual que en la versión 1)
// =============================

// Motor A = rueda DERECHA
#define ENA 25
#define IN1 26
#define IN2 27

// Motor B = rueda IZQUIERDA
#define ENB 33
#define IN3 32
#define IN4 19

// Servomotor de dirección
#define pin_S 18
Servo servoDireccion;

// PWM de los motores con la misma librería que los servos (periférico LEDC)
ESP32PWM pwmA;
ESP32PWM pwmB;

// NUEVO: servomotor de barrido, lleva el sensor central
#define PIN_SERVO_BARRIDO 23
Servo servoBarrido;

// Sensores ultrasónicos
#define Echo_1 12  // derecho (movido desde el 5). OJO: GPIO 12 es pin de arranque;
                   // si el ESP32 no bootea con el sensor conectado, usar 34/35/36/39
#define Trig_1 17

#define Echo_2 4   // central: AHORA VA SOBRE EL SERVO DE BARRIDO
#define Trig_2 16

#define Echo_3 22  // izquierdo
#define Trig_3 21

// ---------- Modo de funcionamiento ----------
// true:  versión 2, elige la salida con el barrido
// false: versión 1, elige con los sensores laterales (maniobra corregida)
const bool MODO_BARRIDO = true;    // false = versión 1 (reactiva, la del resumen de Temuco)
// true: parte detenido y espera "Iniciar corrida" desde la página
const bool ARRANCAR_EN_PAUSA = true;

// ---------- Red WiFi propia del auto ----------
const char* AP_SSID  = "AutoEPI-Radar";
const char* AP_CLAVE = "epi2026auto";  // mínimo 8 caracteres

// ---------- Umbrales de distancia (igual que en la versión 1) ----------
const int DIST_CRITICA      = 25; // cm -> maniobra de evasión (2.5: antes 20)
const int DIST_PRECAUCION   = 50; // cm -> reducir velocidad / corregir rumbo (2.5: antes 40)
const int MARGEN_HISTERESIS = 5;  // cm de margen para no oscilar entre zonas

// ---------- Ángulos del servo de dirección (CALIBRADOS 11-oct-2026) ----------
// Medidos con calibracion_servos.ino: centro 100°, izquierda hasta 120°,
// derecha hasta 75°. En este auto, ángulo MAYOR = ruedas a la IZQUIERDA.
// Si se desvía al avanzar recto, corregir ANG_CENTRO de a 1°.
const int DIR_MIN        = 78;   // tope derecho (75) + 3° de margen
const int DIR_MAX        = 117;  // tope izquierdo (120) - 3° de margen
const int ANG_CENTRO     = 100;
const int ANG_IZQ_SUAVE  = 110;
const int ANG_DER_SUAVE  = 89;
const int ANG_IZQ_FUERTE = 117;
const int ANG_DER_FUERTE = 78;

// ÚNICA función que escribe el servo de dirección: recorta a los topes medidos
int telAng = 100;         // 2.5: último ángulo de dirección (telemetría)
void direccion(int ang) {
  telAng = constrain(ang, DIR_MIN, DIR_MAX);
  servoDireccion.write(telAng);
}

// ---------- Velocidades (igual que en la versión 1) ----------
const int VEL_NORMAL     = 200;  // 2.5: antes 220
const int VEL_PRECAUCION = 145;  // PWM mínimo en piso (125) + 20
const int VEL_EVASION    = 180;
const int PWM_MINIMO     = 125;  // medido: por debajo la rueda no gira

// ---------- NUEVO 2.4: tracción diferencial ----------
// Fracción de velocidad de la rueda trasera INTERIOR respecto de la exterior.
//   1.0 = ambas iguales (sin asistencia)   0.0 = interior detenida (pivota sobre ella)
//   negativo = interior gira al revés (giro más cerrado todavía, p.ej. -0.4)
const float FACTOR_INTERNA_SUAVE  = 0.45f;  // correcciones de rumbo
const float FACTOR_INTERNA_FUERTE = 0.0f;   // maniobras de evasión; probar -0.4 si gira poco
const int   DESVIO_MAX            = 17;     // grados de servo del ángulo fuerte respecto del centro

// ---------- Tiempos de maniobra (ms) ----------
const unsigned long T_SERVO_DIRECCION     = 150; // CALIBRAR: que las ruedas alcancen a girar
// 2.6: la evasión es en lazo cerrado; estos son TOPES, no duraciones fijas
const unsigned long T_MAX_RETROCESO = 1500; // ms máximos retrocediendo si el frente no se despeja
const unsigned long T_MAX_ARCO      = 1500; // ms máximos avanzando en arco
const unsigned long T_PIVOTE        = 900;  // ms de giro sobre el eje (≈ 60-90° en piso liso; CALIBRAR)
const unsigned long T_MUESTREO      = 60;   // ms entre lecturas del sensor central durante la maniobra
const int DIST_SALIDA_RETROCESO = 45;       // cm: retrocede hasta ver esto al frente
const int DIST_SALIDA_ARCO      = 60;       // cm: avanza en arco hasta ver esto al frente
const int MAX_INTENTOS_EVASION  = 3;

// ---------- 2.6: rumbo proporcional ----------
const float KP_RUMBO        = 0.35f; // grados de servo por cm de diferencia (der - izq)
const int   DIST_LAT_ACTIVA = 60;    // cm: el control lateral actúa si algún lateral está más cerca
const long  LAT_MAX         = 100;   // cm: los "sin eco" (999) se recortan a esto para el cálculo

// ---------- NUEVO: parámetros del barrido ----------
const int ANG_BARRIDO_MIN = 45;   // ±45° del frente; más allá roza los soportes laterales
const int ANG_BARRIDO_MAX = 135;
const int PASO_BARRIDO    = 15;   // 7 lecturas entre 45° y 135°
const int N_BARRIDO = (ANG_BARRIDO_MAX - ANG_BARRIDO_MIN) / PASO_BARRIDO + 1;
const int  OFFSET_CENTRO_BARRIDO = 0;    // CALIBRAR (paso a)
const bool CERO_A_LA_DERECHA     = true; // verificado 11-oct-2026: 30° mira a la derecha

// NUEVO 2.2: fusión de los sensores laterales en el mapa del barrido
const bool FUSIONAR_LATERALES = true;
const int  ANG_SENSOR_LATERAL = 45;  // MEDIR: ángulo de montaje de los laterales respecto del frente
const int  LECTURAS_POR_ANGULO   = 3;    // mediana de 3; con 1 el barrido es más rápido
const unsigned long T_ASENTAR_SERVO    = 60;  // espera tras cada paso antes de medir
const unsigned long T_ENTRE_LECTURAS   = 25;
const unsigned long T_ENTRE_SENSORES   = 40;  // antes 15: el eco de un sensor podía colarse en el siguiente
const unsigned long T_IR_AL_INICIO     = 250;
const unsigned long T_VOLVER_AL_FRENTE = 250;

// ---------- NUEVO: decisión con el barrido ----------
const int UMBRAL_HUECO_CM       = 50; // una dirección está libre si hay más de esto
const int MIN_LECTURAS_HUECO    = 2;  // lecturas libres seguidas para aceptar un hueco
const int UMBRAL_RECTO          = 10; // hueco casi al frente: basta retroceder

// ---------- Tipos ----------
enum Zona { LIBRE, PRECAUCION, CRITICA };
enum Lado { IZQUIERDA, DERECHA };
struct Hueco {
  bool existe;
  int ini, fin;  // índices de la primera y la última lectura del hueco
  int centro;    // ángulo del centro respecto del frente (+ izquierda, - derecha)
};

// ---------- Estado ----------
Zona zonaAnterior = LIBRE;

long distBarrido[N_BARRIDO];  // distancias del último barrido (cm)
int  angBarrido[N_BARRIDO];   // ángulo de cada lectura respecto del frente
Hueco ultimoHueco = { false, -1, -1, 0 };
unsigned long numBarrido = 0;

bool pausado = false;
bool atrapado = false;
bool solicitudBarrido = false;
bool modoManual = false;            // 2.7
unsigned long tUltimoManual = 0;    // 2.7: último "mv" recibido
const unsigned long T_MANUAL_WATCHDOG = 800;
bool recentrarSensor = false;

// Cronómetro y contadores de la corrida (se ven en la página)
bool corridaActiva = false;
unsigned long tAcumulado = 0, tInicioTramo = 0;
int evasionesCorrida = 0, barridosCorrida = 0, atrapamientosCorrida = 0;

WebServer server(80);
extern const char PAGINA_MAPA[];  // la página está al final del archivo

// NUEVO 2.5: telemetría para la página
long telIzq = 999, telCent = 999, telDer = 999;
const char* telZona = "-";
int telVel = 0;           // última velocidad pedida a los motores
const int N_LOG = 12;
String logLineas[N_LOG];
int logIdx = 0;
void registrar(const String& m) {
  Serial.println(m);
  logLineas[logIdx] = m;
  logIdx = (logIdx + 1) % N_LOG;
}

// ---------- Prototipos ----------
long leerDistanciaCm(int pinTrig, int pinEcho);
long leerMediana(int pinTrig, int pinEcho, int n);
Zona determinarZona(long distCent, Zona anterior);
void ruedaDerecha(int pwm);
void ruedaIzquierda(int pwm);
int  pwmRueda(float fraccion, int vel);
void traccion(int vel, bool adelante, int lento, float factor);
void avanzar(int velocidad);
void retroceder(int velocidad);
void detener();
void esperar(unsigned long ms);
int  anguloDireccion(Lado lado, bool fuerte);
Lado opuesto(Lado lado);
long leerCentro();
long mantenerHasta(int objetivo, unsigned long tMax);
long retrocederOrientando(Lado lado);
long retrocederRecto();
long avanzarEnArco(Lado lado, bool fuerte);
void pivotar(Lado lado, unsigned long ms);
bool evadirCerrado(Lado lado, bool alFrente);
void moverSensor(int angServo);
int  anguloRespectoFrente(int angServo);
void barrer();
int  indiceMasCercano(int angFrente);
void fusionarLaterales();
Hueco buscarHueco();
void imprimirBarrido();
void evadirConBarrido();
void evadirReactivo(long distIzqInicial, long distDerInicial);
void iniciarCorrida();
void pausarCorrida(bool porAtrapamiento);
void reanudarCorrida();
unsigned long tiempoCorrida();
void procesarComando(const String& c);
String estadoJSON();
void configurarWeb();

// =====================================================
// Sensores
// =====================================================

long leerDistanciaCm(int pinTrig, int pinEcho) {
  digitalWrite(pinTrig, LOW);
  delayMicroseconds(2);
  digitalWrite(pinTrig, HIGH);
  delayMicroseconds(10);
  digitalWrite(pinTrig, LOW);

  long duracion = pulseIn(pinEcho, HIGH, 30000);

  if (duracion == 0) {
    return 999; // sin señal eco
  }

  return duracion * 0.0343 / 2;
}

// NUEVO: mediana de n lecturas (descarta ecos falsos aislados)
long leerMediana(int pinTrig, int pinEcho, int n) {
  long v[5];
  n = constrain(n, 1, 5);
  for (int k = 0; k < n; k++) {
    v[k] = leerDistanciaCm(pinTrig, pinEcho);
    if (k < n - 1) esperar(T_ENTRE_LECTURAS);
  }
  for (int a = 0; a < n - 1; a++) {        // ordenamiento burbuja (n <= 5)
    for (int b = 0; b < n - 1 - a; b++) {
      if (v[b] > v[b + 1]) {
        long t = v[b];
        v[b] = v[b + 1];
        v[b + 1] = t;
      }
    }
  }
  return v[n / 2];
}

// Determina la zona actual aplicando histéresis (igual que en la versión 1)
Zona determinarZona(long distCent, Zona anterior) {
  switch (anterior) {
    case CRITICA:
      if (distCent > DIST_CRITICA + MARGEN_HISTERESIS) return PRECAUCION;
      return CRITICA;
    case PRECAUCION:
      if (distCent <= DIST_CRITICA) return CRITICA;
      if (distCent > DIST_PRECAUCION + MARGEN_HISTERESIS) return LIBRE;
      return PRECAUCION;
    default: // LIBRE
      if (distCent <= DIST_CRITICA) return CRITICA;
      if (distCent <= DIST_PRECAUCION) return PRECAUCION;
      return LIBRE;
  }
}

// =====================================================
// Motores (NUEVO: no se mueven si el auto está en pausa)
// =====================================================

// NUEVO 2.4: escribe una rueda con signo (+ adelante, - atrás, 0 detenida).
// A = rueda DERECHA (IN1/IN2/ENA), B = rueda IZQUIERDA (IN3/IN4/ENB).
void ruedaDerecha(int pwm) {
  digitalWrite(IN1, pwm > 0 ? HIGH : LOW);
  digitalWrite(IN2, pwm < 0 ? HIGH : LOW);
  pwmA.write(abs(pwm));
}
void ruedaIzquierda(int pwm) {
  digitalWrite(IN3, pwm > 0 ? HIGH : LOW);
  digitalWrite(IN4, pwm < 0 ? HIGH : LOW);
  pwmB.write(abs(pwm));
}

// Convierte una fracción (-1..1) de 'vel' en un PWM que la rueda sí pueda
// seguir: las fracciones pequeñas se escalan entre PWM_MINIMO y vel.
int pwmRueda(float fraccion, int vel) {
  if (fabs(fraccion) < 0.05f) return 0;
  int mag = PWM_MINIMO + (int)(fabs(fraccion) * (vel - PWM_MINIMO));
  mag = constrain(mag, PWM_MINIMO, 255);
  return fraccion > 0 ? mag : -mag;
}

// Tracción con asistencia: la rueda del lado 'lento' va a 'factor' de la
// otra. lento = -1 -> ambas iguales.
void traccion(int vel, bool adelante, int lento, float factor) {
  if (pausado) { detener(); return; }
  telVel = adelante ? vel : -vel;
  int signo = adelante ? 1 : -1;
  int pwmLleno = signo * constrain(vel, 0, 255);
  int pwmLento = signo * pwmRueda(factor, vel);
  if (lento == DERECHA)        { ruedaDerecha(pwmLento); ruedaIzquierda(pwmLleno); }
  else if (lento == IZQUIERDA) { ruedaDerecha(pwmLleno); ruedaIzquierda(pwmLento); }
  else                         { ruedaDerecha(pwmLleno); ruedaIzquierda(pwmLleno); }
}

// AVANZAR / RETROCEDER rectos (igual que antes)
void avanzar(int velocidad)    { traccion(velocidad, true,  -1, 1.0f); }
void retroceder(int velocidad) { traccion(velocidad, false, -1, 1.0f); }

// DETENER
void detener() {
  telVel = 0;
  pwmA.write(0);
  pwmB.write(0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// 2.7: conducción manual desde la app. v: -255..255 (+ adelante),
// giro: -100..100 (+ derecha). Escribe las ruedas directamente (no pasa por
// 'pausado') y aplica la misma tracción diferencial del modo autónomo.
void conducirManual(int v, int giro) {
  v = constrain(v, -255, 255);
  giro = constrain(giro, -100, 100);
  int desvio = giro * DESVIO_MAX / 100;            // + derecha
  direccion(ANG_CENTRO - desvio);
  if (abs(v) < PWM_MINIMO / 2) { detener(); return; }
  int vel = (abs(v) < PWM_MINIMO) ? PWM_MINIMO : abs(v);
  int signo = (v > 0) ? 1 : -1;
  float factor = 1.0f - ((float)abs(desvio) / DESVIO_MAX) * (1.0f - FACTOR_INTERNA_SUAVE);
  int pwmLleno = signo * vel;
  int pwmLento = signo * pwmRueda(factor, vel);
  telVel = signo * vel;
  if (desvio > 0)      { ruedaDerecha(pwmLento); ruedaIzquierda(pwmLleno); }
  else if (desvio < 0) { ruedaDerecha(pwmLleno); ruedaIzquierda(pwmLento); }
  else                 { ruedaDerecha(pwmLleno); ruedaIzquierda(pwmLleno); }
}

// NUEVO: espera atendiendo la página. Si se pulsa "Pausar", los motores
// se detienen de inmediato, aunque sea en medio de una maniobra.
void esperar(unsigned long ms) {
  unsigned long t0 = millis();
  while (millis() - t0 < ms) {
    server.handleClient();
    if (pausado) detener();
    delay(2);
  }
}

// =====================================================
// Dirección y maniobras (NUEVO: maniobra corregida)
// =====================================================
// Al RETROCEDER con las ruedas giradas hacia un lado, la trompa del auto
// gira hacia el lado CONTRARIO (como al estacionar). Por eso, para salir
// hacia 'lado', retrocede con las ruedas hacia el lado opuesto y luego
// avanza con las ruedas hacia 'lado'.

int anguloDireccion(Lado lado, bool fuerte) {
  if (lado == IZQUIERDA) return fuerte ? ANG_IZQ_FUERTE : ANG_IZQ_SUAVE;
  return fuerte ? ANG_DER_FUERTE : ANG_DER_SUAVE;
}

Lado opuesto(Lado lado) {
  return (lado == IZQUIERDA) ? DERECHA : IZQUIERDA;
}

// ---------- 2.6: maniobras en lazo cerrado ----------
// Todas leen el sensor central mientras se mueven y terminan cuando el
// frente se despeja o se agota el tope de tiempo. Devuelven la última
// distancia frontal vista.

long leerCentro() {
  long d = leerDistanciaCm(Trig_2, Echo_2);
  telCent = d;
  return d;
}

// Mantiene el movimiento actual hasta que el frente vea >= objetivo o pase tMax
long mantenerHasta(int objetivo, unsigned long tMax) {
  unsigned long t0 = millis();
  long d = leerCentro();
  while (millis() - t0 < tMax && d < objetivo) {
    if (pausado) { detener(); return d; }
    esperar(T_MUESTREO);
    d = leerCentro();
  }
  return d;
}

// Retrocede con la trompa girando hacia 'lado' (ruedas al opuesto, cola
// frenada del lado opuesto) hasta que el frente se despeje.
long retrocederOrientando(Lado lado) {
  detener();
  esperar(80);
  direccion(anguloDireccion(opuesto(lado), false));
  esperar(T_SERVO_DIRECCION);
  traccion(VEL_EVASION, false, opuesto(lado), FACTOR_INTERNA_SUAVE);
  long d = mantenerHasta(DIST_SALIDA_RETROCESO, T_MAX_RETROCESO);
  detener();
  esperar(50);
  return d;
}

long retrocederRecto() {
  detener();
  esperar(80);
  direccion(ANG_CENTRO);
  esperar(T_SERVO_DIRECCION);
  retroceder(VEL_EVASION);
  long d = mantenerHasta(DIST_SALIDA_RETROCESO, T_MAX_RETROCESO);
  detener();
  esperar(50);
  return d;
}

// Avanza en arco hacia 'lado' (interior frenada/invertida) hasta que el
// frente vea >= DIST_SALIDA_ARCO. Si se vuelve a acercar a crítica, corta.
long avanzarEnArco(Lado lado, bool fuerte) {
  direccion(anguloDireccion(lado, fuerte));
  esperar(T_SERVO_DIRECCION);
  traccion(VEL_EVASION, true, lado, fuerte ? FACTOR_INTERNA_FUERTE : FACTOR_INTERNA_SUAVE);
  unsigned long t0 = millis();
  long d = leerCentro();
  while (millis() - t0 < T_MAX_ARCO && d < DIST_SALIDA_ARCO) {
    if (pausado) { detener(); return d; }
    if (d <= DIST_CRITICA - 5) break;   // se está metiendo en otro obstáculo
    esperar(T_MUESTREO);
    d = leerCentro();
  }
  detener();
  return d;
}

// Gira sobre el eje hacia 'lado': rueda interior en reversa, exterior adelante
void pivotar(Lado lado, unsigned long ms) {
  direccion(anguloDireccion(lado, true));
  esperar(T_SERVO_DIRECCION);
  traccion(VEL_EVASION, true, lado, -1.0f);
  esperar(ms);
  detener();
  direccion(ANG_CENTRO);
}

// =====================================================
// NUEVO: barrido y mapa local
// =====================================================

void moverSensor(int angServo) {
  servoBarrido.write(constrain(angServo + OFFSET_CENTRO_BARRIDO, 0, 180));
}

// Ángulo respecto del frente del auto: 0 = frente,
// positivo = izquierda, negativo = derecha
int anguloRespectoFrente(int angServo) {
  return CERO_A_LA_DERECHA ? (angServo - 90) : (90 - angServo);
}

// Con el auto detenido, recorre de ANG_BARRIDO_MIN a ANG_BARRIDO_MAX
void barrer() {
  detener();
  moverSensor(ANG_BARRIDO_MIN);
  esperar(T_IR_AL_INICIO);

  for (int i = 0; i < N_BARRIDO; i++) {
    int angServo = ANG_BARRIDO_MIN + i * PASO_BARRIDO;
    moverSensor(angServo);
    esperar(T_ASENTAR_SERVO);
    distBarrido[i] = leerMediana(Trig_2, Echo_2, LECTURAS_POR_ANGULO);
    angBarrido[i]  = anguloRespectoFrente(angServo);
  }

  moverSensor(90);              // el sensor vuelve a mirar al frente
  numBarrido++;
  if (FUSIONAR_LATERALES) fusionarLaterales();
  ultimoHueco = buscarHueco();
  imprimirBarrido();
  esperar(T_VOLVER_AL_FRENTE);  // antes de avanzar, el sensor debe estar al frente
}

// NUEVO 2.2: celda del mapa cuyo ángulo está más cerca de angFrente
int indiceMasCercano(int angFrente) {
  int mejor = 0;
  for (int i = 1; i < N_BARRIDO; i++) {
    if (abs(angBarrido[i] - angFrente) < abs(angBarrido[mejor] - angFrente)) mejor = i;
  }
  return mejor;
}

// NUEVO 2.2: los laterales (fijos, en las esquinas) se combinan con la celda
// más cercana a su ángulo de montaje tomando el MÍNIMO: si cualquiera de los
// dos sensores ve algo cerca en esa dirección, la dirección está ocupada.
void fusionarLaterales() {
  long dIzq = leerMediana(Trig_3, Echo_3, LECTURAS_POR_ANGULO);
  esperar(T_ENTRE_SENSORES);
  long dDer = leerMediana(Trig_1, Echo_1, LECTURAS_POR_ANGULO);
  int iIzq = indiceMasCercano(+ANG_SENSOR_LATERAL);   // + = izquierda
  int iDer = indiceMasCercano(-ANG_SENSOR_LATERAL);
  if (dIzq < distBarrido[iIzq]) distBarrido[iIzq] = dIzq;
  if (dDer < distBarrido[iDer]) distBarrido[iDer] = dDer;
  Serial.printf("L,%lu,izq=%ld,der=%ld\n", numBarrido, dIzq, dDer);
}

// Busca el hueco libre más ancho: lecturas seguidas con más de
// UMBRAL_HUECO_CM. Se usa el centro del hueco y no la lectura máxima,
// porque un máximo aislado puede ser un eco perdido en una pared oblicua.
Hueco buscarHueco() {
  Hueco mejor = { false, -1, -1, 0 };
  int mejorAncho = 0;
  int i = 0;
  while (i < N_BARRIDO) {
    if (distBarrido[i] <= UMBRAL_HUECO_CM) {
      i++;
      continue;
    }
    int ini = i;
    while (i < N_BARRIDO && distBarrido[i] > UMBRAL_HUECO_CM) i++;
    int fin = i - 1;
    int ancho = fin - ini + 1;
    int centro = (angBarrido[ini] + angBarrido[fin]) / 2;
    // Si dos huecos tienen el mismo ancho, se prefiere el más cercano al frente
    bool esMejor = (ancho > mejorAncho) ||
                   (ancho == mejorAncho && abs(centro) < abs(mejor.centro));
    if (ancho >= MIN_LECTURAS_HUECO && esMejor) {
      mejor.existe = true;
      mejor.ini = ini;
      mejor.fin = fin;
      mejor.centro = centro;
      mejorAncho = ancho;
    }
  }
  return mejor;
}

// Registro para planilla: B,barrido,ángulo,distancia
void imprimirBarrido() {
  for (int i = 0; i < N_BARRIDO; i++) {
    Serial.printf("B,%lu,%d,%ld\n", numBarrido, angBarrido[i], distBarrido[i]);
  }
  if (ultimoHueco.existe) {
    registrar("barrido " + String(numBarrido) + ": hueco centro=" + String(ultimoHueco.centro) + "°, lecturas=" + String(ultimoHueco.fin - ultimoHueco.ini + 1));
  } else {
    registrar("barrido " + String(numBarrido) + ": ningun hueco");
  }
}

// =====================================================
// Evasión
// =====================================================

// 2.6: evasión en lazo cerrado, común a ambos modos. 'lado' es hacia donde
// debe quedar la trompa; 'alFrente' indica que el hueco está casi al frente
// (basta retroceder recto y seguir).
bool evadirCerrado(Lado lado, bool alFrente) {
  const char* nl = (lado == IZQUIERDA) ? "izquierda" : "derecha";
  long d = 0;
  for (int intento = 1; intento <= MAX_INTENTOS_EVASION; intento++) {
    if (pausado) return false;
    if (intento == 1) {
      if (alFrente) {
        d = retrocederRecto();
      } else {
        d = retrocederOrientando(lado);
        if (pausado) return false;
        d = avanzarEnArco(lado, true);
      }
    } else if (intento == 2) {
      registrar(String("  pivote hacia ") + nl);
      pivotar(lado, T_PIVOTE);
      d = leerCentro();
    } else {
      registrar(String("  retrocede y pivota hacia ") + (lado == IZQUIERDA ? "derecha" : "izquierda"));
      d = retrocederRecto();
      if (pausado) return false;
      pivotar(opuesto(lado), T_PIVOTE);
      d = leerCentro();
    }
    direccion(ANG_CENTRO);
    if (d > DIST_CRITICA + MARGEN_HISTERESIS) {
      registrar("  salida ok en intento " + String(intento) + ", frente " + String(d) + " cm");
      return true;
    }
    registrar("  intento " + String(intento) + " sin salida, frente " + String(d) + " cm");
  }
  return false;
}

// Versión 2: el barrido elige el lado (y si el hueco está al frente)
void evadirConBarrido() {
  if (pausado) return;
  barrer();
  barridosCorrida++;
  Lado lado;
  bool alFrente = false;
  if (ultimoHueco.existe) {
    lado = (ultimoHueco.centro > 0) ? IZQUIERDA : DERECHA;
    alFrente = abs(ultimoHueco.centro) < UMBRAL_RECTO;
  } else {
    // sin hueco: lado con la celda más lejana del mapa
    long mejor = -1; int iMejor = 0;
    for (int i = 0; i < N_BARRIDO; i++) if (distBarrido[i] > mejor) { mejor = distBarrido[i]; iMejor = i; }
    lado = (angBarrido[iMejor] >= 0) ? IZQUIERDA : DERECHA;
  }
  registrar(String("evasion (barrido) hacia ") + (lado == IZQUIERDA ? "izquierda" : "derecha") + (alFrente ? ", hueco al frente" : ""));
  if (!evadirCerrado(lado, alFrente) && !pausado) {
    registrar("ATRAPADO: ningun intento despejo el frente. Revisen y pulsen Reanudar.");
    pausarCorrida(true);
  }
}

// Versión 1 (reactiva): el lado lo dan los sensores laterales
void evadirReactivo(long distIzqInicial, long distDerInicial) {
  Lado lado = (distIzqInicial > distDerInicial) ? IZQUIERDA : DERECHA;  // hacia el lado más libre
  registrar(String("evasion reactiva hacia ") + (lado == IZQUIERDA ? "izquierda" : "derecha"));
  if (!evadirCerrado(lado, false) && !pausado) {
    registrar("ATRAPADO: ningun intento despejo el frente. Revisen y pulsen Reanudar.");
    pausarCorrida(true);
  }
}

// =====================================================
// NUEVO: control de corridas desde la página
// =====================================================

void iniciarCorrida() {
  tAcumulado = 0;
  evasionesCorrida = 0;
  barridosCorrida = 0;
  atrapamientosCorrida = 0;
  atrapado = false;
  zonaAnterior = LIBRE;
  direccion(ANG_CENTRO);
  recentrarSensor = true;
  tInicioTramo = millis();
  corridaActiva = true;
  pausado = false;
  registrar("CORRIDA iniciada");
}

void pausarCorrida(bool porAtrapamiento) {
  pausado = true;
  detener();
  if (porAtrapamiento) {
    atrapado = true;
    atrapamientosCorrida++;
  }
  if (corridaActiva) {
    tAcumulado += millis() - tInicioTramo;
    corridaActiva = false;
    Serial.printf("CORRIDA,%s,%.1f s,evasiones=%d,barridos=%d,atrapamientos=%d\n",
                  MODO_BARRIDO ? "v2" : "v1", tAcumulado / 1000.0,
                  evasionesCorrida, barridosCorrida, atrapamientosCorrida);
  }
}

void reanudarCorrida() {
  atrapado = false;
  if (!corridaActiva) {
    tInicioTramo = millis();
    corridaActiva = true;
  }
  recentrarSensor = true;
  pausado = false;
}

// Solo cuenta el tiempo en marcha, no el tiempo en pausa
unsigned long tiempoCorrida() {
  return tAcumulado + (corridaActiva ? millis() - tInicioTramo : 0);
}

void procesarComando(const String& c) {
  if (c == "iniciar")        { modoManual = false; iniciarCorrida(); }
  else if (c == "pausa")     pausarCorrida(false);
  else if (c == "reanudar")  { modoManual = false; reanudarCorrida(); }
  else if (c == "manual") {                       // 2.7
    pausarCorrida(false);
    modoManual = true;
    tUltimoManual = millis();
    registrar("MODO MANUAL (app)");
  }
  else if (c == "mv") {                           // 2.7
    if (!modoManual) return;
    tUltimoManual = millis();
    conducirManual(server.arg("v").toInt(), server.arg("g").toInt());
  }
  else if (c == "barrer")    solicitudBarrido = true;  // lo ejecuta loop()
  else {
    // Calibración: siempre con el auto en pausa
    pausarCorrida(false);
    if (c == "s0")        moverSensor(0);
    else if (c == "s90")  moverSensor(90);
    else if (c == "s180") moverSensor(180);
    else if (c == "dizq") direccion(ANG_IZQ_FUERTE);
    else if (c == "dcen") direccion(ANG_CENTRO);
    else if (c == "dder") direccion(ANG_DER_FUERTE);
  }
}

String estadoJSON() {
  int n = (numBarrido > 0) ? N_BARRIDO : 0;
  String s = "{\"sens\":{\"izq\":" + String(telIzq) + ",\"cen\":" + String(telCent) + ",\"der\":" + String(telDer) +
             ",\"zona\":\"" + String(telZona) + "\",\"vel\":" + String(telVel) + ",\"ang\":" + String(telAng) + "},\"log\":[";
  for (int k = 0; k < N_LOG; k++) {
    int i = (logIdx + k) % N_LOG;
    if (!logLineas[i].length()) continue;
    String l = logLineas[i]; l.replace("\"", "'");
    s += "\"" + l + "\",";
  }
  if (s.endsWith(",")) s.remove(s.length() - 1);
  s += "],\"modo\":\"";
  s += MODO_BARRIDO ? "v2" : "v1";
  s += "\",\"manual\":";
  s += modoManual ? "true" : "false";
  s += ",\"estado\":\"";
  s += atrapado ? "atrapado" : (modoManual ? "manual" : (pausado ? "pausa" : "marcha"));
  s += "\",\"barrido\":{\"id\":";
  s += numBarrido;
  s += ",\"n\":";
  s += n;
  s += ",\"paso\":";
  s += PASO_BARRIDO;
  s += ",\"umbral\":";
  s += UMBRAL_HUECO_CM;
  s += ",\"ang\":[";
  for (int i = 0; i < n; i++) {
    if (i > 0) s += ",";
    s += angBarrido[i];
  }
  s += "],\"d\":[";
  for (int i = 0; i < n; i++) {
    if (i > 0) s += ",";
    s += distBarrido[i];
  }
  s += "],\"hueco\":{\"existe\":";
  s += ultimoHueco.existe ? "true" : "false";
  s += ",\"ini\":";
  s += ultimoHueco.ini;
  s += ",\"fin\":";
  s += ultimoHueco.fin;
  s += ",\"centro\":";
  s += ultimoHueco.centro;
  s += "}},\"corrida\":{\"t\":";
  s += tiempoCorrida();
  s += ",\"evasiones\":";
  s += evasionesCorrida;
  s += ",\"barridos\":";
  s += barridosCorrida;
  s += ",\"atrapamientos\":";
  s += atrapamientosCorrida;
  s += "}}";
  return s;
}

void configurarWeb() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_CLAVE);

  server.on("/", []() {
    server.send(200, "text/html; charset=utf-8", PAGINA_MAPA);
  });
  server.on("/estado", []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", estadoJSON());
  });
  server.on("/cmd", []() {
    procesarComando(server.arg("c"));
    server.send(200, "text/plain", "ok");
  });
  server.begin();

  Serial.printf("Mapa: conecten el celular a la red %s y abran http://%s\n",
                AP_SSID, WiFi.softAPIP().toString().c_str());
}

// =====================================================
// Programa principal
// =====================================================

void setup() {
  Serial.begin(115200);

  pinMode(Echo_1, INPUT);
  pinMode(Trig_1, OUTPUT);
  pinMode(Echo_2, INPUT);
  pinMode(Trig_2, OUTPUT);
  pinMode(Echo_3, INPUT);
  pinMode(Trig_3, OUTPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  servoDireccion.setPeriodHertz(50);
  servoDireccion.attach(pin_S, 500, 2400);
  direccion(ANG_CENTRO);

  // NUEVO: servo de barrido
  servoBarrido.setPeriodHertz(50);
  servoBarrido.attach(PIN_SERVO_BARRIDO, 500, 2400);
  moverSensor(90);

  // Motores: 1 kHz, 8 bits (0..255), igual que analogWrite
  pwmA.attachPin(ENA, 1000, 8);
  pwmB.attachPin(ENB, 1000, 8);

  pausado = ARRANCAR_EN_PAUSA;
  detener();
  configurarWeb();
  registrar(String("ARRANQUE. Motivo del ultimo reset: ") + (int)esp_reset_reason() +
            " (1 encendido, 3 sw, 4 panic, 5-7 watchdog, 9 BROWNOUT)");
  delay(1000);

  if (!ARRANCAR_EN_PAUSA) {  // como en la versión 1: parte solo
    corridaActiva = true;
    tInicioTramo = millis();
  }
}

void loop() {
  server.handleClient();

  if (solicitudBarrido) {  // "Barrer ahora" desde la página
    solicitudBarrido = false;
    barrer();
  }
  if (modoManual) {                               // 2.7
    if (millis() - tUltimoManual > T_MANUAL_WATCHDOG) detener();
    // los sensores se siguen publicando para la app
    telIzq  = leerDistanciaCm(Trig_3, Echo_3); esperar(T_ENTRE_SENSORES);
    telCent = leerDistanciaCm(Trig_2, Echo_2); esperar(T_ENTRE_SENSORES);
    telDer  = leerDistanciaCm(Trig_1, Echo_1); esperar(T_ENTRE_SENSORES);
    return;
  }
  if (pausado) {
    detener();
    delay(5);
    return;
  }
  if (recentrarSensor) {   // tras calibrar o reanudar, el sensor vuelve al frente
    recentrarSensor = false;
    moverSensor(90);
    esperar(T_VOLVER_AL_FRENTE);
  }

  long distIzq  = leerDistanciaCm(Trig_3, Echo_3); // Sensor izquierdo
  esperar(T_ENTRE_SENSORES);
  long distCent = leerDistanciaCm(Trig_2, Echo_2); // Sensor centro (servo al frente)
  esperar(T_ENTRE_SENSORES);
  long distDer  = leerDistanciaCm(Trig_1, Echo_1); // Sensor derecho
  esperar(T_ENTRE_SENSORES);

  Serial.printf("IZQ: %ld cm | CENT: %ld cm | DER: %ld cm\n",
                distIzq, distCent, distDer);
  telIzq = distIzq; telCent = distCent; telDer = distDer;

  Zona zonaActual = determinarZona(distCent, zonaAnterior);
  telZona = (zonaActual == CRITICA) ? "CRITICA" : (zonaActual == PRECAUCION ? "precaucion" : "libre");
  if (zonaActual != zonaAnterior) registrar(String("zona ") + telZona + " (centro " + String(distCent) + " cm)");
  zonaAnterior = zonaActual;

  if (zonaActual == CRITICA) {
    evasionesCorrida++;
    if (MODO_BARRIDO) evadirConBarrido();
    else              evadirReactivo(distIzq, distDer);
  }
  else {
    // 2.6: rumbo proporcional. e > 0 -> más espacio a la derecha -> girar a
    // la derecha (en este auto, ángulo menor). Solo actúa si algún lateral
    // está más cerca que DIST_LAT_ACTIVA; en espacio abierto va recto.
    long li = min(distIzq, LAT_MAX), ld = min(distDer, LAT_MAX);
    int ang = ANG_CENTRO, lento = -1;
    float factor = 1.0f;
    if (min(li, ld) < DIST_LAT_ACTIVA) {
      int desvio = (int)(KP_RUMBO * (float)(ld - li));
      desvio = constrain(desvio, -DESVIO_MAX, DESVIO_MAX);
      ang = ANG_CENTRO - desvio;
      if (desvio != 0) {
        lento  = (desvio > 0) ? DERECHA : IZQUIERDA;
        factor = 1.0f - ((float)abs(desvio) / DESVIO_MAX) * (1.0f - FACTOR_INTERNA_SUAVE);
      }
    }
    // Velocidad según el frente: normal si libre, baja linealmente hacia crítica
    int vel;
    if (distCent >= 999)                 vel = VEL_PRECAUCION;   // sin eco: puede ser cable suelto
    else if (distCent >= DIST_PRECAUCION) vel = VEL_NORMAL;
    else vel = map((int)distCent, DIST_CRITICA, DIST_PRECAUCION, VEL_PRECAUCION, VEL_NORMAL);
    direccion(ang);
    traccion(vel, true, lento, factor);
  }

  esperar(50);
}

// =====================================================
// NUEVO: página del mapa (se sirve desde el ESP32, sin internet)
// =====================================================
const char PAGINA_MAPA[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Mapa local del auto</title>
<style>
:root{--papel:#fbfcfd;--cuadro:#d6e4f0;--tinta:#1b2a41;--tenue:#3d4e68;--azul:#1f5fad;--rojo:#c62e2e}
*{box-sizing:border-box}
body{margin:0;background:var(--papel);color:var(--tinta);font:16px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
main{max-width:560px;margin:0 auto;padding:14px}
h1{font-family:"DIN Alternate","Roboto Condensed","sans-serif-condensed","Arial Narrow",sans-serif;font-size:26px;font-weight:700;margin:2px 0 4px}
#estado{margin:0 0 10px;color:var(--tenue);min-height:46px}
canvas{display:block;width:100%;height:auto;border:1px solid var(--cuadro);border-radius:4px}
.cifras{display:grid;grid-template-columns:repeat(3,1fr);margin:12px 0;border-top:2px solid var(--tinta);border-bottom:1px solid var(--cuadro)}
.cifras div{padding:6px 4px}
.cifras span{display:block;font-size:13px;color:var(--tenue)}
.cifras strong{font-family:"DIN Alternate","Roboto Condensed","sans-serif-condensed",sans-serif;font-size:26px;font-variant-numeric:tabular-nums}
.acciones{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin:12px 0}
.fila3{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
button{font:inherit;font-weight:600;min-height:48px;border-radius:6px;border:2px solid var(--tinta);background:var(--papel);color:var(--tinta)}
button.principal{background:var(--tinta);color:#fff}
button.detener{background:var(--rojo);border-color:var(--rojo);color:#fff}
button:focus-visible{outline:3px solid var(--azul);outline-offset:2px}
table{width:100%;border-collapse:collapse;font-variant-numeric:tabular-nums;margin-top:6px}
caption{text-align:left;font-weight:600;padding-bottom:4px}
th,td{padding:5px 6px;border-bottom:1px solid var(--cuadro);text-align:right}
th:first-child,td:first-child{text-align:left}
details{margin:18px 0;border-top:2px solid var(--tinta);padding-top:8px}
summary{font-weight:600;cursor:pointer;min-height:32px}
details p{margin:10px 0 6px;color:var(--tenue)}
</style></head><body><main>
<h1>Mapa local del auto</h1>
<p id="estado">Conectando con el auto…</p>
<div class="cifras" id="sens">
<div><span>Izquierdo</span><strong id="sIzq">—</strong></div>
<div><span>Central</span><strong id="sCen">—</strong></div>
<div><span>Derecho</span><strong id="sDer">—</strong></div>
</div>
<p id="zona" style="margin:0 0 8px;font-weight:600">—</p>
<canvas id="mapa" width="520" height="300" aria-label="Mapa del último barrido"></canvas>
<div class="cifras">
<div><span>Tiempo de corrida</span><strong id="t">0,0 s</strong></div>
<div><span>Evasiones</span><strong id="ev">0</strong></div>
<div><span>Barridos</span><strong id="nb">0</strong></div>
</div>
<div class="acciones">
<button class="principal" onclick="cmd('iniciar')">Iniciar corrida</button>
<button class="detener" onclick="cmd('pausa')">Pausar</button>
<button onclick="cmd('reanudar')">Reanudar</button>
<button onclick="cmd('barrer')">Barrer ahora</button>
</div>
<table><caption>Último barrido</caption>
<thead><tr><th>Ángulo</th><th>Distancia</th></tr></thead><tbody id="tabla"></tbody></table>
<h2 style="font-size:14px;color:var(--tenue);margin:14px 0 4px">Registro de eventos</h2>
<pre id="log" style="background:#fff;border:1px solid var(--cuadro);border-radius:4px;padding:8px;font-size:12px;white-space:pre-wrap;min-height:90px;margin:0 0 6px"></pre>
<details><summary>Calibración (pausa el auto)</summary>
<p>Sensor sobre el servo de barrido</p>
<div class="fila3"><button onclick="cmd('s0')">0°</button><button onclick="cmd('s90')">90°</button><button onclick="cmd('s180')">180°</button></div>
<p>Dirección de las ruedas</p>
<div class="fila3"><button onclick="cmd('dizq')">Izquierda</button><button onclick="cmd('dcen')">Centro</button><button onclick="cmd('dder')">Derecha</button></div>
</details>
</main>
<script>
const RANGO = 200;  // cm visibles en el mapa
const MODOS = {v2: 'Versión 2 (con barrido)', v1: 'Versión 1 (reactiva)'};
const ESTADOS = {
  marcha: 'corrida en marcha',
  pausa: 'en pausa',
  manual: 'modo manual (app)',
  atrapado: 'atrapado, ningún barrido encontró un hueco. Despeja el entorno y presiona Reanudar'
};
const cv = document.getElementById('mapa');
const g = cv.getContext('2d');

function cmd(c) { fetch('/cmd?c=' + c).catch(() => {}); }

function punto(phi, d, cx, cy, esc) {
  const r = Math.min(d, RANGO) * esc, a = phi * Math.PI / 180;
  return [cx - r * Math.sin(a), cy - r * Math.cos(a)];
}

function dibujar(b) {
  const W = cv.width, H = cv.height, cx = W / 2, cy = H - 18;
  const esc = Math.min(cx - 16, cy - 16) / RANGO;
  g.clearRect(0, 0, W, H);
  g.lineWidth = 1;
  // Papel milimetrado: una línea cada 10 cm, más marcada cada 50 cm
  for (let k = -30; k <= 30; k++) {
    const x = cx + k * 10 * esc;
    if (x < 0 || x > W) continue;
    g.strokeStyle = k % 5 === 0 ? '#c9d9e9' : '#e7eff7';
    g.beginPath(); g.moveTo(x, 0); g.lineTo(x, H); g.stroke();
  }
  for (let k = 0; k <= 30; k++) {
    const y = cy - k * 10 * esc;
    if (y < 0) break;
    g.strokeStyle = k % 5 === 0 ? '#c9d9e9' : '#e7eff7';
    g.beginPath(); g.moveTo(0, y); g.lineTo(W, y); g.stroke();
  }
  // Distancias y lados
  g.strokeStyle = '#9fb6d3'; g.fillStyle = '#5a6f8f'; g.font = '12px system-ui, sans-serif';
  for (let r = 50; r <= RANGO; r += 50) {
    g.beginPath(); g.arc(cx, cy, r * esc, Math.PI, 2 * Math.PI); g.stroke();
    g.fillText(r + ' cm', cx + 4, cy - r * esc + 14);
  }
  g.fillText('izquierda', 6, H - 4);
  g.textAlign = 'right'; g.fillText('derecha', W - 6, H - 4); g.textAlign = 'left';

  if (b.n > 0 && b.hueco.existe) {
    // Hueco elegido, marcado como con destacador
    const a1 = b.ang[b.hueco.ini], a2 = b.ang[b.hueco.fin];
    const hi = Math.min(90, Math.max(a1, a2) + b.paso / 2);
    const lo = Math.max(-90, Math.min(a1, a2) - b.paso / 2);
    g.fillStyle = 'rgba(255, 228, 92, 0.55)';
    g.beginPath(); g.moveTo(cx, cy);
    g.arc(cx, cy, RANGO * esc, (-90 - hi) * Math.PI / 180, (-90 - lo) * Math.PI / 180, false);
    g.closePath(); g.fill();
  }
  for (let i = 0; i < b.n; i++) {
    // Cada lectura: azul si está libre, rojo si hay obstáculo, círculo vacío si no hubo eco
    const d = b.d[i], sinEco = d >= 999, libre = d > b.umbral;
    const p = punto(b.ang[i], sinEco ? RANGO : d, cx, cy, esc);
    g.setLineDash(sinEco ? [4, 5] : []);
    g.strokeStyle = '#8a94a6'; g.lineWidth = 1;
    g.beginPath(); g.moveTo(cx, cy); g.lineTo(p[0], p[1]); g.stroke();
    g.setLineDash([]);
    g.beginPath(); g.arc(p[0], p[1], 5, 0, 2 * Math.PI);
    if (sinEco) { g.stroke(); } else { g.fillStyle = libre ? '#1f5fad' : '#c62e2e'; g.fill(); }
  }
  if (b.n > 0 && b.hueco.existe) {
    // Dirección de salida
    const p = punto(b.hueco.centro, RANGO * 0.9, cx, cy, esc);
    const a = Math.atan2(p[1] - cy, p[0] - cx);
    g.strokeStyle = '#1b2a41'; g.fillStyle = '#1b2a41'; g.lineWidth = 2.5;
    g.beginPath(); g.moveTo(cx, cy); g.lineTo(p[0], p[1]); g.stroke();
    g.beginPath(); g.moveTo(p[0], p[1]);
    g.lineTo(p[0] - 12 * Math.cos(a - 0.4), p[1] - 12 * Math.sin(a - 0.4));
    g.lineTo(p[0] - 12 * Math.cos(a + 0.4), p[1] - 12 * Math.sin(a + 0.4));
    g.closePath(); g.fill();
  }
  if (!b.n) {
    g.fillStyle = '#3d4e68'; g.font = '15px system-ui, sans-serif'; g.textAlign = 'center';
    g.fillText('Todavía no hay barridos', cx, cy - 110 * esc);
    g.textAlign = 'left';
  }
  g.fillStyle = '#1b2a41'; g.fillRect(cx - 9, cy - 6, 18, 14);  // el auto
}

function fd(d) { return d >= 999 ? 'sin eco' : d + ' cm'; }
function actualizar(e) {
  const b = e.barrido;
  if (e.sens) {
    document.getElementById('sIzq').textContent = fd(e.sens.izq);
    document.getElementById('sCen').textContent = fd(e.sens.cen);
    document.getElementById('sDer').textContent = fd(e.sens.der);
    document.getElementById('zona').textContent = 'Zona: ' + e.sens.zona + ' · motores ' + e.sens.vel + ' · dirección ' + e.sens.ang + '°';
    document.getElementById('zona').style.color = e.sens.zona === 'CRITICA' ? '#c62e2e' : (e.sens.zona === 'precaucion' ? '#b8860b' : '#1f7a3a');
  }
  if (e.log) document.getElementById('log').textContent = e.log.join('\n');
  document.getElementById('estado').textContent =
    (MODOS[e.modo] || e.modo) + ': ' + (ESTADOS[e.estado] || e.estado) + '. Barridos realizados: ' + b.id + '.';
  document.getElementById('t').textContent = (e.corrida.t / 1000).toFixed(1).replace('.', ',') + ' s';
  document.getElementById('ev').textContent = e.corrida.evasiones;
  document.getElementById('nb').textContent = e.corrida.barridos;
  let filas = '';
  for (let i = 0; i < b.n; i++) {
    const a = b.ang[i], d = b.d[i];
    const angulo = a > 0 ? a + '° izq.' : a < 0 ? (-a) + '° der.' : '0° (frente)';
    filas += '<tr><td>' + angulo + '</td><td>' + (d >= 999 ? 'sin eco' : d + ' cm') + '</td></tr>';
  }
  document.getElementById('tabla').innerHTML = filas ||
    '<tr><td colspan="2">Todavía no hay barridos. Presiona «Barrer ahora» con el auto detenido.</td></tr>';
  dibujar(b);
}

function ciclo() {
  fetch('/estado', {cache: 'no-store'})
    .then(r => r.json())
    .then(actualizar)
    .catch(() => {
      document.getElementById('estado').textContent =
        'Sin conexión con el auto. Verifica que el celular siga conectado a la red del auto.';
    })
    .finally(() => setTimeout(ciclo, 400));
}

dibujar({n: 0});
ciclo();
</script></body></html>)rawliteral";
