#include "World Tile Map.h"

#include "MemMan.h"
#include "worlddef.h"

#include <cstring>
#include <limits>
#include <type_traits>

MAP_ELEMENT* gpWorldLevelData = nullptr;

namespace
{
class WorldTileMapStorage
{
public:
	WorldTileMapStorage() = default;
	WorldTileMapStorage(const WorldTileMapStorage&) = delete;
	WorldTileMapStorage& operator=(const WorldTileMapStorage&) = delete;

	~WorldTileMapStorage()
	{
		release();
	}

	BOOLEAN allocate(UINT32 tileCount)
	{
		if (tileCount == 0 ||
			tileCount >
				std::numeric_limits<UINT32>::max() / sizeof(MAP_ELEMENT))
		{
			return FALSE;
		}

		const UINT32 allocationSize =
			tileCount * static_cast<UINT32>(sizeof(MAP_ELEMENT));
		MAP_ELEMENT* replacement =
			static_cast<MAP_ELEMENT*>(MemAlloc(allocationSize));
		if (!replacement)
		{
			return FALSE;
		}

		std::memset(replacement, 0, allocationSize);

		release();
		tiles_ = replacement;
		tileCount_ = tileCount;
		publish();
		return TRUE;
	}

	void reset()
	{
		if (tiles_)
		{
			std::memset(
				tiles_, 0,
				tileCount_ * static_cast<UINT32>(sizeof(MAP_ELEMENT)));
		}
	}

	void release()
	{
		if (tiles_)
		{
			MemFree(tiles_);
			tiles_ = nullptr;
		}
		tileCount_ = 0;
		publish();
	}

	UINT32 size() const
	{
		return tileCount_;
	}

private:
	void publish()
	{
		gpWorldLevelData = tiles_;
	}

	MAP_ELEMENT* tiles_ = nullptr;
	UINT32 tileCount_ = 0;
};

static_assert(
	std::is_trivially_destructible<MAP_ELEMENT>::value,
	"World tile storage relies on MAP_ELEMENT's legacy trivial lifetime");

WorldTileMapStorage gWorldTileMap;
}

BOOLEAN AllocateWorldTileMap(UINT32 tileCount)
{
	return gWorldTileMap.allocate(tileCount);
}

void ResetWorldTileMap()
{
	gWorldTileMap.reset();
}

void ReleaseWorldTileMap()
{
	gWorldTileMap.release();
}

UINT32 GetWorldTileMapSize()
{
	return gWorldTileMap.size();
}
