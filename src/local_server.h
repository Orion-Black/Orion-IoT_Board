#ifndef LOCAL_SERVER_H
#define LOCAL_SERVER_H

#include <WiFi.h>

// Inicializa todo el sistema local (MDNS + WebSerial + HW)
bool iniciarServidorLocal();

// Apaga el servidor local (limpio)
void detenerServidorLocal();

// Loop de mantenimiento (GPS, tareas no bloqueantes)
void loopServidorLocal();

#endif
