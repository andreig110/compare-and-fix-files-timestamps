#include <iostream>
#include <string>
#include <chrono>

#include <io.h>
#include <fcntl.h>
#include <windows.h>

using namespace std;
using namespace std::chrono;

struct ProgramOptions
{
    bool recurseSubDirs; // Recurse subdirectories.
    bool simulate; // Do not change files timestamp. The program will not make any changes to the file system.
} programOptions;

void PrintLastError()
{
    wcout << "- Last error: " << GetLastError() << endl;
}

void FixFileTime(const wstring& destFilePath, bool isFile,
    const FILETIME* creationTime, const FILETIME* lastAccessTime, const FILETIME* lastWriteTime)
{
    DWORD flagsAndAttributes;
    if (isFile)
        flagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
    else // if is directory
        flagsAndAttributes = FILE_FLAG_BACKUP_SEMANTICS;
    HANDLE hFile = CreateFileW(destFilePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, flagsAndAttributes, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        wcout << "Error opening file with the GENERIC_WRITE access right: " << destFilePath << endl;
        PrintLastError();
        return;
    }

    wcout << destFilePath << " ... ";
    BOOL result = SetFileTime(hFile, creationTime, lastAccessTime, lastWriteTime);
    if (result)
        wcout << "done." << endl;
    else
    {
        wcout << "error!" << endl;
        PrintLastError();
    }
    CloseHandle(hFile);
}

void UpdateFileTimeIfEarlier(WIN32_FIND_DATAW& sourceFindFileData, WIN32_FIND_DATAW& destFindFileData, const wstring& destFilePath)
{
    PFILETIME creationTime, lastAccessTime, lastWriteTime;

    if (CompareFileTime(&sourceFindFileData.ftCreationTime, &destFindFileData.ftCreationTime) < 0)
        creationTime = &sourceFindFileData.ftCreationTime;
    else
        creationTime = NULL; // = &destFindFileData.ftCreationTime

    if (CompareFileTime(&sourceFindFileData.ftLastAccessTime, &destFindFileData.ftLastAccessTime) < 0)
        lastAccessTime = &sourceFindFileData.ftLastAccessTime;
    else
        lastAccessTime = NULL;

    if (CompareFileTime(&sourceFindFileData.ftLastWriteTime, &destFindFileData.ftLastWriteTime) < 0)
        lastWriteTime = &sourceFindFileData.ftLastWriteTime;
    else
        lastWriteTime = NULL;

    bool isDir = sourceFindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
    if (creationTime != NULL || lastAccessTime != NULL || lastWriteTime != NULL)
        FixFileTime(destFilePath, !isDir, creationTime, lastAccessTime, lastWriteTime);
}

void ProcessDirsRecursively(const wstring& sourcePath, const wstring& destPath)
{
    wstring sourceSearchPath = sourcePath + L"\\*";
    WIN32_FIND_DATAW sourceFindFileData;
    HANDLE sourceHFind = FindFirstFileW(sourceSearchPath.c_str(), &sourceFindFileData);
    if (sourceHFind == INVALID_HANDLE_VALUE)
    {
        wcout << "Error finding files in source directory: " << sourcePath << endl;
        PrintLastError();
        return;
    }

    do
    {
        wstring sourceFileName = sourceFindFileData.cFileName;
        if (sourceFileName == L"." || sourceFileName == L"..")
            continue;

        // Find the corresponding file in the destination folder (if exists).
        wstring destFilePath = destPath + L"\\" + sourceFileName;
        WIN32_FIND_DATAW destFindFileData;
        HANDLE destHFind = FindFirstFileW(destFilePath.c_str(), &destFindFileData);
        if (destHFind == INVALID_HANDLE_VALUE)
        {
            // TODO: statistics
            continue;
        }
        FindClose(destHFind);

        UpdateFileTimeIfEarlier(sourceFindFileData, destFindFileData, destFilePath);

        bool isDir = sourceFindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;

        wstring sourceFilePath = sourcePath + L"\\" + sourceFileName;
        if (isDir)
            ProcessDirsRecursively(sourceFilePath, destFilePath);
    } while (FindNextFileW(sourceHFind, &sourceFindFileData));

    DWORD lastError = GetLastError();
    if (lastError != ERROR_NO_MORE_FILES)
        wcout << "FindNextFileW() error (" << lastError << ") for source directory: " << sourcePath << endl;

    FindClose(sourceHFind);
}

int wmain(int argc, wchar_t* argv[])
{
    // Change stdout to Unicode UTF-16 mode.
    _setmode(_fileno(stdout), _O_U16TEXT);

    if (argc < 3)
    {
        wcout << "Usage: cfft.exe [options] <source> <destination>" << endl;
        return 1;
    }

    // Parse command line arguments.
    wstring sourcePath, destPath;
    if (argc == 3)
    {
        sourcePath = argv[1];
        destPath = argv[2];
    }
    else //if (argc > 3)
    {
        // Set program options from command line arguments.
        for (int i = 1; i < argc - 2; ++i)
        {
            if (wcscmp(argv[i], L"-r") == 0)
                programOptions.recurseSubDirs = true;
            else if (wcscmp(argv[i], L"--simulate") == 0)
                programOptions.simulate = true;
            else
            {
                wcout << "Unknown option: " << argv[i] << endl;
                return 2;
            }
        }
        sourcePath = argv[argc - 2];
        destPath = argv[argc - 1];
    }

    if (programOptions.simulate)
    {
        wcout << "Simulating..." << endl;
        wcout << "The timestamp of the next files should be fixed:" << endl;
    }
    else
        wcout << "Fixing files timestamp:" << endl;

    auto start = high_resolution_clock::now();
    ProcessDirsRecursively(sourcePath, destPath);
    auto end = high_resolution_clock::now();

    auto tpDuration = (end - start).count();
    double duration = tpDuration / 1e3;
    wstring unit = L" \u03BCs";
    duration = int(duration * 10.0) / 10.0;

    // Printing statistics.
    wcout << endl << "Statistics:" << endl;
    wcout << "Execution time: " << duration << unit << endl;

    return 0;
}