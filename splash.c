// NULD loading screen for link.elf (2026-11-03): the "NULD — identidad" canvas drawn by the GS. A thread at a lower
// priority than the main one draws it while the main thread sleeps in the IOP reset, DHCP, the HTTP connects and
// the cover downloads: a perspective floor and a wire sphere (gfx_line, alpha fading with depth), the four logo
// variants (A ring, B "$ nulld", C NULD//, D label) 3 s each with a glitch cut (the logo redrawn in scissored bands,
// shifted), the status line with a blinking cursor, scanlines. The logo type is vector (logo_data.c, tools/logo.py):
// the real Michroma / Archivo / IBM Plex Mono outlines, which no atlas has room for at these sizes.
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <kernel.h>
#include "gfx.h"
#include "splash.h"

#define BG 0x06070A
#define FG 0xE6E8EC
#define MUTED 0x8A919C
#define ACID 0xC8FF2E
#define BLUE 0x2B4BFF
#define PI 3.14159265f

static volatile int run, done, tid = -1;
static char msg[200];
const char *splash_ver = "1.0";
static unsigned char stack[0x8000] __attribute__((aligned(16)));

// ---- strokes: a point list, then gfx_stroke ----
static float px[160], py[160];
static int np;
static void P(float x, float y) { if (np < 160) px[np] = x, py[np] = y, np++; }
static void stroke(float t, unsigned c) { gfx_stroke(px, py, np, t, c); np = 0; }
static void seg(float x0, float y0, float x1, float y1, float t, unsigned c) { np = 0, P(x0, y0), P(x1, y1), stroke(t, c); }

// ---- vector text: the real logo fonts (logo_data.c) ----
static const vglyph *vfind(const vfont *f, char ch)
{
	for (int i = 0; i < f->n; i++)
		if (f->g[i].ch == ch) return &f->g[i];
	return NULL;
}
float splash_vtext_w(const vfont *f, float cap, float track, const char *s)
{
	float sc = cap / f->cap, w = 0;
	for (const char *p = s; *p; p++) {
		const vglyph *g = vfind(f, *p);
		w += (g ? g->adv : f->upm * 0.6f) * sc + (p[1] ? track * f->upm * sc : 0); // a space: 0.6 em
	}
	return w;
}
void splash_vtext(const vfont *f, float x, float base, float cap, float track, const char *s, unsigned rgb)
{
	float sc = cap / f->cap;
	for (const char *p = s; *p; p++) {
		const vglyph *g = vfind(f, *p);
		if (g) gfx_vmesh(g->v, g->nv, g->tri, g->ntri, g->ctr, g->nctr, x, base, sc, rgb);
		x += (g ? g->adv : f->upm * 0.6f) * sc + track * f->upm * sc;
	}
}

void splash_mark(float cx, float cy, float r)
{
	float t = r * 0.18f;
	np = 0;
	for (int i = -1; i <= 49; i++) P(cx + (r - t / 2) * cosf(i * 2 * PI / 48), cy + (r - t / 2) * sinf(i * 2 * PI / 48));
	stroke(t, FG);
	float d = r * 0.985f;
	seg(cx - d, cy + d, cx + d, cy - d, t, ACID);
}

