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
		if (m_Manager)
			m_Manager->UnregisterCommand(m_Handle);
	}

}