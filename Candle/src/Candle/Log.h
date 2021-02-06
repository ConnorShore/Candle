#pragma once

#include <memory>

#include "Core.h"
#include "spdlog/spdlog.h"

namespace Candle {

	class CANDLE_API Log
	{
	public:
		static void Init();

		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

	private:
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
	};

}

// Core log macros
#define CANDLE_CORE_TRACE(...)		::Candle::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define CANDLE_CORE_INFO(...)		::Candle::Log::GetCoreLogger()->info(__VA_ARGS__)
#define CANDLE_CORE_WARN(...)		::Candle::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define CANDLE_CORE_ERROR(...)		::Candle::Log::GetCoreLogger()->error(__VA_ARGS__)
#define CANDLE_CORE_FATAL(...)		::Candle::Log::GetCoreLogger()->fatal(__VA_ARGS__)

// Client log macros
#define CANDLE_TRACE(...)	::Candle::Log::GetClientLogger()->trace(__VA_ARGS__)
#define CANDLE_INFO(...)		::Candle::Log::GetClientLogger()->info(__VA_ARGS__)
#define CANDLE_WARN(...)		::Candle::Log::GetClientLogger()->warn(__VA_ARGS__)
#define CANDLE_ERROR(...)	::Candle::Log::GetClientLogger()->error(__VA_ARGS__)
#define CANDLE_FATAL(...)	::Candle::Log::GetClientLogger()->fatal(__VA_ARGS__)