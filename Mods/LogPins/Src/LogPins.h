#pragma once

#include <random>
#include <unordered_map>

#include "IPluginInterface.h"

class LogPins : public IPluginInterface
{
public:
	void PreInit() override;

private:
	DEFINE_PLUGIN_DETOUR(LogPins, bool, SignalInputPin, ZEntityRef, uint32_t, const ZObjectRef&);
	DEFINE_PLUGIN_DETOUR(LogPins, bool, SignalOutputPin, ZEntityRef, uint32_t, const ZObjectRef&);
	
private:
	std::unordered_map<uint32_t, bool> m_knownInputPins;
	std::unordered_map<uint64_t, bool> m_knownInputEntities;
	std::unordered_map<uint32_t, bool> m_knownOutputPins;
	std::unordered_map<uint64_t, bool> m_knownOutputEntities;
};

DEFINE_ZHM_PLUGIN(LogPins)
