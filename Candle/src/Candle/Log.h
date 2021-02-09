#pragma once

#include "Core.h"
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

namespace Candle {

	class CANDLE_API Log
	{
	public:
		static void Init();

		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_coreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_clientLogger; }

	private:
		static std::shared_ptr<spdlog::logger> s_clientLogger;
		static std::shared_ptr<spdlog::logger> s_coreLogger;
	};

}

// Core log macros
#define CANDLE_CORE_TRACE(...)		::Candle::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define CANDLE_CORE_INFO(...)		::Candle::Log::GetCoreLogger()->info(__VA_ARGS__)
#define CANDLE_CORE_WARN(...)		::Candle::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define CANDLE_CORE_ERROR(...)		::Candle::Log::GetCoreLogger()->error(__VA_ARGS__)
#define CANDLE_CORE_CRITICAL(...)	::Candle::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Client log macros
#define CANDLE_TRACE(...)		::Candle::Log::GetClientLogger()->trace(__VA_ARGS__)
#define CANDLE_INFO(...)		::Candle::Log::GetClientLogger()->info(__VA_ARGS__)
#define CANDLE_WARN(...)		::Candle::Log::GetClientLogger()->warn(__VA_ARGS__)
#define CANDLE_ERROR(...)		::Candle::Log::GetClientLogger()->error(__VA_ARGS__)
#define CANDLE_CRITICAL(...)	::Candle::Log::GetClientLogger()->critical(__VA_ARGS__)