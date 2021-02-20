#pragma once

#include "Candle/Core.h"

namespace Candle {

	class CANDLE_API Timestep
	{
	public:
		Timestep(float time = 0.0f) 
			: _time(time) { }

		float GetSeconds() const { return _time; }
		float GetMilliseconds() const { return _time * 1000; }

		operator float() const { return _time; } // Makes it so we can use class as a float

	private:
		float _time;
	};
}