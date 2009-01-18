// $Id:$

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
// Also modified to use ANSI calls explicitly
// Slightly reformatted/simplified to fit openMSX coding style.

#ifdef _MSC_VER

#include "win32-dirent.hh"
#include <windows.h>
#include <cstring>
#include <cstdlib>

DIR* opendir(const char* name)
{
	if (!name || !*name) return NULL;
	size_t len = strlen(name);
	char* file = static_cast<char*>(malloc(len + 3));
	strcpy(file, name);
	if ((file[len - 1] != '/') && (file[len - 1] != '\\')) {
		strcat(file, "/*");
	} else {
		strcat(file, "*");
	}

	HANDLE hnd;
	WIN32_FIND_DATAA find;
	if ((hnd = FindFirstFileA(file, &find)) == INVALID_HANDLE_VALUE) {
		free(file);
		return NULL;
	}

	DIR* dir = static_cast<DIR*>(malloc(sizeof(DIR)));
	dir->mask = file;
	dir->fd = int(hnd);
	dir->data = malloc(sizeof(WIN32_FIND_DATAA));
	dir->filepos = 0;
	memcpy(dir->data, &find, sizeof(WIN32_FIND_DATAA));
	return dir;
}

dirent* readdir(DIR* dir)
{
	static dirent entry;
	entry.d_ino = 0;
	entry.d_type = 0;

	WIN32_FIND_DATAA* find = static_cast<WIN32_FIND_DATAA*>(dir->data);
	if (dir->filepos) {
		if (!FindNextFileA(reinterpret_cast<HANDLE>(dir->fd), find)) {
			return NULL;
		}
	}

	entry.d_off = dir->filepos;
	strncpy(entry.d_name, find->cFileName, sizeof(entry.d_name));
	entry.d_reclen = strlen(find->cFileName);
	dir->filepos++;
	return &entry;
}

int closedir(DIR* dir)
{
	HANDLE hnd = reinterpret_cast<HANDLE>(dir->fd);
	free(dir->data);
	free(dir->mask);
	free(dir);
	return FindClose(hnd) ? 0 : -1;
}

void rewinddir(DIR* dir)
{
	HANDLE hnd = reinterpret_cast<HANDLE>(dir->fd);
	WIN32_FIND_DATAA* find = static_cast<WIN32_FIND_DATAA*>(dir->data);

	FindClose(hnd);
	hnd = FindFirstFileA(dir->mask, find);
	dir->fd = INT_PTR(hnd);
	dir->filepos = 0;
}

void seekdir(DIR* dir, off_t offset)
{
	rewinddir(dir);
	for (off_t n = 0; n < offset; ++n) {
		if (FindNextFileA(reinterpret_cast<HANDLE>(dir->fd),
			          static_cast<WIN32_FIND_DATAA*>(dir->data))) {
			dir->filepos++;
		}
	}
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

#endif
