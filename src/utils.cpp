#include "utils.h"

wstring FormatDuration(long long llDuration)
{
	double duration = llDuration / 1e3;
	wstring unit = L" \u03BCs";
	return to_wstring(duration) + unit;
}