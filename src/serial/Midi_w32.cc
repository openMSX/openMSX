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


#ifdef __WIN32__

#define WIN32_LEAN_AND_MEAN
#define _WIN32_IE 0x0400

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <windows.h>
#include <mmsystem.h>

#define MAXPATHLEN MAX_PATH

namespace openmsx {

/*
 * MIDI I/O helper functions for Win32.
 */
// MIDI SYSTEM MESSAGES max length is not defined...
// TODO: support variable length.
#define OPENMSX_W32_MIDI_SYSMES_MAXLEN 4096


struct vfn_midi {
	unsigned idx;
	unsigned devid;
	HMIDI    handle;
	char     vfname[MAXPATHLEN + 1];
	char     devname[MAXPNAMELEN];
};

struct outbuf {
	DWORD    shortmes;
	unsigned longmes_cnt;
	char     longmes[OPENMSX_W32_MIDI_SYSMES_MAXLEN];
	MIDIHDR  header;
};

static struct vfn_midi *vfnt_midiout, *vfnt_midiin;
static unsigned vfnt_midiout_num, vfnt_midiin_num;

static int *state_out;
static struct outbuf *buf_out;

static MIDIHDR inhdr;
static char inlongmes[OPENMSX_W32_MIDI_SYSMES_MAXLEN];


static void w32_midiDevNameConv(char *dst, char *src)
{
	unsigned len = strlen(src);
	unsigned i;
	for (i = 0; i < len; ++i) {
		if ((src[i] < '0') || (src[i] > 'z') ||
		    ((src[i] > '9') && (src[i] < 'A')) ||
		    ((src[i] > 'Z') && (src[i] < 'a'))) {
			dst[i] = '_';
		} else {
			dst[i] = src[i];
		}
	}
	dst[i] = '\0';
}


// MIDI-OUT
static int w32_midiOutFindDev(unsigned *idx, unsigned *dev, const char *vfn)
{
	for (unsigned i = 0; i < vfnt_midiout_num; ++i) {
		if (!strcmp(vfnt_midiout[i].vfname, vfn)) {
			*idx = i;
			*dev = vfnt_midiout[i].devid;
			return 0;
		}
	}
	return -1;
}


int w32_midiOutInit()
{
	vfnt_midiout_num = 0;
	unsigned num = midiOutGetNumDevs();
	if (!num) {
		return 0;
	}
	if ((state_out = (int*)malloc((num + 1) * sizeof(int))) == NULL) {
		return 1;
	}
	memset(state_out, 0, (num + 1) * sizeof(int));

	if ((buf_out = (struct outbuf*)malloc((num + 1) * sizeof(struct outbuf))) == NULL) {
		return 1;
	}
	memset(buf_out, 0, (num + 1) * sizeof(struct outbuf));

	if ((vfnt_midiout = (struct vfn_midi*)malloc((num + 1) * sizeof(struct vfn_midi))) == NULL) {
		return 1;
	}

	MIDIOUTCAPSA cap;
	if (midiOutGetDevCapsA(MIDI_MAPPER, &cap, sizeof(cap)) != MMSYSERR_NOERROR) {
		return 2;
	}
	vfnt_midiout[0].devid = MIDI_MAPPER;
	w32_midiDevNameConv(vfnt_midiout[0].devname, cap.szPname);
	strncpy(vfnt_midiout[0].vfname, "midi", MAXPATHLEN + 1);
	vfnt_midiout_num ++;

	for (unsigned i = 0; i < num; ++i) {
		if (midiOutGetDevCapsA(i, &cap, sizeof(cap)) != MMSYSERR_NOERROR) {
			return 0;	// atleast MIDI-MAPPER is available...
		}
		vfnt_midiout[i + 1].devid = i;
		w32_midiDevNameConv(vfnt_midiout[i + 1].devname, cap.szPname);
		snprintf(vfnt_midiout[i + 1].vfname, MAXPATHLEN + 1, "midi%u", i);
		vfnt_midiout_num++;
	}
	return 0;
}

int w32_midiOutClean()
{
	vfnt_midiout_num = 0;
	free(vfnt_midiout);
	free(buf_out);
	free(state_out);
	return 0;
}


unsigned w32_midiOutGetVFNsNum()
{
	return vfnt_midiout_num;
}

int w32_midiOutGetVFN(char *vfn, unsigned nmb)
{
	if (!vfnt_midiout_num || (nmb >= vfnt_midiout_num)) {
		vfn[0] = '\0';
		return 1;
	}
	strcpy(vfn, vfnt_midiout[nmb].vfname);
	return 0;
}

int w32_midiOutGetRDN(char *rdn, unsigned nmb)
{
	if (!vfnt_midiout_num || (nmb >= vfnt_midiout_num)) {
		rdn[0] = '\0';
		return 1;
	}
	strcpy(rdn, vfnt_midiout[nmb].devname);
	return 0;
}


unsigned w32_midiOutOpen(const char *vfn)
{
	unsigned idx, devid;
	if (w32_midiOutFindDev(&idx, &devid,vfn)) {
		return (unsigned)-1;
	}
	if (midiOutOpen((HMIDIOUT*)&vfnt_midiout[idx].handle, devid, 0, 0 ,0) != MMSYSERR_NOERROR) {
		return (unsigned)-1;
	}
	return idx;
}

int w32_midiOutClose(unsigned idx)
{
	midiOutReset((HMIDIOUT)vfnt_midiout[idx].handle);
	if (midiOutClose((HMIDIOUT)vfnt_midiout[idx].handle) == MMSYSERR_NOERROR) {
		return 0;
	} else {
		return -1;
	}
}


static int w32_midiOutFlushExclusiveMsg(unsigned idx)
{
	int i;
	buf_out[idx].header.lpData = buf_out[idx].longmes;
	buf_out[idx].header.dwBufferLength = buf_out[idx].longmes_cnt;
	buf_out[idx].header.dwFlags = 0;
	if ((i = midiOutPrepareHeader((HMIDIOUT)vfnt_midiout[idx].handle, &buf_out[idx].header, sizeof(buf_out[idx].header))) != MMSYSERR_NOERROR) {
		fprintf(stderr, "midiOutPrepareHeader() returned %i.\n", i);
		fflush(stderr);
		exit(1);
	}
	if ((i = midiOutLongMsg((HMIDIOUT)vfnt_midiout[idx].handle, &buf_out[idx].header, sizeof(buf_out[idx].header))) != MMSYSERR_NOERROR) {
		fprintf(stderr, "midiOutLongMsg() returned %i.\n", i);
		fflush(stderr);
		exit(1);
	}
	// Wait sending in driver.
	// This may take long...
	while (!(buf_out[idx].header.dwFlags & MHDR_DONE)) {
		Sleep(1);
	}
	// Sending Exclusive done.
	if ((i = midiOutUnprepareHeader((HMIDIOUT)vfnt_midiout[idx].handle, &buf_out[idx].header, sizeof(buf_out[idx].header))) != MMSYSERR_NOERROR) {
		fprintf(stderr, "midiOutUnPrepareHeader() returned %i.\n", i);
		fflush(stderr);
		exit(1);
	}
	buf_out[idx].longmes_cnt = 0;
	return 0;
}

int w32_midiOutPut(unsigned char value, unsigned idx)
{
	if ((state_out[idx] & 0x1000) || ((value & 0x0ff) == 0x0f0)) {
		if (!(state_out[idx] & 0x1000)) {
			// SYSTEM MESSAGE Exclusive start
			state_out[idx] |= 0x1000;
		}
		if (buf_out[idx].longmes_cnt >= OPENMSX_W32_MIDI_SYSMES_MAXLEN) {
			return -1;
		}
		buf_out[idx].longmes[buf_out[idx].longmes_cnt++] = value;

		if (value == 0x0f7) {
			// SYSTEM MESSAGES Exclusive end
			w32_midiOutFlushExclusiveMsg(idx);
			state_out[idx] &= ~0x1000;
		}
	} else {
		switch (state_out[idx]) {
		case 0x0000:
			switch (value & 0x0f0) {
			case 0x080:	// Note Off
			case 0x090:	// Note On
			case 0x0a0:	// Key Pressure
			case 0x0b0:	// Control Change
			case 0x0e0:	// Pitch Wheel
				state_out[idx] = 0x0082;
				buf_out[idx].shortmes = ((DWORD)value) & 0x0ff;
				break;
			case 0x0c0:	// Program Change
			case 0x0d0:	// After Touch
				state_out[idx] = 0x0041;
				buf_out[idx].shortmes = ((DWORD)value) & 0x0ff;
				break;
			case 0x0f0:	// SYSTEM MESSAGE (other than "EXCLUSIVE")
				switch (value &0x0f) {
					case 0x02:	// Song Position Pointer
						state_out[idx] = 0x0082;
						buf_out[idx].shortmes = ((DWORD)value) & 0x0ff;
						break;
					case 0x01:	// Time Code
					case 0x03:	// Song Select
						state_out[idx] = 0x0041;
						buf_out[idx].shortmes = ((DWORD)value) & 0x0ff;
						break;
					default:	// Timing Clock, Sequencer Start, Sequencer Continue,
							// Sequencer Stop, Cable Check, System Reset, and Unknown...
						state_out[idx] = 0;
						buf_out[idx].shortmes = ((DWORD)value) & 0x0ff;
						midiOutShortMsg((HMIDIOUT)vfnt_midiout[idx].handle,buf_out[idx].shortmes);
						break;
				}
				break;
			default:
				state_out[idx] = 0;
				buf_out[idx].shortmes = ((DWORD)value) & 0x0ff;
				midiOutShortMsg((HMIDIOUT)vfnt_midiout[idx].handle,buf_out[idx].shortmes);
				break;
			}
			break;
		case 0x0041:
			buf_out[idx].shortmes |= ((((DWORD)value) & 0x0ff) << 8);
			midiOutShortMsg((HMIDIOUT)vfnt_midiout[idx].handle,buf_out[idx].shortmes);
			state_out[idx] = 0;
			break;
		case 0x0082:
			buf_out[idx].shortmes |= ((((DWORD)value) & 0x0ff) << 8);
			state_out[idx] = 0x0081;
			break;
		case 0x0081:
			buf_out[idx].shortmes |= ((((DWORD)value) & 0x0ff) << 16);
			midiOutShortMsg((HMIDIOUT)vfnt_midiout[idx].handle,buf_out[idx].shortmes);
			state_out[idx] = 0;
			break;
		default:
			// not reach...
			midiOutShortMsg((HMIDIOUT)vfnt_midiout[idx].handle,((DWORD)value) & 0x0ff);
			break;
		}
	}
	return 0;
}


// MIDI-IN
static int w32_midiInFindDev(unsigned *idx, unsigned *dev, const char *vfn)
{
	for (unsigned i = 0; i < vfnt_midiin_num; ++i) {
		if (!strcmp(vfnt_midiin[i].vfname, vfn)) {
			*idx = i;
			*dev = vfnt_midiin[i].devid;
			return 0;
		}
	}
	return -1;
}


int w32_midiInInit()
{
	vfnt_midiin_num = 0;
	unsigned num = midiInGetNumDevs();
	if (!num) {
		return 0;
	}
	if ((vfnt_midiin = (struct vfn_midi*)malloc((num + 1) * sizeof(struct vfn_midi))) == NULL) {
		return	1;
	}
	for (unsigned i = 0; i < num; ++i) {
		MIDIINCAPSA cap;
		if (midiInGetDevCapsA(i, &cap, sizeof(cap)) != MMSYSERR_NOERROR) {
			return 1;
		}
		vfnt_midiin[i].devid = i;
		w32_midiDevNameConv(vfnt_midiin[i].devname, cap.szPname);
		snprintf(vfnt_midiin[i].vfname, MAXPATHLEN + 1, "midi%u", i);
		vfnt_midiin_num++;
	}
	return 0;
}

int w32_midiInClean()
{
	vfnt_midiin_num = 0;
	free(vfnt_midiout);
	return 0;
}


unsigned w32_midiInGetVFNsNum()
{
	return vfnt_midiin_num;
}

int w32_midiInGetVFN(char *vfn, unsigned nmb)
{
	if (!vfnt_midiin_num || (nmb >= vfnt_midiin_num)) {
		vfn[0] = '\0';
		return 1;
	}
	strcpy(vfn, vfnt_midiin[nmb].vfname);
	return 0;
}

int w32_midiInGetRDN(char *rdn, unsigned nmb)
{
	if (!vfnt_midiin_num || (nmb >= vfnt_midiin_num)) {
		rdn[0] = '\0';
		return 1;
	}
	strcpy(rdn, vfnt_midiin[nmb].devname);
	return 0;
}


unsigned w32_midiInOpen(const char *vfn, unsigned thrdid)
{
	unsigned idx, devid;
	if (w32_midiInFindDev(&idx, &devid, vfn)) {
		return (unsigned)-1;
	}
	if (midiInOpen((HMIDIIN*)&vfnt_midiin[idx].handle, devid, thrdid, 0, CALLBACK_THREAD) != MMSYSERR_NOERROR) {
		return (unsigned)-1;
	}
	memset(&inhdr, 0, sizeof(MIDIHDR));
	inhdr.lpData = inlongmes;
	inhdr.dwBufferLength = OPENMSX_W32_MIDI_SYSMES_MAXLEN;
	if (midiInPrepareHeader((HMIDIIN)vfnt_midiin[idx].handle, (LPMIDIHDR)&inhdr, sizeof(MIDIHDR)) != MMSYSERR_NOERROR) {
		return (unsigned)-1;
	}
	if (midiInAddBuffer((HMIDIIN)vfnt_midiin[idx].handle, (LPMIDIHDR)&inhdr, sizeof(MIDIHDR)) != MMSYSERR_NOERROR) {
		return (unsigned)-1;
	}
	return idx;
}

int w32_midiInClose(unsigned idx)
{
	midiInStop((HMIDIIN)vfnt_midiin[idx].handle);
	midiInReset((HMIDIIN)vfnt_midiin[idx].handle);
	midiInUnprepareHeader((HMIDIIN)vfnt_midiin[idx].handle, (LPMIDIHDR)&inhdr, sizeof(MIDIHDR));
	if (midiInClose((HMIDIIN)vfnt_midiin[idx].handle) == MMSYSERR_NOERROR) {
		return 0;
	} else {
		return -1;
	}
}

} // namespace openmsx

#endif // __WIN32__
