#pragma once

#include "AgentConfig.h"
#include <windows.h>

class ConnectorDNS
{
private:
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
    BYTE* hiBeat = NULL;
    ULONG hiBeatSize = 0;
    ULONG hiRetries = 3; // how many times we may re-attempt HI when no data is provided
    ULONG seq = 0;
    ULONG idx = 0;

    BYTE* recvData = NULL;
    int   recvSize = 0;

    // downlink reassembly buffer for application-layer fragmentation
    BYTE* downBuf    = NULL;
    ULONG downTotal  = 0;
    ULONG downFilled = 0;
    ULONG downAckOffset = 0; // last acknowledged offset for ACK mechanism

    // compression / flow-control flags
    BOOL  compressEnabled = TRUE;   // allow payload compression for large frames
    ULONG lastDownTotal   = 0;      // last full downlink size (for adaptive sleep)
    ULONG lastUpTotal     = 0;      // last full uplink size (for adaptive sleep)

    // Indicates whether the last DNS query (HI/PUT/GET) completed successfully
    // from the connector's perspective (DnsQueryTxt returned TRUE and response
    // parsed without fatal errors). Used by higher-level logic to drive
    // transport failover decisions.
    BOOL  lastQueryOk     = FALSE;

public:
    ConnectorDNS();

    BOOL SetConfig(ProfileDNS profile, BYTE* beat, ULONG beatSize);
    void CloseConnector();

    void  SendData(BYTE* data, ULONG data_size);
    BYTE* RecvData();
    int   RecvSize();
    void  RecvClear();

    // Helpers for adaptive sleep in AgentMain (read-only + reset)
    ULONG GetLastUpTotal() const { return lastUpTotal; }
    ULONG GetLastDownTotal() const { return lastDownTotal; }
    void  ResetTrafficTotals() { lastUpTotal = 0; lastDownTotal = 0; }
    
    BOOL  WasLastQueryOk() const { return lastQueryOk; }

    const BYTE* GetResolvers() const { return profile.resolvers; }
    void        UpdateResolvers(BYTE* resolvers);

    BOOL  IsBusy() const { return (downBuf != NULL); }
    ULONG GetDownAckOffset() const { return downAckOffset; }
};
