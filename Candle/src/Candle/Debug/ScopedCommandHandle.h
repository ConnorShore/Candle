#pragma once

#include "ConsoleTypes.h"

namespace Candle {

	class ConsoleManager;

	// Owns one registration and unregisters it on destruction. ConsoleManager must outlive it.
	// Threading: main thread only
	class ScopedCommandHandle
	{
	public:
		explicit ScopedCommandHandle(ConsoleManager* manager, ConsoleCommandHandle handle);
		~ScopedCommandHandle();

		// Remove copy constructor and assignment
		ScopedCommandHandle(ScopedCommandHandle&) = delete;
		ScopedCommandHandle& operator=(ScopedCommandHandle&) = delete;

		// Move constructor and assignment
		ScopedCommandHandle(ScopedCommandHandle&& other) noexcept
			: m_Manager(other.m_Manager), m_Handle(other.m_Handle)
		{
			other.m_Handle = kInvalidCommandHandle;
			other.m_Manager = nullptr;
		}
		ScopedCommandHandle& operator=(ScopedCommandHandle&& other) noexcept
		{
			if (this == &other)
				return *this;

			Reset();	// Unregisters the command this handle already owns
			m_Handle = other.m_Handle;
			m_Manager = other.m_Manager;

			other.m_Handle = kInvalidCommandHandle;
			other.m_Manager = nullptr;
			return *this;
		}

		[[nodiscard]] ConsoleCommandHandle GetHandle() const { return m_Handle; }

	private:
		friend class ConsoleManager;

		void Reset();

	private:
		ConsoleManager* m_Manager;
		ConsoleCommandHandle m_Handle;
	};
}