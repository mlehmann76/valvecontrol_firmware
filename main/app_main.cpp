#include <stdlib.h>
#include <string.h>

#include "config.h"

#include "MainClass.h"

extern "C" void app_main() {
    MainClass::instance()->setup();
    MainClass::instance()->loop();
}
