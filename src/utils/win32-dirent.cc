/* Copyright (C) 2001, 2006 Free Software Foundation, Inc.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

// NB: Taken from http://www.koders.com/c/fidB38A2E97F60B8A0C903631F8C60078DE8C6C8433.aspx
// Also modified to use Unicode calls explicitly
// Slightly reformatted/simplified to fit openMSX coding style.

#ifdef _WIN32

#include "win32-dirent.hh"
#include "utf8_checked.hh"
#include "MSXException.hh"
#include "xrange.hh"
#include "zstring_view.hh"
#include <Windows.h>
#include <cstring>
#include <cstdlib>
#include <exception>
#include <iterator>

namespace openmsx {

DIR* opendir(const char* name)
{
	if (!name || !*name) return nullptr;

	zstring_view name2 = name;
	std::wstring nameW = utf8::utf8to16(name2);
	if (!name2.ends_with('/') && !name2.ends_with("\\")) {
		nameW += L"\\*";
	} else {
		nameW += L"*";
	}

	HANDLE hnd;
	WIN32_FIND_DATAW find;
	if ((hnd = FindFirstFileW(nameW.c_str(), &find)) == INVALID_HANDLE_VALUE) {
		return nullptr;
	}

	auto* dir = new DIR;
	dir->mask = nameW;
	dir->fd = reinterpret_cast<INT_PTR>(hnd);
	dir->data = new WIN32_FIND_DATAW;
	dir->filepos = 0;
	memcpy(dir->data, &find, sizeof(find));
	return dir;
}

dirent* readdir(DIR* dir)
{
	static dirent entry;
	entry.d_ino = 0;
	entry.d_type = DT_UNKNOWN;

	auto find = static_cast<WIN32_FIND_DATAW*>(dir->data);
	if (dir->filepos) {
		if (!FindNextFileW(reinterpret_cast<HANDLE>(dir->fd), find)) {
			return nullptr;
		}
	}

	std::string d_name = utf8::utf16to8(find->cFileName);
	strncpy(entry.d_name, d_name.c_str(), std::size(entry.d_name));

	entry.d_off = dir->filepos;
	entry.d_reclen = static_cast<unsigned short>(strlen(entry.d_name));
	dir->filepos++;
	return &entry;
}

int closedir(DIR* dir)
{
	auto hnd = reinterpret_cast<HANDLE>(dir->fd);
	delete static_cast<WIN32_FIND_DATAW*>(dir->data);
	delete dir;
	return FindClose(hnd) ? 0 : -1;
}

void rewinddir(DIR* dir)
{
	auto hnd = reinterpret_cast<HANDLE>(dir->fd);
	auto find = static_cast<WIN32_FIND_DATAW*>(dir->data);

	FindClose(hnd);
	hnd = FindFirstFileW(dir->mask.c_str(), find);
	dir->fd = reinterpret_cast<INT_PTR>(hnd);
	dir->filepos = 0;
}

void seekdir(DIR* dir, off_t offset)
{
	rewinddir(dir);
	repeat(offset, [&]{
		if (FindNextFileW(reinterpret_cast<HANDLE>(dir->fd),
		                  static_cast<WIN32_FIND_DATAW*>(dir->data))) {
			dir->filepos++;
		}
	});
}

off_t telldir(DIR* dir)
{
	return dir->filepos;
}

// For correctness on 64-bit Windows, this function would need to maintain an
// internal map of ints to HANDLEs, since HANDLEs are sizeof(void*). It's not
// used at the moment in the openMSX sources, so let's not bother for now.
/*int dirfd(DIR* dir)
{
	return dir->fd;
}*/

} // namespace openmsx

#endif
