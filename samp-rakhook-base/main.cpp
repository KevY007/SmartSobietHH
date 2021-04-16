#include "main.h"

DWORD				g_SAMP = NULL;
HANDLE				g_DllHandle;

bool				samp037 = false;

bool				gmod = false;
bool				dispupd = false;
bool				stealth = false;
int					key1 = VK_INSERT;
int					key2 = VK_SHIFT;

void cmd_gmodtog(char* param)
{
	gmod = !gmod;

	if (gmod)
	{
		if (!stealth) dispGameText("~g~Enabled!", 1000, 5);
	}
	else
	{
		if(!stealth) dispGameText("~r~Disabled!", 1000, 5);

		struct actor_info* self = actor_info_get_self();
		self->flags &= ~ACTOR_FLAGS_INVULNERABLE;
	}
	
	return;
}

void cmd_gmodtogupd(char* param)
{
	dispupd = !dispupd;

	if (dispupd && !stealth) dispGameText("~y~Will show updates now!", 1000, 5);
	else if(!dispupd && !stealth) dispGameText("~b~Will not show updates now!", 1000, 5);

	CIniWriter iniWriter(".\\smarthh.ini");
	iniWriter.WriteBoolean("Settings", "ShowUpdates", dispupd);
	return;
}

void cmd_gmodtogstealth(char* param)
{
	stealth = !stealth;

	if (stealth) dispGameText("~g~Stealth mode on!", 1000, 5);
	else dispGameText("~b~Stealth mode off!", 1000, 5);

	CIniWriter iniWriter(".\\smarthh.ini");
	iniWriter.WriteBoolean("Settings", "StealthMode", stealth);
	return;
}