// ---- the four variants (canvas board "Logo alterno"), centred on (640, 300); dx shifts them for the glitch ----
static int blink(float s) { return (int)(s * 1.9f) & 1; }
static void variant(int k, float dx, float s)
{
	if (k == 0) { // A: the ring + NULD in Michroma (128 px type: 96 px caps)
		float cap = 96, ww = splash_vtext_w(&vfont_michroma, cap, 0.06f, "NULD"), x = 640 - (132 + 40 + ww) / 2 + dx;
		splash_mark(x + 66, 300, 66);
		splash_vtext(&vfont_michroma, x + 172, 300 + cap / 2, cap, 0.06f, "NULD", FG);
	} else if (k == 1) { // B: $ nulld in IBM Plex Mono 500 (132 px type) + a block cursor
		float fs = 132, cap = fs * vfont_plex_medium.cap / vfont_plex_medium.upm, base = 300 + cap / 2;
		float ww = splash_vtext_w(&vfont_plex_medium, cap, -0.02f, "$ nulld"), x = 640 - (ww + 0.52f * fs) / 2 + dx;
		splash_vtext(&vfont_plex_medium, x, base, cap, -0.02f, "$", ACID);
		splash_vtext(&vfont_plex_medium, x + splash_vtext_w(&vfont_plex_medium, cap, -0.02f, "$ ") - 0.02f * fs, base, cap, -0.02f, "nulld", FG);
		if (!blink(s)) gfx_rect((int)(x + ww + 0.08f * fs), (int)(base - cap), (int)(0.44f * fs), (int)(cap + 0.1f * fs), ACID);
	} else if (k == 2) { // C: NULD// in Archivo 800 at 125 % (132 px type)
		float cap = 132 * 0.686f, ww = splash_vtext_w(&vfont_archivo_heavy, cap, 0.02f, "NULD//"), x = 640 - ww / 2 + dx;
		float wn = splash_vtext_w(&vfont_archivo_heavy, cap, 0.02f, "NULD") + 0.02f * 132;
		splash_vtext(&vfont_archivo_heavy, x, 300 + cap / 2, cap, 0.02f, "NULD", FG);
		splash_vtext(&vfont_archivo_heavy, x + wn, 300 + cap / 2, cap, 0.02f, "//", ACID);
	} else { // D: the hardware label
		int bw = 680, x = 300 + (int)dx, y = 196, top = 150, bot = 64;
		gfx_rect(x, y, bw, 2, FG), gfx_rect(x, y + top + bot, bw, 2, FG);
		gfx_rect(x, y, 2, top + bot + 2, FG), gfx_rect(x + bw - 2, y, 2, top + bot + 2, FG);
		gfx_rect(x, y + top, bw, 2, FG);
		splash_mark(x + 34 + 52, y + top / 2 + 2, 52);
		float cap = 88 * 0.686f;
		splash_vtext(&vfont_archivo_heavy, x + 34 + 104 + 32, y + top / 2 + 2 + cap / 2, cap, 0.02f, "NULD", FG);
		static const unsigned char bars[30] = {3, 1, 2, 1, 4, 1, 1, 3, 2, 1, 1, 2, 4, 1, 3, 1, 2, 2, 1, 4, 1, 1, 3, 1, 2, 1, 4, 2, 1, 3};
		int bx = x + 18;
		for (int i = 0; i < 30; i++) gfx_rect(bx, y + top + 14, bars[i], bot - 26, FG), bx += bars[i] + 2;
		gfx_rect(bx + 14, y + top, 2, bot, FG);
		gfx_text(&gfx_font_mono, bx + 32, y + top + 12, "SCPH-75001 · EE 294.912 MHz", 0xC9CED6);
		char v[48];
		snprintf(v, sizeof(v), "v%s · 2026 · nuld.local", splash_ver);
		gfx_text(&gfx_font_mono, bx + 32, y + top + 34, v, ACID);
		int sx = x + (int)fmodf(s * 175, (float)bw); // the scanner passing over the label
		gfx_alpha(0x60), gfx_rect(sx, y + 2, 2, top + bot - 2, ACID), gfx_alpha(0x80);
	}
}
static const int variant_top[4] = {230, 232, 252, 190}, variant_bot[4] = {370, 372, 348, 420};

// ---- background ----
void splash_floor(float s, unsigned rgb, int amax)
{
	gfx_alpha(0x80), gfx_rect(0, 0, GFX_W, GFX_H, BG);
	const float cx = 640, yh = 432, k = 288; // y = yh + k / z
	float ph = fmodf(s * 0.9f, 1);
	for (int i = 0; i < 16; i++) { // across, scrolling towards the viewer; far ones fade before they crowd
		float z = i + 1 - ph;
		if (z < 0.5f) continue;
		float y = yh + k / z, a = amax * (1 - (z - 1) / 9);
		// a 1 px rect, not gfx_line: AA1 lines take their alpha from coverage, and a horizontal one has full
		// coverage, so the depth fade was lost and the far lines crowded bright at the horizon (PCSX2, 2026-11-03)
		if (a >= 2 && y < GFX_H) gfx_alpha((int)a), gfx_rect(0, (int)y, GFX_W, 1, rgb);
	}
	gfx_alpha(0x80);
	for (int i = -14; i <= 14; i++) // depth lines to the vanishing point
		gfx_line(cx + 300.f * i / 0.6f, yh + k / 0.6f, cx + 300.f * i / 10, yh + k / 10, rgb, amax, 0);
}
static void sphere(float s)
{
	const float cx = 640, cy = 300, R = 196, ct = cosf(-0.28f), st = sinf(-0.28f);
	float rot = s * 2 * PI / 28;
	float x0 = 0, y0 = 0, a0 = 0;
	for (int m = 0; m < 9 + 5; m++)
		for (int i = 0, n = m < 9 ? 24 : 36; i <= n; i++) {
			float lat, lon;
			if (m < 9) lat = -PI / 2 + PI * i / n, lon = m * PI / 9 + rot;
			else lat = (m - 11) * PI / 6, lon = 2 * PI * i / n + rot;
			float x = cosf(lat) * cosf(lon), y = sinf(lat), z = cosf(lat) * sinf(lon);
			float y2 = y * ct - z * st, z2 = y * st + z * ct;
			float sx = cx + R * x, sy = cy - R * y2, a = 0x12 + (z2 + 1) * 0x1C;
			if (i) gfx_line(x0, y0, sx, sy, ACID, (int)a0, (int)a);
			x0 = sx, y0 = sy, a0 = a;
		}
}

