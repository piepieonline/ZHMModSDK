#include "GameConnection.h"
#include "SetPropertyHelpers.h"

#include "Hooks.h"
#include "Logging.h"
#include "Functions.h"

#include <Glacier/ZScene.h>
#include <Glacier/ZEntity.h>
#include <Glacier/ZSpatialEntity.h>
#include <Glacier/ZObject.h>
#include <Glacier/ZActor.h>
#include <Glacier/EntityFactory.h>
#include <Glacier/Hash.h>

#define DEFAULT_SERVER "127.0.0.1"
#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT 37275

#pragma comment(lib, "Ws2_32.lib")

#define SSTR( x ) static_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()

class IItem : public ZEntityImpl {};
class ZEntityBaseReplica : public ZEntityImpl {};

GameConnection* GameConnection::instance;
std::deque<std::string> GameConnection::receivedMessages;
std::deque<std::string> GameConnection::sendingMessages;
std::shared_mutex GameConnection::receivedMessagesLock;
std::shared_mutex GameConnection::sendingMessagesLock;

std::mutex GameConnection::sendingMutex;
std::condition_variable GameConnection::sendingCV;

void GameConnection::Init()
{
	Hooks::SignalInputPin->AddDetour(this, &GameConnection::SignalInputPin);
	Hooks::SignalOutputPin->AddDetour(this, &GameConnection::SignalOutputPin);

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

		GameConnection::AddToSendList("GameStarted");

		std::thread sendToSocketThread(&GameConnection::SendToSocketThread);
		sendToSocketThread.detach();

		std::thread receiveFromSocketThread(&GameConnection::ReceiveFromSocketThread);
		receiveFromSocketThread.detach();
	}


	/*
		closesocket(s);
		WSACleanup();

	*/
}

void GameConnection::OnDraw3D(IRenderer* p_Renderer)
{
	GameConnection::ProcessSocketMessageQueue();

	for (int i = lastKnownActorId; i < *Globals::NextActorId; ++i)
	{
		auto* s_Actor = Globals::ActorManager->m_aActiveActors[i].m_pInterfaceRef;
		ZEntityRef s_Ref;
		s_Actor->GetID(&s_Ref);

		GameConnection::UpdateTrackableMap(s_Ref);
	}

	m_EntityMutex.lock_shared();

	if (drawVolume)
	{
		p_Renderer->DrawOBB3D(SVector3(-volumeSize.x / 2.f, -volumeSize.y / 2.f, -volumeSize.z / 2.f), SVector3(volumeSize.x / 2.f, volumeSize.y / 2.f, volumeSize.z / 2.f), volumeTrans, highlightColour);
	}
	else
	{
		auto it = m_EntitiesToTrack.find(entityIDToHighlight);
		if (it != m_EntitiesToTrack.end())
		{
			auto& s_Entity = m_EntitiesToTrack[entityIDToHighlight];

			auto* s_SpatialEntity = s_Entity.QueryInterface<ZSpatialEntity>();

			if (s_SpatialEntity)
			{
				SMatrix s_Transform;
				Functions::ZSpatialEntity_WorldTransform->Call(s_SpatialEntity, &s_Transform);

				float4 s_Min, s_Max;

				s_SpatialEntity->CalculateBounds(s_Min, s_Max, 1, 0);

				if (s_Min.x == 0 && s_Min.y == 0 && s_Min.z == 0 && s_Max.x == 0 && s_Max.y == 0 && s_Max.z == 0)
				{
					s_Max = float4(.1f, .1f, .1f, s_Max.w);
				}

				p_Renderer->DrawOBB3D(SVector3(s_Min.x, s_Min.y, s_Min.z), SVector3(s_Max.x, s_Max.y, s_Max.z + .1f), s_Transform, highlightColour);
			}
		}
	}

	m_EntityMutex.unlock_shared();
}

