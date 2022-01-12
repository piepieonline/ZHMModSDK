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
}

DECLARE_PLUGIN_DETOUR(LogPins, bool, SignalInputPin, ZEntityRef entityRef, uint32_t pinId, const ZObjectRef& objectRef)
{
	std::ostringstream ss;
	ss << "" << pinId << "_" << (*entityRef.m_pEntity)->m_nEntityId;
	std::string s = ss.str();

	auto it = m_knownInputs.find(s);
	// if (it == m_knownInputs.end())
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
	// if (it == m_knownOutputs.end())
	{
		Logger::Info("Pin Output: {} on {}", pinId, (*entityRef.m_pEntity)->m_nEntityId);

		DumpDetails(entityRef, pinId, objectRef);

		m_knownOutputs[s] = true;
	}

	return HookResult<bool>(HookAction::Continue());
}

DECLARE_ZHM_PLUGIN(LogPins);
