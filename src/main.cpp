#include <Wire.h>
#include <BH1750.h>
#include <DHT.h>
#include <NimBLEDevice.h>
#include <esp_sleep.h>
#include <math.h>

// 1 = serielle Debugausgaben aktiv, 0 = aus (spart Flash)
#define DEBUG_SERIAL 0

// ---- Pins (XIAO ESP32-C3) ----
static const int PIN_SDA = 6;
static const int PIN_SCL = 7;
static const int PIN_BATT_ADC = 4;
static const int PIN_DHT = 5;

// ---- Spannungsteiler + Kalibrierung ----
static const float DIVIDER_FACTOR = 2.0f;   // 100k / 100k
static const float BATT_CAL = 1.0f;

// ---- Fixed sleep time ----
static const uint32_t SLEEP_FIXED_S = 120;

// ---- BLE ----
static const uint32_t ADV_BURST_MS = 1000;
static const uint16_t ADV_MIN_INTERVAL = 32;
static const uint16_t ADV_MAX_INTERVAL = 64;
static const esp_power_level_t BLE_TX_POWER = ESP_PWR_LVL_N0;

// ---- RTC ----
RTC_DATA_ATTR uint8_t packet_id = 0;

BH1750 lightMeter;
DHT dht(PIN_DHT, DHT22);

// ---- Battery struct ----
struct BattRead {
  float v_batt;
};

// ---- DHT struct ----
struct DhtRead {
  float temperature_c;
  float humidity_pct;
};

// ---- Akku messen ----
static BattRead readBatteryVoltage() {
  pinMode(PIN_BATT_ADC, INPUT);

#if defined(ARDUINO_ARCH_ESP32)
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BATT_ADC, ADC_11db);
#endif

  (void)analogReadMilliVolts(PIN_BATT_ADC);
  delay(5);

  const int N = 16;
  uint32_t sum_mv = 0;
  for (int i = 0; i < N; i++) {
    sum_mv += analogReadMilliVolts(PIN_BATT_ADC);
    delay(2);
  }

  float mv_adc = sum_mv / (float)N;
  float v_adc = mv_adc / 1000.0f;
  float v_batt = (v_adc * DIVIDER_FACTOR) * BATT_CAL;

  (void)mv_adc;
  return { v_batt };
}

// ---- DHT22 messen ----
static DhtRead readDht22() {
  float t = NAN;
  float h = NAN;

  for (int i = 0; i < 3; i++) {
    h = dht.readHumidity();
    t = dht.readTemperature();
    if (isfinite(t) && isfinite(h)) {
      break;
    }
    delay(50);
  }

  if (!isfinite(t)) t = 0.0f;
  if (!isfinite(h)) h = 0.0f;

  return { t, h };
}

// ---- BH1750 PowerDown ----
static void bh1750PowerDown() {
  Wire.beginTransmission(0x23);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) {
    Wire.beginTransmission(0x5C);
    Wire.write(0x00);
    Wire.endTransmission();
  }
}

// ---- BTHome v2 payload ----
static std::string buildBTHome(
  uint8_t pid,
  float lux,
  float battV,
  float tempC,
  float humidityPct
) {
  // BTHome v2 (unencrypted) payload layout:
  // 0x40 info, 0x00 packet id, pid
  // 0x05 illuminance (lux * 100, 3 bytes)
  // 0x0C battery voltage (mV, 2 bytes)
  // 0x02 temperature (degC * 100, signed 2 bytes)
  // 0x03 humidity (%RH * 100, unsigned 2 bytes)
  uint32_t lux100 = (lux <= 0) ? 0 : (uint32_t)(lux * 100.0f + 0.5f);
  if (lux100 > 0xFFFFFF) lux100 = 0xFFFFFF;

  uint32_t mv = (battV <= 0) ? 0 : (uint32_t)(battV * 1000.0f + 0.5f);
  if (mv > 0xFFFF) mv = 0xFFFF;

  int32_t temp100 = (int32_t)(tempC * 100.0f + (tempC >= 0 ? 0.5f : -0.5f));
  if (temp100 > 32767) temp100 = 32767;
  if (temp100 < -32768) temp100 = -32768;

  uint32_t hum100 = (humidityPct <= 0) ? 0 : (uint32_t)(humidityPct * 100.0f + 0.5f);
  if (hum100 > 10000) hum100 = 10000;

  uint8_t p[17];
  int i = 0;

  p[i++] = 0x40;        // info: no encryption
  p[i++] = 0x00;        // packet id
  p[i++] = pid;

  // Illuminance
  p[i++] = 0x05;
  p[i++] = lux100 & 0xFF;
  p[i++] = (lux100 >> 8) & 0xFF;
  p[i++] = (lux100 >> 16) & 0xFF;

  // Battery voltage
  p[i++] = 0x0C;
  p[i++] = mv & 0xFF;
  p[i++] = (mv >> 8) & 0xFF;

  // Temperature
  p[i++] = 0x02;
  p[i++] = (uint8_t)(temp100 & 0xFF);
  p[i++] = (uint8_t)((temp100 >> 8) & 0xFF);

  // Humidity
  p[i++] = 0x03;
  p[i++] = hum100 & 0xFF;
  p[i++] = (hum100 >> 8) & 0xFF;

  return std::string((char*)p, i);
}

// ---- BLE advertise ----
static void advertiseOnce(float lux, float battV, float tempC, float humidityPct) {
  packet_id++;

  NimBLEDevice::init("");
  NimBLEDevice::setPower(BLE_TX_POWER);

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();

  NimBLEAdvertisementData ad;
  ad.setFlags(0x06);
  ad.setServiceData(
    NimBLEUUID((uint16_t)0xFCD2),
    buildBTHome(packet_id, lux, battV, tempC, humidityPct)
  );

  adv->setAdvertisementData(ad);
  adv->setMinInterval(ADV_MIN_INTERVAL);
  adv->setMaxInterval(ADV_MAX_INTERVAL);

  adv->start();
  delay(ADV_BURST_MS);
  adv->stop();

  NimBLEDevice::deinit(true);
}

// ---- SETUP ----
void setup() {
#if DEBUG_SERIAL
  Serial.begin(115200);
  delay(50);
#endif

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setTimeOut(50);

  dht.begin();

  bool bh_ok = lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE);
  lightMeter.configure(BH1750::ONE_TIME_HIGH_RES_MODE);
  delay(180);

  float lux = lightMeter.readLightLevel();
  if (!bh_ok || !isfinite(lux) || lux < 0) lux = 0;

  bh1750PowerDown();

  BattRead b = readBatteryVoltage();
  DhtRead d = readDht22();

#if DEBUG_SERIAL
  Serial.print("BH1750 ok: ");
  Serial.println(bh_ok ? "yes" : "no");
  Serial.print("Lux: ");
  Serial.println(lux, 2);
  Serial.print("Battery V: ");
  Serial.println(b.v_batt, 3);
  Serial.print("Temp C: ");
  Serial.println(d.temperature_c, 2);
  Serial.print("Humidity %: ");
  Serial.println(d.humidity_pct, 2);
#endif

  advertiseOnce(lux, b.v_batt, d.temperature_c, d.humidity_pct);

#if DEBUG_SERIAL
  Serial.print("Sleep (s): ");
  Serial.println(SLEEP_FIXED_S);
#endif

  esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_FIXED_S * 1000000ULL);
  esp_deep_sleep_start();
}

void loop() {}
