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

    const String m_config_file = "/confrm.config";

    const uint8_t m_config_version = 1;
    struct config_s {
      char current_version[32];
      char rollback_version[32];
      char update_in_progress;
    };

    config_s m_config;
    bool m_config_status = false;

    bool init_config(const bool reset_config = false);
    bool save_config(config_s);

    String m_package_name;
    String m_confrm_url;

    bool check_for_updates(void);

    String m_next_version;
    String m_next_blob;

    bool do_update(void);

    esp_timer_handle_t m_timer;
    static void timer_callback(void *ptr);
};

#endif
