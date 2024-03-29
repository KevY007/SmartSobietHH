#include "main.h"

struct stSAMP* g_stSAMP;
struct stChatInfo* g_Chat;
struct stInputInfo* g_Input;

struct stSAMP_037* g_stSAMP037;
struct stChatInfo_037* g_Chat037;
struct stInputInfo_037* g_Input037;
struct pool* pool_actor;

static signed char hex_to_dec(signed char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	if (ch >= 'a' && ch <= 'f')
		return ch - 'A' + 10;

	return -1;
}

uint8_t* hex_to_bin(const char* str)
{
	int		len = (int)strlen(str);
	uint8_t* buf, * sbuf;

	if (len == 0 || len % 2 != 0)
		return NULL;

	sbuf = buf = (uint8_t*)malloc(len / 2);

	while (*str)
	{
		signed char bh = hex_to_dec(*str++);
		signed char bl = hex_to_dec(*str++);

		if (bl == -1 || bh == -1)
		{
			free(sbuf);
			return NULL;
		}

		*buf++ = (uint8_t)(bl | (bh << 4));
	}

	return sbuf;
}

bool isBadPtr_handlerAny(void* pointer, ULONG size, DWORD dwFlags)
{
	DWORD						dwSize;
	MEMORY_BASIC_INFORMATION	meminfo;

	if (NULL == pointer)
		return true;

	memset(&meminfo, 0x00, sizeof(meminfo));
	dwSize = VirtualQuery(pointer, &meminfo, sizeof(meminfo));

	if (0 == dwSize)
		return true;

	if (MEM_COMMIT != meminfo.State)
		return true;

	if (0 == (meminfo.Protect & dwFlags))
		return true;

	if (size > meminfo.RegionSize)
		return true;

	if ((unsigned)((char*)pointer - (char*)meminfo.BaseAddress) > (unsigned)(meminfo.RegionSize - size))
		return true;

	return false;
}

bool isBadPtr_readAny(void* pointer, ULONG size)
{
	return isBadPtr_handlerAny(pointer, size, PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
		PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY);
}

bool isBadPtr_writeAny(void* pointer, ULONG size)
{
	return isBadPtr_handlerAny(pointer, size,
		PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY);
}

static int __page_size_get(void)
{
	static int	page_size = -1;
	SYSTEM_INFO si;

	if (page_size == -1)
	{
		GetSystemInfo(&si);
		page_size = (int)si.dwPageSize;
	}

	return page_size;
}

static int __page_write(void* _dest, const void* _src, uint32_t len)
{
	static int		page_size = __page_size_get();
	uint8_t* dest = (uint8_t*)_dest;
	const uint8_t* src = (const uint8_t*)_src;
	DWORD			prot_prev = 0;
	int				prot_changed = 0;
	int				ret = 1;

	while (len > 0)
	{
		ret = 1;
		int page_offset = (int)((UINT_PTR)dest % page_size);
		int page_remain = page_size - page_offset;
		int this_len = len;

		if (this_len > page_remain)
			this_len = page_remain;

		if (isBadPtr_writeAny(dest, this_len))
		{
			if (!VirtualProtect((void*)dest, this_len, PAGE_EXECUTE_READWRITE, &prot_prev))
				ret = 0;
			else
				prot_changed = 1;
		}

		if (ret)
			memcpy(dest, src, this_len);

		if (prot_changed)
		{
			DWORD	dummy;
			if (!VirtualProtect((void*)dest, this_len, prot_prev, &dummy))
				printf("__page_write() could not restore original permissions for ptr %p\n", dest);
		}

		dest += this_len;
		src += this_len;
		len -= this_len;
	}

	return ret;
}

