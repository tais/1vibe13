/* 
 * bfVFS : vfs/Ext/7z/vfs_7z_library.cpp
 *  - implements Library interface, creates library object from uncompressed 7-zip archive files
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

#ifdef VFS_WITH_7ZIP

#include <vfs/Ext/7z/vfs_7z_library.h>
#include <vfs/Core/Location/vfs_lib_dir.h>
#include <vfs/Core/File/vfs_lib_file.h>
#include <vfs/Core/vfs_file_raii.h>

#include <cstring>

namespace sz
{
extern "C"
{
#include <7z.h>
#include <7zCrc.h>
#include <7zAlloc.h>
//#include <Archive/7z/7zAlloc.h>
//#include <Archive/7z/7zExtract.h>
//#include <Archive/7z/7zIn.h>
}
}

/********************************************************************************************/
/***                                   my 7z extensions                                   ***/
/********************************************************************************************/

namespace szExt
{
	typedef struct CSzVfsFile
	{
		vfs::tReadableFile*	file;
	} CSzVfsFile;

	typedef struct CVfsFileInStream
	{
		sz::ISeekInStream	s;
		CSzVfsFile			file;
	} CVfsFileInStream;


	// LZMA SDK 26.01: ISeekInStream callbacks take a const ISeekInStreamPtr
	// (was void*). `s` is the first member of CVfsFileInStream, so the address
	// of the interface is the address of the container.
	static sz::SRes VfsFileInStream_Read(sz::ISeekInStreamPtr pp, void *buf, ::size_t *size)
	{
		CVfsFileInStream *p = (CVfsFileInStream *)pp;
		::size_t to_read = *size;
		sz::SRes res;
		try
		{
			*size = (::size_t)p->file.file->read((vfs::Byte*)buf,to_read);
			res = SZ_OK;
		}
		catch(std::exception &ex)
		{
			VFS_LOG_ERROR(ex.what());
			res = SZ_ERROR_READ;
		}
		return res;
	}

	static sz::SRes VfsFileInStream_Seek(sz::ISeekInStreamPtr pp, sz::Int64 *pos, sz::ESzSeek origin)
	{
		CVfsFileInStream *p = (CVfsFileInStream *)pp;

		vfs::IBaseFile::ESeekDir eSD;
		switch (origin)
		{
			case sz::SZ_SEEK_SET:
				eSD = vfs::IBaseFile::SD_BEGIN;
				break;
			case sz::SZ_SEEK_CUR:
				eSD = vfs::IBaseFile::SD_CURRENT;
				break;
			case sz::SZ_SEEK_END:
				eSD = vfs::IBaseFile::SD_END;
				break;
			default: 
				return SZ_ERROR_PARAM;
		}
		vfs::offset_t _pos = (vfs::offset_t)(*pos);
		sz::SRes res;
		try
		{
			p->file.file->setReadPosition(_pos,eSD);
			*pos = p->file.file->getReadPosition();
			res = SZ_OK;
		}
		catch(std::exception& ex)
		{
			VFS_LOG_ERROR(ex.what());
			res = SZ_ERROR_READ;
		}
		return res;
	}

	void VfsFileInStream_CreateVTable(CVfsFileInStream *p)
	{
		p->s.Read = VfsFileInStream_Read;
		p->s.Seek = VfsFileInStream_Seek;
	}
}; // end namespace szExt

/********************************************************************************************/
/********************************************************************************************/
/********************************************************************************************/

vfs::CUncompressed7zLibrary::CUncompressed7zLibrary(
	tReadableFile *libraryFile,
	vfs::Path const& mountPoint,
	bool ownFile,
	vfs::ObjBlockAllocator<vfs::CLibFile>* allocator)
: vfs::CUncompressedLibraryBase(libraryFile, mountPoint, ownFile), _allocator(allocator)
{
}

vfs::CUncompressed7zLibrary::~CUncompressed7zLibrary()
{
}


#define k_Copy 0

