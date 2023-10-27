#pragma once

#include <string>

using namespace std;

// This function takes a duration in nanoseconds and formats it as a wstring with the appropriate unit of time
wstring FormatDuration(long long nanoseconds);
