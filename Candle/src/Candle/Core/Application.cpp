#include "cdlpch.h"
#include "Application.h"

#include <glm/glm.hpp>
#include <SDL3/SDL.h>

namespace Candle
{
    void Application::HelloWorld()
    {
        if (!SDL_Init(SDL_INIT_VIDEO))
        {
			std::cout << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
            return;
        }

		glm::vec3 test(1.0f, 2.0f, 3.0f);
        std::cout << "Hello, World: " << test.x + test.y + test.z << std::endl;
    }

}
