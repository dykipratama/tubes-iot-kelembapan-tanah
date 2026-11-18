#include <WiFi.h>
#include <MQTT.h>

// --- 1. Konfigurasi WiFi (Sesuai Bagian A) ---
const char* WIFI_NAME     = "WIFI 1";
const char* WIFI_PASSWORD = "123456789";

// --- 2. Konfigurasi Server Uji (Bagian B.1) ---
const char* HTTP_HOST     = "httpbin.org";
const char* HTTP_PATH     = "/post";
const char* MQTT_BROKER   = "broker.hivemq.com";
const int   MQTT_PORT     = 1883;
const char* MQTT_TOPIC    = "tubesiot/esp32_01/echo";
const char* MQTT_CLIENTID = "esp32_dik_2311101103";

// --- 3. Parameter Eksperimen (Bagian B.2 & B.3) ---
const int TOTAL_PESAN         = 100;
const unsigned long JEDA_MS   = 2000; // Interval 2 detik antar awal kirim
const unsigned long TIMEOUT_MS = 5000; // Batas timeout 5 detik

IPAddress ipHttp;
IPAddress ipMqtt;

// Buffer MQTT 1024 byte agar payload 500 byte (S4 & S6) aman tanpa terpotong
WiFiClient netMqtt;
MQTTClient mqttClient(1024);

volatile bool echoDiterima = false;
unsigned long waktuEchoMs  = 0;
String expectedPrefix      = "";

// Callback saat pesan echo diterima kembali dari broker MQTT
void onMqttMessage(String &topic, String &payload) {
  if (payload.startsWith(expectedPrefix)) {
    waktuEchoMs  = millis();
    echoDiterima = true;
  }
}

// Membuat payload tepat berukuran targetBytes (misal: "S1,0001,xxxxxxxxxxxx")
String buatPayload(const char* skenario, int seq, int targetBytes) {
  char header[16];
  snprintf(header, sizeof(header), "%s,%04d,", skenario, seq);
  String p = String(header);
  while ((int)p.length() < targetBytes) {
    p += 'x';
  }
  return p;
}

void pastikanWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.disconnect();
  WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
  }
}

// Menjalankan S1 (20B) dan S2 (500B) - HTTP/1.1 POST murni (Connection: close)
void jalankanHTTP(const char* skenario, int payloadBytes) {
  for (int seq = 1; seq <= TOTAL_PESAN; seq++) {
    pastikanWiFi();
    String payload = buatPayload(skenario, seq, payloadBytes);
    long rssi = WiFi.RSSI();

    WiFiClient client;
    client.setTimeout(TIMEOUT_MS);

    unsigned long t_kirim = millis();
    bool ok = false;
    unsigned long t_terima = 0;

    // Koneksi TCP baru per pesan (Connection: close) sesuai Bagian B.1 & B.3
    if (client.connect(ipHttp, 80)) {
      client.print(String("POST ") + HTTP_PATH + " HTTP/1.1\r\n" +
                   "Host: " + HTTP_HOST + "\r\n" +
                   "Content-Type: text/plain\r\n" +
                   "Content-Length: " + String(payload.length()) + "\r\n" +
                   "Connection: close\r\n\r\n" +
                   payload);

      // Tunggu header HTTP 200 OK
      String statusLine = client.readStringUntil('\n');
      if (statusLine.indexOf("200") > 0) {
        // Baca sampai koneksi ditutup server (Content-Length selesai) atau timeout
        while ((client.connected() || client.available()) && (millis() - t_kirim) <= TIMEOUT_MS) {
          while (client.available()) {
            client.read();
          }
          delay(1);
        }
        t_terima = millis();
        if ((t_terima - t_kirim) <= TIMEOUT_MS) {
          ok = true;
        }
      }
      client.stop();
    }

    if (ok) {
      unsigned long rtt = t_terima - t_kirim;
      Serial.printf("%s,%d,%d,%lu,%lu,%lu,OK,%ld\n",
                    skenario, seq, payloadBytes, t_kirim, t_terima, rtt, rssi);
    } else {
      Serial.printf("%s,%d,%d,%lu,,,LOST,%ld\n",
                    skenario, seq, payloadBytes, t_kirim, rssi);
    }

    unsigned long elapsed = millis() - t_kirim;
    if (elapsed < JEDA_MS) {
      delay(JEDA_MS - elapsed);
    }
  }
}

