#pragma once

#include "Candle/Renderer/GraphicsContext.h"

struct GLFWwindow;

namespace Candle {
    class OpenGLContext : public GraphicsContext
    {
    public:
        OpenGLContext(GLFWwindow* windowHandle);

        void Init() override;
        void SwapBuffer() override;

    private:
        GLFWwindow* _windowHandle;
    };
}

