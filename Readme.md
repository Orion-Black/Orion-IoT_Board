# Orion IoT Board Firmware 🌌

![ESP32](https://img.shields.io/badge/Device-ESP32-blue)
![License](https://img.shields.io/badge/License-MIT-green)

**Orion IoT** es un sistema integral de monitoreo y control basado en **ESP32**. Este firmware gestiona una placa de desarrollo personalizada capaz de operar en múltiples modos (Local, Cloud, Test y Configuración), integrando una interfaz física OLED, control por servidor local, y conectividad avanzada con Home Assistant e InfluxDB.

<p align="center">
  <img src="Orion_PCB_View.png" alt="PCB Orion IoT" style="width: 48%;"/>
</p>

## 🚀 Características Principales

### 1. Interfaz de Usuario Física (HMI)
- **OLED SSD1306:** Visualización de menús, estado del sistema y escaneo de redes.
- **Navegación Analógica:** Selección de menús mediante Potenciómetro.
- **Botones de Control:** Sistema de 3 botones (Confirmar, Borrar/Atrás, Enviar).
- **Teclado en Pantalla:** Ingreso de contraseñas WiFi directamente desde el dispositivo.

### 2. Modos de Operación
- **🏠 Modo Local:** Servidor Web interno con **WebSerial**. Permite enviar comandos de texto para controlar relés y leer sensores sin internet vía `orion-iot.local/webserial` dentro de la misma red. 
- **☁️ Modo Cloud (Azure IoT):**
  - **Home Assistant:** Integración nativa vía **MQTT Discovery**. Los dispositivos aparecen automáticamente sin configuración YAML. (Puerto 8123)
  - **InfluxDB & Grafana:** Envío directo de telemetría a base de datos de series temporales para historicos y permite la creación de visualizaciones en dashboard a traves de grafana. (Puerto 8086 y 3000 respectivamente)
  - **Node-Red:** Permite crear rutinas de automatizaciones inteligentes. (Puerto 1880)
- **🛠️ Modo Test:** Suite de diagnóstico integrada para verificar relés, servos, GPS y sensores antes del despliegue con pruebas automaticas. 

El modo cloud esta alojado en `http://orion-iot.canadacentral.cloudapp.azure.com:<PUERTO>`

## 🛠️ Hardware y Pinout

El sistema corre sobre un ESP32 (DevKit V1) con la siguiente distribución de pines:

| Componente | Función | Pin ESP32 | Notas |
| :--- | :--- | :--- | :--- |
| **OLED Display** | Pantalla | GPIO 21 (SDA), 22 (SCL) | I2C |
| **Relés (x4)** | Control Cargas | GPIO 26, 27, 14, 12 | Activos en HIGH |
| **Cerradura** | Solenoide | GPIO 13 | Salida digital |
| **Servos (x3)** | Movimiento | GPIO 15, 2, 4 | PWM |
| **DHT11** | Temp/Humedad | GPIO 23 | Sensor Digital |
| **LDR** | Luz | GPIO 34 | Entrada Analógica |
| **GPS** | Geolocalización | GPIO 16 (RX), 17 (TX) | UART2 |
| **Potenciómetro** | Navegación UI | GPIO 35 | Entrada Analógica |
| **Botones UI** | Control | 32 (OK), 33 (DEL), 25 (SEND)| Input Pullup |
| **Extensiones I2C** | Añadir perifericos | GPIO 21 (SDA), 22 (SCL) | I2C |
| **Debug Serial** | Debug UART | 1 (TX0), 3 (RX0)| UART |

## 📦 Librerías Requeridas

Este proyecto utiliza las siguientes dependencias. Asegúrate de instalarlas en el Gestor de Librerías de Arduino:

- `Adafruit GFX Library` & `Adafruit SSD1306`
- `ESPAsyncWebServer` & `AsyncTCP`
- `WebSerial`
- `PubSubClient` (MQTT)
- `ESP8266 Influxdb` (Funciona para ESP32 - InfluxDB Client)
- `ArduinoJson`
- `TinyGPSPlus`
- `DHT sensor library`
- `ESP32Servo`

## ⚙️ Configuración

Antes de compilar, es necesario configurar las credenciales y puntos de conexión.

### 1. WiFi
Editar `wifi_defaults.h` para establecer la red de respaldo:
```cpp
#define DEFAULT_SSID "Tu_Red_WiFi"
#define DEFAULT_PASS "Tu_Password"
```

## 2. Cloud & MQTT

Editar `cloud_mode.cpp` con los datos de tu servidor (Azure VM, Raspberry Pi, etc.):

```cpp
// MQTT (Home Assistant)
const char* mqtt_server = "TU_IP_PUBLICA";
const char* mqtt_user   = "tu_usuario";
const char* mqtt_pass   = "tu_password";

// InfluxDB
#define INFLUXDB_URL    "http://TU_DOMINIO:8086"
#define INFLUXDB_TOKEN  "tu-token-admin"
#define INFLUXDB_ORG    "tu-org"
#define INFLUXDB_BUCKET "sensores"
```
---
## 📖 Manual de Uso
### Navegación
- Gira el **Potenciómetro** para mover el cursor `-->`.
- Presiona **Confirmar (GPIO 32)** para entrar o seleccionar.
- Presiona **Borrar (GPIO 33)** para volver atrás o salir de un modo.
---
### Comandos Modo Local (WebSerial)
Accede a:
```
http://orion-iot.local/webserial
```
Y usa comandos como:
```
help                  # Muestra una lista de comandos disponibles
relay set 1 on        # Encender Relé 1
lock open             # Abrir cerradura por 3 segundos
sensor all            # Leer todos los sensores
sys info              # Ver estado del sistema
sys reset             # Reinicia el dispositivo IoT
```

---

## 📊 Integración Cloud

El dispositivo publica en los siguientes tópicos MQTT para Home Assistant:

- **Configuración:**  
  `homeassistant/+/orion/+/config`

- **Estado:**  
  `orion/sensors/state`  
  `orion/gps/state`

- **Comandos:**  
  `orion/relay1/set`  
  `orion/lock/set`

En **InfluxDB**, busca el measurement:
```
estado_sistema
_field
```
para visualizar los datos históricos.
---

## 📄 Licencia

Este proyecto está bajo la **Licencia MIT** – ver el archivo `LICENSE` para más detalles.

Desarrollado por **Jesús Gonzalez Becerril** – Proyecto **Orion IoT**

