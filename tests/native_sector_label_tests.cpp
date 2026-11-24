#include "types.h"
#include "GameSettings.h"
#include "strategicmap.h"
#include "strategic.h"
#include "Font.h"
#include "MemMan.h"
#include "WordWrap.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <iterator>
#include <string>
#include <vector>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = FALSE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "%s\n", message ? message : ""); std::exit(1); }
extern CHAR16 gzSectorNames[256][4][MAX_SECTOR_NAME_LENGTH];
extern HVOBJECT FontObjs[MAX_FONTS];

int failures = 0;

template <std::size_t N>
void CheckShortened(const wchar_t* input, UINT32 width, const wchar_t* expected)
{
    struct Guarded { wchar_t before[4]; wchar_t text[N]; wchar_t after[4]; } value;
    std::fill(std::begin(value.before), std::end(value.before), L'!');
    std::fill(std::begin(value.text), std::end(value.text), L'?');
    std::fill(std::begin(value.after), std::end(value.after), L'!');
    if (std::wcslen(input) >= N) std::exit(1);
    std::wcscpy(value.text, input);
    if (!ReduceStringLength(value.text, width, 0) ||
        std::wcscmp(value.text, expected) != 0 ||
        StringPixLength(value.text, 0) > width ||
        !std::all_of(std::begin(value.before), std::end(value.before), [](wchar_t c) { return c == L'!'; }) ||
        !std::all_of(std::begin(value.after), std::end(value.after), [](wchar_t c) { return c == L'!'; }))
    {
        ++failures;
        std::printf("FAIL: native label clipping for capacity %zu, width %u\n", N, width);
    }
}

void CheckShortenedTownLabel()
{
    auto* table = CreateEnglishTransTable();
    if (!table || !InitializeFontManager(8, table)) std::exit(1);
    std::vector<ETRLEObject> glyphs(table->usNumberOfSymbols);
    MemFree(table); // The native manager owns the shared translation data.
    for (auto& glyph : glyphs) glyph.usWidth = 6;
    glyphs[GetIndex(L'.')].usWidth = 2;
    SGPVObject font{};
    font.usNumberOfObjects = static_cast<UINT16>(glyphs.size());
    font.pETRLEObject = glyphs.data();
    FontObjs[0] = &font;

    // RenderTownIDString supplies 80 characters, not 1024. A large canary
    // catches the old distant terminator write deterministically even without ASan.
    struct Guarded { wchar_t before[4]; wchar_t text[80]; wchar_t after[1024]; } value;
    std::fill(std::begin(value.before), std::end(value.before), L'!');
    std::fill(std::begin(value.text), std::end(value.text), L'?');
    std::fill(std::begin(value.after), std::end(value.after), L'!');
    std::wcscpy(value.text, L"B13: Drassen Airport");
    if (!ReduceStringLength(value.text, 80, 0) ||
        std::wcscmp(value.text, L"B13: Drassen...") != 0 ||
        !std::all_of(std::begin(value.before), std::end(value.before), [](wchar_t c) { return c == L'!'; }) ||
        !std::all_of(std::begin(value.after), std::end(value.after), [](wchar_t c) { return c == L'!'; }))
    {
        ++failures;
        std::puts("FAIL: shortened native town label overwrote caller buffer or changed clipping");
    }
    CheckShortened<80>(L"B13: Drassen Airport", 80, L"B13: Drassen...");
    CheckShortened<4>(L"ABC", 18, L"ABC");
    CheckShortened<4>(L"ABC", 12, L"...");
    CheckShortened<2>(L"W", 3, L".");
    CheckShortened<3>(L"WW", 4, L"..");
    CheckShortened<4>(L"ABC", 0, L"");
    CheckShortened<4>(L"ABC", 1, L"");
    CheckShortened<1>(L"", 0, L"");
    CheckShortened<80>(L"%s 100% ABC", 48, L"%s 100%...");
    CheckShortened<16>(L"\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9", 24, L"\u00e9\u00e9\u00e9...");
    const std::wstring longLabel(2000, L'A');
    const std::wstring longExpected = std::wstring(1500, L'A') + L"...";
    CheckShortened<2001>(longLabel.c_str(), 9006, longExpected.c_str());

    wchar_t sentinel = L'!';
    wchar_t unterminated[3] = {L'A', L'B', L'C'};
    if (ReduceStringLength(&sentinel, 0, 1, 0) || sentinel != L'!' ||
        ReduceStringLength(nullptr, 80, 1, 0) ||
        ReduceStringLength(unterminated, 1, 0) ||
        !std::equal(std::begin(unterminated), std::end(unterminated), L"ABC"))
    {
        ++failures;
        std::puts("FAIL: invalid label storage must be rejected without mutation");
    }
    // Zero-width glyphs still advance through a bounded input.
    glyphs[GetIndex(L'A')].usWidth = 0;
    CheckShortened<8>(L"AAAWW", 6, L"AAA...");
    FontObjs[0] = nullptr;
    ShutdownFontManager();
}
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
    CheckShortenedTownLabel();
    if (failures) return 1;
    std::puts("native sector label tests: ok");
    return 0;
}
