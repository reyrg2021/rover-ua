/*
 * =====================================================================
 *  CALIBRACIÓN DE LOS DOS SERVOS - VEHÍCULO AUTÓNOMO EPI
 *  Mismos pines que vehiculo_barrido_v2.ino
 * =====================================================================
 *
 *  ANTES DE ALIMENTAR: desacoplar el varillaje del servo de dirección
 *  (sacar el tornillo del brazo) y el soporte del sensor del servo de
 *  barrido. Los servos deben poder girar libres.
 *
 *  Monitor Serie a 115200, con "Nueva línea" al final. Comandos:
 *
 *    c        ambos servos a 90° (centro). Montar el brazo con las ruedas
 *             rectas y el soporte con el sensor al frente, SIN apagar.
 *    d<ang>   servo de DIRECCIÓN al ángulo indicado, p.ej. d80
 *    b<ang>   servo de BARRIDO al ángulo indicado, p.ej. b150
 *    +  -     último servo usado: +5° / -5°
 *    >  <     último servo usado: +1° / -1°
 *    s        barrido automático LENTO del servo de barrido entre
 *             B_MIN y B_MAX (ver abajo). NO existe para la dirección a
 *             propósito: con el varillaje acoplado clavaría los topes.
 *    x        libera (detach) los dos servos: dejan de hacer fuerza.
 *             Usar si uno zumba contra un tope. Luego 'c' los reactiva.
 *    ?        imprime el estado y esta ayuda
 *
 *  Cómo encontrar los topes de la dirección (con el varillaje ya puesto):
 *  desde 90, bajar de 5 en 5 (d85, d80, ...) mirando las ruedas. El tope
 *  es donde las ruedas dejan de moverse o el servo empieza a zumbar.
 *  Retroceder 3 a 5 grados y anotar. Repetir subiendo (d95, d100, ...).
 *  Esos dos valores van a DIR_MIN y DIR_MAX en la versión 2.
 */

#include <ESP32Servo.h>

#define PIN_SERVO_DIRECCION 18
#define PIN_SERVO_BARRIDO   23

// Límites para el barrido automático. Si el servo zumba en 0 o 180, usar 10 y 170.
const int B_MIN = 0;
const int B_MAX = 180;
const int PASO_LENTO_MS = 30;   // ms por grado en el barrido automático

// Límites de seguridad de la dirección DURANTE la calibración. Partir
// amplios; a medida que encuentren los topes, pueden acotarlos aquí.
const int D_MIN_SEGURO = 20;
const int D_MAX_SEGURO = 160;

Servo servoDireccion;
Servo servoBarrido;

int angDireccion = 90;
int angBarrido   = 90;
bool servosActivos = false;
char ultimoServo = 'd';   // a quién aplican + - > <

void activarServos() {
  if (servosActivos) return;
  servoDireccion.setPeriodHertz(50);
  servoDireccion.attach(PIN_SERVO_DIRECCION, 500, 2400);
  servoBarrido.setPeriodHertz(50);
  servoBarrido.attach(PIN_SERVO_BARRIDO, 500, 2400);
  servosActivos = true;
}

void liberarServos() {
  servoDireccion.detach();
  servoBarrido.detach();
  servosActivos = false;
  Serial.println("Servos liberados (sin fuerza). 'c' para reactivar.");
}

void moverDireccion(int ang) {
  activarServos();
  angDireccion = constrain(ang, D_MIN_SEGURO, D_MAX_SEGURO);
  servoDireccion.write(angDireccion);
  ultimoServo = 'd';
  Serial.printf("DIRECCION -> %d°\n", angDireccion);
}

void moverBarrido(int ang) {
  activarServos();
  angBarrido = constrain(ang, 0, 180);
  servoBarrido.write(angBarrido);
  ultimoServo = 'b';
  Serial.printf("BARRIDO   -> %d°\n", angBarrido);
}

void barridoLento() {
  activarServos();
  Serial.printf("Barrido lento %d -> %d -> %d. Observen si zumba en los extremos.\n", B_MIN, B_MAX, B_MIN);
  for (int a = angBarrido; a >= B_MIN; a--) { servoBarrido.write(a); delay(PASO_LENTO_MS); }
  delay(500);
  for (int a = B_MIN; a <= B_MAX; a++)      { servoBarrido.write(a); delay(PASO_LENTO_MS); }
  delay(500);
  for (int a = B_MAX; a >= 90; a--)         { servoBarrido.write(a); delay(PASO_LENTO_MS); }
  angBarrido = 90;
  ultimoServo = 'b';
  Serial.println("Barrido terminado, sensor en 90°. ¿Hacia qué lado apuntaba en 0°? (CERO_A_LA_DERECHA)");
}

void ayuda() {
  Serial.println("\n--- CALIBRACION ---");
  Serial.printf("Estado: direccion=%d°  barrido=%d°  servos=%s  (+/- actuan sobre '%c')\n",
                angDireccion, angBarrido, servosActivos ? "activos" : "liberados", ultimoServo);
  Serial.println("c centro | d<ang> direccion | b<ang> barrido | + - (5°) | > < (1°) | s barrido lento | x liberar | ? ayuda");
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n===== CALIBRACION DE SERVOS =====");
  Serial.println("Los servos parten LIBERADOS. Con el varillaje y el soporte desacoplados, envie 'c'.");
  ayuda();
}

void loop() {
  if (!Serial.available()) return;
  String linea = Serial.readStringUntil('\n');
  linea.trim();
  if (linea.length() == 0) return;

  char c = linea.charAt(0);
  int valor = linea.substring(1).toInt();

  switch (c) {
    case 'c':
      moverDireccion(90);
      moverBarrido(90);
      Serial.println("Ambos en 90°. Monten el brazo con las ruedas rectas y el sensor al frente, sin apagar.");
      break;
    case 'd': moverDireccion(valor); break;
    case 'b': moverBarrido(valor);   break;
    case '+': (ultimoServo == 'd') ? moverDireccion(angDireccion + 5) : moverBarrido(angBarrido + 5); break;
    case '-': (ultimoServo == 'd') ? moverDireccion(angDireccion - 5) : moverBarrido(angBarrido - 5); break;
    case '>': (ultimoServo == 'd') ? moverDireccion(angDireccion + 1) : moverBarrido(angBarrido + 1); break;
    case '<': (ultimoServo == 'd') ? moverDireccion(angDireccion - 1) : moverBarrido(angBarrido - 1); break;
    case 's': barridoLento(); break;
    case 'x': liberarServos(); break;
    default:  ayuda(); break;
  }
}
