/*
 * =====================================================================
 *  DIAGNÓSTICO POR SUBSISTEMAS - VEHÍCULO AUTÓNOMO EPI
 *  Misma asignación de pines que vehiculo_barrido_v2.ino
 * =====================================================================
 *
 *  Para qué sirve: probar CADA parte por separado desde el Monitor Serie
 *  (115200 baudios, "Sin ajuste de línea" o "Nueva línea", da igual),
 *  con el auto SOBRE UN SOPORTE (ruedas en el aire).
 *
 *  Al arrancar imprime el MOTIVO DEL ÚLTIMO RESET. Si dice BROWNOUT, el
 *  problema es de alimentación, no de código.
 *
 *  Comandos (una letra + Enter):
 *    1  sensores: imprime IZQ/CENT/DER en continuo (cualquier tecla sale)
 *    2  servo de dirección: centro -> izquierda -> centro -> derecha -> centro
 *    3  servo de barrido: 0 -> 90 -> 180 -> 90 (sensor central debe mirar al frente en 90)
 *    4  motores: avanza 1 s, para, retrocede 1 s, para (mirar que ambas
 *       ruedas giren en el mismo sentido y a velocidad parecida)
 *    5  prueba de carga: motores a VEL_NORMAL mientras ambos servos van
 *       de extremo a extremo 5 veces. Si el ESP32 se reinicia (vuelve a
 *       aparecer "ARRANQUE"), la alimentación no aguanta.
 *    6  barrido completo con el sensor central (imprime ángulo,distancia)
 *    p  parar todo
 */

#include <ESP32Servo.h>
#include <esp_system.h>

// ---------- Pines (idénticos a la versión 2) ----------
#define ENA 25
#define IN1 26
#define IN2 27
#define ENB 33
#define IN3 32
#define IN4 19

#define PIN_SERVO_DIRECCION 18
#define PIN_SERVO_BARRIDO   23

#define Echo_1 5    // derecho
#define Trig_1 17
#define Echo_2 4    // central (sobre el servo de barrido)
#define Trig_2 16
#define Echo_3 22   // izquierdo
#define Trig_3 21

// ---------- Ángulos y velocidades (idénticos a la versión 2) ----------
const int ANG_CENTRO     = 90;
const int ANG_IZQ_FUERTE = 45;
const int ANG_DER_FUERTE = 145;
const int VEL_NORMAL     = 220;

Servo servoDireccion;
Servo servoBarrido;

// PWM de motores con la misma librería que los servos: así nunca compiten
// por los mismos timers del ESP32 (analogWrite y ESP32Servo usan el mismo
// periférico LEDC).
ESP32PWM pwmA;
ESP32PWM pwmB;

// =====================================================
// Utilidades
// =====================================================

const char* motivoReset(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "encendido normal";
    case ESP_RST_SW:       return "reset por software";
    case ESP_RST_PANIC:    return "PANIC (error del programa)";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      return "WATCHDOG (el programa se colgó)";
    case ESP_RST_BROWNOUT: return "BROWNOUT -> ALIMENTACION INSUFICIENTE";
    case ESP_RST_EXT:      return "reset externo (botón EN)";
    default:               return "desconocido";
  }
}

long leerDistanciaCm(int pinTrig, int pinEcho) {
  digitalWrite(pinTrig, LOW);
  delayMicroseconds(2);
  digitalWrite(pinTrig, HIGH);
  delayMicroseconds(10);
  digitalWrite(pinTrig, LOW);
  long duracion = pulseIn(pinEcho, HIGH, 30000);
  if (duracion == 0) return 999;  // sin eco
  return duracion * 0.0343 / 2;
}

// Interpreta una lectura para el estudiante
const char* diagnosticoLectura(long d) {
  if (d >= 999) return "SIN ECO (nada en 5 m, cable Echo/GND suelto o VCC sin 5 V)";
  if (d <= 2)   return "ECO INMEDIATO (Echo pegado en alto o Trig/Echo cruzados)";
  return "ok";
}

void motores(int velocidad, bool adelante) {
  digitalWrite(IN1, adelante ? HIGH : LOW);
  digitalWrite(IN2, adelante ? LOW  : HIGH);
  digitalWrite(IN3, adelante ? HIGH : LOW);
  digitalWrite(IN4, adelante ? LOW  : HIGH);
  pwmA.write(velocidad);
  pwmB.write(velocidad);
}

