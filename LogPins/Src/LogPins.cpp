#include "LogPins.h"

#include "Hooks.h"
#include "Logging.h"
#include "Functions.h"

void LogPins::PreInit()
{
	Hooks::SignalInputPin->AddDetour(this, &LogPins::SignalInputPin);
	Hooks::SignalOutputPin->AddDetour(this, &LogPins::SignalOutputPin);
}

DECLARE_PLUGIN_DETOUR(LogPins, bool, SignalInputPin, ZEntityRef zEntityRef, uint32_t pinId, const ZObjectRef& zObjectRef)
{
	Logger::Debug("Pin Input: {}", pinId);

	return HookResult<bool>(HookAction::Continue(), true);
}

DECLARE_PLUGIN_DETOUR(LogPins, bool, SignalOutputPin, ZEntityRef zEntityRef, uint32_t pinId, const ZObjectRef& zObjectRef)
{
	Logger::Debug("Pin Output: {}", pinId);

	return HookResult<bool>(HookAction::Continue(), true);
}

DECLARE_ZHM_PLUGIN(LogPins);
