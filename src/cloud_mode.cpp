#include "cloud_mode.h"
#include "definitions.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// --- NUEVA LIBRERÍA ---
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// Hardware Libraries
#include <DHT11.h>
#include <LDR_10K.h>
#include <TinyGPS++.h>
#include <ESP32Servo.h>

// --- DEFINICIÓN DE HARDWARE ---
#define PIN_RELAY_1 26
#define PIN_RELAY_2 27
#define PIN_RELAY_3 14
#define PIN_RELAY_4 12
#define PIN_LOCK    13
#define PIN_DHT     23
#define PIN_LDR     34
#define PIN_GPS_RX  16
#define PIN_GPS_TX  17

// --- CONFIGURACIÓN MQTT (Home Assistant) ---
const char* mqtt_server = "130.107.73.33"; // Tu IP Azure
const int mqtt_port = 1883;
const char* mqtt_user = ""; 
const char* mqtt_pass = ""; 

// --- CONFIGURACIÓN INFLUXDB (Grafana) ---
#define INFLUXDB_URL "http://orion-iot.canadacentral.cloudapp.azure.com:8086"
#define INFLUXDB_TOKEN "mi-token-super-seguro-admin" // Asegúrate de que sea correcto
#define INFLUXDB_ORG "orioniot"
#define INFLUXDB_BUCKET "sensores"
#define TZ_INFO "CST6CDT,M4.1.0,M10.5.0" // Hora CDMX para timestamps correctos

// --- OBJETOS DE RED ---
WiFiClient espClient;
PubSubClient client(espClient);
InfluxDBClient clientInflux(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);

// Punto de datos Influx
Point sensorData("estado_sistema"); // "Measurement" en Influx

Adafruit_SSD1306* cloudDisplay;

// --- OBJETOS DE HARDWARE ---
DHT11 dhtCloud(PIN_DHT);
LDR_10K ldrCloud(PIN_LDR);
TinyGPSPlus gpsCloud;
HardwareSerial gpsSerialCloud(2);

// --- VARIABLES ---
unsigned long lastMsg = 0;
const long interval = 5000; // Leer y enviar cada 5s
unsigned long lockTimer = 0;
bool lockOpen = false;

// --- DECLARACIÓN DE FUNCIONES ---
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void publishDiscovery();
void leerYPublicarSensores(); // Esta función ahora enviará a MQTT y a Influx

// ---------------------------------------------------------
// LÓGICA DE CONTROL (CALLBACK MQTT)
// ---------------------------------------------------------
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  String strTopic = String(topic);

  Serial.print("MQTT CMD ["); Serial.print(topic); Serial.print("]: "); Serial.println(msg);

  if (strTopic.endsWith("relay1/set")) {
    digitalWrite(PIN_RELAY_1, (msg == "ON") ? HIGH : LOW);
    client.publish("orion/relay1/state", msg.c_str());
  }
  else if (strTopic.endsWith("relay2/set")) {
    digitalWrite(PIN_RELAY_2, (msg == "ON") ? HIGH : LOW);
    client.publish("orion/relay2/state", msg.c_str());
  }
  else if (strTopic.endsWith("relay3/set")) {
    digitalWrite(PIN_RELAY_3, (msg == "ON") ? HIGH : LOW);
    client.publish("orion/relay3/state", msg.c_str());
  }
  else if (strTopic.endsWith("relay4/set")) {
    digitalWrite(PIN_RELAY_4, (msg == "ON") ? HIGH : LOW);
    client.publish("orion/relay4/state", msg.c_str());
  }
  else if (strTopic.endsWith("lock/set")) {
    if (msg == "UNLOCK") {
      digitalWrite(PIN_LOCK, HIGH);
      client.publish("orion/lock/state", "UNLOCKED");
      lockOpen = true;
      lockTimer = millis();
    } else {
      digitalWrite(PIN_LOCK, LOW);
      client.publish("orion/lock/state", "LOCKED");
    }
  }
}

