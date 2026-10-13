// Libreria Servo
#include <ESP32Servo.h>
// =============================
// L298N - ESP32
// =============================

// Motor A
#define ENA 25
#define IN1 26
#define IN2 27

// Motor B
#define ENB 33
#define IN3 32
#define IN4 19

// Servomotor
#define pin_S 18
Servo servoDireccion;

// sensores ultrasónicos 
#define Echo_1 5
#define Trig_1 17

#define Echo_2 4 // Gnd se desconecta con facilidad
#define Trig_2 16

#define Echo_3 22
#define Trig_3 21

// ---------- Umbrales de distancia ----------
const int DIST_CRITICA      = 20; // cm -> maniobra de evasión
const int DIST_PRECAUCION   = 40; // cm -> reducir velocidad / corregir rumbo
const int MARGEN_HISTERESIS = 5;  // cm de margen para no oscilar entre zonas

// ---------- Ángulos del servo ----------
const int ANG_CENTRO     = 90;
const int ANG_IZQ_SUAVE  = 55;  // corrección leve (precaución)
const int ANG_DER_SUAVE  = 125;
const int ANG_IZQ_FUERTE = 45;  // giro de evasión
const int ANG_DER_FUERTE = 145;

// ---------- Velocidades ----------
const int VEL_NORMAL     = 220;
const int VEL_PRECAUCION = 150;
const int VEL_EVASION    = 180;

// Estado de zona con histéresis
enum Zona { LIBRE, PRECAUCION, CRITICA };
Zona zonaAnterior = LIBRE;

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

// Determina la zona actual aplicando histéresis
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

// AVANZAR
void avanzar(int velocidad) {
  analogWrite(ENA, velocidad);
  analogWrite(ENB, velocidad);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

// RETROCEDER
void retroceder(int velocidad) {
  analogWrite(ENA, velocidad);
  analogWrite(ENB, velocidad);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

// DETENER
void detener() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// Maniobra de evasión: gira las ruedas HACIA la zona libre y retrocede así
// (ruedas ya giradas), y después traza el arco de salida hacia adelante
// SIEMPRE hacia el lado CONTRARIO al que giró en reversa.
// (nunca mueve el servo con el auto detenido, porque así no gira el vehículo)
void evadirObstaculo(long distIzqInicial, long distDerInicial) {
  // Reversa el efecto del giro suele ser inverso al de avanzar
  // (como al estacionar un auto).
  bool giroIzqEnReversa = (distIzqInicial > distDerInicial);
  int anguloReversa = giroIzqEnReversa ? ANG_IZQ_SUAVE : ANG_DER_SUAVE;

  detener();
  delay(100);

  servoDireccion.write(anguloReversa);
  delay(45); // esperar a que el servo llegue a posición antes de mover

  retroceder(VEL_EVASION);
  delay(850);
  detener();
  delay(50);

  // Releemos los laterales 
  long distIzq = leerDistanciaCm(Trig_3, Echo_3);
  delay(15);
  long distDer = leerDistanciaCm(Trig_1, Echo_1);

  if (distIzq <= DIST_CRITICA && distDer <= DIST_CRITICA) {
    // también bloqueado a ambos lados -> no hay hacia dónde ir, se queda detenido
    servoDireccion.write(ANG_CENTRO);
    return;
  }

  // Avanza girando hacia el lado CONTRARIO al que giró en reversa
  if (giroIzqEnReversa) {
    servoDireccion.write(ANG_DER_FUERTE); // en reversa giró a la izquierda -> ahora gira a la derecha
  } else {
    servoDireccion.write(ANG_IZQ_FUERTE); // en reversa giró a la derecha -> ahora gira a la izquierda
  }
  delay(250); // esperar a que el servo llegue a posición antes de mover

  avanzar(VEL_EVASION); // AHORA sí gira, porque se mueve con las ruedas giradas
  delay(500);
  detener();

  servoDireccion.write(ANG_CENTRO);
}

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

  servoDireccion.setPeriodHertz(50);
  servoDireccion.attach(pin_S, 500, 2400);
  servoDireccion.write(ANG_CENTRO);

  detener();
  delay(1000);
}

void loop() {
  long distIzq  = leerDistanciaCm(Trig_3, Echo_3); // Sensor izquierdo
  delay(15);
  long distCent = leerDistanciaCm(Trig_2, Echo_2); // Sensor centro
  delay(15);
  long distDer  = leerDistanciaCm(Trig_1, Echo_1); // Sensor derecho
  delay(15);

  Serial.printf("IZQ: %ld cm | CENT: %ld cm | DER: %ld cm\n",
                distIzq, distCent, distDer);

  Zona zonaActual = determinarZona(distCent, zonaAnterior);
  zonaAnterior = zonaActual;

  if (zonaActual == CRITICA) {
    evadirObstaculo(distIzq, distDer);
  }
  else if (zonaActual == PRECAUCION) {
    int anguloCorreccion = ANG_CENTRO;// Corrección leve de rumbo si solo un lateral

    if (distIzq <= DIST_PRECAUCION && distDer > DIST_PRECAUCION) {
      anguloCorreccion = ANG_DER_SUAVE; // se aleja del lado izquierdo

    } else if (distDer <= DIST_PRECAUCION && distIzq > DIST_PRECAUCION) {
      anguloCorreccion = ANG_IZQ_SUAVE; // se aleja del lado derecho
    }
    servoDireccion.write(anguloCorreccion);
    avanzar(VEL_PRECAUCION);
  }
  else {
    servoDireccion.write(ANG_CENTRO);
    avanzar(VEL_NORMAL);
  }

  delay(50);
}