sz::UInt64 GetSum(const sz::UInt64 *values, sz::UInt32 index)
{
	sz::UInt64 sum = 0;
	sz::UInt32 i;
	for (i = 0; i < index; i++)
	{
		sum += values[i];
	}
	return sum;
}

bool vfs::CUncompressed7zLibrary::init()
{
	if(!m_libraryFile)
	{
		return false;
	}
	try
	{
		szExt::CVfsFileInStream		archiveStream;
		sz::CLookToRead2			lookStream;
		sz::CSzArEx					db;
		sz::SRes					res;
		// LZMA SDK 26.01: ISzAlloc::Alloc/Free take ISzAllocPtr; aggregate-init
		// from the SDK's stock allocators.
		sz::ISzAlloc				allocImp     = { sz::SzAlloc, sz::SzFree };
		sz::ISzAlloc				allocTempImp = { sz::SzAllocTemp, sz::SzFreeTemp };

		vfs::COpenReadFile rfile(m_libraryFile);

		archiveStream.file.file = m_libraryFile;

		szExt::VfsFileInStream_CreateVTable(&archiveStream);

		// CLookToRead -> CLookToRead2: the caller must now supply the read
		// buffer. realStream is the ISeekInStream; SzArEx_Open takes the
		// ILookInStream `vt`.
		const size_t kLookBufSize = (size_t)1 << 16;
		lookStream.buf = (sz::Byte*)allocImp.Alloc(&allocImp, kLookBufSize);
		if(!lookStream.buf)
		{
			VFS_THROW(_BS(L"Out of memory opening 7z archive [") << m_libraryFile->getPath() << L"]" << _BS::wget);
		}
		lookStream.bufSize    = kLookBufSize;
		lookStream.realStream = &archiveStream.s;
		sz::LookToRead2_CreateVTable(&lookStream, False);
		LookToRead2_INIT(&lookStream);

		sz::CrcGenerateTable();

		sz::SzArEx_Init(&db);
		if( SZ_OK != (res = sz::SzArEx_Open(&db, &lookStream.vt, &allocImp, &allocTempImp)) )
		{
			allocImp.Free(&allocImp, lookStream.buf);
			VFS_THROW(_BS(L"Could not open 7z archive [") << m_libraryFile->getPath() << L"]" << _BS::wget);
		}

		vfs::TDirectory<ILibrary::tWriteType>* pLD = NULL;
		vfs::Path oDir, oFile;
		vfs::Path oDirPath;

		const size_t FBUFFER_SIZE = 1024;
		std::vector<vfs::UInt16> fname_buffer;
		fname_buffer.resize(FBUFFER_SIZE);

		// SzArEx_Extract decode cache, carried across the loop so a solid block is
		// decoded only once. outBuffer MUST be NULL before the first call.
		sz::UInt32 blockIndex    = 0xFFFFFFFF;
		sz::Byte*  outBuffer     = NULL;
		size_t     outBufferSize = 0;

		for(vfs::UInt32 i = 0; i < db.NumFiles; i++)
		{
			// LZMA SDK 26.01 removed the CSzFileItem array (db.db.Files); files
			// are now described by parallel arrays + accessor macros.
			if (SzArEx_IsDir(&db, i))
			{
				continue;
			}
			size_t fsize = SzArEx_GetFileNameUtf16(&db, i, NULL);
			if(fsize >= fname_buffer.size())
			{
				fname_buffer.resize(fsize + 32);
			}
			fsize = SzArEx_GetFileNameUtf16(&db, i, &fname_buffer[0]);
			fname_buffer[fsize] = 0;
			// SzArEx_GetFileNameUtf16 writes UTF-16 (2-byte) code units, but wchar_t is
			// 4 bytes on macOS/Linux: casting the UInt16 buffer to wchar_t* misreads every
			// character AND runs off the end of the buffer (a 4-byte read needs 4 zero
			// bytes to terminate, but the buffer terminator is a single 2-byte 0). Widen
			// each UTF-16 unit to wchar_t explicitly (member names are BMP/ASCII).
			std::wstring wname;
			for(size_t k = 0; k < fname_buffer.size() && fname_buffer[k] != 0; ++k)
			{
				wname.push_back((wchar_t)fname_buffer[k]);
			}
			vfs::Path sPath(wname.c_str());
			sPath.splitLast(oDir,oFile);
			oDirPath = m_mountPoint;
			if(!oDir.empty())
			{
				oDirPath += oDir;
			}

			// Decode this member with the SDK extractor. The old code computed a
			// raw archive offset and read bytes straight from the file, which only
			// worked for stored/uncompressed one-file-per-folder archives.
			// SzArEx_Extract handles ANY valid 7z -- compressed or stored, any
			// folder layout (it decodes the member's solid block, cached above).
			size_t fileOffset = 0, fileSize = 0;
			if( SZ_OK != sz::SzArEx_Extract(&db, &lookStream.vt, i, &blockIndex,
					&outBuffer, &outBufferSize, &fileOffset, &fileSize,
					&allocImp, &allocTempImp) )
			{
				// a single unreadable member shouldn't sink the whole archive
				VFS_LOG_ERROR(_BS(L"7z: could not extract member ") << (vfs::UInt32)i
					<< L" from [" << m_libraryFile->getPath() << L"]" << _BS::wget);
				continue;
			}

			// get or create according directory object
			tDirCatalogue::iterator it = m_dirs.find(oDirPath);
			if(it != m_dirs.end())
			{
				pLD = it->second;
			}
			else
			{
				pLD = new vfs::CLibDirectory(oDir,oDirPath);
				m_dirs.insert(std::make_pair(oDirPath,pLD));
			}
			// create file
			vfs::CLibFile *pFile = vfs::CLibFile::create(oFile,pLD,this,_allocator);
			// add file to directory
			VFS_THROW_IFF( pLD->addFile(pFile), L"" );

			// cache the decoded bytes; read() serves from here (offset unused)
			m_fileData.insert(std::make_pair(pFile,SFileData((vfs::size_t)fileSize, 0)));
			std::vector<vfs::Byte>& dst = m_decodedData[pFile];
			dst.assign(outBuffer + fileOffset, outBuffer + fileOffset + fileSize);
		}
		// decoded bytes are cached in m_decodedData, so the parsed archive index,
		// the decode buffer and the lookahead buffer can all be released now.
		if(outBuffer)
		{
			allocImp.Free(&allocImp, outBuffer);
		}
		sz::SzArEx_Free(&db, &allocImp);
		allocImp.Free(&allocImp, lookStream.buf);
		return true;
	}
	catch(std::exception& ex)
	{
		VFS_LOG_ERROR(ex.what());
		return false;
	}
}

vfs::size_t vfs::CUncompressed7zLibrary::read(tFileType *fileHandle, vfs::Byte* data, vfs::size_t bytesToRead)
{
	try
	{
		tFileData::iterator fit = m_fileData.find(fileHandle);
		std::map<tFileType*, std::vector<vfs::Byte> >::iterator dit = m_decodedData.find(fileHandle);
		if(fit == m_fileData.end() || dit == m_decodedData.end())
		{
			VFS_THROW(L"invalid file handle in 7z library read");
		}
		SFileData&                    file  = fit->second;
		std::vector<vfs::Byte> const& bytes = dit->second;

		if( (file._currentReadPosition + bytesToRead) > file._fileSize )
		{
			bytesToRead = file._fileSize - file._currentReadPosition;
		}
		if(bytesToRead == 0)
		{
			return 0; // eof
		}
		memcpy(data, &bytes[file._currentReadPosition], bytesToRead);
		file._currentReadPosition += bytesToRead;
		return bytesToRead;
	}
	catch(std::exception& ex)
	{
		VFS_RETHROW(L"", ex);
	}
}

#endif // VFS_WITH_7ZIP
