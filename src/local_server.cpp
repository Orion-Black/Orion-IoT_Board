#include "local_server.h"

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>
#include <ESPmDNS.h>
#include <DHT11.h>
#include <LDR_10K.h>
#include <ESP32Servo.h>
#include <TinyGPS++.h>

// HARDWARE
// ---------------------------------------------------------
#define PIN_RELAY_1 26
#define PIN_RELAY_2 27
#define PIN_RELAY_3 14
#define PIN_RELAY_4 12
#define PIN_LOCK 13

#define PIN_LDR 34
#define PIN_DHT 23
#define PIN_GPS_RX 16
#define PIN_GPS_TX 17

#define PIN_SERVO_1 15
#define PIN_SERVO_2 2
#define PIN_SERVO_3 4

// ---------------------------------------------------------
AsyncWebServer server(80);

// Sensores
DHT11 dht11(PIN_DHT);
LDR_10K ldr(PIN_LDR);

// GPS
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);

// Servos
Servo servo1;
Servo servo2;
Servo servo3;

// ---------------------------------------------------------
bool servidorActivo = false;

// ---------------------------------------------------------
// COMANDOS (idénticos a tu implementación)
// ---------------------------------------------------------
String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void procesarComando(String cmd) {
  cmd.trim();
  String categoria = getValue(cmd, ' ', 0);
  String accion = getValue(cmd, ' ', 1);
  String objetivo = getValue(cmd, ' ', 2);
  String valor = getValue(cmd, ' ', 3);

  // --- RELAYS ---
  if (categoria == "relay") {
    int pin = -1;
    if (objetivo == "1") pin = PIN_RELAY_1;
    else if (objetivo == "2") pin = PIN_RELAY_2;
    else if (objetivo == "3") pin = PIN_RELAY_3;
    else if (objetivo == "4") pin = PIN_RELAY_4;

    if (pin != -1) {
      if (accion == "set") {
        bool estado = (valor == "on");
        digitalWrite(pin, estado ? HIGH : LOW);
        WebSerial.println("OK: Relay " + objetivo + (estado ? " ENCENDIDO" : " APAGADO"));
      } else if (accion == "get") {
        int estado = digitalRead(pin);
        WebSerial.println("Info: Relay " + objetivo + " esta " + (estado ? "ON" : "OFF"));
      }
    } else {
      WebSerial.println("Error: Relay desconocido (use 1-4)");
    }
  }

  // --- LOCK ---
  else if (categoria == "lock") {
    if (accion == "open") {
      WebSerial.println("Abriendo cerradura por 3s...");
      digitalWrite(PIN_LOCK, HIGH);
      delay(3000);  // Bloqueante breve intencional por seguridad
      digitalWrite(PIN_LOCK, LOW);
      WebSerial.println("Cerradura cerrada.");
    } else if (accion == "set") {
      bool estado = (objetivo == "on");
      digitalWrite(PIN_LOCK, estado ? HIGH : LOW);
      WebSerial.println("OK: Cerradura " + String(estado ? "ACTIVADA" : "DESACTIVADA"));
    }
  }

  // --- SERVOS ---
  else if (categoria == "servo") {
    if (accion == "set") {
      int angulo = valor.toInt();
      if (angulo < 0) angulo = 0;
      if (angulo > 180) angulo = 180;

      if (objetivo == "1") {
        servo1.write(angulo);
        WebSerial.println("Servo 1 -> " + String(angulo));
      } else if (objetivo == "2") {
        servo2.write(angulo);
        WebSerial.println("Servo 2 -> " + String(angulo));
      } else if (objetivo == "3") {
        servo3.write(angulo);
        WebSerial.println("Servo 3 -> " + String(angulo));
      } else {
        WebSerial.println("Error: Servo desconocido (use 1-3)");
      }
    }
  }

  // --- COMANDOS DE SENSORES ---
  else if (categoria == "sensor") {
    if (accion == "dht") {
      int temp = 0, hum = 0;
      int res = dht11.readTemperatureHumidity(temp, hum);
      if (res == 0) WebSerial.println("DHT: " + String(temp) + "C, " + String(hum) + "%");
      else WebSerial.println("DHT Error: " + String(res));
    } else if (accion == "ldr") {
      int raw = ldr.getAnalogLDR();
      int pct = ldr.getPercentageLDR();
      WebSerial.println("LDR: " + String(pct) + "% (Raw: " + String(raw) + ")");
    } else if (accion == "gps") {
      if (gps.location.isValid()) {
        String gpsData = "GPS: Lat=" + String(gps.location.lat(), 6) + " Lon=" + String(gps.location.lng(), 6) + " Alt=" + String(gps.altitude.meters(), 2) + " Sats=" + String(gps.satellites.value());
        WebSerial.println(gpsData);
      } else {
        WebSerial.println("GPS: Buscando satelites... (Asegurate de estar al aire libre)");
      }
    } else if (accion == "all") {
      // Recursividad simple para imprimir todo
      procesarComando("sensor dht");
      procesarComando("sensor ldr");
      procesarComando("sensor gps");
    }
  }

  // --- COMANDOS DE SISTEMA ---
  else if (categoria == "sys") {
    if (accion == "info") {
      WebSerial.println("--- SYSTEM INFO ---");
      WebSerial.println("IP: " + WiFi.localIP().toString());
      WebSerial.println("RSSI: " + String(WiFi.RSSI()) + " dBm");
      WebSerial.println("Uptime: " + String(millis() / 1000) + " s");
    } else if (accion == "reset") {
      WebSerial.println("Reiniciando...");
      delay(500);
      ESP.restart();
    }
  }

  else if (categoria == "help" || categoria == "?") {
    WebSerial.println("--- COMANDOS ---");
    WebSerial.println("ACTUADORES: ");
    WebSerial.println("apagar/encender --> relay set <1-4> on/off");
    WebSerial.println("leer estado --> relay get <1-4>");
    WebSerial.println("Abrir cerradura --> lock open -> (Abierto 3Seg.)");
    WebSerial.println("Cerradura manual --> lock set on/off");
    WebSerial.println("Mover servo --> servo set <1-3> <0-180>");
    WebSerial.println("------");
    WebSerial.println("SENSORES: ");
    WebSerial.println("Leer todo --> sensor all");
    WebSerial.println("Leer GPS --> sensor GPS");
    WebSerial.println("Leer DHT --> sensor dht");
    WebSerial.println("Leer luz --> sensor ldr");
    WebSerial.println("Sistema: ");
    WebSerial.println("Info Hardware --> sys info");
    WebSerial.println("Reiniciar --> sys reset");
  }

  else {
    WebSerial.println("Comando no reconocido. Prueba: 'help' o '?'");
  }
}