void ThrMainFS() 
{
	while (g_SAMP == NULL)
	{
		g_SAMP = (DWORD)GetModuleHandle("samp.dll");
		Sleep(1000);
	}
	
	bool initSampRak = false;
	while (true)
	{
		if (!initSampRak)
		{
			if (memcmp_safe((uint8_t*)g_SAMP + 0xBABE, hex_to_bin(SAMP037_CMP), 10))
			{
				samp037 = true;
			}
			else samp037 = false;

			if (samp037)
			{
				g_stSAMP037 = stGetSampInfo037();

				if (isBadPtr_writeAny(g_stSAMP037, sizeof(stSAMP_037)))
				{
					continue;
				}

				if (isBadPtr_writeAny(g_stSAMP037->pPools, sizeof(stSAMPPools_037)))
				{
					continue;
				}

				g_Chat037 = stGetSampChatInfo037();
				if (isBadPtr_writeAny(g_Chat037, sizeof(stChatInfo_037)))
				{
					continue;
				}

				g_Input037 = stGetInputInfo037();
				if (isBadPtr_writeAny(g_Input037, sizeof(stInputInfo_037)))
				{
					continue;
				}

				if (g_stSAMP037->pRakClientInterface == NULL)
				{
					continue;
				}

				g_RakClient = new RakClient(g_stSAMP037->pRakClientInterface);
				g_stSAMP037->pRakClientInterface = new HookedRakClientInterface();

				SetupSAMPHook("HandleRPCPacket", SAMP037_HOOKENTER_HANDLE_RPC, hook_handle_rpc_packet, DETOUR_TYPE_JMP, 6, "FF5701");
				SetupSAMPHook("HandleRPCPacket2", SAMP037_HOOKENTER_HANDLE_RPC2, hook_handle_rpc_packet2, DETOUR_TYPE_JMP, 8, "FF5701");
				SetupSAMPHook("CNETGAMEDESTR1", SAMP037_HOOKENTER_CNETGAME_DESTR, CNetGame__destructor, DETOUR_TYPE_CALL_FUNC, 5, "E8");
				SetupSAMPHook("CNETGAMEDESTR2", SAMP037_HOOKENTER_CNETGAME_DESTR2, CNetGame__destructor, DETOUR_TYPE_CALL_FUNC, 5, "E8");
			}
			else
			{
				g_stSAMP = stGetSampInfo();

				if (isBadPtr_writeAny(g_stSAMP, sizeof(stSAMP)))
				{
					continue;
				}

				if (isBadPtr_writeAny(g_stSAMP->pPools, sizeof(stSAMPPools)))
				{
					continue;
				}

				g_Chat = stGetSampChatInfo();
				if (isBadPtr_writeAny(g_Chat, sizeof(stChatInfo)))
				{
					continue;
				}

				g_Input = stGetInputInfo();
				if (isBadPtr_writeAny(g_Input, sizeof(stInputInfo)))
				{
					continue;
				}

				if (g_stSAMP->pRakClientInterface == NULL)
				{
					continue;
				}

				g_RakClient = new RakClient(g_stSAMP->pRakClientInterface);
				g_stSAMP->pRakClientInterface = new HookedRakClientInterface();

				SetupSAMPHook("HandleRPCPacket", SAMP_HOOKENTER_HANDLE_RPC, hook_handle_rpc_packet, DETOUR_TYPE_JMP, 6, "FF5701");
				SetupSAMPHook("HandleRPCPacket2", SAMP_HOOKENTER_HANDLE_RPC2, hook_handle_rpc_packet2, DETOUR_TYPE_JMP, 8, "FF5701");
				SetupSAMPHook("CNETGAMEDESTR1", SAMP_HOOKENTER_CNETGAME_DESTR, CNetGame__destructor, DETOUR_TYPE_CALL_FUNC, 5, "E8");
				SetupSAMPHook("CNETGAMEDESTR2", SAMP_HOOKENTER_CNETGAME_DESTR2, CNetGame__destructor, DETOUR_TYPE_CALL_FUNC, 5, "E8");
			}

			initSampRak = true;
			addClientCommand("gmtog", cmd_gmodtog);
			addClientCommand("gmtogstealth", cmd_gmodtogstealth);
			addClientCommand("gmtogupd", cmd_gmodtogupd);

			CIniReader iniReader(".\\smarthh.ini");
			stealth = iniReader.ReadBoolean("Settings", "StealthMode", false);
			dispupd = iniReader.ReadInteger("Settings", "ShowUpdates", false);
			key1 = iniReader.ReadInteger("Settings", "Toggle_VK_Key_1", VK_INSERT);
			key2 = iniReader.ReadInteger("Settings", "Toggle_VK_Key_2", VK_SHIFT);

			CIniWriter iniWriter(".\\smarthh.ini");
			iniWriter.WriteBoolean("Settings", "StealthMode", stealth);
			iniWriter.WriteInteger("Settings", "ShowUpdates", dispupd);
			iniWriter.WriteInteger("Settings", "Toggle_VK_Key_1", key1);
			iniWriter.WriteInteger("Settings", "Toggle_VK_Key_2", key2);

			
			if(!stealth) addMessageToChatWindow(D3DCOLOR_XRGB(255, 255, 255), "Smart HH /gmtog /gmtogstealth /gmtogupd (stealth to hide)");
		}
		if (initSampRak)
		{
			pool_actor = *(struct pool**)0x00B74490;
			if (pool_actor == nullptr || pool_actor->start == nullptr || pool_actor->size <= 0)
				continue;

			// Code if you wanna execute any. Put it in here so it doesn't execute before samp has loaded.

			if ((GetKeyState(key1) & 0x100) != 0 && (GetKeyState(key2) & 0x100) != 0) {
				cmd_gmodtog("");
			}

			if (gmod)
			{
				//info->flags |= ACTOR_FLAGS_INVULNERABLE;

				struct actor_info* self = actor_info_get_self();
				self->flags |= 4;
				self->flags |= 8;
				self->flags |= 64;
				self->flags |= 128;
			}
		}
		Sleep(100); // Adjust according to your needs
	}
}

