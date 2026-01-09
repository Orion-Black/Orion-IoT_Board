#ifndef CLOUD_MODE_H
#define CLOUD_MODE_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

// Inicializa la conexión MQTT y manda las configuraciones a Home Assistant
void iniciarModoCloud(Adafruit_SSD1306* displayPtr);

// Bucle principal: mantiene la conexión y publica datos de sensores
void loopModoCloud();

#endif