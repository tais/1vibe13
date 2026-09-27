#include "types.h"
#include "GameSettings.h"
#include "strategicmap.h"
#include "strategic.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <iterator>
#include <string>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = FALSE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "%s\n", message ? message : ""); std::exit(1); }
extern CHAR16 gzSectorNames[256][4][MAX_SECTOR_NAME_LENGTH];

int failures = 0;
template <std::size_t N>
void CheckLabel(INT16 x, INT16 y, INT8 z, const wchar_t* expected)
{
    struct Guarded { wchar_t before[4]; wchar_t text[N]; wchar_t after[64]; } value;
    std::fill(std::begin(value.before), std::end(value.before), L'!');
    std::fill(std::begin(value.text), std::end(value.text), L'?');
    std::fill(std::begin(value.after), std::end(value.after), L'!');
    GetSectorIDString(x, y, z, value.text, FALSE);
    const bool intact = std::all_of(std::begin(value.before), std::end(value.before), [](wchar_t c) { return c == L'!'; }) &&
        std::all_of(std::begin(value.after), std::end(value.after), [](wchar_t c) { return c == L'!'; });
    if (!intact || value.text[N - 1] != L'\0' || std::wcscmp(value.text, expected) != 0)
    { ++failures; std::printf("FAIL: native sector label does not respect capacity %zu\n", N); }
}

int main()
{
    // The native talking-face overlay uses this exact 50-character capacity.
    for (auto& name : gzSectorNames[SECTOR(13, 9)])
        std::wmemcpy(name, L"Alma", 5);
    CheckLabel<50>(13, 9, 0, L"I13: Alma");
    CheckLabel<10>(13, 9, 0, L"I13: Alma");
    CheckLabel<6>(13, 9, 0, L"I13: ");
    CheckLabel<1>(13, 9, 0, L"");
    CheckLabel<1>(0, 9, 0, L"");
    for (auto& name : gzSectorNames[SECTOR(13, 9)])
    {
        std::fill(std::begin(name), std::end(name) - 1, L'x');
        name[MAX_SECTOR_NAME_LENGTH - 1] = L'\0';
    }
    const std::wstring truncated = L"I13: " + std::wstring(44, L'x');
    CheckLabel<50>(13, 9, 0, truncated.c_str());
    for (auto& name : gzSectorNames[SECTOR(13, 9)])
        std::wmemcpy(name, L"%s 100%", 8);
    CheckLabel<50>(13, 9, 0, L"I13: %s 100%");
    for (const auto x : {-1, 0, 17}) CheckLabel<1>(x, 9, 0, L"");
    for (const auto y : {-1, 0, 17}) CheckLabel<1>(13, y, 0, L"");
    for (const auto z : {-1, 4}) CheckLabel<1>(13, 9, z, L"");
    wchar_t sentinel = L'!';
    GetSectorIDString(13, 9, 0, &sentinel, 0, FALSE);
    GetSectorIDString(13, 9, 0, nullptr, 50, FALSE);
    if (sentinel != L'!') ++failures;
    if (failures) return 1;
    std::puts("native sector label tests: ok");
    return 0;
}
