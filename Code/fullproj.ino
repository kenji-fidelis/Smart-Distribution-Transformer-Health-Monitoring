#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/* ================= WiFi ================= */
#define WIFI_SSID // "name"
#define WIFI_PASSWORD // "password"

/* ================= Firebase REST ================= */
// NO https:// and NO trailing slash
#define FIREBASE_HOST "transformer-health-monit-750d0-default-rtdb.firebaseio.com"
#define FIREBASE_PATH "/transformer1.json"

/* ================= Pin Definitions ================= */
#define DHTPIN 27
#define DHTTYPE DHT22

#define ONE_WIRE_BUS 4     // DS18B20 DATA (S)
#define POT_PIN 34         // ADC1 pin (WiFi-safe)

/* ================= Sensor Objects ================= */
DHT dht(DHTPIN, DHTTYPE);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

/* ================= Helper Functions ================= */

// Load Factor from potentiometer (0.0 – 1.4)
float readLoadFactor() {
  int raw = analogRead(POT_PIN);          // 0–4095
  float lf = map(raw, 0, 4095, 0, 140) / 100.0f;
  return constrain(lf, 0.0f, 1.4f);
}

// Ambient temperature from DHT22
float readAmbientTemp() {
  float t = dht.readTemperature();
  if (isnan(t)) {
    Serial.println("DHT22 read failed");
    return 30.0;  // fallback
  }
  return t;
}

// Oil temperature from DS18B20
float readOilTemp() {
  ds18b20.requestTemperatures();
  float t = ds18b20.getTempCByIndex(0);

  if (t == DEVICE_DISCONNECTED_C) {
    Serial.println("DS18B20 not found");
    return 60.0;  // fallback
  }
  return t;
}

/* ================= Transformer Metrics ================= */

float calculateTSI(float oil, float ambient) {
  return oil - ambient;
}

float calculateHealthScore(float lf, float tsi) {
  float lf_pen  = (lf > 1.0f) ? min(lf - 1.0f, 1.0f) : 0.0f;
  float tsi_pen = (tsi > 40.0f) ? min((tsi - 40.0f) / 10.0f, 1.0f) : 0.0f;

  float score = 100.0f - (40.0f * lf_pen + 40.0f * tsi_pen);
  return max(score, 0.0f);
}

String detectFault(float lf, float tsi) {
  if (tsi > 60.0f) return "Overheating";
  if (lf > 1.0f)   return "Overload";
  return "Normal";
}


// new healthscore change
float healthScore = 100.0;   // starts healthy

float calculateStress(float lf, float tsi) {
  float loadStress = max(0.0, lf - 0.9);      // stress after 90%
  float tsiStress  = max(0.0, tsi - 40.0);    // stress after warning
  return (loadStress * 0.6) + (tsiStress * 0.4);
}

void updateHealthScore(float lf, float tsi) {
  float stress = calculateStress(lf, tsi);

  // damage rate increases non-linearly
  float damageRate = stress * stress * 0.5;

  // degrade health
  healthScore -= damageRate;

  // clamp
  if (healthScore < 0) healthScore = 0;
  if (healthScore > 100) healthScore = 100;
}

void applyRecovery(float lf, float tsi) {
  if (lf < 0.8 && tsi < 35) {
    healthScore += 0.05;   // VERY slow recovery
    if (healthScore > 100) healthScore = 100;
  }
}

/* ================= Setup ================= */

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  dht.begin();
  ds18b20.begin();
}

/* ================= Main Loop ================= */

void loop() {
  if (WiFi.status() == WL_CONNECTED) {

    float loadFactor = readLoadFactor();
    float ambient    = readAmbientTemp();
    float oilTemp    = readOilTemp();

    float tsi    = calculateTSI(oilTemp, ambient);
    float health = calculateHealthScore(loadFactor, tsi);
    String fault = detectFault(loadFactor, tsi);

    // JSON payload
    String json = "{";
    json += "\"LoadFactor\":"   + String(loadFactor, 2) + ",";
    json += "\"AmbientTemp\":"  + String(ambient, 1) + ",";
    json += "\"OilTemp\":"      + String(oilTemp, 1) + ",";
    json += "\"TSI\":"          + String(tsi, 1) + ",";
    json += "\"HealthScore\":"  + String(health, 1) + ",";
    json += "\"Fault\":\""      + fault + "\"";
    json += "}";

    HTTPClient http;
    String url = "https://" + String(FIREBASE_HOST) + FIREBASE_PATH;

    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.PUT(json);

    if (httpCode > 0) {
      Serial.print("Firebase OK\n");
    } else {
      Serial.print("Firebase FAILED: ");
      Serial.println(http.errorToString(httpCode));
    }

    http.end();
  }

  delay(2000);   // update every 2 seconds
}
