#include <WiFi.h>
#include "ThingSpeak.h"

// --- 1. Konfigurasi WiFi ---
const char* WIFI_NAME = "YOUR WIFI";       // Ganti dengan nama WiFi / Hotspot kamu
const char* WIFI_PASSWORD = "PASSWORDNYA"; // Ganti dengan password WiFi kamu

// --- 2. Konfigurasi ThingSpeak ---
WiFiClient client;
unsigned long myChannelNumber = 3566273;         // Channel ID ThingSpeak kamu
const char * myWriteAPIKey = "F3G2UJ5Q8SSP0I1D"; // Write API Key kamu

// --- 3. Konfigurasi Sensor ---
const int SENSOR_PIN = 34; // Pin AO terhubung ke GPIO 34 ESP32
const int ADC_DRY = 3200;   // Nilai RAW ADC saat udara kering/tanah kering
const int ADC_WET = 1300;   // Nilai RAW ADC saat dicelupkan ke air/tanah basah

unsigned long lastTime = 0;
unsigned long timerDelay = 30000; // Kirim data setiap 30 detik

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  ThingSpeak.begin(client);
  
  // Menghubungkan ke WiFi
  WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
}

void loop() {
  if ((millis() - lastTime) > timerDelay) {
    // 1. Baca nilai analog dari pin AO
    int rawValue = analogRead(SENSOR_PIN);
    
    // 2. Pemetaan nilai ADC ke persentase (0 - 100%)
    int moisturePercent = map(rawValue, ADC_DRY, ADC_WET, 0, 100);
    moisturePercent = constrain(moisturePercent, 0, 100);

    // 3. Logika penentuan kode status tanah (Angka untuk Widget / Grafik)
    int statusCode = 0;
    String statusTeks = "";

    if (moisturePercent <= 30) {
      statusCode = 1; // 1 = Kering
      statusTeks = "Kering";
    } else if (moisturePercent <= 70) {
      statusCode = 2; // 2 = Lembap
      statusTeks = "Lembap";
    } else {
      statusCode = 3; // 3 = Basah
      statusTeks = "Basah";
    }

    // 4. Tampilkan log pembacaan di Serial Monitor
    Serial.print("RAW ADC: ");
    Serial.print(rawValue);
    Serial.print(" | Kelembapan: ");
    Serial.print(moisturePercent);
    Serial.print("% | Status: ");
    Serial.print(statusTeks);
    Serial.print(" (Kode: ");
    Serial.print(statusCode);
    Serial.println(")");

    // 5. Masukkan data ke Field ThingSpeak
    ThingSpeak.setField(1, moisturePercent); // Field 1: Persentase (0-100%)
    ThingSpeak.setField(2, statusCode);      // Field 2: Kode Status (1, 2, atau 3)
    
    // 6. Kirim data sekaligus ke ThingSpeak Cloud
    int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    if (x == 200) {
      Serial.println("Berhasil dikirim ke ThingSpeak!");
    } else {
      Serial.println("Gagal mengirim data. Kode HTTP: " + String(x));
    }
    
    lastTime = millis();
  }
}