static int __page_read(void* _dest, const void* _src, uint32_t len)
{
	static int	page_size = __page_size_get();
	uint8_t* dest = (uint8_t*)_dest;
	uint8_t* src = (uint8_t*)_src;
	DWORD		prot_prev = 0;
	int			prot_changed = 0;
	int			ret = 1;

	while (len > 0)
	{
		ret = 1;
		int page_offset = (int)((UINT_PTR)src % page_size);
		int page_remain = page_size - page_offset;
		int this_len = len;

		if (this_len > page_remain)
			this_len = page_remain;

		if (isBadPtr_readAny(src, this_len))
		{
			if (!VirtualProtect((void*)src, this_len, PAGE_EXECUTE_READWRITE, &prot_prev))
				ret = 0;
			else
				prot_changed = 1;
		}

		if (ret)
			memcpy(dest, src, this_len);
		else
			memset(dest, 0, this_len);

		if (prot_changed)
		{
			DWORD	dummy;
			if (!VirtualProtect((void*)src, this_len, prot_prev, &dummy))
				printf("__page_read() could not restore original permissions for ptr %p\n", src);
		}

		dest += this_len;
		src += this_len;
		len -= this_len;
	}

	return ret;
}

int memcpy_safe(void* _dest, const void* _src, uint32_t len, int check, const void* checkdata)
{
	static int		page_size = __page_size_get();
	static int		recurse_ok = 1;
	uint8_t			buf[4096];
	uint8_t* dest = (uint8_t*)_dest;
	const uint8_t* src = (const uint8_t*)_src;
	int				ret = 1;

	if (check && checkdata)
	{
		if (!memcmp_safe(checkdata, _dest, len))
			return 0;
	}

	while (len > 0)
	{
		uint32_t	this_len = sizeof(buf);

		if (this_len > len)
			this_len = len;

		if (!__page_read(buf, src, this_len))
			ret = 0;

		if (!__page_write(dest, buf, this_len))
			ret = 0;

		len -= this_len;
		src += this_len;
		dest += this_len;
	}

	return ret;
}

int memcmp_safe(const void* _s1, const void* _s2, uint32_t len)
{
	const uint8_t* s1 = (const uint8_t*)_s1;
	const uint8_t* s2 = (const uint8_t*)_s2;
	uint8_t			buf[4096];

	for (;; )
	{
		if (len > 4096)
		{
			if (!memcpy_safe(buf, s1, 4096))
				return 0;
			if (memcmp(buf, s2, 4096))
				return 0;
			s1 += 4096;
			s2 += 4096;
			len -= 4096;
		}
		else
		{
			if (!memcpy_safe(buf, s1, len))
				return 0;
			if (memcmp(buf, s2, len))
				return 0;
			break;
		}
	}

	return 1;
}

void SetupSAMPHook(const char* szName, DWORD dwFuncOffset, void* Func, int iType, int iSize, const char* szCompareBytes)
{
	CDetour api;
	int strl = strlen(szCompareBytes);
	uint8_t* bytes = hex_to_bin(szCompareBytes);

	if (!strl || !bytes || memcmp_safe((uint8_t*)g_SAMP + dwFuncOffset, bytes, strl / 2))
	{
		if (api.Create((uint8_t*)((uint32_t)g_SAMP) + dwFuncOffset, (uint8_t*)Func, iType, iSize) == 0)
			printf("[HOOK] Failed to hook %s.\n", szName);
	}
	else
	{
		printf("[HOOK] Failed to hook %s (memcmp)\n", szName);
	}

	if (bytes)
		free(bytes);
}

template<typename T>
T GetSAMPPtrInfo(uint32_t offset)
{
	return *(T*)(g_SAMP + offset);
}

struct stSAMP_037* stGetSampInfo037(void)
{
	return GetSAMPPtrInfo<stSAMP_037*>(SAMP037_INFO_OFFSET);
}

struct stChatInfo_037* stGetSampChatInfo037(void)
{
	return GetSAMPPtrInfo<stChatInfo_037*>(SAMP037_CHAT_INFO_OFFSET);
}

struct stInputInfo_037* stGetInputInfo037(void)
{
	return GetSAMPPtrInfo<stInputInfo_037*>(SAMP037_CHAT_INPUT_INFO_OFFSET);
}

