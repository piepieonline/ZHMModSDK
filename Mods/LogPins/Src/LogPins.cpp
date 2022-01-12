#include "LogPins.h"

#include "Hooks.h"
#include "Logging.h"
#include "Functions.h"

#include <Glacier/ZScene.h>
#include <Glacier/ZEntity.h>
#include <Glacier/ZObject.h>


#define SSTR( x ) static_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()

void LogPins::PreInit()
{
	Hooks::SignalInputPin->AddDetour(this, &LogPins::SignalInputPin);
	Hooks::SignalOutputPin->AddDetour(this, &LogPins::SignalOutputPin);
}

void LogPins::DumpDetails(ZEntityRef entityRef, uint32_t pinId, const ZObjectRef& objectRef)
{
	auto pInterface = (*(*entityRef.m_pEntity)->m_pInterfaces)[0];

	if (!pInterface.m_pTypeId ||
		!pInterface.m_pTypeId->m_pType ||
		!pInterface.m_pTypeId->m_pType->m_pTypeName
		) {
		Logger::Info("Pin entity class: UNKNOWN");
	}
	else
	{
		Logger::Info("Pin entity class: {}", pInterface.m_pTypeId->m_pType->m_pTypeName);
	}

	if (objectRef.IsEmpty())
	{
		Logger::Info("Parameter type: None");
	}
	else
	{
		Logger::Info("Parameter type: {}", objectRef.m_pTypeID->m_pType->m_pTypeName);
	}

	std::ostringstream ss;

	auto& s_Properties1 = (*entityRef.m_pEntity)->m_pProperties01;
	auto& s_Properties2 = (*entityRef.m_pEntity)->m_pProperties02;

	if (!s_Properties1)
	{
		ss << "";
	}
	else
	{
		for (auto& s_Property : *s_Properties1)
		{
			try
			{
				ss << " " << s_Property.m_pType->getPropertyInfo()->m_pName;
			}
			catch (...)
			{
			}
		}
	}

	ss << " | ";

	if (!s_Properties2)
	{
		ss << "";
	}
	else
	{
		for (auto& s_Property : *s_Properties2)
		{
			try
			{
				ss << " " << s_Property.m_pType->getPropertyInfo()->m_pName;
			}
			catch (...)
			{
			}
		}
	}

	Logger::Info("Entity props:{}", ss.str());
}

DECLARE_PLUGIN_DETOUR(LogPins, bool, SignalInputPin, ZEntityRef entityRef, uint32_t pinId, const ZObjectRef& objectRef)
{
	std::ostringstream ss;
	ss << "" << pinId << "_" << (*entityRef.m_pEntity)->m_nEntityId;
	std::string s = ss.str();

	auto it = m_knownInputs.find(s);
	if (it == m_knownInputs.end())
	{
		Logger::Info("Pin Input: {} on {}", pinId, (*entityRef.m_pEntity)->m_nEntityId);

		DumpDetails(entityRef, pinId, objectRef);
		
		m_knownInputs[s] = true;
	}

	return HookResult<bool>(HookAction::Continue());
}

DECLARE_PLUGIN_DETOUR(LogPins, bool, SignalOutputPin, ZEntityRef entityRef, uint32_t pinId, const ZObjectRef& objectRef)
{
	std::ostringstream ss;
	ss << "" << pinId << "_" << (*entityRef.m_pEntity)->m_nEntityId;
	std::string s = ss.str();

	auto it = m_knownOutputs.find(s);
	if (it == m_knownOutputs.end())
	{
		Logger::Info("Pin Output: {} on {}", pinId, (*entityRef.m_pEntity)->m_nEntityId);

		DumpDetails(entityRef, pinId, objectRef);

		m_knownOutputs[s] = true;
	}

	return HookResult<bool>(HookAction::Continue());
}

DECLARE_ZHM_PLUGIN(LogPins);
