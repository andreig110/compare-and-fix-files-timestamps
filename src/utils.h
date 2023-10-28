#pragma once

#include <Windows.h>

#include <string>

using namespace std;

bool FileOrDirExists(const wstring &fileName);

// This function takes a duration in nanoseconds and formats it as a wstring with the appropriate unit of time
wstring FormatDuration(long long nanoseconds);
