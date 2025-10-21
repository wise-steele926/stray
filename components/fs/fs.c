#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include "fs.h"

#include "cJSON.h"

static const char *TAG = "spiffs";


struct init_config_struct {
	uint8_t volume;
	uint8_t brt_lvl;
	uint8_t valcode;	
	
	char SSID[32];
	char PSK[32];
	

};


void init_spiffs (void){

    ESP_LOGD(TAG, "Initializing SPIFFS");

    static esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_spiffs_format(conf.partition_label);
        return;
    } else {
        ESP_LOGD(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    // Check consistency of reported partition size info.
    if (used > total) {
        ESP_LOGW(TAG, "Number of used bytes cannot be larger than total. Performing SPIFFS_check().");
        ret = esp_spiffs_check(conf.partition_label);
        // Could be also used to mend broken files, to clean unreferenced pages, etc.
        // More info at https://github.com/pellepl/spiffs/wiki/FAQ#powerlosses-contd-when-should-i-run-spiffs_check
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
            return;
        } else {
            ESP_LOGD(TAG, "SPIFFS_check() successful");
        }
    }

    ESP_LOGD(TAG, "loadConfiguration");
    loadConf();
   
}

void loadConf(void) { 

    struct init_config_struct config;
	const char *filename = "/spiffs/settings.json";   
	FILE* file = fopen(filename, "r"); //File file = SPIFFS.open(filename, "r");
	if (!file) {
		ESP_LOGE(TAG,"Failed to open file");
		return;
	}

    // read the file contents into a string
    char buffer[1024];
    int len = fread(buffer, 1, sizeof(buffer), file);
    fclose(file);


    ESP_LOGD(TAG, "Deserialize.....");
	cJSON *root2 = cJSON_Parse(buffer);

	if (cJSON_GetObjectItem(root2, "volume")) {
		int volume = cJSON_GetObjectItem(root2,"volume")->valueint;
		ESP_LOGD(TAG, "volume=%d",volume);
	}
	if (cJSON_GetObjectItem(root2, "valcode")) {
		int valcode = cJSON_GetObjectItem(root2,"valcode")->valueint;
		ESP_LOGD(TAG, "valcode=%d",valcode);
	}

    cJSON *name = cJSON_GetObjectItemCaseSensitive(root2, "name");
    if (cJSON_IsString(name) && (name->valuestring != NULL)) {
        ESP_LOGD(TAG,"Name: %s\n", name->valuestring);
    }

    cJSON *brightness = cJSON_GetObjectItem(root2,"brightness");
    if (cJSON_GetObjectItem(brightness,"level")){
        int level = cJSON_GetObjectItem(brightness,"level")->valueint;
        ESP_LOGD(TAG, "level=%d",level);
    }
	
	cJSON_Delete(root2);
    // cJSON_Delete(brightness);
}
