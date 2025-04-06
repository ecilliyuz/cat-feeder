#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <Servo.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <FS.h>

// WiFi bilgileri
const char* ssid = "";  // WiFi adınızı girin
const char* password = "";  // WiFi şifrenizi girin

// Web sunucu port
ESP8266WebServer server(80);

// Servo motor pinleri
#define SERVO_PIN D1  // Servo motor sinyal pini (GPIO2)

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

// Ana HTML sayfası
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="tr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Kedi Besleyici Kontrol</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 0;
            background-color: #f5f5f5;
            color: #333;
        }
        
        .container {
            max-width: 800px;
            margin: 0 auto;
            padding: 20px;
        }
        
        h1 {
            color: #2c3e50;
            text-align: center;
        }
        
        .card {
            background-color: white;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
            margin-bottom: 20px;
            padding: 20px;
        }
        
        h2 {
            color: #3498db;
            margin-top: 0;
        }
        
        .control-group {
            display: flex;
            align-items: center;
            margin-bottom: 15px;
            flex-wrap: wrap;
        }
        
        label {
            margin-right: 10px;
            min-width: 120px;
        }
        
        input[type="number"] {
            width: 70px;
            padding: 8px;
            border: 1px solid #ddd;
            border-radius: 4px;
            margin-right: 10px;
        }
        
        button {
            padding: 8px 16px;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-weight: bold;
            margin-right: 10px;
        }
        
        .primary-btn {
            background-color: #3498db;
            color: white;
        }
        
        .secondary-btn {
            background-color: #2ecc71;
            color: white;
        }
        
        .danger-btn {
            background-color: #e74c3c;
            color: white;
        }
        
        .feeding-schedule {
            background-color: #f9f9f9;
            border-radius: 6px;
            padding: 15px;
            margin-bottom: 15px;
            border-left: 4px solid #3498db;
        }
        
        .schedule-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 10px;
        }
        
        .toggle-container {
            display: flex;
            align-items: center;
        }
        
        .toggle {
            position: relative;
            display: inline-block;
            width: 50px;
            height: 24px;
        }
        
        .toggle input {
            opacity: 0;
            width: 0;
            height: 0;
        }
        
        .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: #ccc;
            transition: .4s;
            border-radius: 24px;
        }
        
        .slider:before {
            position: absolute;
            content: "";
            height: 16px;
            width: 16px;
            left: 4px;
            bottom: 4px;
            background-color: white;
            transition: .4s;
            border-radius: 50%;
        }
        
        input:checked + .slider {
            background-color: #2ecc71;
        }
        
        input:checked + .slider:before {
            transform: translateX(26px);
        }
        
        .status-container {
            text-align: center;
            min-height: 24px;
        }
        
        #status-message {
            display: inline-block;
            padding: 8px 16px;
            border-radius: 4px;
            font-weight: bold;
        }
        
        .success {
            background-color: #d5f5e3;
            color: #27ae60;
        }
        
        .error {
            background-color: #fadbd8;
            color: #c0392b;
        }
        
        /* Mobil görünüm için */
        @media (max-width: 600px) {
            .control-group {
                flex-direction: column;
                align-items: flex-start;
            }
            
            label, input, button {
                margin-bottom: 10px;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Kedi Besleyici</h1>
        
        <div class="card">
            <h2>Manuel Besleme</h2>
            <div class="control-group">
                <label for="manual-feed-duration">Besleme Süresi (ms):</label>
                <input type="number" id="manual-feed-duration" min="100" max="2000" value="500">
                <button id="manual-feed-btn" class="primary-btn">Şimdi Besle</button>
            </div>
        </div>
        
        <div class="card">
            <h2>Besleme Programı</h2>
            <div id="feeding-schedules">
                <!-- Besleme programları buraya JavaScript ile eklenecek -->
            </div>
            <button id="add-feeding-btn" class="secondary-btn">Yeni Besleme Ekle</button>
            <button id="save-schedules-btn" class="primary-btn">Programları Kaydet</button>
        </div>
        
        <div class="status-container">
            <p id="status-message"></p>
        </div>
    </div>
    
    <!-- Besleme programı şablonu -->
    <template id="feeding-template">
        <div class="feeding-schedule">
            <div class="schedule-header">
                <h3>Besleme #<span class="feeding-number"></span></h3>
                <div class="toggle-container">
                    <label class="toggle">
                        <input type="checkbox" class="feeding-enabled">
                        <span class="slider"></span>
                    </label>
                </div>
            </div>
            <div class="control-group">
                <label>Saat:</label>
                <input type="number" class="feeding-hour" min="0" max="23" value="8">
                <label>Dakika:</label>
                <input type="number" class="feeding-minute" min="0" max="59" value="0">
            </div>
            <div class="control-group">
                <label>Besleme Süresi (ms):</label>
                <input type="number" class="feeding-duration" min="100" max="2000" value="500">
                <button class="remove-feeding-btn danger-btn">Sil</button>
            </div>
        </div>
    </template>
    
    <script>
    document.addEventListener('DOMContentLoaded', function() {
        // DOM elemanları
        const feedingSchedulesContainer = document.getElementById('feeding-schedules');
        const feedingTemplate = document.getElementById('feeding-template');
        const addFeedingBtn = document.getElementById('add-feeding-btn');
        const saveSchedulesBtn = document.getElementById('save-schedules-btn');
        const manualFeedBtn = document.getElementById('manual-feed-btn');
        const manualFeedDuration = document.getElementById('manual-feed-duration');
        const statusMessage = document.getElementById('status-message');
        
        // Maksimum besleme sayısı
        const MAX_FEEDINGS = 5;
        
        // Sayfa yüklendiğinde besleme programlarını getir
        fetchFeedingSchedules();
        
        // Besleme programlarını getir
        function fetchFeedingSchedules() {
            fetch('/settings')
                .then(response => response.json())
                .then(data => {
                    // Mevcut besleme programlarını temizle
                    feedingSchedulesContainer.innerHTML = '';
                    
                    // Besleme programlarını ekle
                    data.feedings.forEach((feeding, index) => {
                        addFeedingSchedule(index + 1, feeding);
                    });
                    
                    updateFeedingCount();
                })
                .catch(error => {
                    console.error('Besleme programları yüklenirken hata oluştu:', error);
                    showStatus('Besleme programları yüklenirken hata oluştu!', false);
                });
        }
        
        // Yeni besleme programı ekle butonuna tıklandığında
        addFeedingBtn.addEventListener('click', function() {
            const feedingCount = document.querySelectorAll('.feeding-schedule').length;
            
            if (feedingCount < MAX_FEEDINGS) {
                const newFeeding = {
                    hour: 8,
                    minute: 0,
                    duration: 500,
                    enabled: true
                };
                
                addFeedingSchedule(feedingCount + 1, newFeeding);
                updateFeedingCount();
            } else {
                showStatus(`Maksimum ${MAX_FEEDINGS} besleme programı ekleyebilirsiniz!`, false);
            }
        });
        
        // Besleme programı sayısını güncelle ve buton durumunu ayarla
        function updateFeedingCount() {
            const feedingCount = document.querySelectorAll('.feeding-schedule').length;
            addFeedingBtn.disabled = feedingCount >= MAX_FEEDINGS;
        }
        
        // Besleme programı ekle
        function addFeedingSchedule(number, feeding) {
            const template = document.importNode(feedingTemplate.content, true);
            
            // Besleme numarasını ayarla
            template.querySelector('.feeding-number').textContent = number;
            
            // Değerleri ayarla
            template.querySelector('.feeding-hour').value = feeding.hour;
            template.querySelector('.feeding-minute').value = feeding.minute;
            template.querySelector('.feeding-duration').value = feeding.duration;
            template.querySelector('.feeding-enabled').checked = feeding.enabled;
            
            // Silme butonu olayı
            template.querySelector('.remove-feeding-btn').addEventListener('click', function(event) {
                const feedingElement = event.target.closest('.feeding-schedule');
                feedingElement.remove();
                
                // Besleme numaralarını güncelle
                document.querySelectorAll('.feeding-schedule').forEach((element, idx) => {
                    element.querySelector('.feeding-number').textContent = idx + 1;
                });
                
                updateFeedingCount();
            });
            
            feedingSchedulesContainer.appendChild(template);
        }
        
        // Programları kaydet butonu
        saveSchedulesBtn.addEventListener('click', function() {
            const feedingElements = document.querySelectorAll('.feeding-schedule');
            const feedings = [];
            
            feedingElements.forEach(element => {
                feedings.push({
                    hour: parseInt(element.querySelector('.feeding-hour').value),
                    minute: parseInt(element.querySelector('.feeding-minute').value),
                    duration: parseInt(element.querySelector('.feeding-duration').value),
                    enabled: element.querySelector('.feeding-enabled').checked
                });
            });
            
            // Maksimum besleme sayısına tamamla (API için)
            while (feedings.length < MAX_FEEDINGS) {
                feedings.push({
                    hour: 0,
                    minute: 0,
                    duration: 500,
                    enabled: false
                });
            }
            
            // Ayarları kaydet
            saveSchedulesBtn.disabled = true;
            saveSchedulesBtn.textContent = 'Kaydediliyor...';
            
            fetch('/settings', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({ feedings: feedings })
            })
            .then(response => response.json())
            .then(data => {
                if (data.status === 'success') {
                    showStatus('Besleme programları başarıyla kaydedildi!', true);
                } else {
                    showStatus('Besleme programları kaydedilirken hata oluştu!', false);
                }
            })
            .catch(error => {
                console.error('Besleme programları kaydedilirken hata oluştu:', error);
                showStatus('Besleme programları kaydedilirken hata oluştu!', false);
            })
            .finally(() => {
                saveSchedulesBtn.disabled = false;
                saveSchedulesBtn.textContent = 'Programları Kaydet';
            });
        });
        
        // Manuel besleme butonu
        manualFeedBtn.addEventListener('click', function() {
            const duration = manualFeedDuration.value;
            
            manualFeedBtn.disabled = true;
            manualFeedBtn.textContent = 'Besleniyor...';
            
            fetch('/feed', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded'
                },
                body: `duration=${duration}`
            })
            .then(response => response.json())
            .then(data => {
                if (data.status === 'success') {
                    showStatus('Manuel besleme başarıyla gerçekleştirildi!', true);
                } else {
                    showStatus('Manuel besleme sırasında hata oluştu!', false);
                }
            })
            .catch(error => {
                console.error('Manuel besleme sırasında hata oluştu:', error);
                showStatus('Manuel besleme sırasında hata oluştu!', false);
            })
            .finally(() => {
                manualFeedBtn.disabled = false;
                manualFeedBtn.textContent = 'Şimdi Besle';
            });
        });
        
        // Durum mesajını göster
        function showStatus(message, isSuccess) {
            statusMessage.textContent = message;
            statusMessage.className = isSuccess ? 'success' : 'error';
            
            // 3 saniye sonra mesajı temizle
            setTimeout(() => {
                statusMessage.textContent = '';
                statusMessage.className = '';
            }, 3000);
        }
    });
    </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  Serial.println("\nKedi Besleyici Başlatılıyor...");
  
  // SPIFFS dosya sistemini başlat ve biçimlendir
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS başlatılamadı! Biçimlendiriliyor...");
    SPIFFS.format();
    if (!SPIFFS.begin()) {
      Serial.println("SPIFFS yine başlatılamadı! Cihazı yeniden başlatın.");
      return;
    }
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
  server.send(200, "text/html", INDEX_HTML);
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
  feederServo.write(120); // Servoyu dök pozisyonuna getir
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