#pragma once
#include "Arduino.h"
struct esp_partition_t { size_t size; };
inline const esp_partition_t* esp_ota_get_next_update_partition(void*) {
  static esp_partition_t partition={0x1e0000};return &partition;
}
