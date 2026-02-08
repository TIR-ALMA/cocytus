#include <Windows.h>
#include "file.h"
#include "crt.h"
#include "utils.h"

BOOL FileGetSize(HANDLE FileHandle, PDWORD FileSize)
{
	LARGE_INTEGER size;
	if (GetFileSizeEx(FileHandle, &size))
	{
		*FileSize = (DWORD)size.QuadPart;
		return TRUE;
	}
	*FileSize = 0;
	return FALSE;
}

BOOL FileOpen(HANDLE* FileHandle, CONST LPWSTR Path, ACCESS_MASK AccessMask, ULONG CreateDisposition)
{
	DWORD desiredAccess = 0;
	DWORD creationDisposition = 0;

	// Преобразование маски доступа
	if (AccessMask & GENERIC_READ) desiredAccess |= GENERIC_READ;
	if (AccessMask & GENERIC_WRITE) desiredAccess |= GENERIC_WRITE;
	if (AccessMask & GENERIC_EXECUTE) desiredAccess |= GENERIC_EXECUTE;
	if (AccessMask & GENERIC_ALL) desiredAccess |= GENERIC_ALL;

	// Преобразование режима создания
	switch (CreateDisposition)
	{
	case FILE_CREATE:
		creationDisposition = CREATE_NEW;
		break;
	case FILE_OPEN:
		creationDisposition = OPEN_EXISTING;
		break;
	case FILE_OPEN_IF:
		creationDisposition = OPEN_ALWAYS;
		break;
	case FILE_OVERWRITE:
		creationDisposition = TRUNCATE_EXISTING;
		break;
	case FILE_OVERWRITE_IF:
		creationDisposition = CREATE_ALWAYS;
		break;
	default:
		creationDisposition = OPEN_EXISTING;
	}

	*FileHandle = CreateFileW(
		Path,
		desiredAccess,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		creationDisposition,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	return (*FileHandle != INVALID_HANDLE_VALUE);
}

BOOL FileWrite(HANDLE FileHandle, CONST LPVOID Buffer, DWORD Length)
{
	DWORD bytesWritten;
	return WriteFile(FileHandle, Buffer, Length, &bytesWritten, NULL) &&
		   bytesWritten == Length;
}

BOOL FileRead(HANDLE FileHandle, LPVOID* Buffer, DWORD Length, PDWORD ReadLength)
{
	DWORD bytesRead;

	if ((*Buffer = Malloc(Length)) == NULL)
		return FALSE;

	if (ReadFile(FileHandle, *Buffer, Length, &bytesRead, NULL))
	{
		*ReadLength = bytesRead;
		return TRUE;
	}

	Free(*Buffer);
	return FALSE;
}

BOOL FileWriteBuffer(CONST LPWSTR Path, CONST LPVOID Buffer, DWORD Length, BOOL Append)
{
	HANDLE fileHandle;
	DWORD flags = FILE_ATTRIBUTE_NORMAL;
	DWORD disposition = Append ? OPEN_ALWAYS : CREATE_ALWAYS;

	fileHandle = CreateFileW(
		Path,
		GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		disposition,
		flags,
		NULL
	);

	if (fileHandle == INVALID_HANDLE_VALUE)
		return FALSE;

	if (Append)
		SetFilePointer(fileHandle, 0, NULL, FILE_END);

	DWORD bytesWritten;
	BOOL result = WriteFile(fileHandle, Buffer, Length, &bytesWritten, NULL) &&
				  bytesWritten == Length;

	CloseHandle(fileHandle);
	return result;
}

BOOL FileReadBuffer(CONST LPWSTR Path, LPVOID* Buffer, PDWORD Length)
{
	HANDLE fileHandle;
	HANDLE hFile = CreateFileW(
		Path,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(hFile, &fileSize))
	{
		CloseHandle(hFile);
		return FALSE;
	}

	DWORD dwFileSize = (DWORD)fileSize.QuadPart;
	LPVOID pBuffer = Malloc(dwFileSize);
	if (!pBuffer)
	{
		CloseHandle(hFile);
		return FALSE;
	}

	DWORD bytesRead;
	BOOL result = ReadFile(hFile, pBuffer, dwFileSize, &bytesRead, NULL);

	CloseHandle(hFile);

	if (result)
	{
		*Buffer = pBuffer;
		*Length = bytesRead;
		return TRUE;
	}

	Free(pBuffer);
	return FALSE;
}

BOOL FileCreateDirectory(CONST LPWSTR Path)
{
	return CreateDirectoryW(Path, NULL);
}

BOOL FileDelete(CONST LPWSTR Path)
{
	return DeleteFileW(Path);
}

BOOL FileCopy(CONST LPWSTR OriginalPath, CONST LPWSTR NewPath, BOOL DeleteOriginal)
{
	BOOL result = CopyFileW(OriginalPath, NewPath, FALSE);
	if (result && DeleteOriginal)
		DeleteFileW(OriginalPath);
	return result;
}

BOOL IsValidNtPath(const LPWSTR Path)
{
	if (!Path || wcslen(Path) < 4) return FALSE;
	return (wcsncmp(Path, L"\\??\\", 4) == 0);
}

BOOL DosPathToNtPath(LPWSTR* Path)
{
	if (IsValidNtPath(*Path))
		return TRUE;

	LPWSTR newPath = (LPWSTR)Malloc((wcslen(L"\\??\\") + wcslen(*Path) + 1) * sizeof(WCHAR));
	if (!newPath) return FALSE;

	wcscpy(newPath, L"\\??\\");
	wcscat(newPath, *Path);

	Free(*Path);
	*Path = newPath;
	return TRUE;
}

