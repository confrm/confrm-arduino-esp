#include <string>
#include <vector>
#include <cstring>
#include <sys/time.h>

#include <HTTPClient.h> // NOLINT
#include <WiFiClient.h>

#include "esp_ota_ops.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"

#include "mbedtls/sha256.h"

// Storage includes for persistent data
#include "FS.h"
#include "SPIFFS.h"

#include "confrm.h"
#include "simple_json.h"

// Storage includes for persistent data
#include "FS.h"
#include "SPIFFS.h"

#define SHORT_REST_RESPONSE_LENGTH 256

String simple_url_encode(String input) {
  input.replace(" ", "%20");
  return input;
}

// https://stackoverflow.com/a/62612409
static char h2b(char c) {
    return '0'<=c && c<='9' ? c - '0'      :
           'A'<=c && c<='F' ? c - 'A' + 10 :
           'a'<=c && c<='f' ? c - 'a' + 10 :
           /* else */         -1;
}

int hex2bin(unsigned char* bin,  unsigned int bin_len, const char* hex) {
    for(unsigned int i=0; i<bin_len; i++) {
        char b[2] = {h2b(hex[2*i+0]), h2b(hex[2*i+1])};
        if(b[0]<0 || b[1]<0) return -1;
        bin[i] = b[0]*16 + b[1];
    }
    return 0;
}

String Confrm::short_rest(String url, int &httpCode, String type,
                          String payload) {

  // If not configured this cannot work
  if (!m_config_status) {
    return "";
  }

  HTTPClient http;
  http.begin(url);

  if (type == "GET") {
    httpCode = http.GET();
  } else if (type == "PUT") {
    httpCode = http.PUT(payload);
  } else if (type == "POST") {
    httpCode = http.POST(payload);
  } else {
    ESP_LOGE(TAG, "Unsupported call type");
    httpCode = -1;
    http.end();
    return "";
  }

  if (httpCode < 0) {
    ESP_LOGI(TAG, "Unable to connect to confrm server");
    http.end();
    return "";
  }

  int len = http.getSize();
  if (len >= SHORT_REST_RESPONSE_LENGTH) {
    ESP_LOGE(TAG, "confrm server sending too much data...");
    http.end();
    return "";
  }

  // create buffer for read
  // One longer to ensure buf is null terminated
  uint8_t *buff = new uint8_t[SHORT_REST_RESPONSE_LENGTH + 1];
  memset(buff, 0, SHORT_REST_RESPONSE_LENGTH + 1);
  uint8_t *buff_ptr = buff;

  // get tcp stream
  WiFiClient *stream = http.getStreamPtr();

  // read all data from server
  while (http.connected() && (len > 0 || len == -1)) {
    size_t size = stream->available();
    if (size) {
      size_t to_read = SHORT_REST_RESPONSE_LENGTH - 1 - (buff_ptr - buff);
      if (to_read > len)
        to_read = len;
      int c = stream->readBytes(buff_ptr, to_read);
      buff_ptr += c;
      if (len > 0) {
        len -= c;
      }
    }

    String retstr = String((char *)buff);
    delete[] buff;

    // Save data to output string
    return retstr;
  }

  http.end();
  return "";
}

bool Confrm::check_for_updates() {

  set_time();

  int httpCode = 0;
  String request = m_confrm_url + "/check_for_update/?name=" + m_package_name +
                   "&node_id=" + WiFi.macAddress();
  String response = short_rest(request, httpCode, "GET");

  if (httpCode != 200 || response == "" || response == "{}") {
    return false;
  }

  std::vector<SimpleJSONElement> content = simple_json(response);

  String ver = get_simple_json_string(content, "current_version");
  ESP_LOGI(TAG, "Current version of %s on confrm server is: %s", m_package_name,
           ver);

  bool force = get_simple_json_bool(content, "force");

  if (force || 0 != strcmp(m_config.current_version, ver.c_str())) {
    if (force) {
      ESP_LOGI(TAG, "Server is forcing an update");
    } else {
      ESP_LOGI(TAG, "Different version available, update required...");
    }
    m_next_version = ver;
    m_next_blob = get_simple_json_string(content, "blob");
    hex2bin(m_next_hash, 32, get_simple_json_string(content, "hash").c_str());
    return true;
  }

  return false;
}

