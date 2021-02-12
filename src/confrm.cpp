//#include <string>
//#include <sys/time.h>
#include <vector>

// Architecture specific includes
#if defined(ARDUINO_ARCH_ESP32)

#include <mutex>

#include <HTTPClient.h> // NOLINT
#include <WiFiClient.h>

#include "esp_ota_ops.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"

#include "mbedtls/sha256.h"

// Storage includes for persistent data
#include "FS.h"
#include "SPIFFS.h"

#define CONFRM_TRY_CATCH

#elif defined(ARDUINO_ARCH_ESP8266)

#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

#if not defined(CONFRM_ESP8266_FS)
#define CONFRM_ESP8266_FS LittleFS
#endif

#if CONFRM_ESP8266_FS == LittleFS
#include <LittleFS.h>
#else
#include "SPIFFS.h"
#endif

#define ESP_LOGE(tag, fmt, ...) ESP_LOGN("ERROR", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) ESP_LOGN("DEBUG", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) ESP_LOGN("INFO", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGN(L, tag, fmt, ...)                                             \
  do {                                                                         \
    String my_fmt = "[" + String(tag) + "] - " + L + " " + fmt + "\n";         \
    Serial.printf(my_fmt.c_str(), ##__VA_ARGS__);                              \
  } while (0)

static const char *TAG = "confrm";

#endif

#include "confrm.h"
#include "simple_json.h"

#define SHORT_REST_RESPONSE_LENGTH 256

String simple_url_encode(String input) {
  input.replace(" ", "%20");
  return input;
}

// https://stackoverflow.com/a/62612409
static char h2b(char c) {
  return '0' <= c && c <= '9'   ? c - '0'
         : 'A' <= c && c <= 'F' ? c - 'A' + 10
         : 'a' <= c && c <= 'f' ? c - 'a' + 10
                                :
                                /* else */ -1;
}

int hex2bin(unsigned char *bin, unsigned int bin_len, const char *hex) {
  for (unsigned int i = 0; i < bin_len; i++) {
    char b[2] = {h2b(hex[2 * i + 0]), h2b(hex[2 * i + 1])};
    if (b[0] < 0 || b[1] < 0)
      return -1;
    bin[i] = b[0] * 16 + b[1];
  }
  return 0;
}

String Confrm::short_rest(String url, int &httpCode, String type,
                          String payload) {

  // If not configured this cannot work
  if (!m_config_status) {
    return "";
  }

#if defined(ARDUINO_ARCH_ESP32)
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
#elif defined(ARDUINO_ARCH_ESP8266)

  WiFiClient client;
  HTTPClient http;

  if (http.begin(client, url)) {

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

    String response = http.getString();
    http.end();

    return response;

  } else {
    ESP_LOGI(TAG, "Unable to connect to confrm server");
    return "";
  }
#endif
}

bool Confrm::check_for_updates() {

  set_time();

  int httpCode = 0;
  String request = m_confrm_url +
                   "/check_for_update/?package=" + m_package_name +
                   "&node_id=" + WiFi.macAddress();
  String response = short_rest(request, httpCode, "GET");

  if (httpCode != 200 || response == "" || response == "{}") {
    return false;
  }

  std::vector<SimpleJSONElement> content;
#if defined(CONFRM_TRY_CATCH)
  try {
    content = simple_json(response);
  } catch (...) {
    ESP_LOGI(TAG, "Error parsing json");
    return false;
  }
#else
  content = simple_json(response);
  if (content.size() == 0) {
    ESP_LOGI(TAG, "No JSON data was extracted, there may have been and error "
                  "processing the response");
    return false;
  }
#endif

  String ver = get_simple_json_string(content, "current_version");
  ESP_LOGI(TAG, "Current version of %s on confrm server is: %s",
           m_package_name.c_str(), ver.c_str());

  bool force = get_simple_json_bool(content, "force");
  bool reboot = get_simple_json_bool(conent, "reboot");

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
  } else if (reboot) {
    hard_restart();
  }

  return false;
}

#if defined(ARDUINO_ARCH_ESP32)
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
  String request = m_confrm_url + "/blob/?package=" + m_package_name +
                   "&blob=" + m_next_blob;
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
  return false;
}
#elif defined(ARDUINO_ARCH_ESP8266)
/*
 * ESP8266 specific update routine, use the HTTP update class,
 * would be good to use the same as ESP32 but the API is considerably
 * more complex.
 *
 * This version makes use of a 'simpler' CRC for error checking, whereas
 * the ESP32 uses a SHA256.
 */
