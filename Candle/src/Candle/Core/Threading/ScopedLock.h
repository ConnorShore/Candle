#pragma once

namespace Candle {

	template<typename LockType>
	class ScopedLock
	{
	public:
		ScopedLock(LockType& lock) : m_Lock(lock)
		{
			m_Lock.Aquire();
		}
		ScopedLock(ScopedLock&& other) noexcept : m_Lock(other.m_Lock)
		{
			other.m_Lock.Release();
		}

		~ScopedLock()
		{
			m_Lock.Release();
		}

		// Delete copy constructor and assignment operator to prevent copying
		ScopedLock(const ScopedLock&) = delete;
		ScopedLock& operator=(const ScopedLock&) = delete;

	private:
		LockType& m_Lock;
	};

}