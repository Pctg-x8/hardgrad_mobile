#include "RenderEngine.h"

void handle_command(android_app* ap, int32_t cmd)
{
    switch(cmd)
    {
    case APP_CMD_INIT_WINDOW:
        // Application Start
        __android_log_print(ANDROID_LOG_INFO, "HardGrad", "Starting App...");
        RenderEngine::instance.init(ap);
        break;
    case APP_CMD_TERM_WINDOW:
        // Application Closed
        break;
    }
}
void android_main(android_app* app)
{
    app_dummy();
    app->onAppCmd = &handle_command;

    if(!Vulkan::load_functions())
    {
        __android_log_print(ANDROID_LOG_FATAL, "HardGrad::Vulkan", "Vulkan Library loading failed.");
        return;
    }

    while(!app->destroyRequested)
    {
        int events;
        android_poll_source* source;
        if(ALooper_pollAll(0, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0)
        {
            if(source != nullptr) source->process(app, source);
        }

        if(RenderEngine::instance.readyForRender)
        {
            RenderEngine::instance.update();
            RenderEngine::instance.render_frame();
        }
    }
    return;
}

