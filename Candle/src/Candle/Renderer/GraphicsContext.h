#pragma once

#include "Candle/Core.h"

namespace Candle {
	class CANDLE_API GraphicsContext
	{
	public:
		virtual void Init() = 0;
		virtual void SwapBuffer() = 0;
	};
}