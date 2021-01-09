#include <mutex>
#include <WiFi.h>
#include <confrm.h>

const char* ssid = "";
const char* password = "";

Confrm *g_confrm;

TaskHandle_t g_update_config;
std::mutex g_mutex;

uint32_t g_flash_time = 100;
const int c_led_pin = 2;

void setup() {

  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  pinMode(c_led_pin, OUTPUT);

  g_confrm = new Confrm("flasher", "http://10.0.1.106:8000");

  xTaskCreatePinnedToCore(
    UpdateConfigFunction,
    "Config Updater",
    10000,
    NULL,
    1,
    &g_update_config,
    0
    );

}

void UpdateConfigFunction(void *pvParameters) {
  uint32_t tmp = 0;
  for (;;) {
    {
      tmp = g_confrm->get_config("flash_time").toInt();
      std::lock_guard<std::mutex> guard(g_mutex);
      g_flash_time = (tmp > 0) ? tmp : g_flash_time;
    }
    delay(100);
  }
}

void loop() {

  digitalWrite(c_led_pin, HIGH);
  {
    std::lock_guard<std::mutex> guard(g_mutex);
    delay(g_flash_time);
  }
  digitalWrite(c_led_pin, LOW);
  delay(100);

}