// ---------------------------------------------------------
// INICIO DEL MODO CLOUD
// ---------------------------------------------------------
void iniciarModoCloud(Adafruit_SSD1306* displayPtr) {
  cloudDisplay = displayPtr;

  // 1. Hardware Init
  pinMode(PIN_RELAY_1, OUTPUT); digitalWrite(PIN_RELAY_1, LOW);
  pinMode(PIN_RELAY_2, OUTPUT); digitalWrite(PIN_RELAY_2, LOW);
  pinMode(PIN_RELAY_3, OUTPUT); digitalWrite(PIN_RELAY_3, LOW);
  pinMode(PIN_RELAY_4, OUTPUT); digitalWrite(PIN_RELAY_4, LOW);
  pinMode(PIN_LOCK, OUTPUT);    digitalWrite(PIN_LOCK, LOW);
  
  ldrCloud.begin(12);
  gpsSerialCloud.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);

  // 2. Sincronización de Tiempo (NECESARIO PARA INFLUX)
  cloudDisplay->clearDisplay();
  cloudDisplay->setCursor(0,0);
  cloudDisplay->println("Sincronizando reloj...");
  cloudDisplay->display();
  
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // 3. Validación InfluxDB
  cloudDisplay->println("Check InfluxDB...");
  cloudDisplay->display();
  
  if (clientInflux.validateConnection()) {
    Serial.print("InfluxDB OK: "); Serial.println(clientInflux.getServerUrl());
  } else {
    Serial.print("InfluxDB Error: "); Serial.println(clientInflux.getLastErrorMessage());
    cloudDisplay->println("Err Influx!");
    cloudDisplay->display();
    delay(1000);
  }

  // 4. MQTT Init
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setBufferSize(2048); // Buffer grande para Discovery JSON

  // Etiquetas globales para Influx
  sensorData.addTag("dispositivo", "ESP32_Orion_V1");
  sensorData.addTag("ubicacion", "Azure_Demo");
}

// ---------------------------------------------------------
// LOOP PRINCIPAL
// ---------------------------------------------------------
void loopModoCloud() {
  // 1. Alimentar GPS (Siempre, lo más rápido posible)
  while (gpsSerialCloud.available() > 0) {
    gpsCloud.encode(gpsSerialCloud.read());
  }

  // 2. Verificar MQTT
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // 3. Temporizador Cerradura
  if (lockOpen && (millis() - lockTimer > 3000)) {
    digitalWrite(PIN_LOCK, LOW);
    client.publish("orion/lock/state", "LOCKED");
    lockOpen = false;
  }

  // 4. Lectura y Envío de Sensores (Cada 5 seg)
  unsigned long now = millis();
  if (now - lastMsg > interval) {
    lastMsg = now;
    leerYPublicarSensores();
  }
}

// ---------------------------------------------------------
// LECTURA Y ENVÍO DOBLE (MQTT + INFLUX)
// ---------------------------------------------------------
void leerYPublicarSensores() {
  
  // --- A. LEER SENSORES ---
  int temp = 0, hum = 0;
  int dhtStatus = dhtCloud.readTemperatureHumidity(temp, hum);
  int lux = ldrCloud.getPercentageLDR();
  int luxRaw = ldrCloud.getAnalogLDR();

  // --- B. ENVIAR A MQTT (JSON para Home Assistant) ---
  StaticJsonDocument<300> doc;
  doc["temperature"] = temp;
  doc["humidity"] = hum;
  doc["illuminance"] = lux;

  if (gpsCloud.location.isValid()) {
    StaticJsonDocument<200> gpsDoc;
    gpsDoc["latitude"] = gpsCloud.location.lat();
    gpsDoc["longitude"] = gpsCloud.location.lng();
    gpsDoc["gps_accuracy"] = 10;
    char gpsBuffer[200];
    serializeJson(gpsDoc, gpsBuffer);
    client.publish("orion/gps/state", gpsBuffer);
  }

  char jsonBuffer[300];
  serializeJson(doc, jsonBuffer);
  client.publish("orion/sensors/state", jsonBuffer);

  // --- C. ENVIAR A INFLUXDB ---
  sensorData.clearFields(); // Limpiar punto anterior
  
  // Solo enviar datos DHT si la lectura fue válida
  if (dhtStatus == 0) {
    sensorData.addField("temperatura", temp);
    sensorData.addField("humedad", hum);
  }
  sensorData.addField("luz_porcentaje", lux);
  sensorData.addField("luz_raw", luxRaw);

  // Datos GPS si válidos
  if (gpsCloud.location.isValid()) {
    sensorData.addField("latitud", gpsCloud.location.lat());
    sensorData.addField("longitud", gpsCloud.location.lng());
    sensorData.addField("altitud", gpsCloud.altitude.meters());
    sensorData.addField("satelites", (int)gpsCloud.satellites.value());
  }

  // Escribir en BDD
  Serial.println("Enviando a InfluxDB...");
  if (!clientInflux.writePoint(sensorData)) {
    Serial.print("Fallo escritura Influx: ");
    Serial.println(clientInflux.getLastErrorMessage());
  } else {
    Serial.println("InfluxDB Write OK");
  }

  // --- D. ACTUALIZAR PANTALLA OLED ---
  cloudDisplay->clearDisplay();
  cloudDisplay->setCursor(0, 0);
  cloudDisplay->println("== MODO CLOUD ==");
  cloudDisplay->print("MQTT: "); cloudDisplay->println(client.connected() ? "ON" : "OFF");
  cloudDisplay->print("Influx: "); cloudDisplay->println("Enviado");
  
  cloudDisplay->print("T:"); cloudDisplay->print(temp); 
  cloudDisplay->print(" H:"); cloudDisplay->println(hum);
  
  if(gpsCloud.location.isValid()) {
    cloudDisplay->print("GPS: OK Sats:"); cloudDisplay->println(gpsCloud.satellites.value());
  } else {
    cloudDisplay->println("GPS: Buscando...");
  }
  
  cloudDisplay->display();
}

