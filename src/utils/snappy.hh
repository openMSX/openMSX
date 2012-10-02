/////////////////////////////////////////////////////////////////////////
//
// Copyright 2005 and onwards Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// A light-weight compression algorithm.  It is designed for speed of
// compression and decompression, rather than for the utmost in space
// savings.
//
// For getting better compression ratios when you are compressing data
// with long repeated sequences or compressing data that is similar to
// other data, while still compressing fast, you might look at first
// using BMDiff and then compressing the output of BMDiff with
// Snappy.
//
/////////////////////////////////////////////////////////////////////////
//
// The snappy code is modified quite a bit for openMSX. The original
// verion can be obtained here:
//    http://code.google.com/p/snappy/
//
// The main difference are:
// - Rewritten to reuse existing openMSX helper functions and style.
// - Removed possibility to operate on chunks of data, the current code
//   requires the full to-be-(de)compressed memory block in one go.
// - Removed all safety checks during decompression. The original code
//   would return an error on invalid input, this code will crash on
//   such input (but that shouldn't happen because we only feed input
//   that was previously produced by the compression routine (and always
//   keeping that compressed block in memory).
// The motivation for this rewrite is to:
// - Reduce code duplication between the snappy code and the rest of
//   openMSX.
// - Gain some extra speed at the expense of flexibility (which we don't
//   need in openMSX).
//
/////////////////////////////////////////////////////////////////////////

#ifndef SNAPPY_HH
#define SNAPPY_HH

#include <cstddef>

namespace snappy {
	void compress(const char* input, size_t inLen,
	              char* output, size_t& outLen);
	void uncompress(const char* input, size_t inLen,
	                char* output, size_t outLen);
	size_t maxCompressedLength(size_t inLen);
}

#endif