void splash_scan(void)
{
	gfx_alpha(0x09);
	for (int sy = 0; sy < GFX_H; sy += 3) gfx_rect(0, sy, GFX_W, 1, FG);
	gfx_alpha(0x80);
}

void splash_hud(int x, int y, int w, int h, int len)
{
	gfx_rect(x, y, len, 2, ACID), gfx_rect(x, y, 2, len, ACID);
	gfx_rect(x + w - len, y, len, 2, ACID), gfx_rect(x + w - 2, y, 2, len, ACID);
	gfx_rect(x, y + h - 2, len, 2, ACID), gfx_rect(x, y + h - len, 2, len, ACID);
	gfx_rect(x + w - len, y + h - 2, len, 2, ACID), gfx_rect(x + w - 2, y + h - len, 2, len, ACID);
}

void splash_draw(float s)
{
	int k = (int)(s / 3) & 3;
	float in = fmodf(s, 3);
	gfx_alpha(0x80);
	splash_floor(s, ACID, 0x40);
	sphere(s);
	int cut = in < 0.18f || in > 2.9f; // the glitch at both ends of each variant
	if (!cut) variant(k, 0, s);
	else {
		static const signed char off[5] = {-10, 7, -4, 9, -6};
		int f = (int)(s * 60), top = variant_top[k] - 10, h = (variant_bot[k] - variant_top[k] + 20) / 5;
		gfx_scissor(0, 0, GFX_W - 1, top - 1), variant(k, 0, s);
		for (int b = 0; b < 5; b++) {
			gfx_scissor(0, top + b * h, GFX_W - 1, top + (b + 1) * h - 1);
			variant(k, off[(b + f) % 5], s);
		}
		gfx_scissor(0, top + 5 * h, GFX_W - 1, GFX_H - 1), variant(k, 0, s);
		gfx_scissor(0, 0, GFX_W - 1, GFX_H - 1);
		if (in < 0.05f) gfx_alpha(0x0C), gfx_rect(0, 0, GFX_W, GFX_H, FG), gfx_alpha(0x80); // a 3-frame flash on the cut in
	}
	// status line(s), centred under the logo, the last with a cursor
	char line[200];
	const char *p = msg;
	int y = 486;
	for (int l = 0; l < 4 && *p; l++, y += 24) {
		const char *e = strchr(p, '\n');
		int n = e ? (int)(e - p) : (int)strlen(p);
		snprintf(line, sizeof(line), "%.*s", n, p);
		int w = gfx_text_width(&gfx_font_mono, line), x = 640 - w / 2;
		gfx_alpha(0x80), gfx_rect(x - 12, y - 4, w + 24 + (e ? 0 : 14), 24, BG);
		gfx_text(&gfx_font_mono, x, y, line, l ? MUTED : FG);
		if (!e && !blink(s)) gfx_rect(x + w + 4, y, 8, 16, ACID);
		p = e ? e + 1 : p + n;
	}
	// HUD corners and the edge labels
	splash_hud(24, 24, GFX_W - 48, GFX_H - 48, 28);
	gfx_tracking(1);
	char ver[24];
	snprintf(ver, sizeof(ver), "NULD %s", splash_ver);
	gfx_text(&gfx_font_mono, 64, 30, ver, MUTED);
	const char *r = "EE R5900 · 294.912 MHz";
	gfx_text(&gfx_font_mono, GFX_W - 64 - gfx_text_width(&gfx_font_mono, r), 30, r, MUTED);
	gfx_text(&gfx_font_mono, GFX_W - 64 - gfx_text_width(&gfx_font_mono, "SCPH-75001"), GFX_H - 44, "SCPH-75001", MUTED);
	gfx_tracking(0);
	splash_scan();
}

static void frame(float s) { gfx_begin(), splash_draw(s), gfx_end(), gfx_flip(); }

static void splash_thread(void *arg)
{
	(void)arg;
	clock_t t0 = clock();
	while (run) frame((float)(clock() - t0) / CLOCKS_PER_SEC);
	done = 1;
	ExitThread();
}

void splash_msg(const char *s) { snprintf(msg, sizeof(msg), "%s", s); } // ponytail: a torn line for one frame at worst

void splash_start(void)
{
	if (run) return;
	extern void *_gp;
	// under the main thread (0x60): it runs while the main one sleeps on the IOP, the network or the PC
	ee_thread_t t = {.func = splash_thread, .stack = stack, .stack_size = sizeof(stack), .gp_reg = &_gp, .initial_priority = 0x70};
	run = 1, done = 0;
	tid = CreateThread(&t);
	StartThread(tid, NULL);
}

void splash_stop(void)
{
	if (!run) return;
	run = 0;
	while (!done) usleep(5000);
	DeleteThread(tid), tid = -1;
}
