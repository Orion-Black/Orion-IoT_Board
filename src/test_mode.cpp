#include "test_mode.h"
#include "definitions.h"
#include <Arduino.h>
#include <Wire.h>

// Librerías de Hardware
#include <DHT11.h>
#include <ESP32Servo.h>
#include <TinyGPS++.h>

// --- DEFINICIÓN DE HARDWARE (Idéntico a local_server) ---
#define PIN_RELAY_1 26
#define PIN_RELAY_2 27
#define PIN_RELAY_3 14
#define PIN_RELAY_4 12
#define PIN_LOCK    13

#define PIN_DHT     23
#define PIN_LDR     34 // Potenciometro está en 35, LDR en 34

#define PIN_GPS_RX  16
#define PIN_GPS_TX  17

#define PIN_SERVO_1 15
#define PIN_SERVO_2 2
#define PIN_SERVO_3 4

#define POT_PIN     35 // Para navegar el menú

// --- VARIABLES EXTERNAS DEL MAIN.CPP ---
// Necesitamos saber si se pulsó confirmar para entrar a los tests
extern volatile bool confirmPressed; 
extern volatile bool deletePressed; // Para salir del menú
extern UIState uiState;             // Para cambiar de estado al salir
extern bool redrawMenu;             // Para forzar redibujado en Main

// --- OBJETOS GLOBALES LOCALES ---
Adafruit_SSD1306* tDisplay;
const char* testMenuItems[] = {
  "Test Relays",
  "Test Cerradura",
  "Test DHT",
  "Test GPS",
  "Test Servos",
  "Volver Atras"
};
const int TEST_MENU_SIZE = 6;
int testIndex = 0;

// --- FUNCIONES AUXILIARES DE LECTURA POT ---
int leerPotTest() {
  static int filtered = 0;
  int raw = analogRead(POT_PIN);
  filtered = (filtered * 7 + raw) / 8;
  return filtered;
}

int leerIndiceTest(int items) {
  static int lastIndex = -1;
  int value = leerPotTest();
  int stepSize = 4096 / items;
  int index = value / stepSize;
  index = constrain(index, 0, items - 1);
  
  if (index != lastIndex) {
    lastIndex = index;
    return index;
  }
  return -1;
}

// ---------------------------------------------------------
// FUNCIONES DE TEST INDIVIDUALES
// ---------------------------------------------------------

void ejecutarTestRelays() {
  tDisplay->clearDisplay();
  tDisplay->setCursor(0,0);
  tDisplay->println("TEST RELAYS");
  tDisplay->display();

  int relays[] = {PIN_RELAY_1, PIN_RELAY_2, PIN_RELAY_3, PIN_RELAY_4};
  
  for(int i=0; i<4; i++) {
    tDisplay->setCursor(0, 20 + (i*10));
    tDisplay->print("Relay "); tDisplay->print(i+1); tDisplay->print(" ON");
    tDisplay->display();
    
    digitalWrite(relays[i], HIGH);
    delay(500);
    digitalWrite(relays[i], LOW);
    delay(200);
  }
  
  tDisplay->setCursor(0, 55);
  tDisplay->println("Finalizado.");
  tDisplay->display();
  delay(1000);
}

void ejecutarTestCerradura() {
  tDisplay->clearDisplay();
  tDisplay->setCursor(0,0);
  tDisplay->println("TEST CERRADURA");
  tDisplay->println("\nAbriendo...");
  tDisplay->display();

  digitalWrite(PIN_LOCK, HIGH);
  
  // Cuenta regresiva visual
  for(int i=3; i>0; i--) {
    tDisplay->setCursor(60, 30);
    tDisplay->setTextSize(2);
    tDisplay->fillRect(60, 30, 30, 20, SSD1306_BLACK); // Borra numero anterior
    tDisplay->print(i);
    tDisplay->display();
    delay(1000);
  }
  
  digitalWrite(PIN_LOCK, LOW);
  tDisplay->setTextSize(1);
  tDisplay->println("\nCerrado.");
  tDisplay->display();
  delay(1000);
}

void ejecutarTestDHT() {
  DHT11 dhtTest(PIN_DHT);
  tDisplay->clearDisplay();
  tDisplay->setCursor(0,0);
  tDisplay->println("TEST DHT11");
  tDisplay->println("Leyendo...");
  tDisplay->display();
  
  delay(1000); // El DHT necesita tiempo
  
  int temp = 0;
  int hum = 0;
  int res = dhtTest.readTemperatureHumidity(temp, hum);

  tDisplay->clearDisplay();
  tDisplay->setCursor(0,0);
  tDisplay->println("TEST DHT11");
  tDisplay->println("Resultados:\n");
  
  if(res == 0) {
    tDisplay->print("Temp: "); tDisplay->print(temp); tDisplay->println(" C");
    tDisplay->print("Hum:  "); tDisplay->print(hum); tDisplay->println(" %");
  } else {
    tDisplay->println("Error en Sensor!");
    tDisplay->print("Codigo: "); tDisplay->println(res);
  }
  tDisplay->println("\n[CONFIRM] salir");
  tDisplay->display();

  // Esperar botón para salir
  confirmPressed = false;
  while(!confirmPressed) { delay(10); } 
  confirmPressed = false; // Limpiar bandera
}

