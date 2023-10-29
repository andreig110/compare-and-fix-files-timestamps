#include <io.h>
#include <fcntl.h>
#include <Windows.h>

#include <iostream>
#include <string>
#include <chrono>

#include "utils.h"

using namespace std;
using namespace std::chrono;

struct ProgramOptions
{
    bool recurseSubDirs; // Recurse the subdirectories
    bool simulate; // Do not change the timestamps of the files. The program will not make any changes to the file system.
} programOptions;

// A struct to store statistical data
struct Stats
{
    int numFiles = 0;        // Total number of files in the source directory
    int numDirs = 0;         // Total number of directories in the source directory
    int numMissingFiles = 0; // Number of missing files in the destination directory
    int numMissingDirs = 0;  // Number of missing directories in the destination directory
    int numFixedFiles = 0;   // Number of fixed files in the destination directory
    int numFixedDirs = 0;    // Number of fixed directories in the destination directory
    int numUnfixedFiles = 0; // Number of unfixed files in the destination directory
    int numUnfixedDirs = 0;  // Number of unfixed directories in the destination directory
} stats;

// Function to update the number of files/directories in the Stats struct
void UpdateFilesDirsStats(bool isDir)
{
    if (isDir)
        stats.numDirs++;
    else
        stats.numFiles++;
}

// Function to update the number of missing files/directories in the Stats struct
void UpdateMissingStats(bool isDir)
{
    if (isDir)
        stats.numMissingDirs++;
    else
        stats.numMissingFiles++;
}

// Function to update the number of fixed files/directories in the Stats struct
void UpdateFixedStats(bool isFile)
{
    if (isFile)
        stats.numFixedFiles++;
    else
        stats.numFixedDirs++;
}

// Function to update the number of unfixed files/directories in the Stats struct
void UpdateUnfixedStats(bool isFile)
{
    if (isFile)
        stats.numUnfixedFiles++;
    else
        stats.numUnfixedDirs++;
}

// Function to print statistical data
void PrintStats()
{
    wcout << "Total number of files in the source directory: " << stats.numFiles << endl;
    wcout << "Total number of directories in the source directory: " << stats.numDirs << endl;
    wcout << "Total number of files and directories in the source directory: " << stats.numFiles + stats.numDirs << endl << endl;

    wcout << "Number of missing files in the destination directory: " << stats.numMissingFiles << endl;
    wcout << "Number of missing directories in the destination directory: " << stats.numMissingDirs << endl;
    wcout << "Total number of missing files and directories in the destination directory: " << stats.numMissingFiles + stats.numMissingDirs << endl << endl;

    wcout << "Number of fixed files in the destination directory: " << stats.numFixedFiles << endl;
    wcout << "Number of fixed directories in the destination directory: " << stats.numFixedDirs << endl;
    wcout << "Total number of fixed files and directories in the destination directory: " << stats.numFixedFiles + stats.numFixedDirs << endl << endl;

    wcout << "Number of unfixed files in the destination directory: " << stats.numUnfixedFiles << endl;
    wcout << "Number of unfixed directories in the destination directory: " << stats.numUnfixedDirs << endl;
    wcout << "Total number of unfixed files and directories in the destination directory: " << stats.numUnfixedFiles + stats.numUnfixedDirs << endl << endl;
}

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
        UpdateUnfixedStats(isFile);
        return;
    }

    wcout << destFilePath << " ... ";
    BOOL result = SetFileTime(hFile, creationTime, lastAccessTime, lastWriteTime);
    if (result)
    {
        wcout << "done." << endl;
        UpdateFixedStats(isFile);
    }
    else
    {
        wcout << "error!" << endl;
        PrintLastError();
        UpdateUnfixedStats(isFile);
    }
    CloseHandle(hFile);
}

