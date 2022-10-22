#ifndef WIN32_DIRENT_HH
#define WIN32_DIRENT_HH

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

/* Directory stream type.
   The miscellaneous Unix `readdir' implementations read directory data
   into a buffer and return `struct dirent *' pointers into it.  */

// NB: Taken from http://www.koders.com/c/fid5F00BC983E0F005E15E8E590E461D363F6146CEB.aspx
// Slightly reformatted/simplified to fit openMSX coding style.

#ifdef _WIN32

#include <sys/types.h>
#include <basetsd.h> // for INT_PTR
#include <string>

namespace openmsx {

struct dirstream
{
	INT_PTR fd;			// File descriptor.
	void* data;			// Directory block.
	off_t filepos;		// Position of next entry to read.
	std::wstring mask;  // Initial file mask.
};

struct dirent
{
	long d_ino;
	off_t d_off;
	unsigned short d_reclen;
	unsigned char d_type;	// always DT_UNKNOWN in current implementation
	char d_name[256];
};

inline constexpr unsigned char DT_UNKNOWN = 0;
inline constexpr unsigned char DT_REG = 1;
inline constexpr unsigned char DT_DIR = 2;

#define d_fileno d_ino // Backwards compatibility.

// This is the data type of directory stream objects.
// The actual structure is opaque to users.
using DIR = struct dirstream;

DIR* opendir(const char* name);
struct dirent* readdir(DIR* dir);
int closedir(DIR* dir);
void rewinddir(DIR* dir);
void seekdir(DIR* dir, off_t offset);
off_t telldir(DIR* dir);
//int dirfd(DIR* dir);

} // namespace openmsx

#endif

#endif // WIN32_DIRENT_HH
