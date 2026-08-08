/* 
 * bfVFS : vfs/Core/vfs_vloc.cpp
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

#include <vfs/Core/vfs_vloc.h>
#include <vfs/Core/vfs_vfile.h>
#include <vfs/Core/vfs_profile.h>
#include <vfs/Core/vfs.h>

/************************************************************************/

class vfs::CVirtualLocation::VFileIterator : public vfs::CVirtualLocation::Iterator::IImplementation
{
	friend class vfs::CVirtualLocation;
	typedef vfs::CVirtualLocation::Iterator::IImplementation tBaseClass;

	VFileIterator(vfs::CVirtualLocation* pLoc): tBaseClass(), m_pLoc(pLoc)
	{
		VFS_THROW_IFF(pLoc, L"");
		_vfile_iter = m_pLoc->m_VFiles.begin();
	}
public:
	VFileIterator() : tBaseClass(), m_pLoc(NULL)
	{};
	virtual ~VFileIterator()
	{};
	virtual vfs::CVirtualFile*	value()
	{
		if(m_pLoc && _vfile_iter != m_pLoc->m_VFiles.end())
		{
			return _vfile_iter->second;
		}
		return NULL;
	}
	virtual void				next()
	{
		if(m_pLoc && _vfile_iter != m_pLoc->m_VFiles.end())
		{
			_vfile_iter++;
		}
	}
protected:
	virtual tBaseClass* clone()
	{
		VFileIterator* iter = new VFileIterator(m_pLoc);
		iter->_vfile_iter = _vfile_iter;
		return iter;
	}
private:
	vfs::CVirtualLocation*						m_pLoc;
	vfs::CVirtualLocation::tVFiles::iterator	_vfile_iter;
};

/************************************************************************/

vfs::CVirtualLocation::PreparedFileEntry::PreparedFileEntry() noexcept
	: m_owner(NULL), m_previous(NULL), m_prepared(NULL),
	  m_inserted(false), m_active(false)
{
}

vfs::CVirtualLocation::PreparedFileEntry::~PreparedFileEntry() noexcept
{
	rollback();
}

bool vfs::CVirtualLocation::PreparedFileEntry::isActive() const noexcept
{
	return m_active;
}

void vfs::CVirtualLocation::PreparedFileEntry::rollback() noexcept
{
	if(!m_active || !m_owner) return;
	if(m_inserted)
	{
		m_owner->m_VFiles.erase(m_position);
	}
	else
	{
		m_position->second = m_previous;
	}
	if(m_prepared) m_prepared->destroy();
	m_owner = NULL;
	m_previous = NULL;
	m_prepared = NULL;
	m_active = false;
}

void vfs::CVirtualLocation::PreparedFileEntry::commit() noexcept
{
	if(!m_active) return;
	if(m_previous) m_previous->destroy();
	m_owner = NULL;
	m_previous = NULL;
	m_prepared = NULL;
	m_active = false;
}

/************************************************************************/

vfs::CVirtualLocation::CVirtualLocation(vfs::Path const& path)
: cPath(path), m_exclusive(false)
{};

vfs::CVirtualLocation::~CVirtualLocation()
{
	tVFiles::iterator it = m_VFiles.begin();
	for(; it != m_VFiles.end(); ++it)
	{
		it->second->destroy();
	}
	m_VFiles.clear();
}

void vfs::CVirtualLocation::setIsExclusive(bool exclusive) noexcept
{
	m_exclusive = exclusive;
}
bool vfs::CVirtualLocation::getIsExclusive() const noexcept
{
	return m_exclusive;
}

void vfs::CVirtualLocation::addFile(vfs::IBaseFile* file, vfs::String const& profileName)
{
	addFile(file, profileName, true);
}

void vfs::CVirtualLocation::addFile(vfs::IBaseFile* file, vfs::String const& profileName, bool replaceExisting)
{
	vfs::CVirtualFile *pVFile = NULL;
	tVFiles::iterator it = m_VFiles.find(file->getName());
	if(it == m_VFiles.end())
	{
		vfs::Path fp = file->getPath();
		vfs::CProfileStack& stack = *(getVFS()->getProfileStack());
		pVFile = vfs::CVirtualFile::create(fp,stack);
		it = m_VFiles.insert(m_VFiles.end(), std::pair<vfs::Path,vfs::CVirtualFile*>(file->getName(),pVFile));
	}
	it->second->add(file,profileName,replaceExisting);
}

vfs::IBaseFile* vfs::CVirtualLocation::getFile(vfs::Path const& filename, vfs::String const& profileName) const
{
	tVFiles::const_iterator cit = m_VFiles.find(filename);
	if(cit != m_VFiles.end() && cit->second)
	{
		if(profileName.empty())
		{
			if(m_exclusive)
			{
				return cit->second->file(vfs::CVirtualFile::SF_STOP_ON_WRITABLE_PROFILE);
			}
			else
			{
				return cit->second->file(vfs::CVirtualFile::SF_TOP);
			}
		}
		else
		{
			// you know what you are doing
			return cit->second->file(profileName);
		}
	}
	return NULL;
}
vfs::CVirtualFile* vfs::CVirtualLocation::getVirtualFile(vfs::Path const& filename)
{
	tVFiles::const_iterator cit = m_VFiles.find(filename);
	if(cit != m_VFiles.end())
	{
		return cit->second;
	}
	return NULL;
}

bool vfs::CVirtualLocation::removeFile(vfs::IBaseFile* file)
{
	if(file)
	{
		vfs::Path sDir,sFile;
		file->getPath().splitLast(sDir,sFile);
		tVFiles::iterator it = m_VFiles.find(sFile);
		if(it != m_VFiles.end())
		{
			if(!it->second->remove(file))
			{
				it->second->destroy();
				m_VFiles.erase(it);
			}
			return true;
		}
	}
	return false;
}

bool vfs::CVirtualLocation::prepareFileEntry(
	vfs::IBaseFile* file, vfs::String const& profileName,
	PreparedFileEntry& prepared)
{
	if(!file || prepared.m_active) return false;
	vfs::CVirtualFile* fresh = NULL;
	try
	{
		vfs::CProfileStack& stack = *(getVFS()->getProfileStack());
		fresh = vfs::CVirtualFile::create(file->getPath(), stack);
		fresh->add(file, profileName, true);

		tVFiles::iterator position = m_VFiles.find(file->getName());
		vfs::CVirtualFile* previous = NULL;
		bool inserted = false;
		if(position == m_VFiles.end())
		{
			position = m_VFiles.insert(
				std::make_pair(file->getName(), fresh)).first;
			inserted = true;
		}
		else
		{
			previous = position->second;
			position->second = fresh;
		}

		prepared.m_owner = this;
		prepared.m_position = position;
		prepared.m_previous = previous;
		prepared.m_prepared = fresh;
		prepared.m_inserted = inserted;
		prepared.m_active = true;
		return true;
	}
	catch(...)
	{
		if(fresh) fresh->destroy();
		throw;
	}
}

bool vfs::CVirtualLocation::empty() const
{
	return m_VFiles.empty();
}


vfs::CVirtualLocation::Iterator vfs::CVirtualLocation::iterate()
{
	return Iterator(new VFileIterator(this));
}

