#ifndef IMAGE_LP_WRITER_HPP
#define IMAGE_LP_WRITER_HPP

#include "Platform.hpp"
#include "ImageWriter.hpp"
#include "ImageMaskWriter.hpp"
#include "ImageLZWriter.hpp"
#include "ImageLPReader.hpp"

/*
 * Game Closure Local Palette (GC-2D-LP) Compression
 *
 * This algorithm searches for rectangular zones where the number of colors
 * used is less than a quarter of the number of pixels in the region, or some
 * sort of clever threshold like that that seems to work well; let's be honest
 * this is mainly black magic.
 *
 * Because the colors often repeat between different zones, a global palette is
 * created from each zone palette.  The global palette indices are then used to
 * reference the actual color value in each zone palette.  This global color
 * palette is compressed by converting the RGBA data to YUVA using one of the
 * color filters (the best is selected based on entropy measurement) and then
 * Huffman-encoding each color plane separately.
 *
 * Each zone is also Huffman encoded using simple prediction filters for each
 * of the fields (x,y,w,h,used).
 *
 * If more than one color is used in a zone, then the zone palette index for
 * each pixel is Huffman encoded.  If there is just one color in a zone, then
 * no bits are emitted per-pixel since it can be assumed (this happens often).
 *
 * When there are only a few colors or only a few zones, compression is not
 * performed on the overhead since it would probably not help.
 */

namespace cat {


//// ImageLPWriter

class ImageLPWriter {
	static const int ZONEW = ImageLPReader::ZONEW;
	static const int ZONEH = ImageLPReader::ZONEH;
	static const int ZONE_MAX_COLORS = 2;
	static const int MAX_COLORS = ImageLPReader::MAX_COLORS;
	static const int MAXW = 255 + ZONEW;
	static const int MAXH = 255 + ZONEH;
	static const int HUFF_COLOR_THRESH = ImageLPReader::HUFF_COLOR_THRESH;
	static const int HUFF_ZONE_THRESH = ImageLPReader::HUFF_ZONE_THRESH;
	static const int MAX_HUFF_SYMS = MAX_COLORS;

	void clear();

	u32 _colors[MAX_COLORS];

	const u8 *_rgba;
	int _width, _height;

	ImageMaskWriter *_mask;
	ImageLZWriter *_lz;

	// Visited bitmask
	u16 *_visited;

	CAT_INLINE void visit(int x, int y, int index) {
		_visited[x + y * _width] = index + 1; // will be +1 off actual index
	}

	struct Match {
		u32 colors[MAX_COLORS];
		u16 colorIndex[MAX_COLORS];
		int used;
		u16 x, y;
		u16 w, h;
		u16 codes[MAX_HUFF_SYMS];
		u8 codelens[MAX_HUFF_SYMS];
	};

	std::vector<Match> _exact_matches;

	bool expandMatch(int &used, u16 &x, u16 &y, u16 &w, u16 &h);
	void add(int used, u16 x, u16 y, u16 w, u16 h);
	int match();

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		u32 color_list_size, total_palette_entries;
		u32 color_list_overhead;
		u32 zone_list_overhead;
		u32 pixel_overhead;
		u32 pixels_covered;
		u32 zone_count;
		u32 overall_bytes;

		double compressionRatio;
	} Stats;
#endif

public:
	CAT_INLINE ImageLPWriter() {
		_visited = 0;
		_rgba = 0;
	}
	virtual CAT_INLINE ~ImageLPWriter() {
		clear();
	}

	int initFromRGBA(const u8 *rgba, int width, int height, ImageMaskWriter &mask, ImageLZWriter &lz);

	CAT_INLINE void writePixel(int match, int x, int y, ImageWriter &writer) {
		Match *m = &_exact_matches[match - 1];

		if (m->used > 1) {
			u32 color = _rgba[x + y * _width];
			int index = 0;

			// Find the color index that matches
			for (int ii = 0; ii < m->used; ++ii) {
				if (color == m->colors[ii]) {
					index = ii;
					break;
				}
			}

			writer.writeBits(m->codes[index], m->codelens[index]);
		}
	}

	CAT_INLINE u32 visited(int x, int y) {
		return _visited[x + y * _width];
	}

	void write(ImageWriter &writer);
#ifdef CAT_COLLECT_STATS
	bool dumpStats();
#else
	CAT_INLINE bool dumpStats() {
		return false;
	}
#endif
};


} // namespace cat

#endif // IMAGE_LP_WRITER_HPP