// Update the destination file's timestamps if the source file's timestamps are earlier
void UpdateFileTimeIfEarlier(WIN32_FIND_DATAW& sourceFindFileData, WIN32_FIND_DATAW& destFindFileData, const wstring& destFilePath)
{
    PFILETIME creationTime, lastAccessTime, lastWriteTime;

    // Check if the source file's creation timestamp is earlier than the destination file's creation timestamp
    if (CompareFileTime(&sourceFindFileData.ftCreationTime, &destFindFileData.ftCreationTime) < 0)
        creationTime = &sourceFindFileData.ftCreationTime;
    else
        creationTime = NULL; // = &destFindFileData.ftCreationTime

    // Check if the source file's last access timestamp is earlier than the destination file's last access timestamp
    if (CompareFileTime(&sourceFindFileData.ftLastAccessTime, &destFindFileData.ftLastAccessTime) < 0)
        lastAccessTime = &sourceFindFileData.ftLastAccessTime;
    else
        lastAccessTime = NULL;

    // Check if the source file's last write timestamp is earlier than the destination file's last write timestamp
    if (CompareFileTime(&sourceFindFileData.ftLastWriteTime, &destFindFileData.ftLastWriteTime) < 0)
        lastWriteTime = &sourceFindFileData.ftLastWriteTime;
    else
        lastWriteTime = NULL;

    // Check if any of the timestamps need to be updated
    if (creationTime != NULL || lastAccessTime != NULL || lastWriteTime != NULL)
    {
        bool isDir = destFindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
        // Check if simulation mode is not enabled
        if (!programOptions.simulate)
        {
            // Call FixFileTime() to update the destination file's timestamps
            FixFileTime(destFilePath, !isDir, creationTime, lastAccessTime, lastWriteTime);
        }
        else // if simulation mode is enabled
        {
            wcout << destFilePath; // Print the path of the file whose timestamps need to be fixed
            wcout << "  ("
                << ((creationTime != NULL) ? "cr, " : "")
                << ((lastAccessTime != NULL) ? "la, " : "")
                << ((lastWriteTime != NULL) ? "lw)" : ")")
                << endl;
            UpdateUnfixedStats(!isDir);
        }
    }
}

void ProcessDirsRecursively(const wstring& sourcePath, const wstring& destPath)
{
    // Construct a search path for the source directory
    wstring sourceSearchPath = sourcePath + L"\\*";
    // Find the first file or directory in the source directory that matches the search path
    WIN32_FIND_DATAW sourceFindFileData;
    HANDLE sourceHFind = FindFirstFileW(sourceSearchPath.c_str(), &sourceFindFileData);
    if (sourceHFind == INVALID_HANDLE_VALUE)
    {
        wcout << "Error finding files in source directory: " << sourcePath << endl;
        PrintLastError();
        return;
    }

    // Loop through all files and directories in the source directory
    do
    {
        wstring sourceFileName = sourceFindFileData.cFileName;
        if (sourceFileName == L"." || sourceFileName == L"..")
            continue;

        bool sourceIsDir = sourceFindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
        UpdateFilesDirsStats(sourceIsDir);

        // Construct a file path for the corresponding file or directory in the destination directory
        wstring destFilePath = destPath + L"\\" + sourceFileName;
        // Find the corresponding file or directory in the destination directory (if exists)
        WIN32_FIND_DATAW destFindFileData;
        HANDLE destHFind = FindFirstFileW(destFilePath.c_str(), &destFindFileData);
        if (destHFind == INVALID_HANDLE_VALUE)
        {
            // Update the number of missing files/directories and print info about the missing file/directory
            if (sourceIsDir)
                wcout << "Hint: Missing directory in the destination directory: " << destFilePath << endl;
            else
                wcout << "Hint: Missing file in the destination directory: " << destFilePath << endl;
            UpdateMissingStats(sourceIsDir);
            continue;
        }
        FindClose(destHFind);

        // Update the destination file's timestamps if the source file's timestamps is earlier
        UpdateFileTimeIfEarlier(sourceFindFileData, destFindFileData, destFilePath);

        // Construct a file path for the file or directory in the source directory
        wstring sourceFilePath = sourcePath + L"\\" + sourceFileName;
        bool destIsDir = destFindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
        // Recursively process directories if both source and destination are directories
        if (sourceIsDir && destIsDir)
            ProcessDirsRecursively(sourceFilePath, destFilePath);
        else if (sourceIsDir || destIsDir)
        {
            // Print a warning message if the source/destination directory
            // cannot be processed because either only the source is a directory
            // or only the destination is a directory.
            wcout << "Warning: Unable to process directory "
                << "\"" << sourceFileName << "\" "
                << "in the " << (sourceIsDir ? "source" : "destination") << " directory "
                << "\"" << (sourceIsDir ? sourcePath : destPath) << "\" "
                << "because the object with the same name in the "
                << (sourceIsDir ? "destination" : "source")
                << " directory is a file, not a directory. ... skipped."
                << endl;
        }
    } while (FindNextFileW(sourceHFind, &sourceFindFileData));

    DWORD lastError = GetLastError();
    if (lastError != ERROR_NO_MORE_FILES)
        wcout << "FindNextFileW() error (" << lastError << ") for source directory: " << sourcePath << endl;

    FindClose(sourceHFind);
}

