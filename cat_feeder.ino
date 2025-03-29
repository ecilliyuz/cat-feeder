#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <Servo.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <FS.h>

// WiFi bilgileri
const char* ssid = "WIFI_ISMI";  // WiFi adınızı girin
const char* password = "WIFI_SIFRESI";  // WiFi şifrenizi girin

// Web sunucu port
ESP8266WebServer server(80);

// Servo motor pinleri
#define SERVO_PIN D4  // Servo motor sinyal pini (GPIO2)

// Servo nesnesi
Servo feederServo;

// NTP İstemcisi (saat senkronizasyonu için)
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800); // TÜRKİYE için UTC+3 (10800 saniye)

// Besleme yapılandırması
struct FeedingSchedule {
  int hour;
  int minute;
  int feedDuration; // Milisaniye cinsinden servo dönüş süresi
  bool enabled;
};

// En fazla 5 besleme saati ayarlanabilir
const int MAX_FEEDINGS = 5;
FeedingSchedule feedingSchedules[MAX_FEEDINGS];

// Son kontrol zamanı
unsigned long lastCheckTime = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("\nKedi Besleyici Başlatılıyor...");
  
  // SPIFFS dosya sistemini başlat
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS başlatılamadı!");
    return;
  }
  
  // Varsayılan besleme programını ayarla
  for (int i = 0; i < MAX_FEEDINGS; i++) {
    feedingSchedules[i].hour = 0;
    feedingSchedules[i].minute = 0;
    feedingSchedules[i].feedDuration = 500;
    feedingSchedules[i].enabled = false;
  }
  
  // Kayıtlı besleme programlarını yükle
  loadFeedingSchedules();
  
  // Servo motoru başlat
  feederServo.attach(SERVO_PIN);
  feederServo.write(0); // Başlangıç pozisyonu
  
  // WiFi'a bağlan
  WiFi.begin(ssid, password);
  Serial.print("WiFi'a bağlanılıyor");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi bağlantısı başarılı");
  Serial.print("IP adresi: ");
  Serial.println(WiFi.localIP());
  
  // NTP zaman sunucusunu başlat
  timeClient.begin();
  
  // Web sunucu yollarını ayarla
  server.on("/", HTTP_GET, handleRoot);
  server.on("/settings", HTTP_GET, handleGetSettings);
  server.on("/settings", HTTP_POST, handlePostSettings);
  server.on("/feed", HTTP_POST, handleManualFeed);
  server.onNotFound(handleNotFound);
  
  // Web sunucuyu başlat
  server.begin();
  Serial.println("Web sunucu başlatıldı");
}

void loop() {
  server.handleClient();
  timeClient.update();
  
  // Her dakika besleme programını kontrol et
  unsigned long currentMillis = millis();
  if (currentMillis - lastCheckTime >= 60000 || lastCheckTime == 0) {
    lastCheckTime = currentMillis;
    checkFeedingSchedule();
  }
}

// Web sayfasını gönder
void handleRoot() {
  File file = SPIFFS.open("/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "Dosya açılamadı!");
    return;
  }
  
  server.streamFile(file, "text/html");
  file.close();
}

// Besleme ayarlarını JSON formatında gönder
void handleGetSettings() {
  DynamicJsonDocument doc(1024);
  JsonArray feedingsArray = doc.createNestedArray("feedings");
  
  for (int i = 0; i < MAX_FEEDINGS; i++) {
    JsonObject feeding = feedingsArray.createNestedObject();
    feeding["hour"] = feedingSchedules[i].hour;
    feeding["minute"] = feedingSchedules[i].minute;
    feeding["duration"] = feedingSchedules[i].feedDuration;
    feeding["enabled"] = feedingSchedules[i].enabled;
  }
  
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

// Besleme ayarlarını güncelle
void handlePostSettings() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
      server.send(400, "text/plain", "JSON ayrıştırma hatası!");
      return;
    }
    
    JsonArray feedingsArray = doc["feedings"];
    
    for (int i = 0; i < feedingsArray.size() && i < MAX_FEEDINGS; i++) {
      JsonObject feeding = feedingsArray[i];
      feedingSchedules[i].hour = feeding["hour"];
      feedingSchedules[i].minute = feeding["minute"];
      feedingSchedules[i].feedDuration = feeding["duration"];
      feedingSchedules[i].enabled = feeding["enabled"];
    }
    
    saveFeedingSchedules();
    server.send(200, "application/json", "{\"status\":\"success\"}");
  } else {
    server.send(400, "text/plain", "Veri bulunamadı!");
  }
}

// Manuel besleme
void handleManualFeed() {
  if (server.hasArg("duration")) {
    int duration = server.arg("duration").toInt();
    feedNow(duration);
    server.send(200, "application/json", "{\"status\":\"success\"}");
  } else {
    feedNow(500); // Varsayılan değer
    server.send(200, "application/json", "{\"status\":\"success\"}");
  }
}

// 404 Hata
void handleNotFound() {
  server.send(404, "text/plain", "Sayfa bulunamadı!");
}

// Besleme programını kontrol et
void checkFeedingSchedule() {
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  
  for (int i = 0; i < MAX_FEEDINGS; i++) {
    if (feedingSchedules[i].enabled &&
        feedingSchedules[i].hour == currentHour &&
        feedingSchedules[i].minute == currentMinute) {
      // Besleme zamanı geldi
      feedNow(feedingSchedules[i].feedDuration);
      Serial.print("Programlı besleme yapıldı. Süre: ");
      Serial.println(feedingSchedules[i].feedDuration);
    }
  }
}

// Besleme yap
void feedNow(int duration) {
  feederServo.write(180); // Servoyu dök pozisyonuna getir
  delay(duration);
  feederServo.write(0);  // Servoyu başlangıç pozisyonuna getir
}

// Besleme programlarını kaydet
void saveFeedingSchedules() {
  DynamicJsonDocument doc(1024);
  JsonArray feedingsArray = doc.createNestedArray("feedings");
  
  for (int i = 0; i < MAX_FEEDINGS; i++) {
    JsonObject feeding = feedingsArray.createNestedObject();
    feeding["hour"] = feedingSchedules[i].hour;
    feeding["minute"] = feedingSchedules[i].minute;
    feeding["duration"] = feedingSchedules[i].feedDuration;
    feeding["enabled"] = feedingSchedules[i].enabled;
  }
  
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Yapılandırma dosyası açılamadı!");
    return;
  }
  
  serializeJson(doc, configFile);
  configFile.close();
  Serial.println("Besleme programları kaydedildi");
}

// Besleme programlarını yükle
void loadFeedingSchedules() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Yapılandırma dosyası bulunamadı, varsayılan değerler kullanılıyor");
    return;
  }
  
  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, buf.get());
  
  if (error) {
    Serial.println("Yapılandırma dosyası yüklenemedi!");
    return;
  }
  
  JsonArray feedingsArray = doc["feedings"];
  
  for (int i = 0; i < feedingsArray.size() && i < MAX_FEEDINGS; i++) {
    JsonObject feeding = feedingsArray[i];
    feedingSchedules[i].hour = feeding["hour"];
    feedingSchedules[i].minute = feeding["minute"];
    feedingSchedules[i].feedDuration = feeding["duration"];
    feedingSchedules[i].enabled = feeding["enabled"];
  }
  
  configFile.close();
  Serial.println("Besleme programları yüklendi");
} 