#include "LDR_10K.h"

#ifdef ARDUINO_ARCH_ESP32
  #include "esp32-hal-adc.h" // analogSetPinAttenuation
#endif

LDR_10K::LDR_10K(uint8_t adcPin, int minRaw, int maxRaw)
: _pin(adcPin), _minRaw(minRaw), _maxRaw(maxRaw), _smoothingSamples(1) {}

void LDR_10K::begin(uint8_t resolutionBits) {
  pinMode(_pin, INPUT);
  analogReadResolution(resolutionBits);
#ifdef ARDUINO_ARCH_ESP32
  // 11 dB ~ rango hasta ~3.3V (útil para la mayoría de divisores LDR)
  analogSetPinAttenuation(_pin, ADC_11db);
#endif
}

int LDR_10K::readAveraged() {
  if (_smoothingSamples <= 1) {
    return analogRead(_pin);
  }
  uint32_t acc = 0;
  for (uint8_t i = 0; i < _smoothingSamples; ++i) {
    acc += analogRead(_pin);
  }
  return (int)(acc / _smoothingSamples);
}

int LDR_10K::getAnalogLDR() {
  return readAveraged();
}

float LDR_10K::mapToUnitInterval(int value, int inMin, int inMax) {
  if (inMax == inMin) return 0.0f;
  float x = (float)(value - inMin) / (float)(inMax - inMin);
  if (x < 0.0f) x = 0.0f;
  if (x > 1.0f) x = 1.0f;
  return x;
}

int LDR_10K::getPercentageLDR() {
  int raw = getAnalogLDR();
  float u = mapToUnitInterval(raw, _minRaw, _maxRaw);
  int pct = (int)roundf(u * 100.0f);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

void LDR_10K::setCalibration(int minRaw, int maxRaw) {
  _minRaw = minRaw;
  _maxRaw = maxRaw;
  if (_minRaw > _maxRaw) {
    int t = _minRaw; _minRaw = _maxRaw; _maxRaw = t;
  }
}

void LDR_10K::setSmoothing(uint8_t samples) {
  if (samples == 0) samples = 1;
  _smoothingSamples = samples;
}
