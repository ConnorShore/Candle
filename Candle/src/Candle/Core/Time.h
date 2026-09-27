#pragma once

#include <chrono>

#include "Logger.h"

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
		Timer() = default;
		~Timer() = default;

		inline void Start()
		{
			m_StartTime = std::chrono::high_resolution_clock::now();
		}

		inline double ElapsedSeconds() const
		{
			auto endTime = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> elapsed = endTime - m_StartTime;
			return elapsed.count();
		}

		inline double ElapsedMilliseconds() const
		{
			auto endTime = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double, std::milli> elapsed = endTime - m_StartTime;
			return elapsed.count();
		}

		inline void Reset()
		{
			Start();
		}

	private:
		std::chrono::high_resolution_clock::time_point m_StartTime;
	};

	class ScopedTimer
	{
	public:
		ScopedTimer(const std::string& name, LogChannel channel = LogChannel::Application)
			: m_Name(name), m_Channel(channel)
		{
			m_Timer.Start();
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