bool Confrm::do_update() {

  // If not configured this cannot work
  if (!m_config_status) {
    return false;
  }

  // Likewise, sanity check the update settings
  if (m_next_version == "" or m_next_blob == "") {
    return false;
  }

  String request = m_confrm_url + "/blob/?package=" + m_package_name +
                   "&blob=" + m_next_blob;

  WiFiClient client;
  // Disable reboot as we need to update the persistent settings
  // if the update is successful before rebooting
  ESPhttpUpdate.rebootOnUpdate(false);
  t_httpUpdate_return ret = ESPhttpUpdate.update(client, request);

  switch (ret) {
  case HTTP_UPDATE_FAILED:
    ESP_LOGE(TAG, "HTTP_UPDATE_FAILED Error (%d): %s\n",
             ESPhttpUpdate.getLastError(),
             ESPhttpUpdate.getLastErrorString().c_str());
    return false;
    break;

  case HTTP_UPDATE_NO_UPDATES:
    ESP_LOGE(TAG, "HTTP Update Failed");
    return false;
    break;

  case HTTP_UPDATE_OK:
    ESP_LOGE(TAG, "HTTP Update Complete - Storing Updated Settings");

    for (int i = 0; i < sizeof(m_config.current_version); i++) {
      if (i < m_next_version.length()) {
        m_config.current_version[i] = m_next_version.c_str()[i];
      } else {
        m_config.current_version[i] = '\0';
      }
    }
    save_config(m_config);

    hard_restart();

    return true;
    break;
  }
  return false;
}

#endif

