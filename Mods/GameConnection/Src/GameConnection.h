#pragma once

#include <shared_mutex>
#include <random>
#include <unordered_map>
#include <sstream>
#include <thread> 
#include <deque>

#include "IPluginInterface.h"

class IBoolCondition;

class GameConnection : public IPluginInterface
{
public:
	void Init() override;
	void OnDraw3D(IRenderer* p_Renderer) override;
	void OnDrawMenu() override;

private:
	DEFINE_PLUGIN_DETOUR(GameConnection, bool, SignalInputPin, ZEntityRef, uint32_t, const ZObjectRef&);
	DEFINE_PLUGIN_DETOUR(GameConnection, bool, SignalOutputPin, ZEntityRef, uint32_t, const ZObjectRef&);

private:
	struct sockaddr_in si_other;
	int s, slen;

	std::shared_mutex m_EntityMutex;
	std::unordered_map<uint64_t, ZEntityRef> m_EntitiesToTrack;

	void UpdateTrackableMap(ZEntityRef entityRef);
	void DumpDetails(ZEntityRef entityRef, uint32_t pinId, const ZObjectRef& objectRef);
	int AddToSendList(std::string);
	void ProcessSocketMessageQueue();

	static void SendToSocketThread();
	static void ReceiveFromSocketThread();

	static GameConnection* instance;
	static std::deque<std::string> receivedMessages;
	static std::deque<std::string> sendingMessages;
	static std::shared_mutex receivedMessagesLock;
	static std::shared_mutex sendingMessagesLock;

	static std::mutex sendingMutex;
	static std::condition_variable sendingCV;

	int lastIndex = 0;

	uint64_t entityIDToHighlight = 0;

	bool drawVolume;
	SMatrix volumeTrans;
	SVector3 volumeSize;

	int lastKnownActorId = 0;

	TArray<ZEntityRef>* propRef;

	SVector4 highlightColour = SVector4(0.f, 0.f, 1.f, 1.f);
};

DEFINE_ZHM_PLUGIN(GameConnection)
