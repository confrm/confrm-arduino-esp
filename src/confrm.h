/** @file
 * Main confrm class definition
 *
 *  Copyright 2020 confrm.io
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _CONFRM_H_
#define _CONFRM_H_

#include <unistd.h> // uintXX_t definition

#include <Arduino.h> // String type

#if defined(ARDUINO_ARCH_ESP32)
#include "esp_timer.h" // esp_timer_handle_t definition
#include <mutex>
#define CONFRM_PLATFORM "esp32"
#elif defined(ARDUINO_ARCH_ESP8266)
#include "Ticker.h"
#define CONFRM_PLATFORM "esp8266"
#endif

class Confrm {

public:
  /**
   * @brief Main confrm class
   *
   * Constructor will check for settings file on persistent storage, then
   * check for updates and do an update if required.
   *
   * If the update_period is set then a callback is added to the esp32 timer
   * and the confrm server polled periodically for updates.
   *
   * @param package_name      Name of the package registered with the confrm
   *                          server
   * @param url               Fully qualified URL to the root of the confrm
   *                          server, e.g. http://192.168.0.42:8080
   * @param node_description  General description of node ("light sensor")
   * @param node_platform     Platform of this node, i.e. esp32
   * @param update_period     Period in seconds for querying the server for
   *                          updates, minimum 2 seconds, maximum xx. Set to
   *                          -1 to disable.
   * @param reset_config      If true all config will be reset
   */
  Confrm(String package_name, String confrm_url, String node_description = "",
         String node_platform = CONFRM_PLATFORM, int32_t update_period = 60,
         bool reset_configuration = false);

  /**
   * @brief Main confrm class with config overrides
   *
   * Constructor will use callbacks to check retrieve settings from persistent
   * storage, then check for updates and do an update if required.
   *
   * The persistent storage is configured by the user, and set as a save and
   * load method using function pointers.
   *
   * The user callback is required to understand the header structure of the
   * data being loaded, which is a version byte followed by a length byte
   * followed by a data field. The length byte indicates the length of the data
   * field. So a data field containing 0xAA, 0xBB using version 3 would be
   * encoded:
   *
   *   0x03, 0x02, 0xAA, 0xBB
   *
   * Confrm will handle conversion between version numbers where possible.
   *
   * If the update_period is set then a callback is added to the esp32 timer
   * and the confrm server polled periodically for updates.
   *
   * @param package_name      Name of the package registered with the confrm
   *                          server
   * @param url               Fully qualified URL to the root of the confrm
   *                          server, e.g. http://192.168.0.42:8080
   * @param load_config       Callback for loading the persistent
   *                          configuration, takes a pointer to a pointer for
   *                          the data, and returns a size_t of the bytes read.
   *                          The calling process will call 'delete' on the
   *                          pointer once the data has been used.
   * @param save_config       Callback for saving the persistent configuration,
   *                          takes a pointer a uint8_t array of to the data
   *                          to be stored and the length as a size_t. Returns
   *                          a boolean if successful.
   * @param node_description  General description of node ("light sensor")
   * @param node_platform     Platform of this node, i.e. esp32
   * @param update_period     Period in seconds for querying the server for
   *                          updates, minimum 2 seconds, maximum xx. Set to
   *                          -1 to disable.
   * @param reset_config      If true all config will be reset
   */
  Confrm(String package_name, String confrm_url,
         size_t (*load_config)(uint8_t **), 
         bool (*save_config)(uint8_t *, size_t),
         String node_description = "",
         String node_platform = CONFRM_PLATFORM, int32_t update_period = 60,
         bool reset_configuration = false);

  /**
   * Queries the confrm server for the given string name
   *
   * @param name    Config name to ask the server for
   * @returns       Empty string if not found, or string result if found
   */
  const String get_config(String name);

  /**
   * Processes the time based updates for confrm.
   *
   * Is primarily used where background timers are not suitable, or where
   * foreground tasks need to take priority over any background activity.
   */
  void yield(void);

  /**
   * Configuration struct, data is read from the non-volatile partition in
   * to this format.
   */
  struct config_s {
    char current_version[32];
  };

  /**
   * Currently supported configuration version, stored along with config.
   * If config version changes, the class should include the ability to
   * update the config to the newer version from any older version.
   *
   * Version 1:
   *
   *  uint8_t config_version
   *  uint8_t sizeof(config_s)
   *  uint8_t * sizeof(config_s)
   *
   */
  const uint8_t m_config_version = 1;

