#include <WiFi.h>
#include <esp_now.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

// OLED display config
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 27
#define OLED_SCL 23

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Battery voltage pin
#define BATTERY_PIN 35

// Receiver MAC address (example)
uint8_t receiverMAC[] = {0x44, 0x1D, 0x64, 0xF3, 0x8E, 0x14};

// Structure for sending data
typedef struct struct_message {
  char msg[32];
  float latitude;
  float longitude;
  float battery;
  bool gps;
} struct_message;

struct_message myData;

bool sendHelpMsg = true;
bool coordUp = true;

float baseLat = 23.747120;
float baseLon = 90.369619;
float offset = 0.0009;

unsigned long lastMsgTime = 0;
unsigned long lastCoordTime = 0;

// Read battery voltage
float readBatteryVoltage() {
  int raw = analogRead(BATTERY_PIN);
  float voltage = (raw / 4095.0) * 3.3 * 2.0;  // Adjusted for voltage divider
  return voltage;
}

// Show info on OLED
void showDisplay(const char* status) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  display.print("MSG: ");
  display.println(myData.msg);

  display.print("Lat: ");
  display.println(myData.latitude, 6);

  display.print("Lon: ");
  display.println(myData.longitude, 6);

  display.print("Bat: ");
  display.print(myData.battery, 2);
  display.println("V");

  display.print("GPS: ");
  display.println(myData.gps ? "OK" : "NO");

  display.setCursor(0, 56);
  display.println(status);

  display.display();
}

// ✅ Updated callback for latest ESP32 core
void onDataSent(const wifi_tx_info_t* info, esp_now_send_status_t status) {
  const char* sendStatus = (status == ESP_NOW_SEND_SUCCESS) ? "Sent!" : "Fail!";
  showDisplay(sendStatus);

  Serial.print("Send Status: ");
  Serial.println(sendStatus);

  if (info) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             info->des_addr[0], info->des_addr[1], info->des_addr[2],
             info->des_addr[3], info->des_addr[4], info->des_addr[5]);
    Serial.print("To MAC: ");
    Serial.println(macStr);
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 init failed");
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Init ESP-NOW...");
  display.display();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    display.println("ESP-NOW fail");
    display.display();
    while (true);
  }

  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    display.println("Peer fail");
    display.display();
    while (true);
  }

  // Init ADC for battery reading
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // First data init
  myData.gps = true;
  myData.battery = readBatteryVoltage();
}

void loop() {
  unsigned long now = millis();

  // Alternate message every 10s
  if (now - lastMsgTime > 10000) {
    sendHelpMsg = !sendHelpMsg;
    lastMsgTime = now;
  }

  // Alternate coordinates every 30s
  if (now - lastCoordTime > 30000) {
    coordUp = !coordUp;
    lastCoordTime = now;
  }

  // Prepare data
  strcpy(myData.msg, sendHelpMsg ? "Help Me" : "Emergency Rescue");
  myData.latitude = coordUp ? baseLat + offset : baseLat - offset;
  myData.longitude = baseLon;
  myData.battery = readBatteryVoltage();
  myData.gps = true;

  // Display on OLED
  showDisplay("Sending...");

  // Send via ESP-NOW
  esp_err_t result = esp_now_send(receiverMAC, (uint8_t*)&myData, sizeof(myData));

  if (result != ESP_OK) {
    Serial.println("Send failed");
    showDisplay("Fail!");
  }

  delay(1000);  // Send every second
}
