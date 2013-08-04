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

#ifndef IMAGE_RGBA_READER_HPP
#define IMAGE_RGBA_READER_HPP

#include "ImageReader.hpp"
#include "ImageMaskReader.hpp"
#include "GCIFReader.h"
#include "Filters.hpp"
#include "EntropyDecoder.hpp"
#include "MonoReader.hpp"
#include "SmartArray.hpp"
#include "LZReader.hpp"

/*
 * Game Closure RGBA Decompression
 *
 * The decompressor rebuilds the static Huffman tables generated by the encoder
 * and then iterates over each pixel from upper left to lower right.
 * Where the Dominant Color mask is set, it emits a pixel of that color.
 *
 * For the remaining pixels, the BCIF "chaos" metric selects which Huffman
 * tables to use, and filtered pixel values are emitted.  The YUV color data is
 * then reversed to RGB and then the spatial filter is reversed back to the
 * original RGB data.
 *
 * LZ and alpha masking are very cheap decoding operations.  The most expensive
 * per-pixel operation is the static Huffman decoding, which is just a table
 * lookup and some bit twiddling for the majority of decoding.  As a result the
 * decoder is exceptionally fast.  It reaches for the Pareto Frontier.
 */

namespace cat {


//// ImageRGBAReader

class ImageRGBAReader {
public:
	static const int MAX_CHAOS_LEVELS = 32;
	static const int MAX_FILTERS = 32;
	static const int ZRLE_SYMS = 128;
	static const int NUM_COLORS = 256;

	static const int NUM_LIT_SYMS = NUM_COLORS;
	static const int NUM_Y_SYMS = NUM_LIT_SYMS + LZReader::ESCAPE_SYMS;
	static const int NUM_U_SYMS = NUM_LIT_SYMS;
	static const int NUM_V_SYMS = NUM_LIT_SYMS;
	static const int NUM_ZRLE_SYMS = 128;

	static const int HUFF_LUT_BITS = 7;

protected:
	ImageMaskReader * CAT_RESTRICT _mask;

	// RGBA output data
	u8 * CAT_RESTRICT _rgba;
	u16 _xsize, _ysize;

	// Tiles
	u16 _tile_bits_x, _tile_bits_y;
	u16 _tile_xsize, _tile_ysize;
	u16 _tile_mask_x, _tile_mask_y;
	u16 _tiles_x, _tiles_y;

	struct FilterSelection {
		YUV2RGBFilterFunction cf;
		RGBAFilterFuncs sf;

		CAT_INLINE bool ready() {
			return cf != 0;
		}
	};

	// Filter functions
	RGBAFilterFuncs _sf[MAX_FILTERS];
	int _sf_count;
	SmartArray<FilterSelection> _filters;

	// Filter/Alpha decoders
	SmartArray<u8> _sf_tiles, _cf_tiles, _a_tiles;
	MonoReader _sf_decoder, _cf_decoder, _a_decoder;

	// RGB decoders
	RGBChaos _chaos;
	EntropyDecoder _y_decoder[MAX_CHAOS_LEVELS];
	EntropyDecoder _u_decoder[MAX_CHAOS_LEVELS];
	EntropyDecoder _v_decoder[MAX_CHAOS_LEVELS];

	// LZ decoder
	LZReader _lz;

	CAT_INLINE FilterSelection *readFilter(u16 x, u16 y, ImageReader & CAT_RESTRICT reader) {
		const u16 tx = x >> _tile_bits_x;
		FilterSelection * CAT_RESTRICT filter = &_filters[tx];

		if (!filter->ready()) {
			filter->cf = YUV2RGB_FILTERS[_cf_decoder.read(tx, reader)];
			filter->sf = _sf[_sf_decoder.read(tx, reader)];
		}

		return filter;
	}

	CAT_INLINE void readSafe(u16 x, u16 y, u8 * CAT_RESTRICT p, ImageReader & CAT_RESTRICT reader) {
		// Calculate YUV chaos
		u8 cy, cu, cv;
		_chaos.get(x, cy, cu, cv);

		u16 pixel_code = _y_decoder[cy].next(reader); 

		// If it is an LZ escape code,
		if (pixel_code >= 256) {
			// TODO
		} else {
			// Read YUV
			u8 YUV[3];
			YUV[0] = (u8)pixel_code;
			YUV[1] = (u8)_u_decoder[cu].next(reader);
			YUV[2] = (u8)_v_decoder[cv].next(reader);

			FilterSelection *filter = readFilter(x, y, reader);

			// Reverse color filter
			filter->cf(YUV, p);

			// Reverse spatial filter
			u8 FPT[3];
			const u8 * CAT_RESTRICT pred = filter->sf.safe(p, FPT, x, y, _xsize);
			p[0] += pred[0];
			p[1] += pred[1];
			p[2] += pred[2];

			// Read alpha pixel
			p[3] = (u8)~_a_decoder.read(x, reader);

			_chaos.store(x, YUV);
		}
	}

	// WARNING: Should be exactly the same as above, except call unsafe()
	CAT_INLINE void readUnsafe(u16 x, u16 y, u8 * CAT_RESTRICT p, ImageReader & CAT_RESTRICT reader) {
		// Calculate YUV chaos
		u8 cy, cu, cv;
		_chaos.get(x, cy, cu, cv);

		u16 pixel_code = _y_decoder[cy].next(reader); 

		// If it is an LZ escape code,
		if (pixel_code >= 256) {
			// TODO
		} else {
			// Read YUV
			u8 YUV[3];
			YUV[0] = (u8)pixel_code;
			YUV[1] = (u8)_u_decoder[cu].next(reader);
			YUV[2] = (u8)_v_decoder[cv].next(reader);

			FilterSelection *filter = readFilter(x, y, reader);

			// Reverse color filter
			filter->cf(YUV, p);

			// Reverse spatial filter
			u8 FPT[3];
			const u8 * CAT_RESTRICT pred = filter->sf.unsafe(p, FPT, x, y, _xsize);
			p[0] += pred[0];
			p[1] += pred[1];
			p[2] += pred[2];

			// Read alpha pixel
			p[3] = (u8)~_a_decoder.read_unsafe(x, reader);

			_chaos.store(x, YUV);
		}
	}

	int readLZMatch(u16 pixel_code, ImageReader & CAT_RESTRICT reader, int x, u8 * CAT_RESTRICT p);
	int readFilterTables(ImageReader & CAT_RESTRICT reader);
	int readRGBATables(ImageReader & CAT_RESTRICT reader);
	int readPixels(ImageReader & CAT_RESTRICT reader);

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		double readFilterTablesUsec, readChaosTablesUsec;
		double readPixelsUsec, overallUsec;
	} Stats;
#endif

public:
	int read(ImageReader & CAT_RESTRICT reader, ImageMaskReader & CAT_RESTRICT maskReader, GCIFImage * CAT_RESTRICT image);

#ifdef CAT_COLLECT_STATS
	bool dumpStats();
#else
	CAT_INLINE bool dumpStats() {
		return false;
	}
#endif
};


} // namespace cat

#endif // IMAGE_CM_READER_HPP

