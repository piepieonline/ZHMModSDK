#pragma once

#include "IPluginInterface.h"
#include "json.hpp"

class OnlineJsonMods : public IPluginInterface
{
public:
	void Init() override;
	void OnDrawMenu() override;

private:
	bool overridingEnabled = true;
	nlohmann::json patches;

	void ReloadPatches();

	DEFINE_PLUGIN_DETOUR(OnlineJsonMods, void, WinHttpCallback, void* dwContext, void* hInternet, void* param_3, int dwInternetStatus, void* param_5, int param_6);
	DEFINE_PLUGIN_DETOUR(OnlineJsonMods, void, ZHttpResultDynamicObject_OnBufferReady, ZHttpResultDynamicObject* th);
};

DEFINE_ZHM_PLUGIN(OnlineJsonMods)