void GameConnection::ProcessSocketMessageQueue()
{
	if (!GameConnection::receivedMessages.empty())
	{
		GameConnection::receivedMessagesLock.lock_shared();
		std::string currentMessage = GameConnection::receivedMessages.front();
		GameConnection::receivedMessages.pop_front();
		GameConnection::receivedMessagesLock.unlock_shared();

		Logger::Info("Received datagram: {}", currentMessage);

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
			if (params[0] == "HighlightEntity")
			{
				entityIDToHighlight = requestedEntityId;

				drawVolume = false;
			}
			else if (params[0] == "HighlightVolume")
			{
				float32 sX;
				std::istringstream(params[8].c_str()) >> sX;
				float32 sY;
				std::istringstream(params[9].c_str()) >> sY;
				float32 sZ;
				std::istringstream(params[10].c_str()) >> sZ;
				volumeSize = SVector3(sX, sY, sZ);

				SMatrix43 newTrans = EularToMatrix43(
					std::stod(params[2].c_str()),
					std::stod(params[3].c_str()),
					std::stod(params[4].c_str()),
					std::stod(params[5].c_str()),
					std::stod(params[6].c_str()),
					std::stod(params[7].c_str())
				);

				volumeTrans = SMatrix(newTrans);

				drawVolume = true;
			}
			else if (params[0] == "UpdateProperty")
			{
				auto it = m_EntitiesToTrack.find(requestedEntityId);
				if (it != m_EntitiesToTrack.end())
				{
					auto requestedEnt = m_EntitiesToTrack[requestedEntityId];

					SetPropertyFromString(requestedEnt, params[2].c_str(), params[3].c_str(), params, 4, &m_EntitiesToTrack);
				}
			}
			else if (params[0] == "SignalPin")
			{
				auto it = m_EntitiesToTrack.find(requestedEntityId);
				if (it != m_EntitiesToTrack.end())
				{
					auto requestedEnt = m_EntitiesToTrack[requestedEntityId];

					char* inputString = new char[params[3].size()];
					for (int i = 0; i < params[3].size(); i++)
					{
						inputString[i] = params[3].c_str()[i];
					}
					inputString[params[3].size()] = '\0';

					auto pinName = ZString(inputString);

					if (params[2] == "Input")
					{
						requestedEnt.SignalInputPin(pinName);
					}
					else
					{
						requestedEnt.SignalOutputPin(pinName);
					}
				}
			}
			else if (params[0] == "GetPlayerPosition")
			{
				TEntityRef<ZHitman5> s_LocalHitman;
				Functions::ZPlayerRegistry_GetLocalPlayer->Call(Globals::PlayerRegistry, &s_LocalHitman);
				const auto s_HitmanSpatial = s_LocalHitman.m_ref.QueryInterface<ZSpatialEntity>();

				std::ostringstream ss;
				ss << "PlayerPosition_" << s_HitmanSpatial->GetWorldMatrix().Trans.x << "_" << s_HitmanSpatial->GetWorldMatrix().Trans.y << "_" << s_HitmanSpatial->GetWorldMatrix().Trans.z;
				GameConnection::AddToSendList(ss.str());
			}
			else if (params[0] == "GetPlayerRotation")
			{
				TEntityRef<ZHitman5> s_LocalHitman;
				Functions::ZPlayerRegistry_GetLocalPlayer->Call(Globals::PlayerRegistry, &s_LocalHitman);
				const auto s_HitmanSpatial = s_LocalHitman.m_ref.QueryInterface<ZSpatialEntity>();

				std::ostringstream ss;
				ss << "PlayerRotation_" << s_HitmanSpatial->GetWorldMatrix().XAxis.x << "_" << s_HitmanSpatial->GetWorldMatrix().XAxis.y << "_" << s_HitmanSpatial->GetWorldMatrix().XAxis.z << "_" << s_HitmanSpatial->GetWorldMatrix().YAxis.x << "_" << s_HitmanSpatial->GetWorldMatrix().YAxis.y << "_" << s_HitmanSpatial->GetWorldMatrix().YAxis.z << "_" << s_HitmanSpatial->GetWorldMatrix().ZAxis.x << "_" << s_HitmanSpatial->GetWorldMatrix().ZAxis.y << "_" << s_HitmanSpatial->GetWorldMatrix().ZAxis.z; // will be converted to Euler by QNE
				GameConnection::AddToSendList(ss.str());
			}
			else if (params[0] == "SetPlayerTransform")
			{
				TEntityRef<ZHitman5> s_LocalHitman;
				Functions::ZPlayerRegistry_GetLocalPlayer->Call(Globals::PlayerRegistry, &s_LocalHitman);
				const auto s_HitmanSpatial = s_LocalHitman.m_ref.QueryInterface<ZSpatialEntity>();

				const auto matrix43 = EularToMatrix43(
					std::stoul(params[2]),
					std::stoul(params[3]),
					std::stoul(params[4]),
					std::stoul(params[5]),
					std::stoul(params[6]),
					std::stoul(params[7])
				);

				SMatrix matrix = SMatrix(matrix43);

				s_HitmanSpatial->SetWorldMatrix(matrix);

				s_LocalHitman.m_ref.GetBaseEntity()->Deactivate(0);
				s_LocalHitman.m_ref.GetBaseEntity()->Activate(0);
			}
			else if (params[0] == "SetHighlightColour")
			{
				highlightColour.x = std::stof(params[2]);
				highlightColour.y = std::stof(params[3]);
				highlightColour.z = std::stof(params[4]);
			}
			else if (params[0] == "ActivateEntity")
			{
				auto it = m_EntitiesToTrack.find(requestedEntityId);
				if (it != m_EntitiesToTrack.end())
				{
					auto requestedEnt = m_EntitiesToTrack[requestedEntityId];
					requestedEnt.QueryInterface<ZEntityImpl>()->Activate(0);
				}
			}
			else if (params[0] == "DeactivateEntity")
			{
				auto it = m_EntitiesToTrack.find(requestedEntityId);
				if (it != m_EntitiesToTrack.end())
				{
					auto requestedEnt = m_EntitiesToTrack[requestedEntityId];
					requestedEnt.QueryInterface<ZEntityImpl>()->Deactivate(0);
				}
			}
			else if (params[0] == "CreateEntity")
			{
				auto s_Hash = Hash::MD5<sizeof(params[3]) - 1>(std::string(params[3]));

				uint32_t s_IDHigh = ((s_Hash.A >> 24) & 0x000000FF)
					| ((s_Hash.A >> 8) & 0x0000FF00)
					| ((s_Hash.A << 8) & 0x00FF0000);

				uint32_t s_IDLow = ((s_Hash.B >> 24) & 0x000000FF)
					| ((s_Hash.B >> 8) & 0x0000FF00)
					| ((s_Hash.B << 8) & 0x00FF0000)
					| ((s_Hash.B << 24) & 0xFF000000);

				const auto s_ID = ResId<"[assembly:/templates/gameplay/ai2/actors.template?/npcactor.entitytemplate].pc_entitytype">;

				TResourcePtr<ZTemplateEntityFactory> s_Resource;
				// Globals::ResourceManager->GetResourcePtr(s_Resource, ZRuntimeResourceID(s_IDHigh, s_IDLow), 0);
				Globals::ResourceManager->GetResourcePtr(s_Resource, s_ID, 0);

				ZEntityRef s_NewEntity;
				Functions::ZEntityManager_NewEntity->Call(Globals::EntityManager, s_NewEntity, "", s_Resource, m_EntitiesToTrack[std::stof(params[2])], nullptr, -1);
			}
		}
	}
}

