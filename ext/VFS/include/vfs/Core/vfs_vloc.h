/* 
 * bfVFS : vfs/Core/vfs_vloc.h
 *  - Virtual Location, stores Virtual Files
 *
 * Copyright (C) 2008 - 2010 (BF) john.bf.smith@googlemail.com
 * 
 * This file is part of the bfVFS library
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _VFS_VLOC_H_
#define _VFS_VLOC_H_

#include <vfs/Core/vfs_types.h>
#include <vfs/Core/Interface/vfs_file_interface.h>
#include <vfs/Core/Interface/vfs_iterator_interface.h>

#include <map>

namespace vfs
{
	class CVirtualFile;

	class VFS_API CVirtualLocation
	{
		class VFileIterator;
		typedef std::map<vfs::Path, CVirtualFile*, vfs::Path::Less> tVFiles;
	public:
		typedef vfs::TIterator<CVirtualFile> Iterator;

		class VFS_API PreparedFileEntry
		{
			friend class CVirtualLocation;
		public:
			PreparedFileEntry() noexcept;
			~PreparedFileEntry() noexcept;
			void commit() noexcept;
			bool isActive() const noexcept;
		private:
			PreparedFileEntry(PreparedFileEntry const&);
			void operator=(PreparedFileEntry const&);
			void rollback() noexcept;

			CVirtualLocation* m_owner;
			tVFiles::iterator m_position;
			CVirtualFile* m_previous;
			CVirtualFile* m_prepared;
			bool m_inserted;
			bool m_active;
		};

		CVirtualLocation(vfs::Path const& sPath);
		~CVirtualLocation();

		const vfs::Path		cPath;

		void				setIsExclusive(bool exclusive) noexcept;
		bool				getIsExclusive() const noexcept;

		void				addFile(vfs::IBaseFile* pile, vfs::String const& profileName);
		void				addFile(vfs::IBaseFile* pile, vfs::String const& profileName, bool replaceExisting);
		vfs::IBaseFile*		getFile(vfs::Path const& filename, vfs::String const& profileName = "") const;
		vfs::CVirtualFile*	getVirtualFile(vfs::Path const& filename);

		bool				removeFile(vfs::IBaseFile* file);
		bool				prepareFileEntry(vfs::IBaseFile* file,
							vfs::String const& profileName,
							PreparedFileEntry& prepared);
		bool				empty() const;
		
		Iterator			iterate();
	private:
		void				operator=(vfs::CVirtualLocation const& vloc);

		bool				m_exclusive;
		tVFiles				m_VFiles;
	};
} // end namespace

#endif // _VFS_VLOC_H_
