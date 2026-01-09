#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//Librerias propias y encabezados personalizados
#include "icons.h"
#include "local_server.h"
#include "test_mode.h"
#include "wifi_defaults.h"
#include "definitions.h"
#include "cloud_mode.h"

/* =======================
   DISPLAY
   ======================= */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* =======================
   PINES
   ======================= */
#define POT_PIN 35

#define BTN_CONFIRM 32
#define BTN_DELETE 33
#define BTN_SEND 25

/* =======================
   MENÚ PRINCIPAL
   ======================= */
const char* menuItems[] = {
  "Modo Local",
  "Modo Cloud",
  "Modo Test",
  "Configuracion"
};
const uint8_t MENU_SIZE = 4;

/* =======================
   MENÚ CONFIGURACIÓN
   ======================= */
const char* configMenuItems[] = {
  "Estado WiFi",
  "IP",
  "Seleccionar WiFi",
  "Volver Atras"
};
const uint8_t CONFIG_MENU_SIZE = 4;

/* =======================
   WIFI
   ======================= */
#define MAX_NETWORKS 10
String ssidList[MAX_NETWORKS];
int wifiCount = 0;

/* =======================
   PASSWORD
   ======================= */
const char charset[] = "aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ0123456789.*-@=_#$%&¿?";
#define CHARSET_SIZE (sizeof(charset) - 1)
#define MAX_PASS_LEN 16

char wifiPassword[MAX_PASS_LEN + 1];
uint8_t passLen = 0;
uint8_t charIndex = 0;

UIState uiState = UI_MAIN_MENU;

/* =======================
   CONTROL
   ======================= */
int menuDelta = 0;

volatile bool confirmPressed = false;
volatile bool deletePressed = false;
volatile bool sendPressed = false;

volatile unsigned long lastISRTime = 0;

int currentIndex = 0;
int configIndex = 0;
int wifiIndex = 0;

bool redrawMenu = true;

/* =======================
   ISR BOTONES
   ======================= */
void IRAM_ATTR isrConfirm() {
  if (millis() - lastISRTime > 200) {
    confirmPressed = true;
    lastISRTime = millis();
  }
}

void IRAM_ATTR isrDelete() {
  if (millis() - lastISRTime > 200) {
    deletePressed = true;
    lastISRTime = millis();
  }
}

void IRAM_ATTR isrSend() {
  if (millis() - lastISRTime > 200) {
    sendPressed = true;
    lastISRTime = millis();
  }
}

bool conectarWiFi(const char* ssid, const char* pass, uint32_t timeoutMs = 10000) {

  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  uint32_t t0 = millis();
  while (!WiFi.isConnected() && (millis() - t0 < timeoutMs)) {
    delay(100);
  }

  if (WiFi.isConnected()) {
    Serial.println("WiFi conectado");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("Fallo conexion WiFi");
  WiFi.disconnect(true);
  return false;
}

/* =======================
   SETUP
   ======================= */
void setup() {
  Serial.begin(115200);

  pinMode(POT_PIN, INPUT);
  pinMode(BTN_CONFIRM, INPUT_PULLUP);
  pinMode(BTN_DELETE, INPUT_PULLUP);
  pinMode(BTN_SEND, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BTN_CONFIRM), isrConfirm, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN_DELETE), isrDelete, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN_SEND), isrSend, FALLING);

  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("Error al iniciar SSD1306"));
    for (;;)
      ;
  }

  display.clearDisplay();
  dibujarImagenOLED();
  delay(1000);
  dibujarInicio();


  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("Iniciando WiFi...");
  display.display();

  bool wifiOK = conectarWiFi(DEFAULT_SSID, DEFAULT_PASS);

  display.clearDisplay();
  display.setCursor(0, 0);

  if (wifiOK) {
    display.println("      Conectado OK");
    display.println();
    display.println("     Usando red Wifi");
    display.println("       por defecto");
  } else {
    display.println("WiFi no disponible");
    display.println("Elija una red WiFi");
  }

  display.display();
  delay(1500);


  redrawMenu = true;
}

/* =======================
   LOOP
   ======================= */
