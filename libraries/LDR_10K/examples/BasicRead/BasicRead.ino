#include <LDR_10K.h>

// Usa el pin ADC que prefieras del ESP32 (ej.: 34 es ADC1_CH6)
#define LDR_PIN 4

LDR_10K ldr(LDR_PIN); // por defecto calibra 300–4095

void setup() {
  Serial.begin(115200);
  ldr.begin(12);          // 12 bits (0–4095) y 11 dB en ESP32
  ldr.setSmoothing(8);    // promedio simple de 8 muestras (opcional)
  // ldr.setCalibration(280, 3900); // si deseas recalibrar el rango
}

void loop() {
  int raw = ldr.getAnalogLDR();
  int pct = ldr.getPercentageLDR();

  Serial.print("RAW=");
  Serial.print(raw);
  Serial.print("  LUX%=");
  Serial.println(pct);

  delay(100);
}
