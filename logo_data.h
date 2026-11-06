#pragma once
// The NULD logo glyphs as vector meshes (tools/logo.py): font units, y up
typedef struct {
	char ch;
	const short *v; int nv;              // points, x y pairs
	const unsigned short *tri; int ntri; // triangles, 3 point indices each
	const unsigned short *ctr; int nctr; // contour starts in v (each runs to the next start, closed)
	short adv;
} vglyph;
typedef struct { const vglyph *g; int n, upm, cap, xh; } vfont; // cap / x height in font units
extern const vfont vfont_michroma, vfont_archivo_heavy, vfont_plex_medium;
