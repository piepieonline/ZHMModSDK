#pragma once

#include <shared_mutex>
#include <random>
#include <unordered_map>
#include <sstream>
#include <thread> 
#include <deque>

#include "IPluginInterface.h"

class IBoolCondition;

class LogPins : public IPluginInterface
{
public:
	void Init() override;
	void OnDraw3D(IRenderer* p_Renderer) override;
	void OnDrawMenu() override;
	void OnDrawUI(bool p_HasFocus) override;

private:
	DECLARE_PLUGIN_DETOUR(LogPins, bool, SignalInputPin, ZEntityRef, uint32_t, const ZObjectRef&);
	DECLARE_PLUGIN_DETOUR(LogPins, bool, SignalOutputPin, ZEntityRef, uint32_t, const ZObjectRef&);

private:
	std::unordered_map<std::string, bool> m_knownInputs;
	std::unordered_map<std::string, bool> m_knownOutputs;

	struct sockaddr_in si_other;
	int s, slen;

	std::shared_mutex m_EntityMutex;
	std::unordered_map<uint64_t, ZEntityRef> m_EntitiesToTrack;

	void UpdateTrackableMap(ZEntityRef entityRef);
	void DumpDetails(ZEntityRef entityRef, uint32_t pinId, const ZObjectRef& objectRef);
	int AddToSendList(std::string);
	void ProcessSocketMessageQueue();

	bool m_HoldingMouse = false;
	void OnMouseDown(SVector2 p_Pos, bool p_FirstClick);

	static void SendToSocketThread();
	static void ReceiveFromSocketThread();

	static LogPins* instance;
	static std::deque<std::string> receivedMessages;
	static std::deque<std::string> sendingMessages;
	static std::shared_mutex receivedMessagesLock;
	static std::shared_mutex sendingMessagesLock;

	static bool sendingPinsEnabled;
	static std::mutex sendingMutex;
	static std::condition_variable sendingCV;

	int lastIndex = 0;

	uint64_t entityToTrack = 0;

	bool drawCoverPlane;
	SMatrix coverPlanePos;
	SVector3 coverPlaneSize;

	int lastKnownActorId = 0;

	TArray<ZEntityRef>* propRef;
};

DECLARE_ZHM_PLUGIN(LogPins)
