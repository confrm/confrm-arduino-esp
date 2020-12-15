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
#include "esp_timer.h" // esp_timer_handle_t definition

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
     * @param package_name  Name of the package registered with the confrm 
     *                      server
     * @param url           Fully qualified URL to the root of the confrm 
     *                      server, e.g. http://192.168.0.42:8080
     * @param update_period Period in seconds for querying the server for
     *                      updates, minimum 2 seconds, maximum xx. Set to
     *                      -1 to disable.
     */
    Confrm(
        String package_name,
        String confrm_url,
        int32_t update_period = -1,
        bool reset_config = false
    );

  private:

    /**
     * Config file stored in non-volatile partition
     */
    const String m_config_file = "/confrm.config";

    /**
     * Currently supported configuration version, stored along with config.
     * If config version changes, the class should include the ability to
     * update the config to the newer version from any older version.
     */
    const uint8_t m_config_version = 1;

    /**
     * Configuration struct, data is read from the non-volatile partition in 
     * to this format.
     */
    struct config_s {
      char current_version[32];
    };
    config_s m_config;
    bool m_config_status = false;

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
     * Package name is the unique key for each node type
     */
    String m_package_name;

    /**
     * @brief URL of the confrm server, including http(s)://
     */
    String m_confrm_url;

    /**
     * @brief Obtain result for short REST API calls
     *
     * Obtains response from server for given API call, uses full URL. If the
     * response is longer than the heap allocation the method will return an
     * empty string.
     * 
     * @param url   URL of REST GET call
     * @return      Response as string, or empty string on error
     */
    String get_short_rest(String url);

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
    String m_next_blob;

    /**
     * @brief Action the required update
     *
     * Does the update by calling the confrm server REST API to download the
     * binary blob for this node.
     *
     * @return Fale if update fails
     */
    bool do_update(void);

    /**
     * Update timer period in seconds
     */
    int m_update_period;

    /**
     * Handle to configured timer object, used for starting and stopping timer
     */
    esp_timer_handle_t m_timer;

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
     * @brief Force hard restart of device
     */ 
    void hard_restart(void);
};

#endif
