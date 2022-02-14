#include "LogPins.h"

#include "Hooks.h"
#include "Logging.h"
#include "Functions.h"

#include <Glacier/ZScene.h>
#include <Glacier/ZEntity.h>
#include <Glacier/ZSpatialEntity.h>
#include <Glacier/ZObject.h>

#define DEFAULT_SERVER "127.0.0.1"
#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT 27015

#pragma comment(lib, "Ws2_32.lib")

#define SSTR( x ) static_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()

class IItem : public ZEntityImpl {};
class ZEntityBaseReplica : public ZEntityImpl {};

LogPins* LogPins::instance;
std::deque<std::string> LogPins::receivedMessages;
std::deque<std::string> LogPins::sendingMessages;

std::mutex LogPins::sendingMutex;
std::condition_variable LogPins::sendingCV;
bool LogPins::sendingPinsEnabled = false;

void LogPins::PreInit()
{
	Hooks::SignalInputPin->AddDetour(this, &LogPins::SignalInputPin);
	Hooks::SignalOutputPin->AddDetour(this, &LogPins::SignalOutputPin);

	s, slen = sizeof(si_other);
	WSADATA wsa;

	//Initialise winsock
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		Logger::Info("Failed to initialise socket");
	}

	instance = this;

	//create socket
	if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR)
	{
		Logger::Info("Failed to create socket");
	}
	else
	{
		//setup address structure
		memset((char*)&si_other, 0, sizeof(si_other));
		si_other.sin_family = AF_INET;
		si_other.sin_port = htons(DEFAULT_PORT);
		si_other.sin_addr.S_un.S_addr = inet_addr(DEFAULT_SERVER);

		LogPins::AddToSendList("hello\r\n");

		std::thread sendToSocketThread(&LogPins::SendToSocketThread);
		sendToSocketThread.detach();

		std::thread receiveFromSocketThread(&LogPins::ReceiveFromSocketThread);
		receiveFromSocketThread.detach();
	}


	/*
		closesocket(s);
		WSACleanup();

	*/
}

void LogPins::OnDraw3D(IRenderer* p_Renderer)
{
	m_EntityMutex.lock_shared();

	//for (auto& s_Entity : m_EntitiesToTrack)

	auto it = m_EntitiesToTrack.find(entityToTrack);
	if (it != m_EntitiesToTrack.end())
	{
		auto& s_Entity = m_EntitiesToTrack[entityToTrack];

		auto* s_SpatialEntity = s_Entity.QueryInterface<ZSpatialEntity>();

		SMatrix s_Transform;
		Functions::ZSpatialEntity_WorldTransform->Call(s_SpatialEntity, &s_Transform);

		float4 s_Min, s_Max;

		s_SpatialEntity->CalculateBounds(s_Min, s_Max, 1, 0);

		if (s_Min.x == 0 && s_Min.y == 0 && s_Min.z == 0 && s_Max.x == 0 && s_Max.y == 0 && s_Max.z == 0)
		{
			s_Max = float4(.1f, .1f, .1f, s_Max.w);
		}

		p_Renderer->DrawOBB3D(SVector3(s_Min.x, s_Min.y, s_Min.z), SVector3(s_Max.x, s_Max.y, s_Max.z + .1f), s_Transform, SVector4(0.f, 0.f, 1.f, 1.f));
	}

	m_EntityMutex.unlock_shared();

	if (drawCoverPlane)
	{
		p_Renderer->DrawOBB3D(SVector3(-coverPlaneSize.x / 2.f, -coverPlaneSize.y / 2.f, -coverPlaneSize.z / 2.f), SVector3(coverPlaneSize.x / 2.f, coverPlaneSize.y / 2.f, coverPlaneSize.z / 2.f), coverPlanePos, SVector4(0.f, 0.f, 1.f, 1.f));
	}
}