void loop() {

  switch (uiState) {

    case UI_MAIN_MENU:
      manejarMenuPrincipal();
      break;

    case UI_CONFIG_MENU:
      manejarMenuConfiguracion();
      break;

    case UI_WIFI_SCAN:
      manejarWifiScan();
      break;

    case UI_WIFI_PASSWORD:
      manejarWifiPassword();
      break;

    case UI_LOCAL_MODE:
      manejarModoLocal();
      break;

    case UI_CLOUD_MODE:
      loopModoCloud();
      // Lógica para salir (botón delete)
      if (deletePressed) {
        deletePressed = false;
        uiState = UI_MAIN_MENU;
        redrawMenu = true;
      }
      break;

    case UI_TEST_MODE:
      // Toda la lógica se delega al archivo test_mode.cpp
      loopModoTest();
      break;
  }
}

/* =======================
   LECTURA POT
   ======================= */

int readPotFiltered() {
  static int filtered = 0;
  int raw = analogRead(POT_PIN);

  // Filtro EMA: 8
  filtered = (filtered * 7 + raw) / 8;
  return filtered;
}

int readIndexStable(uint8_t items) {
  static int lastIndex = -1;

  int value = readPotFiltered();
  int stepSize = 4096 / items;
  int index = value / stepSize;

  index = constrain(index, 0, items - 1);

  if (index != lastIndex) {
    lastIndex = index;
    return index;
  }
  return -1;
}

int readCharIndex() {
  static int last = -1;

  int value = readPotFiltered();
  int step = 4096 / CHARSET_SIZE;
  int idx = value / step;

  idx = constrain(idx, 0, CHARSET_SIZE - 1);

  if (idx != last) {
    last = idx;
    return idx;
  }
  return -1;
}


/* =======================
   MENÚ PRINCIPAL
   ======================= */
void manejarMenuPrincipal() {
  int idx = readIndexStable(MENU_SIZE);
  if (idx >= 0) {
    currentIndex = idx;
    redrawMenu = true;
  }

  if (redrawMenu) {
    drawMenu();
    redrawMenu = false;
  }

  if (confirmPressed) {
    confirmPressed = false;

    if (currentIndex == 0) {  // Modo Local
      if (!WiFi.isConnected()) {
        display.clearDisplay();
        display.println("ERROR:");
        display.println("No hay WiFi");
        display.println("Conectese primero");
        display.display();
        delay(1500);
      } else {
        if (iniciarServidorLocal()) {
          uiState = UI_LOCAL_MODE;
          redrawMenu = true;
        }
      }
    } else if (currentIndex == 1) {  //"Modo Cloud" es el índice 1
      if (!WiFi.isConnected()) {
        // Mostrar error WiFi
      } else {
        iniciarModoCloud(&display);
        uiState = UI_CLOUD_MODE;
        redrawMenu = true;
      }
    } else if (currentIndex == 2) { //"Modo Test" es indice 2

      iniciarModoTest(&display);
      uiState = UI_TEST_MODE;
      redrawMenu = true;

    } else if (currentIndex == 3) { //Modo de configuración

      uiState = UI_CONFIG_MENU;
      redrawMenu = true;
    }
  }
}

void drawMenu() {
  display.clearDisplay();
  for (uint8_t i = 0; i < MENU_SIZE; i++) {
    display.setCursor(0, i * 12);
    display.print(i == currentIndex ? "--> " : "    ");
    display.println(menuItems[i]);
  }
  display.drawFastHLine(0, 50, 128, SSD1306_WHITE);
  display.setCursor(0, 54);
  display.println("   == ORION IOT ==");
  display.display();
}
// -------------------------------
//      Pantalla Modo Local
// -------------------------------
void manejarModoLocal() {

  if (redrawMenu) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.println("Modo Local");
    display.println();
    display.println("Servidor activo en:");
    display.println("orion-iot.local");
    display.println("/webserial");
    display.println();
    display.println("Borrar para salir");
    display.display();
    redrawMenu = false;
  }

  loopServidorLocal();

  if (deletePressed) {
    deletePressed = false;
    detenerServidorLocal();
    uiState = UI_MAIN_MENU;
    redrawMenu = true;
  }
}
// -------------------------------


/* =======================
   CONFIGURACIÓN
   ======================= */
void manejarMenuConfiguracion() {
  int idx = readIndexStable(CONFIG_MENU_SIZE);
  if (idx >= 0) {
    configIndex = idx;
    redrawMenu = true;
  }

  if (redrawMenu) {
    drawConfigMenu();
    redrawMenu = false;
  }

  if (confirmPressed) {
    confirmPressed = false;

    if (configIndex == 2) {
      uiState = UI_WIFI_SCAN;
      scanWifiNetworks();
      redrawMenu = true;
    } else if (configIndex == 3) {
      uiState = UI_MAIN_MENU;
      redrawMenu = true;
    }
  }
}

