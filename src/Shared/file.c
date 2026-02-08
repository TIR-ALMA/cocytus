#include <Windows.h>

#include "file.h"
#include "crt.h"
#include "ntdll.h"
#include "nzt.h"
#include "utils.h"

BOOL FileGetInfo(HANDLE FileHandle, PFILE_STANDARD_INFORMATION Info)
{
	IO_STATUS_BLOCK IO;

	MemoryZero(&IO, sizeof(IO_STATUS_BLOCK));
	MemoryZero(Info, sizeof(FILE_STANDARD_INFORMATION));

	if (API(NtQueryInformationFile)(FileHandle, &IO, Info, sizeof(FILE_STANDARD_INFORMATION), FileStandardInformation) >= 0)
		return TRUE;

	return FALSE;
}

BOOL FileGetSize(HANDLE FileHandle, PDWORD FileSize)
{
	FILE_STANDARD_INFORMATION Info;

	*FileSize = 0;

	if (!FileGetInfo(FileHandle, &Info))
		return FALSE;

	*FileSize = (DWORD)Info.EndOfFile.LowPart; // Исправлено: используем EndOfFile вместо AllocationSize
	return TRUE;
}

BOOL FileOpen(HANDLE* FileHandle, CONST LPWSTR Path, ACCESS_MASK AccessMask, ULONG CreateDisposition)
{
	NTSTATUS			Status;
	UNICODE_STRING		US;
	OBJECT_ATTRIBUTES	OA;
	IO_STATUS_BLOCK		IO;
	BOOL				bStatus = FALSE;

	*FileHandle = INVALID_HANDLE_VALUE;

	MemoryZero(&IO, sizeof(IO_STATUS_BLOCK));
	MemoryZero(&OA, sizeof(OBJECT_ATTRIBUTES));

	OA.Length = sizeof(OBJECT_ATTRIBUTES);
	API(RtlInitUnicodeString)(&US, Path);
	OA.ObjectName = &US;
	OA.Attributes = OBJ_CASE_INSENSITIVE;

	Status = API(NtCreateFile)(FileHandle, AccessMask | SYNCHRONIZE, &OA, &IO, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE, CreateDisposition, FILE_NON_DIRECTORY_FILE | FILE_RANDOM_ACCESS | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0); // Добавлен оператор API()

	if (NT_SUCCESS(Status))
		bStatus = TRUE;

	return bStatus;
}

BOOL FileWrite(HANDLE FileHandle, CONST LPVOID Buffer, DWORD Length)
{
	NTSTATUS		Status;
	IO_STATUS_BLOCK IO;
	
	Status = API(NtWriteFile)(FileHandle, NULL, NULL, NULL, &IO, Buffer, Length, NULL, NULL); // Убраны лишние &
	if (NT_SUCCESS(Status))
		return TRUE;

	return FALSE;
}

BOOL FileRead(HANDLE FileHandle, LPVOID* Buffer, DWORD Length, PDWORD ReadLength)
{
	IO_STATUS_BLOCK IO;
	LARGE_INTEGER	LI;

	LI.LowPart = 0;
	LI.HighPart = 0;

	if ((*Buffer = Malloc(Length)) == 0)
		return FALSE;

	if (API(NtReadFile)(FileHandle, 0, 0, 0, &IO, *Buffer, Length, &LI, 0) >= 0) // Исправлен вызов функции
	{
		*ReadLength = (DWORD)IO.Information;
		return TRUE;
	}

	Free(*Buffer); // Освобождение памяти при ошибке
	return FALSE;
}

BOOL FileWriteBuffer(CONST LPWSTR Path, CONST LPVOID Buffer, DWORD Length, BOOL Append)
{
	BOOL	Status = FALSE;
	HANDLE	FileHandle;
	ULONG	CreateDisposition = Append ? FILE_OPEN_IF : FILE_OVERWRITE_IF; // Исправлено для корректного append

	if (!FileOpen(&FileHandle, Path, GENERIC_WRITE, CreateDisposition))
		return FALSE; // Исправлено: возвращаем FALSE, а не Status

	if (Append) { // Для append устанавливаем указатель в конец файла
		LARGE_INTEGER li;
		li.QuadPart = 0;
		API(NtSetInformationFile)(FileHandle, &IO_STATUS_BLOCK{}, &li, sizeof(li), FileEndOfFileInformation);
	}
	
	Status = FileWrite(FileHandle, Buffer, Length);
	API(NtClose)(FileHandle);

	return Status;
}


