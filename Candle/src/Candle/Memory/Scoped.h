#pragma once

#include <memory>

namespace Candle {

	// TODO: Add custom implemantion
	template<typename T>
	using Scoped = std::unique_ptr<T>;
}