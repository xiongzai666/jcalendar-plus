#include <stdint.h>
#include "ota_identity.h"
#include "version.h"

// IDF places this immediately after its standard 256-byte app descriptor.
// The linker -u flag retains it without replacing the SDK's strong symbol.
struct JCalendarOtaDesc { uint32_t magic;char project[32];char version[32]; };
static_assert(sizeof(JCalendarOtaDesc)==68,"OTA descriptor layout changed");
extern "C" const JCalendarOtaDesc jcalendar_ota_desc
    __attribute__((section(".rodata_custom_desc"),used)) = {
  0x41544f4a, OTA_PROJECT_NAME, J_VERSION
};
