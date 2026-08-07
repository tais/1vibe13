#ifndef ITEM_XML_WRITER_H
#define ITEM_XML_WRITER_H

#include <vfs/Core/Interface/vfs_file_interface.h>

#include <algorithm>
#include <cstddef>

namespace ItemXmlWriter
{
	constexpr std::size_t BoundedItemCount(
		std::size_t requested, std::size_t capacity) noexcept
	{
		return std::min(requested, capacity);
	}

	// These seams deliberately take the requested count. The implementation
	// clamps it to MAXITEMS before touching any live item or auxiliary table.
	bool Write(const vfs::Path& path, std::size_t requestedCount);
	bool Write(vfs::tWritableFile* file, std::size_t requestedCount);
}

#endif