// Menjalankan S3-S6 - MQTT QoS 0 dan QoS 1 sesungguhnya
void jalankanMQTT(const char* skenario, int qos, int payloadBytes) {
  pastikanWiFi();
  mqttClient.disconnect();
  delay(300);

  // Clean session & keep-alive 30 detik (Bagian B.1)
  mqttClient.setOptions(30, true, TIMEOUT_MS);
  mqttClient.begin(ipMqtt, MQTT_PORT, netMqtt);
  mqttClient.onMessage(onMqttMessage);

  while (!mqttClient.connect(MQTT_CLIENTID)) {
    delay(500);
  }
  mqttClient.subscribe(MQTT_TOPIC, qos);

  // Proses SUBACK sebelum pesan pertama dikirim agar tidak masuk RTT
  for (int i = 0; i < 15; i++) {
    mqttClient.loop();
    delay(20);
  }

  for (int seq = 1; seq <= TOTAL_PESAN; seq++) {
    pastikanWiFi();
    if (!mqttClient.connected()) {
      if (mqttClient.connect(MQTT_CLIENTID)) {
        mqttClient.subscribe(MQTT_TOPIC, qos);
      }
    }

    String payload = buatPayload(skenario, seq, payloadBytes);
    char prefixBuf[16];
    snprintf(prefixBuf, sizeof(prefixBuf), "%s,%04d,", skenario, seq);
    expectedPrefix = String(prefixBuf);
    echoDiterima   = false;
    long rssi      = WiFi.RSSI();

    unsigned long t_kirim = millis();
    // Mengirim dengan QoS 0 atau QoS 1 sesuai parameter skenario!
    bool sent = mqttClient.publish(MQTT_TOPIC, payload.c_str(), payload.length(), false, qos);

    if (sent) {
      while (!echoDiterima && (millis() - t_kirim) < TIMEOUT_MS) {
        mqttClient.loop();
        delay(1);
      }
    }

    if (sent && echoDiterima && (waktuEchoMs - t_kirim) <= TIMEOUT_MS) {
      unsigned long rtt = waktuEchoMs - t_kirim;
      Serial.printf("%s,%d,%d,%lu,%lu,%lu,OK,%ld\n",
                    skenario, seq, payloadBytes, t_kirim, waktuEchoMs, rtt, rssi);
    } else {
      Serial.printf("%s,%d,%d,%lu,,,LOST,%ld\n",
                    skenario, seq, payloadBytes, t_kirim, rssi);
    }

    unsigned long elapsed = millis() - t_kirim;
    if (elapsed < JEDA_MS) {
      delay(JEDA_MS - elapsed);
    }
  }

  mqttClient.disconnect();
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  WiFi.mode(WIFI_STA);
  pastikanWiFi();

  // Resolusi DNS dilakukan 1 kali di awal (tidak masuk ke RTT)
  WiFi.hostByName(HTTP_HOST, ipHttp);
  WiFi.hostByName(MQTT_BROKER, ipMqtt);

  Serial.println("=== METADATA EKSPERIMEN ===");
  Serial.print("IP_ESP32: "); Serial.println(WiFi.localIP());
  Serial.print("IP_HTTPBIN: "); Serial.println(ipHttp);
  Serial.print("IP_HIVEMQ: "); Serial.println(ipMqtt);
  Serial.print("TOPICS: "); Serial.println(MQTT_TOPIC);
  Serial.println("===========================");

  Serial.println("\n--- MULAI S1 (HTTP POST 20B) ---");
  Serial.println("skenario,seq,payload_byte,t_kirim_ms,t_terima_ms,rtt_ms,status,rssi_dbm");
  jalankanHTTP("S1", 20);
  delay(2000);

  Serial.println("\n--- MULAI S2 (HTTP POST 500B) ---");
  Serial.println("skenario,seq,payload_byte,t_kirim_ms,t_terima_ms,rtt_ms,status,rssi_dbm");
  jalankanHTTP("S2", 500);
  delay(2000);

  Serial.println("\n--- MULAI S3 (MQTT QoS 0 20B) ---");
  Serial.println("skenario,seq,payload_byte,t_kirim_ms,t_terima_ms,rtt_ms,status,rssi_dbm");
  jalankanMQTT("S3", 0, 20);
  delay(2000);

  Serial.println("\n--- MULAI S4 (MQTT QoS 0 500B) ---");
  Serial.println("skenario,seq,payload_byte,t_kirim_ms,t_terima_ms,rtt_ms,status,rssi_dbm");
  jalankanMQTT("S4", 0, 500);
  delay(2000);

  Serial.println("\n--- MULAI S5 (MQTT QoS 1 20B) ---");
  Serial.println("skenario,seq,payload_byte,t_kirim_ms,t_terima_ms,rtt_ms,status,rssi_dbm");
  jalankanMQTT("S5", 1, 20);
  delay(2000);

  Serial.println("\n--- MULAI S6 (MQTT QoS 1 500B) ---");
  Serial.println("skenario,seq,payload_byte,t_kirim_ms,t_terima_ms,rtt_ms,status,rssi_dbm");
  jalankanMQTT("S6", 1, 500);

  Serial.println("\n=== EKSPERIMEN S1-S6 SELESAI ===");
}

void loop() {
  // Selesai
}void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:

}
