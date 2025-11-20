#pragma once

#include "main.h"

// Simple DEFLATE helpers for DNS beacon session layer using miniz/zlib-style API.
// Both functions allocate output buffers via MemAllocLocal; caller must free
// them with MemFreeLocal when no longer needed.

BOOL DeflateCompress(const BYTE* inBuf, ULONG inLen, BYTE** outBuf, ULONG* outLen);
BOOL DeflateDecompress(const BYTE* inBuf, ULONG inLen, BYTE** outBuf, ULONG expectedLen);
