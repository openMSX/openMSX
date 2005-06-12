/*
 *	Win32 MIDI utility routins for openMSX.
 *
 * Copyright (c) 2003 Reikan.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

// maintain in openMSX: $Id:


#ifdef _WIN32

#include <string>

#define WIN32_LEAN_AND_MEAN
#define _WIN32_IE 0x0400
#include <windows.h>
#include <mmsystem.h>
#define MAXPATHLEN MAX_PATH

namespace openmsx {

extern int w32_midiOutInit();
extern int w32_midiOutClean();
extern unsigned w32_midiOutGetVFNsNum();
extern std::string w32_midiOutGetVFN(unsigned nmb);
extern std::string w32_midiOutGetRDN(unsigned nmb);
extern unsigned w32_midiOutOpen(const char* vfn);
extern int w32_midiOutClose(unsigned idx);
extern int w32_midiOutPut(unsigned char value, unsigned idx);

extern int w32_midiInInit();
extern int w32_midiInClean();
extern unsigned w32_midiInGetVFNsNum();
extern std::string w32_midiInGetVFN(unsigned nmb);
extern std::string w32_midiInGetRDN(unsigned nmb);
extern unsigned w32_midiInOpen(const char* vfn, unsigned thrdid);
extern int w32_midiInClose(unsigned idx);

} // namespace openmsx

#endif // _WIN32
