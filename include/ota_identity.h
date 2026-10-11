#pragma once
#if SI_DRIVER == 15
#define OTA_PROJECT_NAME "jcalendar-plus-z15"
#elif SI_DRIVER == 21
#define OTA_PROJECT_NAME "jcalendar-plus-z21"
#elif SI_DRIVER == 98
#define OTA_PROJECT_NAME "jcalendar-plus-z98"
#elif SI_DRIVER == 1680
#define OTA_PROJECT_NAME "jcalendar-plus-1680"
#else
#error Unsupported OTA screen driver
#endif