bool Confrm::init_config(bool reset) {

  if (m_config_storage_override) {

    /*
     * If config override is set, attempt to read the config.
     *
     * Errors may happen if version is not correct, size is incorrect or
     * the set callbacks cant read the information for any reason.
     *
     * On error it will try and reset the config to blank as that is
     * realistically the only thing we can do and it is fairly simple
     * to recover from that state.
     */

    if (reset) {
      ESP_LOGD(TAG, "Resetting config");
      return reset_config();
    }

    uint8_t *config_ptr = NULL;
    size_t bytes_read = m_config_storage_load(&config_ptr);
    bool result;

    if (bytes_read > 2) {
      uint8_t version = config_ptr[0];
      switch (version) {
      case 1:
        if (config_ptr[1] != sizeof(config_s)) {
          ESP_LOGE(TAG, "Size of config record does not match size of config "
                        "structure, unable to init config. Reinitialising");
          result = reset_config();
          if (result == false) {
            ESP_LOGE(TAG, "Error resetting config, unable to start confrm");
            delete [] config_ptr;
            return false;
          }
        } else {
          memcpy(reinterpret_cast<void *>(&m_config),
                 reinterpret_cast<void *>(config_ptr + 2), sizeof(m_config));
          m_config.current_version[31] = '\0';
          ESP_LOGD(TAG, "confrm config is:");
          ESP_LOGD(TAG, "\tcurrent_version: \"%s\"", m_config.current_version);
        }
        break;
      default:
        ESP_LOGE(TAG, "Unknown config version, resetting config.");
        result = reset_config();
        if (result == false) {
          ESP_LOGE(TAG, "Error resetting config, unable to start confrm");
          delete [] config_ptr;
          return false;
        }
      }
      delete [] config_ptr;
      return true;
    }

    /*
     * Code gets here if there was insufficient bytes returned form the load
     * callback - tidy up by releasing allocated RAM and attempting to reset
     * the config.
     */

    if (config_ptr != NULL) {
      delete [] config_ptr;
    }

    result = reset_config();

    if (result == false) {
      ESP_LOGE(TAG, "Error resetting config, unable to start confrm");
      return false;
    }

    return true;
  }

  /*
   * Code below this point is 'normal' flow, whereby the esp API is used to
   * open a file and store the settings.
   */

  // Attempt to start FS, if it does not start then try formatting it
#if defined(ARDUINO_ARCH_ESP32)
  if (!SPIFFS.begin(true)) {
    ESP_LOGD(TAG, "Failed to init SPIFFS, confrm will not work");
    return false;
  }
#elif defined(ARDUINO_ARCH_ESP8266)
  if (!CONFRM_ESP8266_FS.begin()) {
    ESP_LOGE(TAG, "Error mounting file system, confrm will not work");
    return false;
  }
#endif

  if (reset) {
    ESP_LOGD(TAG, "Resetting config");
    reset_config();
  }

  ESP_LOGD(TAG, "Getting confrm config");

#if defined(ARDUINO_ARCH_ESP32)
  File file = SPIFFS.open(m_config_file.c_str());
#elif defined(ARDUINO_ARCH_ESP8266)
  File file = CONFRM_ESP8266_FS.open(m_config_file.c_str(), "r");
#endif

  if (!file || file.isDirectory()) {
    ESP_LOGD(TAG, "Failed to open file for reading, creating default config");
    config_s config;
    memset(config.current_version, 0, 32);
    if (save_config(config)) {
#if defined(ARDUINO_ARCH_ESP32)
      file = SPIFFS.open(m_config_file.c_str());
#elif defined(ARDUINO_ARCH_ESP8266)
      file = CONFRM_ESP8266_FS.open(m_config_file.c_str(), "r");
#endif
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
    bool result;
    ESP_LOGD(TAG, "Config version %d", version);
    switch (version) {
    case 1:
      uint8_t length;
      file.read(&length, 1);
      if (length != sizeof(config_s)) {
        ESP_LOGE(TAG, "Size of config record (%d) does not match size of "
                      "config structure (%d), unable to init config. "
                      "Reinitialising", length, sizeof(config_s));
        file.close();
        result = reset_config();
        if (result == false) {
          ESP_LOGE(TAG, "Error resetting config, unable to start confrm");
          return false;
        }
      } else {
        file.read((uint8_t *)&m_config, sizeof(config_s));
        m_config.current_version[31] = '\0';
        ESP_LOGD(TAG, "confrm config is:");
        ESP_LOGD(TAG, "\tcurrent_version: \"%s\"", m_config.current_version);
      }
      break;

    default:
      ESP_LOGE(TAG, "Unknown config version");
      file.close();
      return false;
      break;
    }
  } else {
    ESP_LOGD(TAG, "Config file was empty, populating with empty config");
    file.close();
    return reset_config();
  }

  ESP_LOGD(TAG, "Config loaded");
  return true;
}

bool Confrm::reset_config() {
  config_s config;
  memset(config.current_version, 0, 32);
  return save_config(config);
}

bool Confrm::save_config(config_s config) {

  // To ensure string is correctly terminated
  config.current_version[31] = '\0';
  
  // Check for storage overflow before assigning memory - should be an assert but not supported
  size_t config_len = sizeof(m_config);
  if (config_len > 0xFF) {
    ESP_LOGE(TAG, "Config struct is larger than 0xFF bytes long");
    return false;
  }

  if (m_config_storage_override) {
    // Version 1 (type, len, data)
    uint8_t *save_data = new uint8_t[sizeof(m_config) + 2];
    save_data[0] = 0x01; // Version #
    save_data[1] = (uint8_t)config_len;
    memcpy(reinterpret_cast<void *>(save_data + 2), reinterpret_cast<void*>(&m_config), sizeof(m_config));
    bool result = m_config_storage_save(save_data, sizeof(save_data));
    delete [] save_data;
  }

#if defined(ARDUINO_ARCH_ESP32)
  File file = SPIFFS.open(m_config_file.c_str());
#elif defined(ARDUINO_ARCH_ESP8266)
  File file = CONFRM_ESP8266_FS.open(m_config_file.c_str(), "w");
#endif
  if (!file) {
    ESP_LOGD(TAG, "Unable to create file");
    return false;
  }

  file.write(m_config_version);
  file.write((uint8_t)config_len);
  file.write((uint8_t *)config.current_version, 32);

  file.close();

  return true;
}

void Confrm::timer_start() {
  if (m_update_period > 2) {
#if defined(ARDUINO_ARCH_ESP32)
    esp_timer_start_periodic(m_timer, m_update_period * 1000 * 1000ULL);
#elif defined(ARDUINO_ARCH_ESP8266)
    ESP_LOGI(TAG, "Timer not supported for esp8266, please use yield");
#endif
  }
}

void Confrm::timer_stop() {
  if (m_update_period > 2) {
#if defined(ARDUINO_ARCH_ESP32)
    esp_timer_stop(m_timer);
#elif defined(ARDUINO_ARCH_ESP8266)
    ESP_LOGI(TAG, "Timer not supported for esp8266, please use yield");
#endif
  }
}

void Confrm::timer_callback(void *ptr) {
  Confrm *self = reinterpret_cast<Confrm *>(ptr);
  self->yield_do(ptr);
}

void Confrm::yield() {
  uint32_t current_time = millis() / 1000;
  if (m_last_yield_time > current_time) {
    m_last_yield_time = current_time;
  } else if (current_time - m_last_yield_time >= m_update_period) {
    yield_do(reinterpret_cast<void *>(this));
    m_last_yield_time = current_time;
  }
}

void Confrm::yield_do(void *ptr) {
  Confrm *self = reinterpret_cast<Confrm *>(ptr);
#if defined(ARDUINO_ARCH_ESP32)
  std::lock_guard<std::mutex> guard(self->m_mutex);
  self->timer_stop();
#endif
  self->register_node();
  if (self->check_for_updates()) {
    ESP_LOGD(TAG, "Rebooting from timer_callback");
    self->hard_restart(); // The ESP32 does not like updating from the timer
                          // callback
  }
#if defined(ARDUINO_ARCH_ESP32)
  self->timer_start();
#endif
}

void Confrm::set_time() {
  String request = m_confrm_url + "/time/";
  int httpCode = 0;
  String response = short_rest(request, httpCode, "GET");
  if (httpCode == 200 && response != "" && response != "{}") {
    std::vector<SimpleJSONElement> content;
#if defined(CONFRM_TRY_CATCH)
    try {
      content = simple_json(response);
    } catch (...) {
      ESP_LOGI(TAG, "Error parsing json");
      return;
    }
#else
    content = simple_json(response);
    if (content.size() == 0) {
      ESP_LOGI(
          TAG,
          "There may have been an error parsing the JSON, or it was empty");
    }
#endif
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
#if defined(ARDUINO_ARCH_ESP32)
  // Bit hacky, use watchdog timer to force a restart. The ESP_restart()
  // method does not reboot everything and was leading to instability
  esp_task_wdt_init(1, true);
  esp_task_wdt_add(NULL);
#elif defined(ARDUINO_ARCH_ESP8266)
  system_restart();
#endif
  while (true)
    ;
}

const String Confrm::get_config(String name) {
  int httpCode = 0;
#if defined(ARDUINO_ARCH_ESP32)
  std::lock_guard<std::mutex> guard(m_mutex);
#endif
  String request = m_confrm_url + "/config/" + "?package=" + m_package_name +
                   "&node_id=" + WiFi.macAddress() + "&key=" + name;
  String response = short_rest(request, httpCode, "GET");
  if (httpCode == 200) {
    std::vector<SimpleJSONElement> content;
#if defined(CONFRM_TRY_CATCH)
    try {
      content = simple_json(response);
    } catch (...) {
      ESP_LOGI(TAG, "Error parsing json");
      return "";
    }
#else
    content = simple_json(response);
    if (content.size() == 0) {
      ESP_LOGI(
          TAG,
          "There may have been an error parsing the JSON, or it was empty");
      return "";
    }
#endif
    return get_simple_json_string(content, "value");
  }
  return "";
}

Confrm::Confrm(String package_name, String confrm_url, String node_description,
               String node_platform, int32_t update_period,
               const bool reset_configuration) {

#if defined(ARDUINO_ARCH_ESP32)
  std::lock_guard<std::mutex> guard(m_mutex);
  const esp_partition_t *current = esp_ota_get_running_partition();
  ESP_LOGD(TAG, "Booted to %d", current->address);
#endif

  m_package_name = package_name;
  m_confrm_url = confrm_url;
  m_node_description = simple_url_encode(node_description);
  m_node_platform = node_platform;

  m_config_status = init_config(reset_configuration);
  if (!m_config_status) {
    ESP_LOGE(TAG, "Failed to load config");
  }

  // Register the node first, before checking for updates
  register_node();

  if (check_for_updates()) {
    do_update();
    register_node();
  }

  m_update_period = update_period;
  if (m_update_period > 2) {
#if defined(ARDUINO_ARCH_ESP32)
    esp_timer_create_args_t timer_config;
    timer_config.arg = reinterpret_cast<void *>(this);
    timer_config.callback = Confrm::timer_callback;
    timer_config.dispatch_method = ESP_TIMER_TASK;
    timer_config.name = "Confrm update timer";
    esp_timer_create(&timer_config, &m_timer);
    timer_start();
#endif
  }
}

Confrm::Confrm(String package_name, String confrm_url,
               size_t (*load_config)(uint8_t **),
               bool (*save_config)(uint8_t *, size_t), String node_description,
               String node_platform, int32_t update_period,
               const bool reset_config) {

  m_config_storage_override = true;
  m_config_storage_load = load_config;
  m_config_storage_save = save_config;

  Confrm(package_name, confrm_url, node_description, node_platform,
         update_period, reset_config);
}
