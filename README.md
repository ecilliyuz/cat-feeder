# ESP8266 Kedi Besleyici

Bu proje, ESP8266 ve servo motor kullanarak ev yapımı bir kedi besleyici oluşturmayı amaçlar. Web tarayıcısı üzerinden bağlanılabilen arayüz sayesinde besleme saatlerini ve miktarını ayarlayabilirsiniz.

## Özellikler

- Web arayüzü üzerinden kontrol
- Programlanabilir besleme saatleri
- Ayarlanabilir mama miktarı
- Gerçek zamanlı saat takibi
- WiFi bağlantısı

## Gerekli Malzemeler

- ESP8266 (NodeMCU veya Wemos D1 Mini)
- Servo motor (SG90 veya benzeri)
- Jumper kablolar
- Mikro USB kablo
- Güç kaynağı
- 3D baskı veya karton besleme haznesi

## Kurulum

1. Arduino IDE'yi kurun
2. Gerekli kütüphaneleri yükleyin (ESP8266WebServer, ArduinoJson, NTPClient, Servo)
3. Devre bağlantılarını yapın:
   - Servo sinyal pini -> D1
   - Servo VCC -> 3.3V
   - Servo GND -> GND
4. Kodu ESP8266'ya yükleyin
5. Web tarayıcısından ESP8266'nın IP adresine bağlanın

## Kullanım

1. WiFi ağınıza bağlandıktan sonra, ESP8266'nın IP adresine bir web tarayıcısından erişin
2. Arayüz üzerinden besleme saatlerini ayarlayın
3. Servo motoru açık kalma süresini ayarlayarak mama miktarını belirleyin
4. Ayarlarınızı kaydedin

## Kod Yapısı

- `cat_feeder.ino`: Ana Arduino kod dosyası

## Lisans

MIT # cat-feeder
