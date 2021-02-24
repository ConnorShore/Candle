#pragma once

#include "Candle/Core.h"

namespace Candle {

	class CANDLE_API Texture
	{
	public:
		virtual ~Texture() = default;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		
		virtual void Bind(uint32_t slot = 0) const = 0;
	};

	class CANDLE_API Texture2D : public Texture
	{
	public:
		static Ref<Texture2D> Create(const std::string& path);
	};

}