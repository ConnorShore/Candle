#pragma once

#include <cstdlib>

namespace Ember {

	class Buffer
	{
	public:
		Buffer() = default;
		Buffer(void* data, size_t size) : m_Data(data), m_Size(size) {}

		Buffer(Buffer&& other) noexcept : m_Data(other.m_Data), m_Size(other.m_Size)
		{
			other.m_Data = nullptr;
			other.m_Size = 0;
		}
		Buffer(const Buffer&) = delete;

		inline void Allocate(size_t size)
		{
			m_Data = malloc(size);
			m_Size = size;
		}

		inline void* Data() const { return m_Data; }
		inline size_t Size() const { return m_Size; }

	private:
		void* m_Data;
		size_t m_Size;
	};

}