/*
 *  PCL by Davide Libenzi ( Portable Coroutine Library )
 *  Copyright (C) 2003  Davide Libenzi
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Davide Libenzi <davidel@xmailserver.org>
 *
 */

#ifndef __PCL_H__
#define __PCL_H__

struct coroutine;

typedef void(entry_func)(void*);

coroutine* co_create(entry_func* func, void* data, void* stack, unsigned size);
void co_delete(coroutine* coro);

void co_call(coroutine* coro);
void co_resume();
void co_exit_to(coroutine* coro);
void co_exit();

coroutine* co_current();

#endif