void ejecutarTestGPS() {
  TinyGPSPlus gpsTest;
  HardwareSerial gpsSerialTest(2);
  gpsSerialTest.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);

  tDisplay->clearDisplay();
  tDisplay->setCursor(0,0);
  tDisplay->println("TEST GPS");
  tDisplay->println("Buscando Sats...");
  tDisplay->println("[CONFIRM] salir");
  tDisplay->display();

  confirmPressed = false;
  unsigned long lastPrint = 0;

  while(!confirmPressed) {
    // Alimentar GPS constantemente
    while (gpsSerialTest.available() > 0) {
      gpsTest.encode(gpsSerialTest.read());
    }

    // Actualizar pantalla cada 500ms
    if(millis() - lastPrint > 500) {
      lastPrint = millis();
      tDisplay->fillRect(0, 25, 128, 39, SSD1306_BLACK); // Limpiar zona de datos
      tDisplay->setCursor(0, 25);
      
      if(gpsTest.satellites.isValid()){
        tDisplay->print("Sats: "); tDisplay->println(gpsTest.satellites.value());
      } else {
        tDisplay->println("Sats: Buscando...");
      }

      if(gpsTest.location.isValid()) {
         tDisplay->print("Lat: "); tDisplay->println(gpsTest.location.lat(), 5);
         tDisplay->print("Lon: "); tDisplay->println(gpsTest.location.lng(), 5);
         tDisplay->print("Alt: "); tDisplay->print(gpsTest.altitude.meters(), 2); tDisplay->println(" m");
      } else {
         tDisplay->println("Pos: Sin Fix");
      }
      tDisplay->display();
    }
  }
  confirmPressed = false; // Limpiar bandera al salir
}

void ejecutarTestServos() {
  Servo s1, s2, s3;
  s1.attach(PIN_SERVO_1);
  s2.attach(PIN_SERVO_2);
  s3.attach(PIN_SERVO_3);

  int angulos[] = {0, 90, 180};

  tDisplay->clearDisplay();
  tDisplay->setCursor(0,0);
  tDisplay->println("TEST SERVOS");
  tDisplay->display();

  for(int j=0; j<3; j++) { // Recorre angulos
    int angulo = angulos[j];
    
    tDisplay->fillRect(0, 20, 128, 40, SSD1306_BLACK);
    tDisplay->setCursor(0, 20);
    tDisplay->print("Moviendo a: "); tDisplay->print(angulo);
    tDisplay->display();

    s1.write(angulo);
    s2.write(angulo);
    s3.write(angulo);
    delay(1000);
  }

  // Desconectar para evitar ruido
  s1.detach(); s2.detach(); s3.detach();
  
  tDisplay->setCursor(0, 40);
  tDisplay->println("\nFinalizado.");
  tDisplay->display();
  delay(1000);
}

// ---------------------------------------------------------
// FUNCIONES PRINCIPALES
// ---------------------------------------------------------

void iniciarModoTest(Adafruit_SSD1306* displayPtr) {
  tDisplay = displayPtr;
  
  // Asegurar configuración de Hardware
  pinMode(PIN_RELAY_1, OUTPUT); digitalWrite(PIN_RELAY_1, LOW);
  pinMode(PIN_RELAY_2, OUTPUT); digitalWrite(PIN_RELAY_2, LOW);
  pinMode(PIN_RELAY_3, OUTPUT); digitalWrite(PIN_RELAY_3, LOW);
  pinMode(PIN_RELAY_4, OUTPUT); digitalWrite(PIN_RELAY_4, LOW);
  pinMode(PIN_LOCK, OUTPUT);    digitalWrite(PIN_LOCK, LOW);
  
  // No iniciamos Serial del GPS ni Servos aqui, 
  // se inician dentro de su test específico para ahorrar recursos.
}

void dibujarMenuTest() {
  tDisplay->clearDisplay();
  tDisplay->setTextSize(1);
  tDisplay->setTextColor(SSD1306_WHITE);
  tDisplay->setCursor(0, 0);
  //tDisplay->println("== MODO TEST ==");
  
  for (int i = 0; i < TEST_MENU_SIZE; i++) {
    tDisplay->setCursor(0, 0 + (i * 10)); // Espaciado ajustado
    if (i == testIndex) {
      tDisplay->print("-> ");
    } else {
      tDisplay->print("   ");
    }
    tDisplay->println(testMenuItems[i]);
  }
  tDisplay->display();
}

void loopModoTest() {
  // 1. Leer Potenciometro para navegación
  int idx = leerIndiceTest(TEST_MENU_SIZE);
  if (idx >= 0 && idx != testIndex) {
    testIndex = idx;
    dibujarMenuTest(); // Redibujar solo si cambia
  }

  // 2. Primera vez que entra, dibujar
  static bool firstRun = true;
  if (firstRun) {
    dibujarMenuTest();
    firstRun = false;
  }

  // 3. Manejo de Selección
  if (confirmPressed) {
    confirmPressed = false; // Consumir evento
    
    switch (testIndex) {
      case 0: ejecutarTestRelays(); break;
      case 1: ejecutarTestCerradura(); break;
      case 2: ejecutarTestDHT(); break;
      case 3: ejecutarTestGPS(); break;
      case 4: ejecutarTestServos(); break;
      case 5: // Volver Atras
        uiState = UI_MAIN_MENU; 
        redrawMenu = true;
        firstRun = true; // Reset para la proxima vez
        break;
    }
    
    // Al volver de un test, redibujar menú
    if(uiState == UI_TEST_MODE) dibujarMenuTest();
  }
}