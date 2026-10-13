/*
 * =====================================================================
 *  PASO 2 - MOTORES DC + SERVO DE DIRECCIÓN
 *  Sin sensores, sin servo de barrido, sin WiFi.
 * =====================================================================
 *
 *  Calibración de la dirección (11-oct-2026): centro 100°, tope izquierdo
 *  120°, tope derecho 75°. Motor A = rueda derecha, motor B = rueda izquierda.
 *
 *  Primero en el aire, después EN EL PISO (con espacio libre alrededor).
 *  Monitor Serie a 115200. Comandos (letra + Enter):
 *
 *    Dirección:   i izquierda suave   I izquierda fuerte
 *                 c centro
 *                 d derecha suave     D derecha fuerte
 *    Motores:     w adelante   s atrás   x parar   + - velocidad (±10)
 *    Maniobras:   l evasión completa saliendo hacia la IZQUIERDA
 *                 r evasión completa saliendo hacia la DERECHA
 *                 (retrocede con ruedas al lado contrario, luego avanza en
 *                  arco hacia el lado elegido; termina centrado y detenido)
 *    Carga:       t motores a fondo + dirección de tope a tope 5 veces.
 *                 Si reaparece "ARRANQUE" en el monitor, hubo reset.
 *
 *  Qué medir en el piso:
 *    a) Con 'w' y '-': el PWM mínimo que mueve el auto cargado. Ese número
 *       + 20 es el VEL_PRECAUCION del código final.
 *    b) Con 'c' y 'w': que avance recto 2 m. Si se desvía, corregir
 *       ANG_CENTRO de a 1° (mayor = más a la izquierda).
 *    c) Con 'l' y 'r': cuánto gira la trompa. Ajustar T_RETROCESO y
 *       T_AVANCE_ARCO hasta que termine apuntando ~45° hacia el lado elegido.
 */

#include <ESP32Servo.h>
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

// ---------- Dirección (CALIBRADA) ----------
const int DIR_MIN        = 78;   // tope derecho 75 + margen
const int DIR_MAX        = 117;  // tope izquierdo 120 - margen
const int ANG_CENTRO     = 100;
const int ANG_IZQ_SUAVE  = 110;
const int ANG_DER_SUAVE  = 89;
const int ANG_IZQ_FUERTE = 117;
const int ANG_DER_FUERTE = 78;

// ---------- Velocidades ----------
int velocidad = 200;
const int VEL_EVASION = 180;

// ---------- Tiempos de maniobra (ms) - AJUSTAR EN EL PISO ----------
const unsigned long T_SERVO_DIRECCION = 150;
const unsigned long T_RETROCESO       = 850;
const unsigned long T_AVANCE_ARCO     = 500;

Servo servoDireccion;
ESP32PWM pwmA;
ESP32PWM pwmB;

enum Lado { IZQUIERDA, DERECHA };

// =====================================================
// Actuadores
// =====================================================

void direccion(int ang) {
  int a = constrain(ang, DIR_MIN, DIR_MAX);
  servoDireccion.write(a);
  Serial.printf("  direccion -> %d°\n", a);
}

int anguloDireccion(Lado lado, bool fuerte) {
  if (lado == IZQUIERDA) return fuerte ? ANG_IZQ_FUERTE : ANG_IZQ_SUAVE;
  return fuerte ? ANG_DER_FUERTE : ANG_DER_SUAVE;
}

Lado opuesto(Lado lado) { return (lado == IZQUIERDA) ? DERECHA : IZQUIERDA; }

void avanzar(int vel) {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  pwmA.write(vel); pwmB.write(vel);
}

void retroceder(int vel) {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  pwmA.write(vel); pwmB.write(vel);
}

void detener() {
  pwmA.write(0); pwmB.write(0);
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

// =====================================================
// Maniobra de evasión (idéntica a la del código final)
// Al RETROCEDER con las ruedas giradas hacia un lado, la trompa gira hacia
// el lado CONTRARIO (como al estacionar). Para salir hacia 'lado':
// retrocede con ruedas al opuesto, luego avanza en arco hacia 'lado'.
// =====================================================

void evadir(Lado lado) {
  Serial.printf("\nEVASION hacia la %s\n", lado == IZQUIERDA ? "IZQUIERDA" : "DERECHA");
  detener();
  delay(100);

  Serial.println("  1) retrocede con ruedas al lado contrario");
  direccion(anguloDireccion(opuesto(lado), false));
  delay(T_SERVO_DIRECCION);
  retroceder(VEL_EVASION);
  delay(T_RETROCESO);
  detener();
  delay(50);

  Serial.println("  2) avanza en arco hacia el lado elegido");
  direccion(anguloDireccion(lado, true));
  delay(T_SERVO_DIRECCION);
  avanzar(VEL_EVASION);
  delay(T_AVANCE_ARCO);
  detener();

  direccion(ANG_CENTRO);
  Serial.println("  fin. ¿Quedó la trompa apuntando hacia ese lado?");
}

void pruebaCarga() {
  Serial.println("\nCARGA: motores a fondo + direccion tope a tope x5. Si sale 'ARRANQUE', hubo reset.");
  avanzar(255);
  for (int i = 1; i <= 5; i++) {
    direccion(ANG_IZQ_FUERTE); delay(600);
    direccion(ANG_DER_FUERTE); delay(600);
  }
  detener();
  direccion(ANG_CENTRO);
  Serial.println("fin carga: no hubo reset.");
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

void menu() {
  Serial.printf("\n--- PASO 2 (vel=%d) --- i/I izq | c centro | d/D der | w adelante | s atras | x parar | +/- vel | l evasion izq | r evasion der | t carga\n", velocidad);
}

// =====================================================

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n===== ARRANQUE: PASO 2 MOTORES + DIRECCION =====");
  Serial.printf("Motivo del ultimo reset: %s\n", motivoReset(esp_reset_reason()));

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
  menu();
}

void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();
  while (Serial.available()) Serial.read();

  switch (c) {
    case 'i': direccion(ANG_IZQ_SUAVE);  break;
    case 'I': direccion(ANG_IZQ_FUERTE); break;
    case 'c': direccion(ANG_CENTRO);     break;
    case 'd': direccion(ANG_DER_SUAVE);  break;
    case 'D': direccion(ANG_DER_FUERTE); break;
    case 'w': avanzar(velocidad);    Serial.println("adelante"); break;
    case 's': retroceder(velocidad); Serial.println("atras");    break;
    case 'x': detener();             Serial.println("detenido"); break;
    case '+': velocidad = min(255, velocidad + 10); menu(); break;
    case '-': velocidad = max(0,   velocidad - 10); menu(); break;
    case 'l': evadir(IZQUIERDA); break;
    case 'r': evadir(DERECHA);   break;
    case 't': pruebaCarga();     break;
    default:  menu(); break;
  }
}