BOOL FileReadBuffer(CONST LPWSTR Path, LPVOID* Buffer, PDWORD Length)
{
	BOOL   Status	  = FALSE;
	HANDLE FileHandle;
	DWORD  FileSize;

	if (!FileOpen(&FileHandle, Path, GENERIC_READ, FILE_OPEN))
		return FALSE; // Исправлено: возвращаем FALSE, а не Status

	if (!FileGetSize(FileHandle, &FileSize))
	{
		API(NtClose)(FileHandle);
		return FALSE; // Исправлено: закрываем файл и возвращаем FALSE
	}

	Status = FileRead(FileHandle, Buffer, FileSize, Length);
	API(NtClose)(FileHandle); // Исправлено: закрытие файла

	return Status;
}

BOOL FileCreateDirectory(CONST LPWSTR Path)
{
	NTSTATUS			Status;
	IO_STATUS_BLOCK		IO;
	OBJECT_ATTRIBUTES	OA;
	UNICODE_STRING		US;
	HANDLE				Handle;
	BOOL				bStatus = FALSE;

	MemoryZero(&IO, sizeof(IO));
	MemoryZero(&OA, sizeof(OA));

	OA.Attributes	= OBJ_CASE_INSENSITIVE;
	OA.Length		= sizeof(OA);
	API(RtlInitUnicodeString)(&US, Path);
	OA.ObjectName	= &US;

	Status = API(NtCreateFile)(&Handle, GENERIC_WRITE, &OA, &IO, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_CREATE, FILE_DIRECTORY_FILE, NULL, 0);

	if (NT_SUCCESS(Status))
	{
		bStatus = TRUE;
		API(NtClose)(Handle);
	}
	else
	{
		// Если директория уже существует, это тоже успех
		if (Status == STATUS_OBJECT_NAME_COLLISION)
			bStatus = TRUE;
	}

	return bStatus;
}

BOOL FileDelete(CONST LPWSTR Path)
{
	BOOL			  Status = FALSE;
	OBJECT_ATTRIBUTES OA;
	UNICODE_STRING    US;

	MemoryZero(&OA, sizeof(OBJECT_ATTRIBUTES));

	OA.Attributes = OBJ_CASE_INSENSITIVE;
	OA.Length = sizeof(OA);
	API(RtlInitUnicodeString)(&US, Path);
	OA.ObjectName = &US;

	if (API(NtDeleteFile)(&OA) >= 0)
		Status = TRUE;

	return Status;
}

BOOL FileCopy(CONST LPWSTR OriginalPath, CONST LPWSTR NewPath, BOOL DeleteOriginal)
{
	BOOL	Status	 = FALSE;
	LPVOID	File = NULL; // Инициализировано
	DWORD	FileSize = 0; // Инициализировано

	if (!FileReadBuffer(OriginalPath, &File, &FileSize))
		return FALSE; // Исправлено: возвращаем FALSE

	if (!FileWriteBuffer(NewPath, File, FileSize, FALSE)) // Исправлено: FALSE для append
	{
		Free(File);
		return FALSE; // Исправлено: освобождаем память и возвращаем FALSE
	}

	if (DeleteOriginal)
		FileDelete(OriginalPath);

	Free(File);

	return TRUE; // Исправлено: возвращаем TRUE при успехе
}

BOOL IsValidNtPath(const LPWSTR Path)
{
	if (!Path || wcslen(Path) < 4) return FALSE; // Проверка на валидность
	
	return (wcsncmp(Path, L"\\??\\", 4) == 0); // Исправлено: прямое сравнение
}

BOOL DosPathToNtPath(LPWSTR* Path)
{
	LPWSTR NtPath = NULL;

	if (IsValidNtPath(*Path))
		return TRUE;

	LPWSTR prefix = L"\\??\\";
	size_t prefix_len = wcslen(prefix);
	size_t path_len = wcslen(*Path);
	
	NtPath = (LPWSTR)Malloc((prefix_len + path_len + 1) * sizeof(WCHAR));
	if (!NtPath) return FALSE;
	
	wcscpy_s(NtPath, prefix_len + path_len + 1, prefix);
	wcscat_s(NtPath, prefix_len + path_len + 1, *Path);
	
	Free(*Path);
	*Path = NtPath;
	return TRUE;
}