bool Confrm::do_update() {

  // If not configured this cannot work
  if (!m_config_status) {
    return false;
  }

  // Likewise, sanity check the update settings
  if (m_next_version == "" or m_next_blob == "") {
    return false;
  }

  HTTPClient http;
  String request =
      m_confrm_url + "/blob/?name=" + m_package_name + "&blob=" + m_next_blob;
  http.begin(request);
  int httpCode = http.GET();

  if (httpCode == 200) {

    int len = http.getSize();

    // There is data incoming, start the OTA programming process
    esp_err_t err;
    esp_ota_handle_t ota_handle;
    const esp_partition_t *current = esp_ota_get_running_partition();
    const esp_partition_t *next = esp_ota_get_next_update_partition(current);
    err = esp_ota_begin(next, len, &ota_handle);

    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Error initializing OTA process");
      http.end();
      return false;
    }

    // create buffer for read
    uint8_t buff[128] = {0};

    // Create buffer and context for sha256 calc, start the hashing
    unsigned char hash[32];
    mbedtls_sha256_context ctx2;
    mbedtls_sha256_init(&ctx2);
    mbedtls_sha256_starts(&ctx2, 0);

    // get tcp stream
    WiFiClient *stream = http.getStreamPtr();

    // read all data from server
    while (http.connected() && (len > 0 || len == -1)) {
      esp_task_wdt_reset();
      size_t size = stream->available();
      if (size) {
        size_t to_read = sizeof(buff);
        if (to_read > len)
          to_read = len;
        int c = stream->readBytes(buff, to_read);
        mbedtls_sha256_update(&ctx2, buff, c);
        err = esp_ota_write(ota_handle, buff, c);
        if (err != ESP_OK) {
          ESP_LOGE(TAG, "Error initializing OTA process");
          esp_ota_end(ota_handle);
          http.end();
          return false;
        }
        if (len > 0) {
          len -= c;
        }
      }
    }

    esp_ota_end(ota_handle);

    mbedtls_sha256_finish(&ctx2, hash);
    mbedtls_sha256_free(&ctx2);
    bool integrity = true;
    for (int i = 0; i < 32; i++) {
      if (hash[i] != m_next_hash[i]) {
        integrity = false;
      }
    }
    if (integrity == false) {
      ESP_LOGE(TAG, "Blob sha256 does not match expected value");
      http.end();
      return false;
    }

    for (int i = 0; i < sizeof(m_config.current_version); i++) {
      if (i < m_next_version.length()) {
        m_config.current_version[i] = m_next_version.c_str()[i];
      } else {
        m_config.current_version[i] = '\0';
      }
    }
    save_config(m_config);

    esp_ota_set_boot_partition(next);
    http.end();
    hard_restart();
  }

  http.end();
}

bool Confrm::init_config(const bool reset_config) {

  // Attempt to start FS, if it does not start then try formatting it
  if (!SPIFFS.begin(true)) {
    ESP_LOGD(TAG, "Failed to init SPIFFS, confrm will not work");
    return false;
  }

  if (reset_config) {
    ESP_LOGD(TAG, "Resetting config");
    config_s config;
    memset(config.current_version, 0, 32);
    save_config(config);
  }

  ESP_LOGD(TAG, "Getting confrm config");

  File file = SPIFFS.open(m_config_file.c_str());

  if (!file || file.isDirectory()) {
    ESP_LOGD(TAG, "Failed to open file for reading, creating default config");
    config_s config;
    memset(config.current_version, 0, 32);
    if (save_config(config)) {
      file = SPIFFS.open(m_config_file.c_str());
      if (!file) {
        ESP_LOGD(TAG, "Error created new config_file, but unable to open, "
                      "confrm will not work");
        return false;
      }
    } else {
      ESP_LOGD(TAG, "Error creating new config_file, confrm will not work");
      return false;
    }
  }

  if (file.available()) {
    uint8_t version = file.read();
    ESP_LOGD(TAG, "Config version %d", version);
    switch (version) {
    case 1:
      size_t bytes_read = file.read((uint8_t *)&m_config, sizeof(config_s));
      // Ensure null terminated strings...
      m_config.current_version[31] = '\0';
      ESP_LOGD(TAG, "confrm config is:");
      ESP_LOGD(TAG, "\tcurrent_version: \"%s\"", m_config.current_version);
      break;
    others:
      ESP_LOGE(TAG, "Unknown config version");
      file.close();
      return false;
      break;
    }
  } else {
    ESP_LOGD(TAG, "Config file was empty, populating with empty config");
    file.close();
    memset(m_config.current_version, 0, 32);
    if (!save_config(m_config)) {
      ESP_LOGD(TAG, "Error created new config_file, confrm will not work");
      return false;
    }
  }

  ESP_LOGD(TAG, "Config loaded");
  return true;
}

