#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

#include "esp_err.h"
#include "esp_log.h"



void init_spiffs (void);
void loadConf(void);