void recvMsg(uint8_t* data, size_t len) {
  String cmd;
  for (size_t i = 0; i < len; i++) cmd += char(data[i]);
  procesarComando(cmd);
  delay(10);  // cede tiempo al scheduler (clave)
}

// ---------------------------------------------------------
// API PUBLICA
// ---------------------------------------------------------
bool iniciarServidorLocal() {

  if (!WiFi.isConnected()) return false;

  // Pines
  pinMode(PIN_RELAY_1, OUTPUT);
  pinMode(PIN_RELAY_2, OUTPUT);
  pinMode(PIN_RELAY_3, OUTPUT);
  pinMode(PIN_RELAY_4, OUTPUT);
  pinMode(PIN_LOCK, OUTPUT);

  servo1.attach(PIN_SERVO_1);
  servo2.attach(PIN_SERVO_2);
  servo3.attach(PIN_SERVO_3);

  ldr.begin(12);
  gpsSerial.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  delay(1000);

  if (MDNS.begin("orion-iot")) {
    Serial.println("mDNS iniciado");
  }

  WebSerial.begin(&server);
  WebSerial.onMessage(recvMsg);
  server.begin();

  servidorActivo = true;
  return true;
}

void detenerServidorLocal() {
  if (!servidorActivo) return;

  server.end();
  MDNS.end();
  servidorActivo = false;
}

void loopServidorLocal() {
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
}
