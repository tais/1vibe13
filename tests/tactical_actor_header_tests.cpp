#include "TacticalActor.h"

#include <type_traits>
#include <utility>

static_assert(std::is_class_v<TacticalActor>);
static_assert(std::is_same_v<
	decltype(std::declval<TacticalActor&>().identity()),
	SoldierIdentityComponent&>);
static_assert(std::is_same_v<
	decltype(std::declval<const TacticalActor&>().identity()),
	const SoldierIdentityComponent&>);
static_assert(noexcept(std::declval<TacticalActor&>().identity()));
static_assert(sizeof(TacticalActor) > 0);

int main()
{
	return 0;
}
