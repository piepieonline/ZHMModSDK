#include "OnlineJsonMods.h"
#include "json.hpp"

#include "Logging.h"
#include "Hooks.h"

#include <KnownFolders.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <shlobj_core.h>
#include <Windows.h>
#include <winhttp.h>

#include <Glacier/ZHttp.h>

#pragma comment(lib,"winhttp.lib")

void OnlineJsonMods::PreInit()
{
	Hooks::Http_WinHttpCallback->AddDetour(this, &OnlineJsonMods::WinHttpCallback);
	Hooks::ZHttpResultDynamicObject_OnBufferReady->AddDetour(this, &OnlineJsonMods::ZHttpResultDynamicObject_OnBufferReady);

	auto loadedMods = nlohmann::json::array();
	try
	{
		char localAppData[8096];
		if (SHGetSpecialFolderPathA(NULL, localAppData, CSIDL_LOCAL_APPDATA, 0))
		{
			std::stringstream ss;
			ss << localAppData << "\\Simple Mod Framework\\lastDeploy.json";
			std::ifstream loadedModsFile(ss.str());
			loadedMods = nlohmann::json::parse(loadedModsFile)["loadOrder"];
		}
	}
	catch (...) {}

	Logger::Info("OnlineJsonMods: Loaded Mods: {}", loadedMods.dump(2));

	for (const auto& filePath : std::filesystem::directory_iterator(".\\Retail\\mods\\OnlineJsonMods\\"))
	{
		try
		{
			Logger::Info("OnlineJsonMods: Loading patches from: {}", filePath);
			std::ifstream f(filePath.path());
			auto loadedPatches = nlohmann::json::parse(f);

			loadedPatches["modLoaded"] = false;
			for (auto loadedMod : loadedMods)
			{
				if (loadedMod == loadedPatches.at("mod"))
					loadedPatches["modLoaded"] = true;
			}
			patches.push_back(loadedPatches);
		}
		catch (...) {}
	}
}

void OnlineJsonMods::OnDrawMenu()
{
	/*
	if (ImGui::Button(LogPins::sendingPinsEnabled ? "DISABLE PINS" : "ENABLE PINS"))
	{
		LogPins::sendingMessagesLock.lock_shared();
		LogPins::sendingMessages.clear();
		LogPins::sendingMessagesLock.unlock_shared();
		LogPins::sendingPinsEnabled = !LogPins::sendingPinsEnabled;
	}
	*/
}

DECLARE_PLUGIN_DETOUR(OnlineJsonMods, void, WinHttpCallback, void* dwContext, void* hInternet, void* param_3, int dwInternetStatus, void* param_5, int length_param_6)
{
	if (dwInternetStatus == WINHTTP_CALLBACK_STATUS_SENDING_REQUEST)
	{
		/*
		// Doesn't work
		const DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_ALL;
		WinHttpSetOption(hInternet, WINHTTP_OPTION_DECOMPRESSION, (LPVOID)&decompression, sizeof(decompression));
		Logger::Info("option set");
		*/

		WinHttpAddRequestHeaders(hInternet, L"Accept-Encoding: identity", (ULONG)-1L, WINHTTP_ADDREQ_FLAG_REPLACE);
		// Logger::Info("header set");
	}

	return HookResult<void>(HookAction::Continue());
}

DECLARE_PLUGIN_DETOUR(OnlineJsonMods, void, ZHttpResultDynamicObject_OnBufferReady, ZHttpResultDynamicObject* th)
{
	if (th->m_buffer.size() > 0)
	{
		std::string incoming(static_cast<char*>(th->m_buffer.data()), th->m_buffer.size());

		try
		{
			auto incomingJson = nlohmann::json::parse(incoming);

			for (auto& [patchSetKey, patchSet] : patches.items())
			{
				if ((patchSet["forceEnable"] || patchSet["modLoaded"]) && !patchSet["forceDisable"])
				{
					for (auto& [patchKey, patch] : patchSet["patches"].items())
					{
						auto matchTarget = nlohmann::json::json_pointer((std::string)patch.at("matchTarget"));
						auto matchValue = (std::string)patch.at("matchValue");
						if (incomingJson.value(matchTarget, "") == matchValue)
						{
							auto randomPatchOption = patch.at("randomPatchOptions")[rand() % patch.at("randomPatchOptions").size()];

							auto outgoingJson = incomingJson.patch(randomPatchOption);

							Logger::Info("Before replacing");
							auto newValue = outgoingJson.dump();
							th->m_buffer = ZBuffer::FromData(newValue);
							Logger::Info("After replacing");
						}
					}
				}
			}
		}
		catch (...) {}
	}

	return HookResult<void>(HookAction::Continue());
}


DECLARE_ZHM_PLUGIN(OnlineJsonMods);
