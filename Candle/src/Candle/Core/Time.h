#pragma once

#include <chrono>
#include <string>

#include "Logger.h"
#include "Candle/Platform/Platform.h"

namespace Candle {

	class TimeStep
	{
	public:
		TimeStep(float time = 0.0f)
			: m_Time(time) {
		}
		~TimeStep() = default;

		inline float GetSeconds() const { return m_Time; }
		inline float GetMilliseconds() const { return m_Time * 1000.0f; }

		inline operator float() const { return m_Time; }

	private:
		float m_Time;
	};

	class Timer
	{
	public:
		Timer() { Reset(); }
		~Timer() = default;

		inline double ElapsedSeconds() const
		{
			return Platform::ToSeconds(m_StartTick, Platform::GetTick());
		}

		inline double ElapsedMilliseconds() const
		{
			return Platform::ToMilliseconds(m_StartTick, Platform::GetTick());
		}

		inline void Reset()
		{
			m_StartTick = Platform::GetTick();
		}

	private:
		Tick m_StartTick;
	};

	class ScopedTimer
	{
	public:
		ScopedTimer(const std::string& name, LogChannel channel = LogChannel::Application)
			: m_Name(name), m_Channel(channel)
		{
			m_Timer.Reset();
		}

		~ScopedTimer()
		{
			double elapsed = m_Timer.ElapsedMilliseconds();
			CDL_CORE_INFO(m_Channel, "ScopedTimer [{}] elapsed time: {} ms", m_Name, elapsed);
		}

	private:
		std::string m_Name;
		LogChannel m_Channel;
		Timer m_Timer;
	};

}