int GameConnection::AddToSendList(std::string message)
{
	GameConnection::sendingMessagesLock.lock_shared();
	GameConnection::sendingMessages.push_back(message);
	GameConnection::sendingMessagesLock.unlock_shared();
	GameConnection::sendingCV.notify_one();

	return 0;
}

void GameConnection::SendToSocketThread()
{
	bool runServer = true;

	while (runServer)
	{
		std::unique_lock<std::mutex> lk(GameConnection::sendingMutex);
		while (GameConnection::sendingMessages.empty())
		{
			GameConnection::sendingCV.wait(lk);
			// std::this_thread::yield();
		}

		GameConnection::sendingMessagesLock.lock_shared();
		if (GameConnection::sendingMessages.empty()) continue;
		std::string currentMessage = GameConnection::sendingMessages.front();
		GameConnection::sendingMessages.pop_front();
		GameConnection::sendingMessagesLock.unlock_shared();

		//start communication
		//send the message
		if (sendto(GameConnection::instance->s, currentMessage.c_str(), currentMessage.length(), 0, (struct sockaddr*)&GameConnection::instance->si_other, GameConnection::instance->slen) == SOCKET_ERROR)
		{
			Logger::Info("AddToSendList() failed with error code : {}", WSAGetLastError());
		}
	}
}

void GameConnection::ReceiveFromSocketThread()
{
	char buf[DEFAULT_BUFLEN];
	std::string pingGameResponse = "Pong";

	while (true)
	{
		memset(buf, '\0', DEFAULT_BUFLEN);
		//try to receive some data, this is a blocking call
		if (recvfrom(GameConnection::instance->s, buf, DEFAULT_BUFLEN, 0, (struct sockaddr*)&GameConnection::instance->si_other, &GameConnection::instance->slen) == SOCKET_ERROR)
		{
			// noError = false;
		}
		else
		{
			std::string message = buf;

			if (message == "Ping")
			{
				if (sendto(GameConnection::instance->s, pingGameResponse.c_str(), pingGameResponse.length(), 0, (struct sockaddr*)&GameConnection::instance->si_other, GameConnection::instance->slen) == SOCKET_ERROR)
				{
					Logger::Info("Pong failed with error code : {}", WSAGetLastError());
				}
			}
			else
			{
				GameConnection::receivedMessagesLock.lock_shared();
				GameConnection::receivedMessages.push_back(message);
				GameConnection::receivedMessagesLock.unlock_shared();
			}
		}
	}
}

