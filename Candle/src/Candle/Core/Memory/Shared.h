#pragma once

#include <memory>

namespace Candle {

	// TODO: Create custom implementation
	template<typename T>
	using Shared = std::shared_ptr<T>;

}