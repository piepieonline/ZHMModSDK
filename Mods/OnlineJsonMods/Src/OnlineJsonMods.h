#pragma once

#include "IPluginInterface.h"
#include "json.hpp"

class OnlineJsonMods : public IPluginInterface
{
public:
	void PreInit() override;
	void OnDrawMenu() override;

private:
	bool overridingEnabled = true;
	nlohmann::json patches = nlohmann::json::array();

	DEFINE_PLUGIN_DETOUR(OnlineJsonMods, void, WinHttpCallback, void* dwContext, void* hInternet, void* param_3, int dwInternetStatus, void* param_5, int param_6);
	DEFINE_PLUGIN_DETOUR(OnlineJsonMods, void, ZHttpResultDynamicObject_OnBufferReady, ZHttpResultDynamicObject* th);
};

DEFINE_ZHM_PLUGIN(OnlineJsonMods)
