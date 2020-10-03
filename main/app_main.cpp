#include <string.h>
#include <stdlib.h>

#include "config_user.h"

#include "MainClass.h"


extern "C" void app_main() {
	MainClass::instance()->setup();
	MainClass::instance()->loop();
}


