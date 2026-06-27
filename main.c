#include <android_native_app_glue.h>
#include "engine.h"

void android_main(struct android_app* state) {
    engine_run(state);
}
