#include "cdlpch.h"
#include "ScopedCommandHandle.h"

#include "ConsoleManager.h"

namespace Candle {

	ScopedCommandHandle::ScopedCommandHandle(ConsoleManager* manager, ConsoleCommandHandle handle)
		: m_Manager(manager), m_Handle(handle)
	{

	}

	ScopedCommandHandle::~ScopedCommandHandle()
	{
		Reset();
	}

	void ScopedCommandHandle::Reset()
	{
		if (m_Manager)
			m_Manager->UnregisterCommand(m_Handle);

		m_Manager = nullptr;
		m_Handle = kInvalidCommandHandle;
	}

}