struct stSAMP* stGetSampInfo(void)
{
	return GetSAMPPtrInfo<stSAMP*>(SAMP_INFO_OFFSET);
}

struct stChatInfo* stGetSampChatInfo(void)
{
	return GetSAMPPtrInfo<stChatInfo*>(SAMP_CHAT_INFO_OFFSET);
}

struct stInputInfo* stGetInputInfo(void)
{
	return GetSAMPPtrInfo<stInputInfo*>(SAMP_CHAT_INPUT_INFO_OFFSET);
}

void addMessageToChatWindow(D3DCOLOR col, const char* text, ...)
{
	if (g_stSAMP != NULL || g_stSAMP037 != NULL)
	{
		va_list ap;
		if (text == NULL)
			return;

		char	tmp[512];
		memset(tmp, 0, 512);

		va_start(ap, text);
		vsnprintf(tmp, sizeof(tmp) - 1, text, ap);
		va_end(ap);

		addToChatWindow(tmp, col);
	}
}

void __stdcall CNetGame__destructor(void)
{
	// release hooked rakclientinterface, restore original rakclientinterface address and call CNetGame destructor
	if (samp037)
	{
		if (g_stSAMP037->pRakClientInterface != NULL)
			delete g_stSAMP037->pRakClientInterface;
		g_stSAMP037->pRakClientInterface = g_RakClient->GetInterface();
		return ((void(__thiscall*) (void*)) (g_SAMP + SAMP037_FUNC_CNETGAMEDESTRUCTOR))(g_stSAMP037);
	}
	else
	{
		if (g_stSAMP->pRakClientInterface != NULL)
			delete g_stSAMP->pRakClientInterface;
		g_stSAMP->pRakClientInterface = g_RakClient->GetInterface();
		return ((void(__thiscall*) (void*)) (g_SAMP + SAMP_FUNC_CNETGAMEDESTRUCTOR))(g_stSAMP);
	}
}

uint8_t _declspec (naked) hook_handle_rpc_packet(void)
{
	static RPCParameters* pRPCParams = nullptr;
	static RPCNode* pRPCNode = nullptr;
	static DWORD dwTmp = 0;

	__asm pushad;
	__asm mov pRPCParams, eax;
	__asm mov pRPCNode, edi;

	HandleRPCPacketFunc(pRPCNode->uniqueIdentifier, pRPCParams, pRPCNode->staticFunctionPointer);

	if (samp037) dwTmp = g_SAMP + SAMP037_HOOKEXIT_HANDLE_RPC;
	else dwTmp = g_SAMP + SAMP_HOOKEXIT_HANDLE_RPC;

	__asm popad;
	__asm add esp, 4 // overwritten code
	__asm jmp dwTmp;
}


uint8_t _declspec (naked) hook_handle_rpc_packet2(void)
{
	static RPCParameters* pRPCParams = nullptr;
	static RPCNode* pRPCNode = nullptr;
	static DWORD dwTmp = 0;

	__asm pushad;
	__asm mov pRPCParams, ecx;
	__asm mov pRPCNode, edi;

	HandleRPCPacketFunc(pRPCNode->uniqueIdentifier, pRPCParams, pRPCNode->staticFunctionPointer);
	if (samp037) dwTmp = g_SAMP + SAMP037_HOOKEXIT_HANDLE_RPC2;
	else dwTmp = g_SAMP + SAMP_HOOKEXIT_HANDLE_RPC2;

	__asm popad;
	__asm jmp dwTmp;
}