bool Confrm::save_config(config_s config) {

  File file = SPIFFS.open(m_config_file.c_str(), FILE_WRITE);
  if (!file) {
    ESP_LOGD(TAG, "Unable to create file");
    return false;
  }

  config.current_version[31] = '\0';

  file.write(m_config_version);
  file.write((uint8_t *)config.current_version, 32);

  file.close();

  return true;
}

void Confrm::timer_start() {
  if (m_update_period > 2) {
    esp_timer_start_periodic(m_timer, m_update_period * 1000 * 1000ULL);
  }
}

void Confrm::timer_stop() {
  if (m_update_period > 2) {
    esp_timer_stop(m_timer);
  }
}

void Confrm::timer_callback(void *ptr) {
  Confrm *self = reinterpret_cast<Confrm *>(ptr);
  self->timer_stop();
  if (self->check_for_updates()) {
    ESP_LOGD(TAG, "Rebooting from timer_callback");
    self->hard_restart(); // The ESP32 does not like updating from the timer
                          // callback
  }
  if (self->m_reregister_period > 0) {
    if (self->m_reregister_period_count > 0) {
      self->m_reregister_period_count -= 1;
    } else {
      self->m_reregister_period_count = self->m_reregister_period;
      self->register_node();
    }
  }
  self->timer_start();
}

void Confrm::set_time() {
  String request = m_confrm_url + "/time/";
  int httpCode = 0;
  String response = short_rest(request, httpCode, "GET");
  if (httpCode == 200 && response != "" && response != "{}") {
    std::vector<SimpleJSONElement> content = simple_json(response);
    int64_t epoch = get_simple_json_number(content, "time");
    struct timeval now;
    now.tv_sec = epoch;
    settimeofday(&now, NULL);
  }
}

void Confrm::register_node() {
  int httpCode = 0;
  String request =
      m_confrm_url + "/register_node/" + "?package=" + m_package_name +
      "&node_id=" + WiFi.macAddress() + "&version=" + m_config.current_version +
      "&description=" + m_node_description + "&platform=" + m_node_platform;
  short_rest(request, httpCode, "PUT");
}

void Confrm::hard_restart() {
  // Bit hacky, use watchdog timer to force a restart. The ESP_restart()
  // method does not reboot everything and was leading to instability
  esp_task_wdt_init(1, true);
  esp_task_wdt_add(NULL);
  while (true)
    ;
}

const String Confrm::get_config(String name) {
  int httpCode = 0;
  String request =
    m_confrm_url + "/config/" + "?package=" + m_package_name +
    "&node_id=" + WiFi.macAddress() + "&key=" + name;
  String response = short_rest(request, httpCode, "GET");
  if (httpCode == 200) {
    std::vector<SimpleJSONElement> content = simple_json(response);
    return get_simple_json_string(content, "value");
  }
  return "";
}

Confrm::Confrm(String package_name, String confrm_url, String node_description,
               String node_platform, int32_t update_period,
               const bool reset_config) {

  const esp_partition_t *current = esp_ota_get_running_partition();
  ESP_LOGD(TAG, "Booted to %d", current->address);

  m_package_name = package_name;
  m_confrm_url = confrm_url;
  m_node_description = simple_url_encode(node_description);
  m_node_platform = node_platform;

  m_config_status = init_config(reset_config);
  if (!m_config_status) {
    ESP_LOGE(TAG, "Failed to load config");
  }

  if (check_for_updates()) {
    do_update();
  }

  m_update_period = update_period;
  if (m_update_period > 2) {
    esp_timer_create_args_t timer_config;
    timer_config.arg = reinterpret_cast<void *>(this);
    timer_config.callback = Confrm::timer_callback;
    timer_config.dispatch_method = ESP_TIMER_TASK;
    timer_config.name = "Confrm update timer";
    esp_timer_create(&timer_config, &m_timer);
    timer_start();
  }

  m_reregister_period_count = m_reregister_period;
  register_node();
}
