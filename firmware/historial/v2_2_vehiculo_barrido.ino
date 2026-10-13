/*
 * =====================================================================
 *  VEHÍCULO AUTÓNOMO - VERSIÓN 2.2: MAPEO LOCAL POR BARRIDO ULTRASÓNICO
 *  Código base construido sobre el código de la EPI (versión 1)
 * =====================================================================
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

// =============================
// L298N - ESP32 (igual que en la versión 1)
// =============================

// Motor A
#define ENA 25
#define IN1 26
#define IN2 27

// Motor B
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
#define Echo_1 5   // derecho
#define Trig_1 17

#define Echo_2 4   // central: AHORA VA SOBRE EL SERVO DE BARRIDO
#define Trig_2 16

#define Echo_3 22  // izquierdo
#define Trig_3 21

// ---------- Modo de funcionamiento ----------
// true:  versión 2, elige la salida con el barrido
// false: versión 1, elige con los sensores laterales (maniobra corregida)
const bool MODO_BARRIDO = true;
// true: parte detenido y espera "Iniciar corrida" desde la página
const bool ARRANCAR_EN_PAUSA = true;

// ---------- Red WiFi propia del auto ----------
const char* AP_SSID  = "AutoEPI-Radar";
const char* AP_CLAVE = "epi2026auto";  // mínimo 8 caracteres

// ---------- Umbrales de distancia (igual que en la versión 1) ----------
const int DIST_CRITICA      = 20; // cm -> maniobra de evasión
const int DIST_PRECAUCION   = 40; // cm -> reducir velocidad / corregir rumbo
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
void direccion(int ang) {
  servoDireccion.write(constrain(ang, DIR_MIN, DIR_MAX));
}

// ---------- Velocidades (igual que en la versión 1) ----------
const int VEL_NORMAL     = 220;
const int VEL_PRECAUCION = 150;
const int VEL_EVASION    = 180;

// ---------- Tiempos de maniobra (ms) ----------
const unsigned long T_SERVO_DIRECCION     = 150; // CALIBRAR: que las ruedas alcancen a girar
const unsigned long T_RETROCESO           = 850; // como en la versión 1
const unsigned long T_AVANCE_ARCO         = 500; // como en la versión 1
const unsigned long T_RETROCESO_CORTO     = 350; // NUEVO: antes de avanzar hacia un hueco frontal
const unsigned long T_RETROCESO_SIN_HUECO = 600; // NUEVO: si no hay hueco, retrocede y vuelve a barrer

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
const int LIMITE_FRONTAL_GRADOS = 35; // hasta aquí el hueco se considera "al frente"; más allá, maniobra de estacionamiento
const int UMBRAL_GIRO_FUERTE    = 25; // desde aquí gira con el ángulo fuerte
const int UMBRAL_RECTO          = 10; // hueco casi al frente: basta retroceder
const int MAX_INTENTOS_BARRIDO  = 3;  // después se declara "atrapado"

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
bool recentrarSensor = false;

// Cronómetro y contadores de la corrida (se ven en la página)
bool corridaActiva = false;
unsigned long tAcumulado = 0, tInicioTramo = 0;
int evasionesCorrida = 0, barridosCorrida = 0, atrapamientosCorrida = 0;

WebServer server(80);
extern const char PAGINA_MAPA[];  // la página está al final del archivo

// ---------- Prototipos ----------
long leerDistanciaCm(int pinTrig, int pinEcho);
long leerMediana(int pinTrig, int pinEcho, int n);
Zona determinarZona(long distCent, Zona anterior);
void avanzar(int velocidad);
void retroceder(int velocidad);
void detener();
void esperar(unsigned long ms);
int  anguloDireccion(Lado lado, bool fuerte);
Lado opuesto(Lado lado);
void retrocederOrientando(Lado lado, unsigned long ms);
void retrocederRecto(unsigned long ms);
void avanzarEnArco(Lado lado, bool fuerte, unsigned long ms);
void moverSensor(int angServo);
int  anguloRespectoFrente(int angServo);
void barrer();
int  indiceMasCercano(int angFrente);
void fusionarLaterales();
Hueco buscarHueco();
void imprimirBarrido();
void salirHaciaHueco(int centro);
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

// AVANZAR
void avanzar(int velocidad) {
  if (pausado) { detener(); return; }
  pwmA.write(velocidad);
  pwmB.write(velocidad);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

// RETROCEDER
void retroceder(int velocidad) {
  if (pausado) { detener(); return; }
  pwmA.write(velocidad);
  pwmB.write(velocidad);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

// DETENER
void detener() {
  pwmA.write(0);
  pwmB.write(0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
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

// Retrocede girando para que la trompa quede apuntando hacia 'lado'
void retrocederOrientando(Lado lado, unsigned long ms) {
  detener();
  esperar(100);
  direccion(anguloDireccion(opuesto(lado), false));
  esperar(T_SERVO_DIRECCION);
  retroceder(VEL_EVASION);
  esperar(ms);
  detener();
  esperar(50);
}

void retrocederRecto(unsigned long ms) {
  detener();
  esperar(100);
  direccion(ANG_CENTRO);
  esperar(T_SERVO_DIRECCION);
  retroceder(VEL_EVASION);
  esperar(ms);
  detener();
  esperar(50);
}

void avanzarEnArco(Lado lado, bool fuerte, unsigned long ms) {
  direccion(anguloDireccion(lado, fuerte));
  esperar(T_SERVO_DIRECCION);
  avanzar(VEL_EVASION);
  esperar(ms);
  detener();
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
    Serial.printf("HUECO,%lu,centro=%d,lecturas=%d\n", numBarrido,
                  ultimoHueco.centro, ultimoHueco.fin - ultimoHueco.ini + 1);
  } else {
    Serial.printf("HUECO,%lu,ninguno\n", numBarrido);
  }
}

// =====================================================
// Evasión
// =====================================================

// NUEVO (versión 2): salir hacia el centro del hueco elegido
void salirHaciaHueco(int centro) {
  Lado lado = (centro > 0) ? IZQUIERDA : DERECHA;
  int desvio = abs(centro);

  if (desvio < UMBRAL_RECTO) {
    // Hueco prácticamente al frente: basta con retroceder un poco
    retrocederRecto(T_RETROCESO_CORTO);
  } else if (desvio <= LIMITE_FRONTAL_GRADOS) {
    // Hueco hacia adelante: a 20 cm del obstáculo no alcanza el radio de
    // giro, así que primero retrocede un poco y luego avanza en arco
    retrocederRecto(T_RETROCESO_CORTO);
    avanzarEnArco(lado, desvio >= UMBRAL_GIRO_FUERTE, T_AVANCE_ARCO);
  } else {
    // Hueco hacia el costado: maniobra de estacionamiento (corregida)
    retrocederOrientando(lado, T_RETROCESO);
    avanzarEnArco(lado, true, T_AVANCE_ARCO);
  }
  direccion(ANG_CENTRO);
}

// NUEVO (versión 2): barre, elige el hueco y sale; si no hay hueco,
// retrocede y vuelve a barrer. Tras varios intentos se declara atrapado.
void evadirConBarrido() {
  for (int intento = 1; intento <= MAX_INTENTOS_BARRIDO; intento++) {
    if (pausado) return;
    barrer();
    barridosCorrida++;
    if (ultimoHueco.existe) {
      salirHaciaHueco(ultimoHueco.centro);
      return;
    }
    retrocederRecto(T_RETROCESO_SIN_HUECO);
  }
  if (pausado) return;
  Serial.println("ATRAPADO: ningún barrido encontró un hueco. Revisen y pulsen Reanudar.");
  pausarCorrida(true);
}

// Versión 1 (reactiva): decide con los dos sensores laterales, como en
// el código de la EPI, pero con la maniobra corregida
void evadirReactivo(long distIzqInicial, long distDerInicial) {
  Lado lado = (distIzqInicial > distDerInicial) ? IZQUIERDA : DERECHA;  // hacia el lado más libre
  retrocederOrientando(lado, T_RETROCESO);

  // Releemos los laterales
  long distIzq = leerDistanciaCm(Trig_3, Echo_3);
  esperar(T_ENTRE_SENSORES);
  long distDer = leerDistanciaCm(Trig_1, Echo_1);

  if (distIzq <= DIST_CRITICA && distDer <= DIST_CRITICA) {
    // bloqueado a ambos lados -> no hay hacia dónde ir, se queda detenido
    direccion(ANG_CENTRO);
    return;
  }
  avanzarEnArco(lado, true, T_AVANCE_ARCO);
  direccion(ANG_CENTRO);
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
  Serial.println("CORRIDA iniciada");
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
  if (c == "iniciar")        iniciarCorrida();
  else if (c == "pausa")     pausarCorrida(false);
  else if (c == "reanudar")  reanudarCorrida();
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
  String s = "{\"modo\":\"";
  s += MODO_BARRIDO ? "v2" : "v1";
  s += "\",\"estado\":\"";
  s += atrapado ? "atrapado" : (pausado ? "pausa" : "marcha");
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

  Zona zonaActual = determinarZona(distCent, zonaAnterior);
  zonaAnterior = zonaActual;

  if (zonaActual == CRITICA) {
    evasionesCorrida++;
    if (MODO_BARRIDO) evadirConBarrido();
    else              evadirReactivo(distIzq, distDer);
  }
  else if (zonaActual == PRECAUCION) {
    int anguloCorreccion = ANG_CENTRO; // Corrección leve de rumbo si solo un lateral

    if (distIzq <= DIST_PRECAUCION && distDer > DIST_PRECAUCION) {
      anguloCorreccion = ANG_DER_SUAVE; // se aleja del lado izquierdo
    } else if (distDer <= DIST_PRECAUCION && distIzq > DIST_PRECAUCION) {
      anguloCorreccion = ANG_IZQ_SUAVE; // se aleja del lado derecho
    }
    direccion(anguloCorreccion);
    avanzar(VEL_PRECAUCION);
  }
  else if (distCent >= 999) {
    // Sin eco al frente: puede ser espacio abierto (>5 m) o un cable suelto.
    // No se trata como vía libre a toda velocidad.
    direccion(ANG_CENTRO);
    avanzar(VEL_PRECAUCION);
  }
  else {
    // Mejora pendiente, fuera a propósito para que la comparación entre
    // versiones sea justa: usar también los laterales en zona libre.
    direccion(ANG_CENTRO);
    avanzar(VEL_NORMAL);
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

function actualizar(e) {
  const b = e.barrido;
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
    .finally(() => setTimeout(ciclo, 700));
}

dibujar({n: 0});
ciclo();
</script></body></html>)rawliteral";
