/*
 * =====================================================================
 *  PASO 1 - SOLO MOTORES DC CON L298N
 *  Mismos pines que el código final. Sin servos, sin sensores, sin WiFi.
 * =====================================================================
 *
 *  Auto SOBRE UN SOPORTE (ruedas en el aire). Monitor Serie a 115200.
 *
 *  Comandos (una letra + Enter):
 *    w   avanzar          s   retroceder        x   detener
 *    a   solo motor A adelante (para saber cuál rueda es A)
 *    b   solo motor B adelante
 *    +   velocidad +25    -   velocidad -25     (parte en 200)
 *    r   rampa 0 -> 255 de a 10, imprimiendo el valor: anoten el PWM
 *        mínimo al que ambas ruedas empiezan a girar
 *    t   test automático: adelante 1 s, pausa, atrás 1 s, pausa
 *
 *  Qué verificar en el L298N antes de cargar:
 *    - pack conectado a 12V/GND: 7,4 a 8,4 V medidos
 *    - jumper 5V-EN puesto -> LED rojo encendido
 *    - jumpers de ENA y ENB QUITADOS (van cables a GPIO 25 y 33)
 *    - GND del L298N unido a GND del ESP32
 *    - IN1->26  IN2->27  ENA->25  IN3->32  IN4->19  ENB->33
 */

#include <ESP32Servo.h>   // solo por ESP32PWM, el mismo PWM que usará el código final

// Motor A
#define ENA 25
#define IN1 26
#define IN2 27
// Motor B
#define ENB 33
#define IN3 32
#define IN4 19

ESP32PWM pwmA;
ESP32PWM pwmB;

int velocidad = 200;

void motorA(int vel, bool adelante) {
  digitalWrite(IN1, adelante ? HIGH : LOW);
  digitalWrite(IN2, adelante ? LOW  : HIGH);
  pwmA.write(vel);
}

void motorB(int vel, bool adelante) {
  digitalWrite(IN3, adelante ? HIGH : LOW);
  digitalWrite(IN4, adelante ? LOW  : HIGH);
  pwmB.write(vel);
}

void avanzar(int vel)    { motorA(vel, true);  motorB(vel, true);  }
void retroceder(int vel) { motorA(vel, false); motorB(vel, false); }

void detener() {
  pwmA.write(0);
  pwmB.write(0);
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

void rampa() {
  Serial.println("Rampa 0 -> 255. Anoten el PWM mínimo al que cada rueda empieza a girar.");
  for (int v = 0; v <= 255; v += 10) {
    Serial.printf("  PWM = %d\n", v);
    avanzar(v);
    delay(600);
  }
  detener();
  Serial.println("fin rampa");
}

void testAutomatico() {
  Serial.println("adelante 1 s");
  avanzar(velocidad);
  delay(1000);
  detener();
  delay(500);
  Serial.println("atrás 1 s");
  retroceder(velocidad);
  delay(1000);
  detener();
  Serial.println("fin test. Ambas ruedas deben girar en el MISMO sentido en cada tramo.");
}

void menu() {
  Serial.printf("\n--- MOTORES (vel=%d) --- w adelante | s atrás | x parar | a solo A | b solo B | + - vel | r rampa | t test\n", velocidad);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  pwmA.attachPin(ENA, 1000, 8);   // 1 kHz, 8 bits: 0..255
  pwmB.attachPin(ENB, 1000, 8);

  detener();
  Serial.println("\n===== PASO 1: MOTORES =====");
  menu();
}

void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();
  while (Serial.available()) Serial.read();

  switch (c) {
    case 'w': avanzar(velocidad);    Serial.println("adelante"); break;
    case 's': retroceder(velocidad); Serial.println("atrás");    break;
    case 'x': detener();             Serial.println("detenido"); break;
    case 'a': detener(); motorA(velocidad, true); Serial.println("solo motor A adelante"); break;
    case 'b': detener(); motorB(velocidad, true); Serial.println("solo motor B adelante"); break;
    case '+': velocidad = min(255, velocidad + 25); menu(); break;
    case '-': velocidad = max(0,   velocidad - 25); menu(); break;
    case 'r': rampa(); break;
    case 't': testAutomatico(); break;
    default: menu(); break;
  }
}
