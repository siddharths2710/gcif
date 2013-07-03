/*
	Copyright (c) 2013 Game Closure.  All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	* Redistributions of source code must retain the above copyright notice,
	  this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	* Neither the name of GCIF nor the names of its contributors may be used
	  to endorse or promote products derived from this software without
	  specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef LZ_MATCH_FINDER_HPP
#define LZ_MATCH_FINDER_HPP

#include "../decoder/Platform.hpp"
#include "../decoder/SmartArray.hpp"

/*
 * LZ Match Finder
 *
 * This LZ system is only designed for RGBA data at this time.
 */

namespace cat {


//// LZMatchFinder

class LZMatchFinder {
	static const int MAX_MATCH = 4096; // pixels
	static const int WIN_SIZE = 1024 * 1024; // pixels

	/*
	 * Encoding cost in bits for RGBA data:
	 *
	 * ~LEN_PREFIX_COST bits for Y-channel escape code and length bit range
	 * ~log2(length)-K bits for length extension bits
	 * log2(40) ~= DIST_PREFIX_COST bits for distance bit range
	 * ~log2(distance)-K bits for the distance extension bits
	 *
	 * Assuming the normal compression ratio of a 32-bit RGBA pixel is 3.6:1,
	 * it saves about SAVED_PIXEL_BITS bits per RGBA pixel that we can copy.
	 *
	 * Two pixels is about breaking even, though it can be a win if it's
	 * from the local neighborhood.  For decoding speed it is preferred to
	 * use LZ since it avoids a bunch of Huffman decodes.  And most of the
	 * big LZ wins are on computer-generated artwork where neighboring
	 * scanlines can be copied, so two-pixel copies are often useful.
	 */
	static const int RGBA_MIN_MATCH = 2; // pixels
	static const int RGBA_DIST_PREFIX_COST = 7; // bits
	static const int RGBA_LEN_PREFIX_COST = 5; // bits
	static const int RGBA_SAVED_PIXEL_BITS = 9; // bits

	static const int RGBA_HASH_BITS = 18;
	static const int RGBA_HASH_SIZE = 1 << RGBA_HASH_BITS;
	static const u64 RGBA_HASH_MULT = 0xc6a4a7935bd1e995ULL;

	// Returns hash for RGBA_MIN_MATCH pixels
	static CAT_INLINE u32 HashRGBA(const u32 * CAT_RESTRICT rgba) {
		(u32)( ( ((u64)rgba[0] << 32) | rgba[1] ) * RGBA_HASH_MULT >> (64 - RGBA_HASH_BITS) );
	}

	/*
	 * Encoding cost in bits for monochrome data:
	 */
	static const int MONO_MIN_MATCH = 6; // pixels
	static const int MONO_DIST_PREFIX_COST = 7; // bits
	static const int MONO_LEN_PREFIX_COST = 5; // bits
	static const int MONO_SAVED_PIXEL_BITS = 2; // bits

	static const int MONO_HASH_BITS = 18;
	static const int MONO_HASH_SIZE = 1 << MONO_HASH_BITS;
	static const u64 MONO_HASH_MULT = 0xc6a4a7935bd1e995ULL;

	// Returns hash for MONO_MIN_MATCH pixels
	static CAT_INLINE u32 HashMono(const u8 * CAT_RESTRICT mono) {
		(u32)( ( ((u64)rgba[0] << 32) | rgba[1] ) * RGBA_HASH_MULT >> (64 - RGBA_HASH_BITS) );
	}

	static const u32 GUARD_OFFSET = 0xffffffff;

	// Hash Chain search structure
	SmartArray<u32> _table;
	SmartArray<u32> _chain;

	// Match list, with guard at end
	struct LZMatch {
		u32 offset;
		u32 distance;
		u16 length;

		CAT_INLINE LZMatch(u32 offset, u32 distance, u16 length) {
			this->offset = offset;
			this->distance = distance;
			this->length = length;
		}
	};

	vector<LZMatch> _matches;
	LZMatch *_next_match;

public:
	bool scanRGBA(const u32 * CAT_RESTRICT rgba, int xsize, int ysize);
	bool scanMono(const u8 * CAT_RESTRICT mono, int xsize, int ysize);

	CAT_INLINE int size() {
		return static_cast<int>( _matches.size() );
	}

	CAT_INLINE void reset() {
		_next_match = &_matches[0];
	}

	// Once the guard offset is hit, pops should be avoided
	CAT_INLINE u32 peekOffset() {
		return _next_match->offset;
	}

	CAT_INLINE LZMatch *pop() {
		return _next_match++;
	}
};


} // namespace cat

#endif // LZ_MATCH_FINDER_HPP
