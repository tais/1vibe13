#include "ImpCreationStateModel.h"

#include <array>
#include <cassert>
#include <cstddef>

int main()
{
	using LaptopImpModel::FindFirstMatchingIndex;
	using LaptopImpModel::FindNextMatchingIndex;
	using LaptopImpModel::FindPreferredOrFirstMatchingIndex;
	using LaptopImpModel::FindPreviousMatchingIndex;
	using LaptopImpModel::IsIndexInRange;

	int publishedValue = 7;
	{
		LaptopImpModel::ScopedRollback<int> rollback(publishedValue);
		publishedValue = 19;
	}
	assert(publishedValue == 7);
	{
		LaptopImpModel::ScopedRollback<int> rollback(publishedValue);
		publishedValue = 23;
		rollback.Commit();
	}
	assert(publishedValue == 23);

	LaptopImpModel::NavigationState<4> navigation(1);
	assert(navigation.CurrentPage() == 1);
	assert(navigation.PreviousPage() == -1);
	assert(!navigation.RequestPage(-1));
	assert(!navigation.RequestPage(4));
	assert(navigation.CurrentPage() == 1);
	assert(!navigation.HasCurrentPageBeenVisited());
	assert(navigation.MarkCurrentPageVisited());
	assert(navigation.HasCurrentPageBeenVisited());
	navigation.CompleteTransition();
	assert(!navigation.PageChanged());
	assert(navigation.RequestPage(3));
	assert(navigation.PageChanged());
	assert(!navigation.HasCurrentPageBeenVisited());
	navigation.MarkCurrentPageVisited();
	navigation.ResetVisited();
	assert(!navigation.HasCurrentPageBeenVisited());
	navigation.Reset(99);
	assert(navigation.CurrentPage() == 0);
	assert(navigation.PreviousPage() == -1);

	assert(!IsIndexInRange(3, -1));
	assert(IsIndexInRange(3, 2));
	assert(!IsIndexInRange(3, 3));

	const std::array<bool, 5> selectable{false, true, false, false, true};
	const auto matches = [&selectable](std::size_t index) {
		return selectable[index];
	};
	assert(FindFirstMatchingIndex(selectable.size(), matches) == 1);
	assert(FindPreferredOrFirstMatchingIndex(selectable.size(), 4, matches) == 4);
	assert(FindPreferredOrFirstMatchingIndex(selectable.size(), 2, matches) == 1);
	assert(FindPreferredOrFirstMatchingIndex(selectable.size(), -1, matches) == 1);
	assert(FindPreferredOrFirstMatchingIndex(selectable.size(), 99, matches) == 1);
	assert(FindNextMatchingIndex(selectable.size(), 1, matches) == 4);
	assert(FindNextMatchingIndex(selectable.size(), 4, matches) == 1);
	assert(FindNextMatchingIndex(selectable.size(), -1, matches) == 1);
	assert(FindPreviousMatchingIndex(selectable.size(), 1, matches) == 4);
	assert(FindPreviousMatchingIndex(selectable.size(), 4, matches) == 1);
	assert(FindPreviousMatchingIndex(selectable.size(), 99, matches) == 4);

	const auto never = [](std::size_t) { return false; };
	assert(!FindFirstMatchingIndex(5, never));
	assert(!FindPreferredOrFirstMatchingIndex(5, 2, never));
	assert(!FindPreferredOrFirstMatchingIndex(0, 0, never));
	assert(!FindNextMatchingIndex(5, 0, never));
	assert(!FindPreviousMatchingIndex(5, 0, never));
	assert(!FindNextMatchingIndex(0, 0, never));
	assert(!FindPreviousMatchingIndex(0, 0, never));

	return 0;
}
