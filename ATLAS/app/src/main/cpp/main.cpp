#include <android_native_app_glue.h>

void android_main(android_app* app) {
    while (!app->destroyRequested) {
        int events;
        android_poll_source* source;
        int result = ALooper_pollOnce(-1, nullptr, &events,
                                      reinterpret_cast<void**>(&source));
        if (result >= 0 && source != nullptr) {
            source->process(app, source);
        }
    }
}
