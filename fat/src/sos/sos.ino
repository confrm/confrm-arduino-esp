#include <WiFi.h>
#include <confrm.h>

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

Confrm *g_confrm;

const uint32_t c_flash_dot = 100;
const uint32_t c_flash_dash = 400;
const uint32_t c_flash_space = 100;

const int c_led_pin = 2;

void setup() {

  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  pinMode(c_led_pin, OUTPUT);

  g_confrm = new Confrm("sos", CONFRM_SERVER, "SOS", "esp32", 10);

}

void dot() {
  digitalWrite(c_led_pin, HIGH);
  delay(c_flash_dot);
  digitalWrite(c_led_pin, LOW);
  delay(c_flash_space);
}

void dash() {
  digitalWrite(c_led_pin, HIGH);
  delay(c_flash_dash);
  digitalWrite(c_led_pin, LOW);
  delay(c_flash_space);
}

void loop() {

  dot();
  dot();
  dot();
  dash();
  dash();
  dash();
  dot();
  dot();
  dot();
  delay(c_flash_space*10);

}
