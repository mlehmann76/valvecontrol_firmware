#include <string.h>
#include <stdlib.h>

#include "config_user.h"

#include "MainClass.h"


#ifdef __cplusplus
extern "C" {
#endif
void app_main();
#ifdef __cplusplus
}
#endif

void app_main() {
	MainClass::instance()->setup();
	MainClass::instance()->loop();
}


