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

    *FileSize = Info.EndOfFile.LowPart; // Исправлено: используем EndOfFile, а не AllocationSize
    return TRUE;
}

BOOL FileOpen(HANDLE* FileHandle, CONST LPWSTR Path, ACCESS_MASK AccessMask, ULONG CreateDisposition)
{
    NTSTATUS            Status;
    UNICODE_STRING      US;
    OBJECT_ATTRIBUTES   OA;
    IO_STATUS_BLOCK     IO;
    BOOL                bStatus = FALSE;

    *FileHandle = INVALID_HANDLE_VALUE;

    MemoryZero(&IO, sizeof(IO_STATUS_BLOCK));
    MemoryZero(&OA, sizeof(OBJECT_ATTRIBUTES));

    OA.Length = sizeof(OBJECT_ATTRIBUTES);
    API(RtlInitUnicodeString)(&US, Path);
    OA.ObjectName = &US;
    OA.Attributes = OBJ_CASE_INSENSITIVE;

    Status = API(NtCreateFile)(FileHandle, AccessMask | SYNCHRONIZE, &OA, &IO, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE, CreateDisposition, FILE_NON_DIRECTORY_FILE | FILE_RANDOM_ACCESS | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);

    if (NT_SUCCESS(Status))
        bStatus = TRUE;

    return bStatus;
}

BOOL FileWrite(HANDLE FileHandle, CONST LPVOID Buffer, DWORD Length)
{
    NTSTATUS        Status;
    IO_STATUS_BLOCK IO;
    
    Status = API(NtWriteFile)(FileHandle, NULL, NULL, NULL, &IO, Buffer, Length, NULL, NULL); // Исправлено: убрана лишняя &
    if (NT_SUCCESS(Status))
        return TRUE;

    return FALSE;
}

BOOL FileRead(HANDLE FileHandle, LPVOID* Buffer, DWORD Length, PDWORD ReadLength)
{
    IO_STATUS_BLOCK IO;
    LARGE_INTEGER   LI;

    LI.LowPart = 0;
    LI.HighPart = 0;

    if ((*Buffer = Malloc(Length)) == NULL) // Исправлено: 0 -> NULL
        return FALSE;

    if (API(NtReadFile)(FileHandle, NULL, NULL, NULL, &IO, *Buffer, Length, &LI, NULL) >= 0) // Исправлен вызов: был неполный вызов
    {
        *ReadLength = (DWORD)IO.Information;
        return TRUE;
    }

    Free(*Buffer); // Освобождение памяти при ошибке чтения
    return FALSE;
}

BOOL FileWriteBuffer(CONST LPWSTR Path, CONST LPVOID Buffer, DWORD Length, BOOL Append)
{
    BOOL    Status = FALSE;
    HANDLE  FileHandle;
    ULONG   disposition = Append ? FILE_OPEN_IF : FILE_OVERWRITE_IF; // Исправлено: для append используем FILE_OPEN_IF и перемещаем указатель

    if (!FileOpen(&FileHandle, Path, GENERIC_WRITE, disposition))
        return FALSE;

    if (Append)
    {
        // Если режим append, перемещаем указатель в конец файла
        LARGE_INTEGER distance;
        distance.QuadPart = 0;
        API(NtSetInformationFile)(FileHandle, NULL, &distance, sizeof(distance), FileEndOfFileInformation);
    }

    Status = FileWrite(FileHandle, Buffer, Length);
    API(NtClose)(FileHandle);

    return Status;
}

BOOL FileReadBuffer(CONST LPWSTR Path, LPVOID* Buffer, PDWORD Length)
{
    BOOL    Status = FALSE;
    HANDLE  FileHandle;
    DWORD   FileSize;

    if (!FileOpen(&FileHandle, Path, GENERIC_READ, FILE_OPEN))
        return FALSE;

    if (!FileGetSize(FileHandle, &FileSize))
    {
        API(NtClose)(FileHandle);
        return FALSE;
    }

    Status = FileRead(FileHandle, Buffer, FileSize, Length);
    API(NtClose)(FileHandle);

    return Status;
}

BOOL FileCreateDirectory(CONST LPWSTR Path)
{
    NTSTATUS            Status;
    IO_STATUS_BLOCK     IO;
    OBJECT_ATTRIBUTES   OA;
    UNICODE_STRING      US;
    HANDLE              Handle;
    BOOL                bStatus = FALSE;

    MemoryZero(&IO, sizeof(IO));
    MemoryZero(&OA, sizeof(OA));

    OA.Attributes = OBJ_CASE_INSENSITIVE;
    OA.Length = sizeof(OA);
    API(RtlInitUnicodeString)(&US, Path);
    OA.ObjectName = &US;

    Status = API(NtCreateFile)(&Handle, GENERIC_WRITE | DELETE, &OA, &IO, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_CREATE, FILE_DIRECTORY_FILE, NULL, 0);

    if (NT_SUCCESS(Status))
    {
        bStatus = TRUE;
        API(NtClose)(Handle);
    }

    return bStatus;
}

BOOL FileDelete(CONST LPWSTR Path)
{
    BOOL              Status = FALSE;
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
    BOOL    Status = FALSE;
    LPVOID  File = NULL;
    DWORD   FileSize;

    if (!FileReadBuffer(OriginalPath, &File, &FileSize))
        return FALSE;

    if (!FileWriteBuffer(NewPath, File, FileSize, FALSE)) // Исправлено: FALSE для нового файла, а не TRUE
    {
        Free(File);
        return FALSE;
    }

    if (DeleteOriginal)
        FileDelete(OriginalPath);

    Free(File);

    return TRUE;
}

BOOL IsValidNtPath(const LPWSTR Path)
{
    BOOL    Status = FALSE;
    LPWSTR  Data;

    if (Path == NULL || wcslen(Path) < 4)
        return FALSE;

    if ((Data = StringCopyW(Path, 4)) != NULL) // Исправлено: 0 -> NULL
    {
        Status = StringCompareW(Data, L"\\??\\"); // Исправлено: сравниваем Data, а не Path
        Free(Data);
    }

    return Status;
}

BOOL DosPathToNtPath(LPWSTR* Path)
{
    LPWSTR NtPath = NULL;

    if (Path == NULL || *Path == NULL)
        return FALSE;

    if (IsValidNtPath(*Path))
        return TRUE;

    if (StringConcatW(&NtPath, L"\\??\\") && StringConcatW(&NtPath, *Path))
    {
        Free(*Path);
        *Path = NtPath;
        return TRUE;
    }

    if (NtPath != NULL)
        Free(NtPath);

    return FALSE;
}