void addToChatWindow(char* text, D3DCOLOR textColor, ChatMessageType msgTy)
{
	if (g_stSAMP == NULL && g_stSAMP037 == NULL)
		return;

	if (g_Chat == NULL && g_Chat037 == NULL)
		return;

	if (text == NULL)
		return;

	if (samp037)
	{
		void(__thiscall * AddToChatWindowBuffer) (void*, ChatMessageType, const char*, const char*, D3DCOLOR, D3DCOLOR) =
			(void(__thiscall*) (void* _this, ChatMessageType Type, const char* szString, const char* szPrefix, D3DCOLOR TextColor, D3DCOLOR PrefixColor))
			(g_SAMP + SAMP037_FUNC_ADDTOCHATWND);

		AddToChatWindowBuffer(g_Chat037, msgTy, text, NULL, textColor, NULL);
	}
	else
	{
		void(__thiscall * AddToChatWindowBuffer) (void*, ChatMessageType, const char*, const char*, D3DCOLOR, D3DCOLOR) =
			(void(__thiscall*) (void* _this, ChatMessageType Type, const char* szString, const char* szPrefix, D3DCOLOR TextColor, D3DCOLOR PrefixColor))
			(g_SAMP + SAMP_FUNC_ADDTOCHATWND);

		AddToChatWindowBuffer(g_Chat, msgTy, text, NULL, textColor, NULL);
	}
}

void say(char* msg)
{
	if (g_SAMP == NULL)
		return;

	if (msg == NULL)
		return;

	if (isBadPtr_readAny(msg, 128))
		return;

	if (msg == NULL)
		return;

	if (msg[0] == '/')
	{
		if(samp037) ((void(__thiscall*) (void* _this, char* message)) (g_SAMP + SAMP037_FUNC_SENDCMD))(g_Input037, msg);
		else ((void(__thiscall*) (void* _this, char* message)) (g_SAMP + SAMP_FUNC_SENDCMD))(g_Input, msg);
	}
	else
	{
		if(samp037) ((void(__thiscall*) (void* _this, char* message)) (g_SAMP + SAMP037_FUNC_SAY)) (g_stSAMP037->pPools->pPlayer->pLocalPlayer, msg);
		else ((void(__thiscall*) (void* _this, char* message)) (g_SAMP + SAMP_FUNC_SAY)) (g_stSAMP->pPools->pPlayer->pLocalPlayer, msg);
	}
}

void dispGameText(char* text, unsigned int time, unsigned short style)
{
	return ((void(__cdecl*)(char*, unsigned int, unsigned short))0x69F2B0)(text, time, style);
	//return addMessageToChatWindow(D3DCOLOR_XRGB(255, 255, 255), szText);
}


void addClientCommand(char* name, CMDPROC function)
{
	if (name == NULL || function == NULL || (g_Input == NULL && g_Input037 == NULL))
		return;

	if (g_Input->iCMDCount == (SAMP_MAX_CLIENTCMDS - 1))
	{
		//Couldn't initialize... limit.
		return;
	}

	if (strlen(name) > 30)
	{
		// Not happening anytime soon.
		return;
	}

	if (samp037)
		((void(__thiscall*) (void* _this, char* command, CMDPROC function)) (g_SAMP + SAMP037_FUNC_ADDCLIENTCMD)) (g_Input037, name, function);
	else
		((void(__thiscall*) (void* _this, char* command, CMDPROC function)) (g_SAMP + SAMP_FUNC_ADDCLIENTCMD)) (g_Input, name, function);
}

int isBadPtr_GTA_pPed(actor_info* pActorInfo)
{
	if (pActorInfo == NULL)
		return 1;
	if (!
		(
			(DWORD)pActorInfo >= (DWORD)pool_actor->start
			&& (DWORD)pActorInfo <= ((DWORD)pool_actor->start + (pool_actor->size * sizeof(actor_info)))
			)
		)
		return 1;
	return (pActorInfo->base.matrix == NULL) || (pActorInfo->base.nType != 3);
}

struct actor_info* actor_info_get_self()
{
	struct actor_info* info;

	info = (struct actor_info*)(UINT_PTR) * (uint32_t*)0x00B7CD98;

	if (isBadPtr_GTA_pPed(info))
		return NULL;

	return info;
}