void ProcessFilesOrDirs(const wstring& sourcePath, const wstring& destPath)
{
    // Find the source file or directory
    WIN32_FIND_DATAW sourceFindFileData;
    HANDLE sourceHFind = FindFirstFileW(sourcePath.c_str(), &sourceFindFileData);
    if (sourceHFind == INVALID_HANDLE_VALUE)
    {
        wcout << "No source file or directory: " << sourcePath << endl;
        PrintLastError();
        return;
    }
    FindClose(sourceHFind);

    bool sourceIsDir = sourceFindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
    UpdateFilesDirsStats(sourceIsDir);

    // Find the destination file or directory
    WIN32_FIND_DATAW destFindFileData;
    HANDLE destHFind = FindFirstFileW(destPath.c_str(), &destFindFileData);
    if (destHFind == INVALID_HANDLE_VALUE)
    {
        wcout << "No destination file or directory: " << destPath << endl;
        PrintLastError();
        UpdateMissingStats(sourceIsDir);
        return;
    }
    FindClose(destHFind);

    // Update the destination file's timestamps if the source file's timestamps is earlier
    UpdateFileTimeIfEarlier(sourceFindFileData, destFindFileData, destPath);

    // Recursively process directories if the option is enabled
    if (programOptions.recurseSubDirs)
    {
        bool destIsDir = destFindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
        if (sourceIsDir && destIsDir)
            ProcessDirsRecursively(sourcePath, destPath);
        else
        {
            wcout << "Cannot process directories recursively:" << endl;
            if (!sourceIsDir)
                wcout << "- The source is not a directory: " << sourcePath << endl;
            if (!destIsDir)
                wcout << "- The destination is not a directory: " << destPath << endl;
        }
    }
}

int wmain(int argc, wchar_t* argv[])
{
    // Change stdout to Unicode UTF-16 mode
    _setmode(_fileno(stdout), _O_U16TEXT);

    // Print usage if not enough arguments
    if (argc < 3)
    {
        wcout << "Usage: cfft.exe [options] <source> <destination>" << endl;
        return 1;
    }

    // Set program options from command line arguments
    if (argc > 3)
    {
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
    }

    wstring sourcePath = argv[argc - 2];
    wstring destPath = argv[argc - 1];

    if (!FileOrDirExists(sourcePath))
    {
        wcout << "No source file or directory: " << sourcePath << endl;
        return 3;
    }

    if (!FileOrDirExists(destPath))
    {
        wcout << "No destination file or directory: " << destPath << endl;
        return 4;
    }

    if (programOptions.simulate)
    {
        wcout << "Simulating..." << endl;
        wcout << "The timestamps for the following files should be fixed:" << endl;
    }
    else
        wcout << "Fixing the files' timestamps:" << endl;

    auto start = high_resolution_clock::now();
    ProcessFilesOrDirs(sourcePath, destPath);
    auto end = high_resolution_clock::now();

    auto duration = duration_cast<nanoseconds>(end - start).count(); // nanoseconds

    // Print statistics
    wcout << endl << "Statistics:" << endl;
    PrintStats();
    wcout << "Execution time: " << FormatDuration(duration) << endl;

    return 0;
}
