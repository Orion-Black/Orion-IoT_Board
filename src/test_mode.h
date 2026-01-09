#ifndef TEST_MODE_H
#define TEST_MODE_H

#include <Adafruit_SSD1306.h>

// Configura los pines y prepara el entorno de pruebas
void iniciarModoTest(Adafruit_SSD1306* displayPtr);

// Bucle principal del menú de pruebas (Maneja la lógica y el dibujo)
void loopModoTest();

#endif