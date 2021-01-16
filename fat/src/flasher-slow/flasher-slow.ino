#include <WiFi.h>
#include <confrm.h>

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

Confrm *g_confrm;

const uint32_t c_flash_time = FLASH_TIME;
const int c_led_pin = 2;

void setup() {

  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  pinMode(c_led_pin, OUTPUT);

  char description[80];
  sprintf(description, "Flasher - %s", DESCRIPTION_STRING);

  g_confrm = new Confrm("flasher", CONFRM_SERVER, description, "esp32", 10);

}

void loop() {

  digitalWrite(c_led_pin, HIGH);
  delay(c_flash_time);
  digitalWrite(c_led_pin, LOW);
  delay(100);

}