bool OnSendRPC(int uniqueID, BitStream* parameters, PacketPriority priority, PacketReliability reliability, char orderingChannel, bool shiftTimestamp)
{
	if (uniqueID == RPC_GiveTakeDamage)
	{
		if (gmod)
		{
			bool bGiveOrTake;
			parameters->ResetReadPointer();
			parameters->Read(bGiveOrTake);

			if (bGiveOrTake) // take == true , give == false
			{
				if(dispupd && !stealth) dispGameText("~g~Stopped TakeDamageRPC", 500, 6);
				return false;
			}
		}
	}
	return true;
}

bool OnSendPacket(BitStream* parameters, PacketPriority priority, PacketReliability reliability, char orderingChannel)
{
	uint8_t packetId;
	parameters->ResetReadPointer();
	parameters->Read(packetId);
	
	if (packetId == PacketEnumeration::ID_BULLET_SYNC) // Example
	{

	}
	return true;
}

void OnReceiveRPC() {} // ON RECEIVE RPC IS THE FUNCTION DOWN HERE!! IT'S NOT THIS IT IS HandleRPCPacketFunc -- LAZY TO RENAME FOR YOU
void HandleRPCPacketFunc(unsigned char id, RPCParameters* rpcParams, void(*callback) (RPCParameters*))
{
	if (rpcParams != nullptr && rpcParams->numberOfBitsOfData >= 8)
	{
		BitStream bsData(rpcParams->input, rpcParams->numberOfBitsOfData / 8, false);
		if(id == RPC_SetPlayerHealth && gmod == true)
		{
			struct actor_info* self = actor_info_get_self();
			float hp = self->hitpoints;
			
			if (hp < 0.1) {
				if (samp037)
				{
					hp = g_stSAMP037->pPools->pPlayer->pLocalPlayer->onFootData.byteHealth;
					if (g_stSAMP037->pPools->pPlayer->pLocalPlayer->sCurrentVehicleID > 0 && g_stSAMP037->pPools->pPlayer->pLocalPlayer->sCurrentVehicleID != 65535)
					{
						hp = g_stSAMP037->pPools->pPlayer->pLocalPlayer->inCarData.bytePlayerHealth;
					}
				}
				else
				{
					hp = g_stSAMP->pPools->pPlayer->pLocalPlayer->onFootData.byteHealth;
					if (g_stSAMP->pPools->pPlayer->pLocalPlayer->sCurrentVehicleID > 0 && g_stSAMP->pPools->pPlayer->pLocalPlayer->sCurrentVehicleID != 65535)
					{
						hp = g_stSAMP->pPools->pPlayer->pLocalPlayer->inCarData.bytePlayerHealth;
					}
				}
			}

			if (hp < 1.0) hp = 100.0;

			BitStream bsData(rpcParams->input, rpcParams->numberOfBitsOfData / 8, false);
			float fHealth;
			bsData.Read(fHealth);

			if (fHealth < hp)
			{
				if (dispupd && !stealth)
				{
					std::stringstream stream;
					stream << "~w~SV hp change: ~r~" << std::fixed << std::setprecision(1) << fHealth;
					char* bufArr = new char[stream.str().length()];
					sprintf(bufArr, "%s", stream.str().c_str());
					bufArr[stream.str().length()] = '\0';

					dispGameText(bufArr, 500, 6);
				}
				return; // exit from the function without processing RPC
			}
		
		}
	}
	callback(rpcParams);
}

bool OnReceivePacket(Packet* p)
{
	if (p->data == nullptr || p->length == 0)
		return true;
	
	if (p->data[0] == PacketEnumeration::ID_VEHICLE_SYNC) // Example.
	{
		
	}
	return true;
}


BOOL WINAPI DllMain(
	HINSTANCE hinstDLL, 
	DWORD fdwReason,    
	LPVOID lpReserved) 
{

	g_DllHandle = hinstDLL;

	DisableThreadLibraryCalls((HMODULE)hinstDLL);

	if (fdwReason != DLL_PROCESS_ATTACH)
		return FALSE;

	if (GetModuleHandle("samp.dll"))
	{
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThrMainFS, NULL, 0, NULL);
	}

	return TRUE;  
}