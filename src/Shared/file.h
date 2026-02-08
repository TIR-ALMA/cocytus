#ifndef __FILE_H__
#define __FILE_H__

#include <windows.h>

BOOL FileGetSize(HANDLE FileHandle, PDWORD FileSize);
BOOL FileOpen(HANDLE* FileHandle, LPCWSTR Path, ACCESS_MASK AccessMask, ULONG CreateDisposition);
BOOL FileWrite(HANDLE FileHandle, LPCVOID Buffer, DWORD Length);
BOOL FileRead(HANDLE FileHandle, LPVOID* Buffer, DWORD Length, PDWORD ReadLength);
BOOL FileWriteBuffer(LPCWSTR Path, LPCVOID Buffer, DWORD Length, BOOL Append);
BOOL FileReadBuffer(LPCWSTR Path, LPVOID* Buffer, PDWORD Length);
BOOL FileCreateDirectory(LPCWSTR Path);
BOOL FileDelete(LPCWSTR Path);
BOOL FileCopy(LPCWSTR OriginalPath, LPCWSTR NewPath, BOOL DeleteOriginal);
BOOL DosPathToNtPath(LPWSTR* Path);

#endif

