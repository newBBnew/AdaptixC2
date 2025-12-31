#pragma once

#include <windows.h>

#ifndef PROFILE_STRUCT
#define PROFILE_STRUCT
typedef struct {
	ULONG  servers_count;
	BYTE** servers;
	WORD*  ports;
	BOOL   use_ssl;
	BYTE*  http_method;
	BYTE*  uri;
	BYTE*  parameter;
	BYTE*  user_agent;
	BYTE*  http_headers;
	ULONG  ans_pre_size;
	ULONG  ans_size;
} ProfileHTTP;

typedef struct {
	BYTE* pipename;
} ProfileSMB;

typedef struct {
	BYTE* prepend;
	WORD  port;
} ProfileTCP;

typedef struct {
	BYTE* domain;
	BYTE* resolvers;
	BYTE* qtype;
	ULONG pkt_size;
	ULONG label_size;
	ULONG ttl;
	BYTE* encrypt_key;
} ProfileDNS;

typedef struct {
	BYTE*  domain;
	BYTE*  urls;
	BYTE*  user_agent;
	ULONG  pkt_size;
	ULONG  label_size;
	ULONG  ttl;
	BYTE*  encrypt_key;
	BYTE*  doh_mode;
} ProfileDoH;

typedef struct {
	ProfileDNS dns;
	ProfileDoH doh;
	BYTE*      mode;
	BYTE*      doh_mode;
} ProfileDNSDoH;
#endif



class AgentConfig
{
public:
	BOOL active;

	ULONG agent_type;
	ULONG listener_type;
	BYTE* encrypt_key;
	ULONG sleep_delay;
	ULONG jitter_delay;
	ULONG kill_date;
	ULONG working_time;

	BYTE  exit_method;
	ULONG exit_task_id;
	ULONG download_chunk_size;

#if defined(BEACON_HTTP)
	ProfileHTTP profile;

#elif defined(BEACON_SMB)
	ProfileSMB profile;

#elif defined(BEACON_TCP)
	ProfileTCP profile;

#elif defined(BEACON_DNS)
	ProfileDNS profile;

#elif defined(BEACON_DOH)
	ProfileDoH profile;

#elif defined(BEACON_DNS_DOH)
	ProfileDNSDoH profile;

#endif

	AgentConfig();

	static void* operator new(size_t sz);
	static void operator delete(void* p) noexcept;
};