void drawConfigMenu() {
  display.clearDisplay();
  bool wifiIsConnected = WiFi.isConnected();
  for (uint8_t i = 0; i < CONFIG_MENU_SIZE; i++) {
    bool wifiIsConnected = WiFi.isConnected();
    display.setCursor(0, i * 12);
    display.print(i == configIndex ? "--> " : "    ");
    if (i == 0) {
      display.print("Estado WiFi: ");
      display.println(wifiIsConnected ? "OK" : "NO");
    } else if (i == 1) {
      display.print("IP: ");
      display.println(wifiIsConnected ? WiFi.localIP() : "-");
    } else {
      display.println(configMenuItems[i]);
    }
  }
  display.display();
}

/* =======================
   WIFI SCAN
   ======================= */
void scanWifiNetworks() {
  display.clearDisplay();
  display.println("Escaneando WiFi...");
  display.display();

  wifiCount = WiFi.scanNetworks();
  wifiCount = min(wifiCount, MAX_NETWORKS);

  for (int i = 0; i < wifiCount; i++) {
    ssidList[i] = WiFi.SSID(i);
  }
  wifiIndex = 0;
}

void manejarWifiScan() {
  int idx = readIndexStable(wifiCount);
  if (idx >= 0) {
    wifiIndex = idx;
    redrawMenu = true;
  }

  if (redrawMenu) {
    drawWifiList();
    redrawMenu = false;
  }

  if (confirmPressed) {
    confirmPressed = false;
    passLen = 0;
    charIndex = 0;
    uiState = UI_WIFI_PASSWORD;
    redrawMenu = true;
  }
}

void drawWifiList() {
  display.clearDisplay();
  for (int i = 0; i < wifiCount; i++) {
    display.setCursor(0, i * 12);
    display.print(i == wifiIndex ? "--> " : "    ");
    display.println(ssidList[i]);
  }
  display.display();
}

/* =======================
   PASSWORD
   ======================= */
void manejarWifiPassword() {

  int idx = readCharIndex();
  if (idx >= 0) {
    charIndex = idx;
    redrawMenu = true;
  }

  if (confirmPressed) {
    confirmPressed = false;
    if (passLen < MAX_PASS_LEN) {
      wifiPassword[passLen++] = charset[charIndex];
      wifiPassword[passLen] = '\0';
    }
    redrawMenu = true;
  }

  if (deletePressed) {
    deletePressed = false;
    if (passLen > 0) {
      passLen--;
      wifiPassword[passLen] = '\0';
    }
    redrawMenu = true;
  }

  if (sendPressed) {
    sendPressed = false;
    conectarWifi();
    uiState = UI_CONFIG_MENU;
    redrawMenu = true;
  }

  if (redrawMenu) {
    drawPasswordScreen();
    redrawMenu = false;
  }
}

void drawPasswordScreen() {

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("SSID: ");
  display.print(ssidList[wifiIndex]);
  display.println();

  display.setCursor(0, 16);
  display.print("PASS: ");
  display.print(wifiPassword);

  display.setCursor(0, 36);
  display.print("CHAR: ");
  display.print(charset[charIndex]);

  display.display();
}

void conectarWifi() {
  display.clearDisplay();
  display.setCursor(20, 30);
  display.println("Conectando...");
  display.display();

  conectarWiFi(
    ssidList[wifiIndex].c_str(),
    wifiPassword);
  display.clearDisplay();
  uiState = UI_CONFIG_MENU;
  redrawMenu = true;
}

/* =======================
   SPLASH
   ======================= */
void dibujarImagenOLED() {
  display.drawBitmap(4, 2, init_icon, 120, 60, SSD1306_WHITE);
  display.display();
}

void dibujarInicio() {
  display.clearDisplay();

  display.setTextSize(4);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(6, 0);
  display.println("ORION");

  display.setTextSize(3);
  display.setCursor(40, 30);
  display.println("IoT");

  display.setTextSize(1);
  display.setCursor(55, 55);
  display.println("V1.0");
  display.display();
  delay(2000);
}