void LogPins::ProcessSocketMessageQueue()
{
	if (!LogPins::receivedMessages.empty())
	{
		std::string currentMessage = LogPins::receivedMessages.front();
		LogPins::receivedMessages.pop_front();

		Logger::Info("Received From Socket: {}", currentMessage);

		std::string delimiter = "|";

		std::vector<std::string> params;

		size_t last = 0;
		size_t next = 0;
		while ((next = currentMessage.find(delimiter, last)) != std::string::npos)
		{
			params.push_back(currentMessage.substr(last, next - last));
			last = next + 1;
		}

		uint64_t requestedEntityId;
		if (last > 0)
		{
			params.push_back(currentMessage.substr(last));
			std::istringstream(params[1].c_str()) >> requestedEntityId;
		}
		else
		{
			params.push_back(currentMessage);
		}

		if (currentMessage.length() > 0)
		{
			if (params[0] == "H")
			{
				entityToTrack = requestedEntityId;
			}
			else if (params[0] == "P")
			{
				auto it = m_EntitiesToTrack.find(requestedEntityId);
				if (it != m_EntitiesToTrack.end())
				{
					auto requestedEnt = m_EntitiesToTrack[requestedEntityId];
					auto s_EntitySpatial = requestedEnt.QueryInterface<ZSpatialEntity>();

					SMatrix s_WorldTransform;
					Functions::ZSpatialEntity_WorldTransform->Call(s_EntitySpatial, &s_WorldTransform);

					float32 x;
					std::istringstream(params[2].c_str()) >> x;
					float32 y;
					std::istringstream(params[3].c_str()) >> y;
					float32 z;
					std::istringstream(params[4].c_str()) >> z;

					float32 rX;
					std::istringstream(params[5].c_str()) >> rX;
					float32 rY;
					std::istringstream(params[6].c_str()) >> rY;
					float32 rZ;
					std::istringstream(params[7].c_str()) >> rZ;

					rX *= 3.14 / 180.f;
					rY *= 3.14 / 180.f;
					rZ *= 3.14 / 180.f;

					SMatrix43 newTrans = SMatrix43();
					newTrans.XAxis = SVector3(cos(rZ), -sin(rZ), 0);
					newTrans.YAxis = SVector3(sin(rZ), cos(rZ), 0);
					newTrans.ZAxis = SVector3(0, 0, 1);
					newTrans.Trans = SVector3(x, y, z);

					requestedEnt.SetProperty("m_mTransform", newTrans);
				}
			}
			else if (params[0] == "C")
			{
				float32 x;
				std::istringstream(params[2].c_str()) >> x;
				float32 y;
				std::istringstream(params[3].c_str()) >> y;
				float32 z;
				std::istringstream(params[4].c_str()) >> z;

				float32 rX;
				std::istringstream(params[5].c_str()) >> rX;
				float32 rY;
				std::istringstream(params[6].c_str()) >> rY;
				float32 rZ;
				std::istringstream(params[7].c_str()) >> rZ;

				rX *= 3.14 / 180.f;
				rY *= 3.14 / 180.f;
				rZ *= 3.14 / 180.f;

				float32 sX;
				std::istringstream(params[8].c_str()) >> sX;
				float32 sY;
				std::istringstream(params[9].c_str()) >> sY;
				float32 sZ;
				std::istringstream(params[10].c_str()) >> sZ;
				coverPlaneSize = SVector3(sX, sY, sZ);

				SMatrix43 newTrans = SMatrix43();
				newTrans.XAxis = SVector3(cos(rZ), -sin(rZ), 0);
				newTrans.YAxis = SVector3(sin(rZ), cos(rZ), 0);
				newTrans.ZAxis = SVector3(0, 0, 1);
				newTrans.Trans = SVector3(x, y, z);

				Matrix43ToSMatrix(newTrans, &coverPlanePos);

				drawCoverPlane = true;
			}
			else if (params[0] == "UpdateProperty")
			{
				auto it = m_EntitiesToTrack.find(requestedEntityId);
				if (it != m_EntitiesToTrack.end())
				{
					auto requestedEnt = m_EntitiesToTrack[requestedEntityId];
					bool shouldRotate = params[3] == "true";
					requestedEnt.SetProperty(params[2].c_str(), shouldRotate);
				}
			}
			else if (params[0] == "GetHeroPosition")
			{
				TEntityRef<ZHitman5> s_LocalHitman;
				Functions::ZPlayerRegistry_GetLocalPlayer->Call(Globals::PlayerRegistry, &s_LocalHitman);
				const auto s_HitmanSpatial = s_LocalHitman.m_ref.QueryInterface<ZSpatialEntity>();

				std::ostringstream ss;
				ss << "GetHeroPosition_" << s_HitmanSpatial->m_mTransform.Trans.x << "_" << s_HitmanSpatial->m_mTransform.Trans.y << "_" << s_HitmanSpatial->m_mTransform.Trans.z;
				LogPins::AddToSendList(ss.str());
			}
			else if (params[0] == "SetHeroPosition")
			{
				TEntityRef<ZHitman5> s_LocalHitman;
				Functions::ZPlayerRegistry_GetLocalPlayer->Call(Globals::PlayerRegistry, &s_LocalHitman);
				const auto s_HitmanSpatial = s_LocalHitman.m_ref.QueryInterface<ZSpatialEntity>();

				float32 x;
				std::istringstream(params[2].c_str()) >> x;
				float32 y;
				std::istringstream(params[3].c_str()) >> y;
				float32 z;
				std::istringstream(params[4].c_str()) >> z;

				float32 rX;
				std::istringstream(params[5].c_str()) >> rX;
				float32 rY;
				std::istringstream(params[6].c_str()) >> rY;
				float32 rZ;
				std::istringstream(params[7].c_str()) >> rZ;

				rX *= 3.14 / 180.f;
				rY *= 3.14 / 180.f;
				rZ *= 3.14 / 180.f;

				/*
				s_HitmanSpatial->m_mTransform.Trans.x = x;
				s_HitmanSpatial->m_mTransform.Trans.y = y;
				s_HitmanSpatial->m_mTransform.Trans.z = z;
				*/
				// s_HitmanSpatial->m_mTransform.Trans = SVector3(x, y, z);

				s_LocalHitman.m_ref.GetBaseEntity()->Deactivate(6000);

				SMatrix43 newTrans = SMatrix43();
				newTrans.XAxis = SVector3(cos(rZ), -sin(rZ), 0);
				newTrans.YAxis = SVector3(sin(rZ), cos(rZ), 0);
				newTrans.ZAxis = SVector3(0, 0, 1);
				newTrans.Trans = SVector3(x, y, z);

				s_HitmanSpatial->m_mTransform = newTrans;


				// s_HitmanSpatial->m_mTransform.XAxis.x = cos(rZ);
				// s_HitmanSpatial->m_mTransform.XAxis.y = -sin(rZ);
				// s_HitmanSpatial->m_mTransform.YAxis.x = sin(rZ);
				// s_HitmanSpatial->m_mTransform.YAxis.y = cos(rZ);
			}
		}
	}
}

