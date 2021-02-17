#pragma once

#include "Candle/Core.h"

namespace Candle {

	class CANDLE_API VertexBuffer
	{
	public:
		virtual ~VertexBuffer() { }

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		static VertexBuffer* Create(float* vertices, size_t size);
	};


	class CANDLE_API IndexBuffer
	{
	public:
		virtual ~IndexBuffer() { }

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual uint32_t GetCount() const = 0;

		static IndexBuffer* Create(uint32_t* indices, uint32_t count);
	};
}