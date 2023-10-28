#include "utils.h"

#include <format>

bool FileOrDirExists(const wstring &fileName)
{
    DWORD fileAttributes = GetFileAttributesW(fileName.c_str());
    return (fileAttributes != INVALID_FILE_ATTRIBUTES);
}

wstring FormatDuration(long long nanoseconds)
{
    double duration;
    wstring unit;

    // Check if the duration is less than 1 microsecond
    if (nanoseconds < int(1e3)) // nanoseconds
    {
        duration = nanoseconds;
        unit = L"ns";
    }
    // Check if the duration is less than 1 millisecond
    else if (nanoseconds < int(1e6)) // microseconds
    {
        duration = nanoseconds / 1e3;
        unit = L"\u03BCs"; // Unicode for mu (micro) symbol
    }
    // Check if the duration is less than 1 second
    else if (nanoseconds < int(1e9)) // milliseconds
    {
        duration = nanoseconds / 1e6;
        unit = L"ms";
    }
    // Otherwise, format the duration in seconds
    else // seconds
    {
        duration = nanoseconds / 1e9;
        unit = L"sec";
    }

    return format(L"{0:.1f} {1}", duration, unit);
}