int LogPins::AddToSendList(std::string message)
{
	LogPins::sendingMessages.push_back(message);
	LogPins::sendingCV.notify_one();
	// Process here for now. Should be done on render!
	LogPins::ProcessSocketMessageQueue();

	return 0;
}

void LogPins::SendToSocketThread()
{
	bool runServer = true;

	while (runServer)
	{
		std::unique_lock<std::mutex> lk(LogPins::sendingMutex);
		while (LogPins::sendingMessages.empty() || !LogPins::sendingPinsEnabled)
		{
			LogPins::sendingCV.wait(lk);
			// std::this_thread::yield();
		}

		std::string currentMessage = LogPins::sendingMessages.front();
		LogPins::sendingMessages.pop_front();

		//start communication
		//send the message
		if (sendto(LogPins::instance->s, currentMessage.c_str(), currentMessage.length(), 0, (struct sockaddr*)&LogPins::instance->si_other, LogPins::instance->slen) == SOCKET_ERROR)
		{
			Logger::Info("AddToSendList() failed with error code : {}", WSAGetLastError());
		}
	}
}

void LogPins::ReceiveFromSocketThread()
{
	bool shouldReceive = true;
	char buf[DEFAULT_BUFLEN];

	while (shouldReceive)
	{
		memset(buf, '\0', DEFAULT_BUFLEN);
		//try to receive some data, this is a blocking call
		if (recvfrom(LogPins::instance->s, buf, DEFAULT_BUFLEN, 0, (struct sockaddr*)&LogPins::instance->si_other, &LogPins::instance->slen) == SOCKET_ERROR)
		{
			// noError = false;
		}
		else
		{
			std::string message = buf;

			if (message == "exit")
			{
				shouldReceive = false;
			}
			else
			{
				LogPins::receivedMessages.push_back(message);
			}
		}
	}
}