void GameConnection::DumpDetails(ZEntityRef entityRef, uint32_t pinId, const ZObjectRef& objectRef)
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

void GameConnection::OnDrawMenu()
{
	if (ImGui::Button("CONNECT TO QNE"))
	{
		GameConnection::AddToSendList("Pong");
	}
}

void GameConnection::UpdateTrackableMap(ZEntityRef entityRef) // , const ZObjectRef& objectRef)
{
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

				std::string propName, propTypeName;
				bool validPropName = false;
				[&]()
				{
					__try
					{
						[&]()
						{
							propName = propInfo->m_pName;
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

				/*
				if ((*entityRef.m_pEntity)->m_nEntityId == 12379792271061294201 && propName == "m_aValues")
				{
					auto* propValue = reinterpret_cast<TArray<ZEntityRef>*>(reinterpret_cast<uintptr_t>(entityRef.m_pEntity) + prop.m_nOffset);

					// propValue->clear();
					propValue->push_back(*propRef->begin());

					// propValue[0] = propRef[0];

					// auto val = propValue[0];

					// propValue->clear();
					// propValue->resize(0);

					// (*propValue) = *arr;

					// entityRef.SetProperty(propName.c_str(), *propRef);
				}
				else if ((*entityRef.m_pEntity)->m_nEntityId == 12379655700448744392 && propName == "m_aValues")
				{
					auto* propValue = reinterpret_cast<TArray<ZEntityRef>*>(reinterpret_cast<uintptr_t>(entityRef.m_pEntity) + prop.m_nOffset);

					propRef = propValue;
				}
				*/

				if (propTypeName.starts_with("TEntityRef<") && propTypeName.ends_with("Entity>")) // == "TEntityRef<ZSpatialEntity>")
				{
					auto propValue = reinterpret_cast<ZEntityRef*>(reinterpret_cast<uintptr_t>(entityRef.m_pEntity) + prop.m_nOffset);

					auto propEnt = propValue->m_pEntity;

					if (propEnt == nullptr) continue;

					// Logger::Trace("Value: {}", (*propEnt)->m_nEntityId);

					auto propIt = m_EntitiesToTrack.find((*propEnt)->m_nEntityId);
					if (propIt == m_EntitiesToTrack.end())
					{
						m_EntitiesToTrack[(*propEnt)->m_nEntityId] = *propValue;
					}
				}
				else if (propTypeName.starts_with("TArray<TEntityRef<") && propTypeName.ends_with("Entity>>"))// propTypeName == "TArray<TEntityRef<ZSpatialEntity>>")
				{
					auto propValue = reinterpret_cast<TArray<ZEntityRef>*>(reinterpret_cast<uintptr_t>(entityRef.m_pEntity) + prop.m_nOffset);

					for (auto containedEnt : *propValue)
					{
						auto propEnt = containedEnt.m_pEntity;

						if (propEnt == nullptr) continue;

						auto entIt = (*propEnt)->m_nEntityId;
						auto propIt = m_EntitiesToTrack.find(entIt);
						if (propIt == m_EntitiesToTrack.end())
						{
							m_EntitiesToTrack[entIt] = containedEnt;
						}
					}
				}
			}
		}

		m_EntityMutex.unlock();
	}
}

DECLARE_PLUGIN_DETOUR(GameConnection, bool, SignalInputPin, ZEntityRef entityRef, uint32_t pinId, const ZObjectRef& objectRef)
{
	GameConnection::UpdateTrackableMap(entityRef);

	return HookResult<bool>(HookAction::Continue());
}

DECLARE_PLUGIN_DETOUR(GameConnection, bool, SignalOutputPin, ZEntityRef entityRef, uint32_t pinId, const ZObjectRef& objectRef)
{
	GameConnection::UpdateTrackableMap(entityRef);

	return HookResult<bool>(HookAction::Continue());
}

DECLARE_ZHM_PLUGIN(GameConnection);
