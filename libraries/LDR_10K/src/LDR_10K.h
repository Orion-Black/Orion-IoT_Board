#pragma once
#include <Arduino.h>

class LDR_10K {
public:
  // minRaw/maxRaw por defecto: 300–4095 → 0–100%
  explicit LDR_10K(uint8_t adcPin, int minRaw = 300, int maxRaw = 4095);

  // resolution: 12 bits por defecto. En ESP32 fija atenuación a 11 dB.
  void begin(uint8_t resolutionBits = 12);

  // Lectura cruda del ADC (promediada si smoothingSamples > 1)
  int getAnalogLDR();

  // Porcentaje 0–100 mapeado y saturado
  int getPercentageLDR();

  // Ajusta el rango de calibración para el mapeo a porcentaje
  void setCalibration(int minRaw, int maxRaw);

  // Ajusta promediado simple (1 = sin suavizado)
  void setSmoothing(uint8_t samples);

private:
  uint8_t _pin;
  int _minRaw;
  int _maxRaw;
  uint8_t _smoothingSamples;

  int readAveraged();
  static float mapToUnitInterval(int value, int inMin, int inMax);
};