// ---------------------------------------------------------
// RECONEXIÓN Y DISCOVERY (Sin Cambios Mayores)
// ---------------------------------------------------------
void sendDiscovery(const char* component, const char* name, const char* unique_id, const char* device_class, bool isSensor, int relayNum = 0) {
    // (Pega aquí la misma función sendDiscovery que te di en el código anterior)
    // Para ahorrar espacio en esta respuesta no la repito, pero ES NECESARIA.
    // Si la necesitas completa de nuevo, dímelo.
    StaticJsonDocument<600> doc;
    String topic_config = "homeassistant/" + String(component) + "/orion/" + String(unique_id) + "/config";
    doc["name"] = name;
    doc["uniq_id"] = unique_id;
    JsonObject dev = doc.createNestedObject("dev");
    dev["ids"] = "orion_esp32_v1";
    dev["name"] = "Orion IoT System";
    dev["mdl"] = "ESP32 Custom";
    dev["mf"] = "Ing. Jesus Gonzalez";

    if (!isSensor) {
      if (String(component) == "light") {
        String rNum = String(relayNum);
        doc["cmd_t"] = "orion/relay" + rNum + "/set";
        doc["stat_t"] = "orion/relay" + rNum + "/state";
        doc["pl_on"] = "ON"; doc["pl_off"] = "OFF";
      } else if (String(component) == "lock") {
        doc["cmd_t"] = "orion/lock/set";
        doc["stat_t"] = "orion/lock/state";
        doc["pl_lock"] = "LOCK"; doc["pl_unlk"] = "UNLOCK";
      }
    } else {
      if (String(component) == "device_tracker") {
         doc["stat_t"] = "orion/gps/state";
         doc["json_attr_t"] = "orion/gps/state";
      } else {
         doc["stat_t"] = "orion/sensors/state";
         doc["val_tpl"] = "{{ value_json." + String(unique_id) + " }}";
         if (device_class[0] != '\0') doc["dev_cla"] = device_class;
         if (String(device_class) == "temperature") doc["unit_of_meas"] = "°C";
         if (String(device_class) == "humidity") doc["unit_of_meas"] = "%";
         if (String(device_class) == "illuminance") doc["unit_of_meas"] = "%";
      }
    }
    char buffer[600];
    serializeJson(doc, buffer);
    client.publish(topic_config.c_str(), buffer, true);
}

void publishDiscovery() {
  Serial.println("Enviando Auto-Discovery...");
  sendDiscovery("light", "Luz Cocina",  "relay1", "", false, 1);
  sendDiscovery("light", "Luz Sala",    "relay2", "", false, 2);
  sendDiscovery("light", "Luz Pasillo", "relay3", "", false, 3);
  sendDiscovery("light", "Luz Cuarto",  "relay4", "", false, 4);
  sendDiscovery("lock", "Cerradura Principal", "lock_main", "", false);
  sendDiscovery("sensor", "Temperatura", "temperature", "temperature", true);
  sendDiscovery("sensor", "Humedad", "humidity", "humidity", true);
  sendDiscovery("sensor", "Luminosidad", "illuminance", "illuminance", true);
  sendDiscovery("device_tracker", "Orion GPS", "gps_tracker", "", true);
}

void reconnect() {
  if (!client.connected()) {
    Serial.print("Reconectando MQTT...");
    String clientId = "ESP32Orion-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("Conectado");
      client.subscribe("orion/relay1/set");
      client.subscribe("orion/relay2/set");
      client.subscribe("orion/relay3/set");
      client.subscribe("orion/relay4/set");
      client.subscribe("orion/lock/set");
      publishDiscovery();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}