private:
  /**
   * Class access mutex
   */
#if defined(ARDUINO_ARCH_ESP32)
  std::mutex m_mutex;
#endif

  /**
   * Last yield time, used to trigger events at the correct time
   */
  uint32_t m_last_yield_time = 0;

  /**
   * Config file stored in non-volatile partition
   */
  const String m_config_file = "/confrm.config";

  /**
   * Instance of config struct for storing current config
   */
  config_s m_config;
  bool m_config_status = false;

  /**
   * Enables the file storage to be overridden
   */
  bool m_config_storage_override = false;
  bool (*m_config_storage_save)(uint8_t *, size_t);
  size_t (*m_config_storage_load)(uint8_t **);

  /**
   * @brief Called to load in config, if it exists.
   *
   * @param reset_config  If true this will reset the config to be empty
   * @return True if config loaded correctly
   */
  bool init_config(const bool reset_config = false);

  /**
   * @brief Save the config object to the non-volatile storage.
   *
   * @param config Configuration to be saved to non-volatile storage.
   * @return True of config saved correctly
   */
  bool save_config(config_s config);


  /**
   * @brief Resets the configuration to an empty instance of the current 
   * version
   */
  bool reset_config(void);

  /**
   * @brief Package name is the unique key for each node type
   */
  String m_package_name;

  /**
   * @brief URL of the confrm server, including http(s)://
   */
  String m_confrm_url;

  /**
   * @brief Description of this node (i.e. Temperature Sensor)
   */
  String m_node_description;

  /**
   * @brief The platform this node identifies as (i.e. esp32)
   */
  String m_node_platform = CONFRM_PLATFORM;

  /**
   * @brief Obtain result for short REST API calls
   *
   * Obtains response from server for given API call, uses full URL. If the
   * response is longer than the heap allocation the method will return an
   * empty string.
   *
   * @param url       URL of REST GET call
   * @param type      GET/PUT/POST
   * @param payload   PUT/POST content, if required
   * @param httpCode  Will contain the return http code
   * @return      Response as string, or empty string on error
   */
  String short_rest(String url, int &httpCode, String type = "GET",
                    String payload = "");

  /**
   * @brief Initialises REST calls to the confrm server to check for updates
   *
   * @return True if update requried
   */
  bool check_for_updates(void);

  /**
   * If there is an update to be done, the update specifics will be stored
   * here.
   */
  String m_next_version;
  unsigned char m_next_hash[32];
  String m_next_blob;

  /**
   * @brief Action the required update
   *
   * Does the update by calling the confrm server REST API to download the
   * binary blob for this node.
   *
   * @return False if update fails
   */
  bool do_update(void);

  /**
   * Update timer period in seconds
   */
  int m_update_period;

  /**
   * @brief Does the yield work
   */
  void yield_do(void *self);

  /**
   * Handle to configured timer object, used for starting and stopping timer
   */
#if defined(ARDUINO_ARCH_ESP32)
  esp_timer_handle_t m_timer;
#endif

  /**
   * @brief Start the timer
   */
  void timer_start(void);

  /**
   * @brief Stop the timer, must be called before doing update...
   */
  void timer_stop(void);

  /**
   * @brief Timer callback
   *
   * This is static so that the compiler knows where to point the callback to.
   * The method will reinterpret_cast the ptr to a Confrm* in order to access
   * check_for_updates and do_update methods.
   *
   * @param ptr Pointer to 'this'
   */
  static void timer_callback(void *ptr);

  /**
   * @brief Pull time/date from confrm server
   */
  void set_time(void);

  /**
   * @brief Registers node with confrm server
   */
  void register_node(void);

  /**
   * @brief Force hard restart of device
   */
  void hard_restart(void);
};

#endif
