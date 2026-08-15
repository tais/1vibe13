#include <SelectedCatalogExport.h>
#include <types.h>

#include <algorithm>
#include <cwchar>
#include <iostream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

extern CHAR16 WeaponType[][30];
extern STR16 pAssignmentStrings[];

namespace
{
struct OwnedEntry
{
	std::wstring section;
	int index;
	std::wstring text;
};

class RecordingSink final : public i18n::SelectedCatalogExportSink
{
public:
	void copyEntry(std::wstring_view section, int index,
		std::wstring_view text) override
	{
		// Deliberately materialize both borrowed views before returning.
		entries.push_back({std::wstring(section), index, std::wstring(text)});
	}

	std::vector<OwnedEntry> entries;
};

int failures = 0;

void Check(bool condition, const char* message)
{
	if (condition) return;
	std::cerr << "FAIL: " << message << '\n';
	++failures;
}

const OwnedEntry* Find(const RecordingSink& sink, std::wstring_view section,
	int index)
{
	const auto found = std::find_if(sink.entries.begin(), sink.entries.end(),
		[=](const OwnedEntry& entry)
		{
			return entry.section == section && entry.index == index;
		});
	return found == sink.entries.end() ? nullptr : &*found;
}

bool HasText(const RecordingSink& sink, std::wstring_view section, int index,
	std::wstring_view text)
{
	const auto* entry = Find(sink, section, index);
	return entry && entry->text == text;
}

std::size_t Count(const RecordingSink& sink, std::wstring_view section)
{
	return static_cast<std::size_t>(std::count_if(
		sink.entries.begin(), sink.entries.end(), [=](const OwnedEntry& entry)
		{
			return entry.section == section;
		}));
}

void ReplaceFixedRow(wchar_t* row, std::size_t capacity,
	std::wstring_view replacement)
{
	std::wmemset(row, L'\0', capacity);
	const auto copied = std::min(capacity - 1, replacement.size());
	std::wmemcpy(row, replacement.data(), copied);
}
}

int main()
{
	RecordingSink startupSnapshot;
	i18n::ExportSelectedCatalog(startupSnapshot);

	Check(startupSnapshot.entries.size() == 3078,
		"English JA2 release snapshot retains all 3,078 emitted entries");
	std::set<std::wstring> emittedSections;
	for (const auto& entry : startupSnapshot.entries)
	{
		Check(!entry.section.empty(), "exported section names are non-empty");
		Check(!entry.text.empty(), "empty legacy values remain suppressed");
		emittedSections.insert(entry.section);
	}
	Check(emittedSections.size() == 237,
		"English JA2 release snapshot retains 237 emitted sections");

	Check(!startupSnapshot.entries.empty() &&
		startupSnapshot.entries.front().section == L"WeaponType" &&
		startupSnapshot.entries.front().index == 0 &&
		startupSnapshot.entries.front().text == L"Other",
		"fixed-row export starts with the historical WeaponType entry");
	Check(Count(startupSnapshot, L"WeaponType") == 9,
		"fixed-row implicit empty tail remains suppressed");
	Check(Find(startupSnapshot, L"TownNames", 0) == nullptr &&
		HasText(startupSnapshot, L"TownNames", 1, L"Omerta"),
		"empty fixed rows are suppressed without renumbering absolute indices");
	Check(HasText(startupSnapshot, L"Assignment", 0, L"Squad 1"),
		"STR16 pointer-slot tables export through the borrowed-view seam");
	Check(HasText(startupSnapshot, L"ProsLabel", 0, L"Pros:"),
		"scalar wchar buffers export through the specialization path");
	Check(HasText(startupSnapshot, L"PersonnelTitle", 0, L"Personnel"),
		"TextPack entries agree with the selected English linked globals");
	Check(startupSnapshot.entries.back().section == L"MPChatbox" &&
		startupSnapshot.entries.back().index == 1 &&
		startupSnapshot.entries.back().text ==
			L"'ENTER' to send, 'ESC' to cancel",
		"export order retains the historical final MPChatbox entry");

	const std::wstring originalWeaponType{WeaponType[0]};
	const STR16 originalAssignment = pAssignmentStrings[0];
	ReplaceFixedRow(WeaponType[0], 30, L"Current fixed row");
	pAssignmentStrings[0] = L"Current pointer slot";

	RecordingSink laterManualSnapshot;
	i18n::ExportSelectedCatalog(laterManualSnapshot);
	Check(HasText(laterManualSnapshot, L"WeaponType", 0,
		L"Current fixed row"),
		"a later manual export snapshots the then-current fixed row");
	Check(HasText(laterManualSnapshot, L"Assignment", 0,
		L"Current pointer slot"),
		"a later manual export snapshots the then-current pointer slot");
	Check(HasText(startupSnapshot, L"WeaponType", 0, originalWeaponType) &&
		HasText(startupSnapshot, L"Assignment", 0, originalAssignment),
		"the immediate-copy sink owns its first snapshot after globals mutate");

	ReplaceFixedRow(WeaponType[0], 30, originalWeaponType);
	pAssignmentStrings[0] = originalAssignment;
	return failures == 0 ? 0 : 1;
}