void LogPins::DumpDetails(ZEntityRef entityRef, uint32_t pinId, const ZObjectRef& objectRef)
{
	auto pInterface = (*(*entityRef.m_pEntity)->m_pInterfaces)[0];

	if (!pInterface.m_pTypeId ||
		!pInterface.m_pTypeId->m_pType ||
		!pInterface.m_pTypeId->m_pType->m_pTypeName
		)
	{
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

void LogPins::OnDrawMenu()
{
	if (ImGui::Button(LogPins::sendingPinsEnabled ? "DISABLE PINS" : "ENABLE PINS"))
	{
		LogPins::sendingMessages.clear();
		LogPins::sendingPinsEnabled = !LogPins::sendingPinsEnabled;
	}
}

DECLARE_PLUGIN_DETOUR(LogPins, bool, SignalInputPin, ZEntityRef entityRef, uint32_t pinId, const ZObjectRef& objectRef)
{
	std::ostringstream ss;
	ss << "I_" << pinId << "_" << (*entityRef.m_pEntity)->m_nEntityId;
	std::string s = ss.str();

	LogPins::AddToSendList(s);

	auto pinsIt = m_knownInputs.find(s);
	if (pinsIt == m_knownInputs.end())
	{
		// Logger::Info("Pin Input: {} on {}", pinId, (*entityRef.m_pEntity)->m_nEntityId);

		// DumpDetails(entityRef, pinId, objectRef);

		m_knownInputs[s] = true;
	}

	return HookResult<bool>(HookAction::Continue());
}

DECLARE_PLUGIN_DETOUR(LogPins, bool, SignalOutputPin, ZEntityRef entityRef, uint32_t pinId, const ZObjectRef& objectRef)
{
	std::ostringstream ss;
	ss << "O_" << pinId << "_" << (*entityRef.m_pEntity)->m_nEntityId;
	std::string s = ss.str();

	LogPins::AddToSendList(s);

	auto pinsIt = m_knownOutputs.find(s);
	if (pinsIt == m_knownOutputs.end())
	{
		// Logger::Info("Pin Output: {} on {}", pinId, (*entityRef.m_pEntity)->m_nEntityId);

		// DumpDetails(entityRef, pinId, objectRef);

		m_knownOutputs[s] = true;
	}


	// Add entity to track
	auto it = m_EntitiesToTrack.find((*entityRef.m_pEntity)->m_nEntityId);
	if (it == m_EntitiesToTrack.end())
	{
		m_EntityMutex.lock();

		m_EntitiesToTrack[(*entityRef.m_pEntity)->m_nEntityId] = entityRef;

		// Add any referenced entities
		TArray<ZEntityProperty>* props = (*entityRef.m_pEntity)->m_pProperties01;

		if (props != nullptr)
		{
			for (auto prop : *props)
			{
				if (prop.m_pType == nullptr || prop.m_pType->getPropertyInfo() == nullptr) continue;
				auto propInfo = prop.m_pType->getPropertyInfo();

				std::string propTypeName;
				bool validPropName = false;
				[&]()
				{
					__try
					{
						[&]()
						{
							propTypeName = propInfo->m_pType->typeInfo()->m_pTypeName;
							validPropName = true;
						}();
					}
					__except (EXCEPTION_EXECUTE_HANDLER)
					{
						propTypeName = "";
					}
				}();

				if (propTypeName.empty() || !validPropName) continue;

				if (propTypeName == "TEntityRef<ZSpatialEntity>")
				{
					auto propValue = reinterpret_cast<TEntityRef<ZSpatialEntity>*>(reinterpret_cast<uintptr_t>(entityRef.m_pEntity) + prop.m_nOffset);

					auto propEnt = propValue->m_ref.m_pEntity;

					if (propEnt == nullptr) continue;

					// Logger::Trace("Value: {}", (*propEnt)->m_nEntityId);

					auto propIt = m_EntitiesToTrack.find((*propEnt)->m_nEntityId);
					if (propIt == m_EntitiesToTrack.end())
					{
						m_EntitiesToTrack[(*propEnt)->m_nEntityId] = propValue->m_ref;
					}
				}
				else if (propTypeName == "TArray<TEntityRef<ZSpatialEntity>>")
				{
					auto propValue = reinterpret_cast<TArray<TEntityRef<ZSpatialEntity>>*>(reinterpret_cast<uintptr_t>(entityRef.m_pEntity) + prop.m_nOffset);

					for (auto containedEnt : *propValue)
					{
						auto propEnt = containedEnt.m_ref.m_pEntity;

						if (propEnt == nullptr) continue;

						auto entIt = (*propEnt)->m_nEntityId;
						auto propIt = m_EntitiesToTrack.find(entIt);
						if (propIt == m_EntitiesToTrack.end())
						{
							m_EntitiesToTrack[entIt] = containedEnt.m_ref;
						}
					}
				}
			}
		}

		m_EntityMutex.unlock();
	}

	return HookResult<bool>(HookAction::Continue());
}

DECLARE_ZHM_PLUGIN(LogPins);