void detener() {
  pwmA.write(0);
  pwmB.write(0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

bool teclaPresionada() {
  if (Serial.available()) {
    while (Serial.available()) Serial.read();
    return true;
  }
  return false;
}

// =====================================================
// Pruebas
// =====================================================

void pruebaSensores() {
  Serial.println("\n[1] SENSORES. Pongan la mano a 10, 20 y 40 cm de cada uno. Tecla para salir.");
  while (!teclaPresionada()) {
    long dIzq = leerDistanciaCm(Trig_3, Echo_3);
    delay(40);
    long dCen = leerDistanciaCm(Trig_2, Echo_2);
    delay(40);
    long dDer = leerDistanciaCm(Trig_1, Echo_1);
    delay(40);
    Serial.printf("IZQ %4ld cm (%s) | CENT %4ld cm (%s) | DER %4ld cm (%s)\n",
                  dIzq, diagnosticoLectura(dIzq),
                  dCen, diagnosticoLectura(dCen),
                  dDer, diagnosticoLectura(dDer));
    delay(150);
  }
  Serial.println("fin sensores");
}

void pruebaServoDireccion() {
  Serial.println("\n[2] SERVO DIRECCION. Observen: 'izquierda' debe girar las ruedas a la izquierda del auto.");
  Serial.println("    Si el servo ZUMBA en un extremo, está forzando el tope mecánico: reduzcan ANG_*_FUERTE.");
  int secuencia[] = { ANG_CENTRO, ANG_IZQ_FUERTE, ANG_CENTRO, ANG_DER_FUERTE, ANG_CENTRO };
  const char* nombres[] = { "centro", "IZQUIERDA", "centro", "DERECHA", "centro" };
  for (int i = 0; i < 5; i++) {
    Serial.printf("  -> %s (%d°)\n", nombres[i], secuencia[i]);
    servoDireccion.write(secuencia[i]);
    delay(1200);
  }
  Serial.println("fin servo direccion");
}

void pruebaServoBarrido() {
  Serial.println("\n[3] SERVO BARRIDO. En 90° el sensor central debe mirar EXACTAMENTE al frente.");
  Serial.println("    Anoten hacia qué lado apunta en 0° (define CERO_A_LA_DERECHA en la v2).");
  int secuencia[] = { 90, 0, 90, 180, 90 };
  for (int i = 0; i < 5; i++) {
    Serial.printf("  -> %d°\n", secuencia[i]);
    servoBarrido.write(secuencia[i]);
    delay(1200);
  }
  Serial.println("fin servo barrido");
}

void pruebaMotores() {
  Serial.println("\n[4] MOTORES (auto en el aire). Ambas ruedas deben girar en el MISMO sentido.");
  Serial.println("  -> adelante");
  motores(VEL_NORMAL, true);
  delay(1000);
  detener();
  delay(600);
  Serial.println("  -> atrás");
  motores(VEL_NORMAL, false);
  delay(1000);
  detener();
  Serial.println("fin motores. Si una rueda va al revés: invertir sus dos cables en el L298N.");
  Serial.println("Si ninguna gira: revisar jumper 5V-EN del L298N, GND común y voltaje del pack.");
}

void pruebaCarga() {
  Serial.println("\n[5] PRUEBA DE CARGA: motores + ambos servos a la vez, 5 ciclos.");
  Serial.println("    Si vuelve a aparecer 'ARRANQUE' en el monitor, el ESP32 se reinició.");
  motores(VEL_NORMAL, true);
  for (int ciclo = 1; ciclo <= 5; ciclo++) {
    Serial.printf("  ciclo %d\n", ciclo);
    servoDireccion.write(ANG_IZQ_FUERTE);
    servoBarrido.write(0);
    delay(700);
    servoDireccion.write(ANG_DER_FUERTE);
    servoBarrido.write(180);
    delay(700);
  }
  detener();
  servoDireccion.write(ANG_CENTRO);
  servoBarrido.write(90);
  Serial.println("fin prueba de carga: el ESP32 NO se reinició durante la prueba.");
}

void pruebaBarrido() {
  Serial.println("\n[6] BARRIDO 0..180 cada 15° con el sensor central (formato: angulo_servo,distancia)");
  servoBarrido.write(0);
  delay(400);
  for (int ang = 0; ang <= 180; ang += 15) {
    servoBarrido.write(ang);
    delay(120);
    long d = leerDistanciaCm(Trig_2, Echo_2);
    Serial.printf("  %3d,%ld\n", ang, d);
    delay(40);
  }
  servoBarrido.write(90);
  Serial.println("fin barrido");
}

void menu() {
  Serial.println("\n--- DIAGNOSTICO: 1 sensores | 2 servo direccion | 3 servo barrido | 4 motores | 5 carga | 6 barrido | p parar ---");
}

// =====================================================
// Programa principal
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n\n===== ARRANQUE =====");
  Serial.printf("Motivo del ultimo reset: %s\n", motivoReset(esp_reset_reason()));

  pinMode(Echo_1, INPUT);  pinMode(Trig_1, OUTPUT);
  pinMode(Echo_2, INPUT);  pinMode(Trig_2, OUTPUT);
  pinMode(Echo_3, INPUT);  pinMode(Trig_3, OUTPUT);
  pinMode(IN1, OUTPUT);  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);  pinMode(IN4, OUTPUT);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  servoDireccion.setPeriodHertz(50);
  servoDireccion.attach(PIN_SERVO_DIRECCION, 500, 2400);
  servoBarrido.setPeriodHertz(50);
  servoBarrido.attach(PIN_SERVO_BARRIDO, 500, 2400);

  pwmA.attachPin(ENA, 1000, 8);  // 1 kHz, 8 bits (0..255), como analogWrite
  pwmB.attachPin(ENB, 1000, 8);

  detener();
  servoDireccion.write(ANG_CENTRO);
  servoBarrido.write(90);
  menu();
}

void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();
  while (Serial.available()) Serial.read();  // descarta el Enter

  switch (c) {
    case '1': pruebaSensores();       break;
    case '2': pruebaServoDireccion(); break;
    case '3': pruebaServoBarrido();   break;
    case '4': pruebaMotores();        break;
    case '5': pruebaCarga();          break;
    case '6': pruebaBarrido();        break;
    case 'p': detener(); Serial.println("detenido"); break;
    default: break;
  }
  menu();
}
