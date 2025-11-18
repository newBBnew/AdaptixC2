#include "Crypt.h"
#include "ApiLoader.h"
#include "utils.h"

void RC4Init(unsigned char* key, unsigned char* S, int keyLength) {
    int i, j = 0;
    unsigned char temp;

    for (i = 0; i < 256; i++) {
        S[i] = (unsigned char)i;
    }

    for (i = 0; i < 256; i++) {
        j = (j + S[i] + key[i % keyLength]) % 256;
        temp = S[i];
        S[i] = S[j];
        S[j] = temp;
    }
}

void RC4EncryptDecrypt(unsigned char* data, int dataLength, unsigned char* S) {
    int i = 0, j = 0, k;
    unsigned char temp;

    for (k = 0; k < dataLength; k++) {
        i = (i + 1) % 256;
        j = (j + S[i]) % 256;

        temp = S[i];
        S[i] = S[j];
        S[j] = temp;

        data[k] ^= S[(S[i] + S[j]) % 256];
    }
}

void EncryptRC4(unsigned char* data, int dataLength, unsigned char* key, int keyLength) {
    unsigned char S[256];
    RC4Init(key, S, keyLength);
    RC4EncryptDecrypt(data, dataLength, S);
}

void DecryptRC4(unsigned char* data, int dataLength, unsigned char* key, int keyLength) {
    EncryptRC4(data, dataLength, key, keyLength);
}

BOOL CompressLZNT1(const BYTE* src, ULONG srcLen, BYTE** dst, ULONG* dstLen)
{
    if (!src || !srcLen || !dst || !dstLen || !ApiNt || !ApiNt->RtlGetCompressionWorkSpaceSize || !ApiNt->RtlCompressBuffer)
        return FALSE;

    USHORT format = COMPRESSION_FORMAT_LZNT1 | COMPRESSION_ENGINE_STANDARD;
    ULONG ws1 = 0, ws2 = 0;
    if (!NT_SUCCESS(ApiNt->RtlGetCompressionWorkSpaceSize(format, &ws1, &ws2)))
        return FALSE;

    PVOID work = MemAllocLocal(ws1);
    if (!work)
        return FALSE;

    // 粗略估计压缩缓冲大小：原始大小 + 1/16 + 0x1000
    ULONG outCap = srcLen + (srcLen / 16) + 0x1000;
    BYTE* outBuf = (BYTE*)MemAllocLocal(outCap);
    if (!outBuf) {
        MemFreeLocal(&work, ws1);
        return FALSE;
    }

    ULONG outSize = 0;
    NTSTATUS st = ApiNt->RtlCompressBuffer(format, (PUCHAR)src, srcLen, outBuf, outCap, 4096, &outSize, work);
    MemFreeLocal(&work, ws1);
    if (!NT_SUCCESS(st) || outSize == 0 || outSize > outCap) {
        MemFreeLocal((LPVOID*)&outBuf, outCap);
        return FALSE;
    }

    *dst    = outBuf;
    *dstLen = outSize;
    return TRUE;
}

BOOL DecompressLZNT1(const BYTE* src, ULONG srcLen, BYTE* dst, ULONG dstLen)
{
    if (!src || !srcLen || !dst || !dstLen || !ApiNt || !ApiNt->RtlDecompressBuffer)
        return FALSE;

    USHORT format = COMPRESSION_FORMAT_LZNT1;
    ULONG outSize = 0;
    NTSTATUS st = ApiNt->RtlDecompressBuffer(format, (PUCHAR)dst, dstLen, (PUCHAR)src, srcLen, &outSize);
    if (!NT_SUCCESS(st) || outSize != dstLen)
        return FALSE;
    return TRUE;
}