/*
 * Copyright (C) 2012 Marcin Kościelnicki <koriakin@0x04.net>
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "hwtest.h"
#include "pgraph.h"
#include "nvhw/pgraph.h"
#include "nvhw/chipset.h"
#include "nva.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 * TODO:
 *  - cliprects
 *  - point VTX methods
 *  - lin/line VTX methods
 *  - tri VTX methods
 *  - rect VTX methods
 *  - tex* COLOR methods
 *  - ifc/bitmap VTX methods
 *  - ifc COLOR methods
 *  - bitmap MONO methods
 *  - blit VTX methods
 *  - blit pixel ops
 *  - tex* pixel ops
 *  - point rasterization
 *  - line rasterization
 *  - lin rasterization
 *  - tri rasterization
 *  - rect rasterization
 *  - texlin calculations
 *  - texquad calculations
 *  - quad rasterization
 *  - blit/ifc/bitmap rasterization
 *  - itm methods
 *  - ifm methods
 *  - notify
 *  - range interrupt
 *  - interrupt handling
 *  - ifm rasterization
 *  - ifm dma
 *  - itm rasterization
 *  - itm dma
 *  - itm pixel ops
 *  - cleanup & refactor everything
 */

namespace {

using namespace hwtest::pgraph;

class MthdCtxSwitchTest : public MthdTest {
	bool special_notify() override {
		return true;
	}
	void adjust_orig_mthd() override {
		insrt(orig.notify, 16, 1, 0);
	}
	void choose_mthd() override {
		int classes[20] = {
			0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
			0x08, 0x09, 0x0a, 0x0b, 0x0c,
			0x0d, 0x0e, 0x1d, 0x1e,
			0x10, 0x11, 0x12, 0x13, 0x14,
		};
		cls = classes[rnd() % 20];
		mthd = 0;
	}
	void emulate_mthd() override {
		bool chsw = false;
		int och = extr(exp.ctx_switch[0], 16, 7);
		int nch = extr(val, 16, 7);
		if ((val & 0x007f8000) != (exp.ctx_switch[0] & 0x007f8000))
			chsw = true;
		if (!extr(exp.ctx_control, 16, 1))
			chsw = true;
		bool volatile_reset = extr(val, 31, 1) && extr(exp.debug[2], 28, 1) && (!extr(exp.ctx_control, 16, 1) || och == nch);
		if (chsw) {
			exp.ctx_control |= 0x01010000;
			exp.intr |= 0x10;
			exp.access &= ~0x101;
		} else {
			exp.ctx_control &= ~0x01000000;
		}
		insrt(exp.access, 12, 5, cls);
		insrt(exp.debug[1], 0, 1, volatile_reset);
		if (volatile_reset) {
			pgraph_volatile_reset(&exp);
		}
		exp.ctx_switch[0] = val & 0x807fffff;
		if (exp.notify & 0x100000) {
			exp.intr |= 0x10000001;
			exp.invalid |= 0x10000;
			exp.access &= ~0x101;
			exp.notify &= ~0x100000;
		}
	}
public:
	MthdCtxSwitchTest(hwtest::TestOptions &opt, uint32_t seed) : MthdTest(opt, seed) {}
};

class MthdNotifyTest : public MthdTest {
	bool special_notify() override {
		return true;
	}
	void choose_mthd() override {
		if (rnd() & 1) {
			val &= 0xf;
		}
		int classes[20] = {
			0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
			0x08, 0x09, 0x0a, 0x0b, 0x0c,
			0x0d, 0x0e, 0x1d, 0x1e,
			0x10, 0x11, 0x12, 0x13, 0x14,
		};
		cls = classes[rnd() % 20];
		mthd = 0x104;
	}
	void emulate_mthd() override {
		if (val && (cls & 0xf) != 0xd && (cls & 0xf) != 0xe)
			exp.invalid |= 0x10;
		if (exp.notify & 0x100000 && !exp.invalid)
			exp.intr |= 0x10000000;
		if (!(exp.ctx_switch[0] & 0x100))
			exp.invalid |= 0x100;
		if (exp.notify & 0x110000)
			exp.invalid |= 0x1000;
		if (exp.invalid) {
			exp.intr |= 1;
			exp.access &= ~0x101;
		} else {
			exp.notify |= 0x10000;
		}
	}
public:
	MthdNotifyTest(hwtest::TestOptions &opt, uint32_t seed) : MthdTest(opt, seed) {}
};

class MthdInvalidTest : public MthdTest {
	int repeats() override {
		return 1;
	}
	void choose_mthd() override {
		// set by constructor
	}
	void emulate_mthd() override {
		exp.intr |= 1;
		exp.invalid |= 1;
		exp.access &= ~0x101;
	}
public:
	MthdInvalidTest(hwtest::TestOptions &opt, uint32_t seed, uint32_t cls, uint32_t mthd) : MthdTest(opt, seed) {
		this->cls = cls;
		this->mthd = mthd;
	}
};

class ClsMthdInvalidTests : public hwtest::Test {
protected:
	virtual int cls() = 0;
	virtual bool is_valid(uint32_t mthd) = 0;
	bool subtests_boring() override {
		return true;
	}
	Subtests subtests() override {
		Subtests res;
		for (uint32_t mthd = 0; mthd < 0x10000; mthd += 4) {
			if (mthd != 0 && !is_valid(mthd)) {
				char buf[5];
				snprintf(buf, sizeof buf, "%04x", mthd);
				res.push_back({strdup(buf), new MthdInvalidTest(opt, rnd(), cls(), mthd)});
			}
		}
		return res;
	}
	ClsMthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : Test(opt, seed) {}
};

class BetaMthdInvalidTests : public ClsMthdInvalidTests {
	int cls() override { return 0x01; }
	bool is_valid(uint32_t mthd) override {
		return mthd == 0x104 || mthd == 0x300;
	}
public:
	BetaMthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : ClsMthdInvalidTests(opt, seed) {}
};

class RopMthdInvalidTests : public ClsMthdInvalidTests {
	int cls() override { return 0x02; }
	bool is_valid(uint32_t mthd) override {
		return mthd == 0x104 || mthd == 0x300;
	}
public:
	RopMthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : ClsMthdInvalidTests(opt, seed) {}
};

class ChromaMthdInvalidTests : public ClsMthdInvalidTests {
	int cls() override { return 0x03; }
	bool is_valid(uint32_t mthd) override {
		return mthd == 0x104 || mthd == 0x304;
	}
public:
	ChromaMthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : ClsMthdInvalidTests(opt, seed) {}
};

class PlaneMthdInvalidTests : public ClsMthdInvalidTests {
	int cls() override { return 0x04; }
	bool is_valid(uint32_t mthd) override {
		return mthd == 0x104 || mthd == 0x304;
	}
public:
	PlaneMthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : ClsMthdInvalidTests(opt, seed) {}
};

class ClipMthdInvalidTests : public ClsMthdInvalidTests {
	int cls() override { return 0x05; }
	bool is_valid(uint32_t mthd) override {
		return mthd == 0x104 || mthd == 0x300 || mthd == 0x304;
	}
public:
	ClipMthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : ClsMthdInvalidTests(opt, seed) {}
};

class PatternMthdInvalidTests : public ClsMthdInvalidTests {
	int cls() override { return 0x06; }
	bool is_valid(uint32_t mthd) override {
		if (mthd == 0x104)
			return true;
		if (mthd == 0x308)
			return true;
		if (mthd >= 0x310 && mthd <= 0x31c)
			return true;
		return false;
	}
public:
	PatternMthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : ClsMthdInvalidTests(opt, seed) {}
};

class PointMthdInvalidTests : public ClsMthdInvalidTests {
	int cls() override { return 0x08; }
	bool is_valid(uint32_t mthd) override {
		if (mthd == 0x104)
			return true;
		if (mthd == 0x304)
			return true;
		if (mthd >= 0x400 && mthd < 0x580)
			return true;
		return false;
	}
public:
	PointMthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : ClsMthdInvalidTests(opt, seed) {}
};

class LineMthdInvalidTests : public ClsMthdInvalidTests {
	int cls() override { return 0x09; }
	bool is_valid(uint32_t mthd) override {
		if (mthd == 0x104)
			return true;
		if (mthd == 0x304)
			return true;
		if (mthd >= 0x400 && mthd < 0x680)
			return true;
		return false;
	}
public:
	LineMthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : ClsMthdInvalidTests(opt, seed) {}
};

class LinMthdInvalidTests : public ClsMthdInvalidTests {
	int cls() override { return 0x0a; }
	bool is_valid(uint32_t mthd) override {
		if (mthd == 0x104)
			return true;
		if (mthd == 0x304)
			return true;
		if (mthd >= 0x400 && mthd < 0x680)
			return true;
		return false;
	}
public:
	LinMthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : ClsMthdInvalidTests(opt, seed) {}
};

class TriMthdInvalidTests : public ClsMthdInvalidTests {
	int cls() override { return 0x0b; }
	bool is_valid(uint32_t mthd) override {
		if (mthd == 0x104)
			return true;
		if (mthd == 0x304)
			return true;
		if (mthd >= 0x310 && mthd < 0x31c)
			return true;
		if (mthd >= 0x320 && mthd < 0x338)
			return true;
		if (mthd >= 0x400 && mthd < 0x600)
			return true;
		return false;
	}
public:
	TriMthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : ClsMthdInvalidTests(opt, seed) {}
};

class RectMthdInvalidTests : public ClsMthdInvalidTests {
	int cls() override { return 0x0c; }
	bool is_valid(uint32_t mthd) override {
		if (mthd == 0x104)
			return true;
		if (mthd == 0x304)
			return true;
		if (mthd >= 0x400 && mthd < 0x480)
			return true;
		return false;
	}
public:
	RectMthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : ClsMthdInvalidTests(opt, seed) {}
};

class TexLinMthdInvalidTests : public ClsMthdInvalidTests {
	int cls() override { return 0x0d; }
	bool is_valid(uint32_t mthd) override {
		if (mthd == 0x104)
			return true;
		if (mthd == 0x304)
			return true;
		if (mthd >= 0x310 && mthd < 0x320)
			return true;
		if (mthd >= 0x350 && mthd < 0x360)
			return true;
		if (mthd >= 0x400 && mthd < 0x480)
			return true;
		return false;
	}
public:
	TexLinMthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : ClsMthdInvalidTests(opt, seed) {}
};

class TexQuadMthdInvalidTests : public ClsMthdInvalidTests {
	int cls() override { return 0x0e; }
	bool is_valid(uint32_t mthd) override {
		if (mthd == 0x104)
			return true;
		if (mthd == 0x304)
			return true;
		if (mthd >= 0x310 && mthd < 0x334)
			return true;
		if (mthd >= 0x350 && mthd < 0x374)
			return true;
		if (mthd >= 0x400 && mthd < 0x480)
			return true;
		return false;
	}
public:
	TexQuadMthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : ClsMthdInvalidTests(opt, seed) {}
};

class TexLinBetaMthdInvalidTests : public ClsMthdInvalidTests {
	int cls() override { return 0x1d; }
	bool is_valid(uint32_t mthd) override {
		if (mthd == 0x104)
			return true;
		if (mthd == 0x304)
			return true;
		if (mthd >= 0x310 && mthd < 0x320)
			return true;
		if (mthd >= 0x350 && mthd < 0x360)
			return true;
		if (mthd >= 0x380 && mthd < 0x388)
			return true;
		if (mthd >= 0x400 && mthd < 0x480)
			return true;
		return false;
	}
public:
	TexLinBetaMthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : ClsMthdInvalidTests(opt, seed) {}
};

class TexQuadBetaMthdInvalidTests : public ClsMthdInvalidTests {
	int cls() override { return 0x1e; }
	bool is_valid(uint32_t mthd) override {
		if (mthd == 0x104)
			return true;
		if (mthd == 0x304)
			return true;
		if (mthd >= 0x310 && mthd < 0x334)
			return true;
		if (mthd >= 0x350 && mthd < 0x374)
			return true;
		if (mthd >= 0x380 && mthd < 0x394)
			return true;
		if (mthd >= 0x400 && mthd < 0x480)
			return true;
		return false;
	}
public:
	TexQuadBetaMthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : ClsMthdInvalidTests(opt, seed) {}
};

class BlitMthdInvalidTests : public ClsMthdInvalidTests {
	int cls() override { return 0x10; }
	bool is_valid(uint32_t mthd) override {
		if (mthd == 0x104)
			return true;
		if (mthd == 0x300 || mthd == 0x304 || mthd == 0x308)
			return true;
		return false;
	}
public:
	BlitMthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : ClsMthdInvalidTests(opt, seed) {}
};

class IfcMthdInvalidTests : public ClsMthdInvalidTests {
	int cls() override { return 0x11; }
	bool is_valid(uint32_t mthd) override {
		if (mthd == 0x104)
			return true;
		if (mthd == 0x304 || mthd == 0x308 || mthd == 0x30c)
			return true;
		if (mthd >= 0x400 && mthd < 0x480)
			return true;
		return false;
	}
public:
	IfcMthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : ClsMthdInvalidTests(opt, seed) {}
};

class BitmapMthdInvalidTests : public ClsMthdInvalidTests {
	int cls() override { return 0x12; }
	bool is_valid(uint32_t mthd) override {
		if (mthd == 0x104)
			return true;
		if (mthd >= 0x308 && mthd < 0x31c)
			return true;
		if (mthd >= 0x400 && mthd < 0x480)
			return true;
		return false;
	}
public:
	BitmapMthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : ClsMthdInvalidTests(opt, seed) {}
};

class IfmMthdInvalidTests : public ClsMthdInvalidTests {
	int cls() override { return 0x13; }
	bool is_valid(uint32_t mthd) override {
		if (mthd == 0x104)
			return true;
		if (mthd >= 0x308 && mthd < 0x318)
			return true;
		if (mthd >= 0x40 && mthd < 0x80)
			return true;
		return false;
	}
public:
	IfmMthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : ClsMthdInvalidTests(opt, seed) {}
};

class ItmMthdInvalidTests : public ClsMthdInvalidTests {
	int cls() override { return 0x14; }
	bool is_valid(uint32_t mthd) override {
		if (mthd == 0x104)
			return true;
		if (mthd >= 0x308 && mthd < 0x318)
			return true;
		return false;
	}
public:
	ItmMthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : ClsMthdInvalidTests(opt, seed) {}
};

class MthdInvalidTests : public hwtest::Test {
	Subtests subtests() override {
		return {
			{"beta", new BetaMthdInvalidTests(opt, rnd())},
			{"rop", new RopMthdInvalidTests(opt, rnd())},
			{"chroma", new ChromaMthdInvalidTests(opt, rnd())},
			{"plane", new PlaneMthdInvalidTests(opt, rnd())},
			{"clip", new ClipMthdInvalidTests(opt, rnd())},
			{"pattern", new PatternMthdInvalidTests(opt, rnd())},
			{"point", new PointMthdInvalidTests(opt, rnd())},
			{"line", new LineMthdInvalidTests(opt, rnd())},
			{"lin", new LinMthdInvalidTests(opt, rnd())},
			{"tri", new TriMthdInvalidTests(opt, rnd())},
			{"rect", new RectMthdInvalidTests(opt, rnd())},
			{"texlin", new TexLinMthdInvalidTests(opt, rnd())},
			{"texquad", new TexQuadMthdInvalidTests(opt, rnd())},
			{"texlinbeta", new TexLinBetaMthdInvalidTests(opt, rnd())},
			{"texquadbeta", new TexQuadBetaMthdInvalidTests(opt, rnd())},
			{"blit", new BlitMthdInvalidTests(opt, rnd())},
			{"ifc", new IfcMthdInvalidTests(opt, rnd())},
			{"bitmap", new BitmapMthdInvalidTests(opt, rnd())},
			{"ifm", new IfmMthdInvalidTests(opt, rnd())},
			{"itm", new ItmMthdInvalidTests(opt, rnd())},
		};
	}
public:
	MthdInvalidTests(hwtest::TestOptions &opt, uint32_t seed) : Test(opt, seed) {}
};

class RopSolidTest : public MthdTest {
	int x, y;
	uint32_t addr[2], pixel[2], epixel[2], rpixel[2];
	int repeats() override { return 100000; };
	void adjust_orig_mthd() override {
		x = rnd() & 0x3ff;
		y = rnd() & 0xff;
		orig.dst_canvas_min = 0;
		orig.dst_canvas_max = 0x01000400;
		/* XXX bits 8-9 affect rendering */
		/* XXX bits 12-19 affect xy_misc_4 clip status */
		orig.xy_misc_1[0] &= 0xfff00cff;
		/* avoid invalid ops */
		orig.ctx_switch[0] &= ~0x001f;
		if (rnd()&1) {
			int ops[] = {
				0x00, 0x0f,
				0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
				0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
				0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x17,
			};
			orig.ctx_switch[0] |= ops[rnd() % ARRAY_SIZE(ops)];
		} else {
			/* BLEND needs more testing */
			int ops[] = { 0x18, 0x19, 0x1a, 0x1b, 0x1c };
			orig.ctx_switch[0] |= ops[rnd() % ARRAY_SIZE(ops)];
			/* XXX Y8 blend? */
			orig.pfb_config |= 0x200;
		}
		orig.pattern_config = rnd()%3; /* shape 3 is a rather ugly hole in Karnough map */
		if (rnd()&1) {
			orig.xy_misc_4[0] &= ~0xf0;
			orig.xy_misc_4[1] &= ~0xf0;
		}
		/* XXX causes interrupts */
		orig.valid[0] &= ~0x11000000;
		insrt(orig.access, 12, 5, 8);
		insrt(orig.pfb_config, 4, 3, 3);
		if (rnd()&1)
			insrt(orig.cliprect_min[rnd()&1], 0, 16, x);
		if (rnd()&1)
			insrt(orig.cliprect_min[rnd()&1], 16, 16, y);
		if (rnd()&1)
			insrt(orig.cliprect_max[rnd()&1], 0, 16, x);
		if (rnd()&1)
			insrt(orig.cliprect_max[rnd()&1], 16, 16, y);
		if (rnd()&1) {
			/* it's vanishingly rare for the chroma key to match perfectly by random, so boost the odds */
			uint32_t ckey = pgraph_to_a1r10g10b10(pgraph_expand_color(&orig, orig.misc32[0]));
			ckey ^= (rnd() & 1) << 30; /* perturb alpha */
			if (rnd()&1) {
				/* perturb it a bit to check which bits have to match */
				ckey ^= 1 << (rnd() % 30);
			}
			orig.chroma = ckey;
		}
		for (unsigned i = 0; i < 1 + extr(orig.pfb_config, 12, 1); i++) {
			addr[i] = nv01_pgraph_pixel_addr(&orig, x, y, i);
			nva_wr32(cnum, 0x1000000+(addr[i]&~3), rnd());
			pixel[i] = nva_rd32(cnum, 0x1000000+(addr[i]&~3)) >> (addr[i] & 3) * 8;
			pixel[i] &= bflmask(nv01_pgraph_cpp(orig.pfb_config)*8);
		}
		val = y << 16 | x;
	}
	void choose_mthd() override {
		cls = 0x08;
		mthd = 0x400;
	}
	void emulate_mthd() override {
		int bfmt = extr(orig.ctx_switch[0], 9, 4);
		int bufmask = (bfmt / 5 + 1) & 3;
		if (!extr(orig.pfb_config, 12, 1))
			bufmask = 1;
		bool cliprect_pass = pgraph_cliprect_pass(&exp, x, y);
		for (unsigned i = 0; i < 1 + extr(orig.pfb_config, 12, 1); i++) {
			if (bufmask & (1 << i) && (cliprect_pass || (i == 1 && extr(exp.canvas_config, 4, 1))))
				epixel[i] = nv01_pgraph_solid_rop(&exp, x, y, pixel[i]);
			else
				epixel[i] = pixel[i];
		}
		exp.vtx_x[0] = x;
		exp.vtx_y[0] = y;
		pgraph_clear_vtxid(&exp);
		pgraph_bump_vtxid(&exp);
		exp.xy_misc_1[0] &= ~0x03000001;
		exp.xy_misc_1[0] |= 0x01000000;
		nv01_pgraph_set_xym2(&exp, 0, 0, 0, 0, 0, x == 0x400 ? 8 : x ? 0 : 2);
		nv01_pgraph_set_xym2(&exp, 1, 0, 0, 0, 0, y == 0x400 ? 8 : y ? 0 : 2);
		exp.valid[0] &= ~0xffffff;
		if (extr(exp.cliprect_ctrl, 8, 1)) {
			exp.intr |= 1 << 24;
			exp.access &= ~0x101;
		}
		if (extr(exp.canvas_config, 24, 1)) {
			exp.intr |= 1 << 20;
			exp.access &= ~0x101;
		}
		if (extr(exp.xy_misc_4[0], 4, 4) || extr(exp.xy_misc_4[1], 4, 4)) {
			exp.intr |= 1 << 12;
			exp.access &= ~0x101;
		}
		if (exp.intr & 0x01101000) {
			for (int i = 0; i < 2; i++) {
				epixel[i] = pixel[i];
			}
		}
	}
	bool other_fail() override {
		bool res = false;
		for (unsigned i = 0; i < 1 + extr(orig.pfb_config, 12, 1); i++) {
			rpixel[i] = nva_rd32(cnum, 0x1000000+(addr[i]&~3)) >> (addr[i] & 3) * 8;
			rpixel[i] &= bflmask(nv01_pgraph_cpp(exp.pfb_config)*8);
			if (rpixel[i] != epixel[i]) {
			
				printf("Difference in PIXEL[%d]: expected %08x real %08x\n", i, epixel[i], rpixel[i]);
				res = true;
			}
		}
		return res;
	}
	void print_fail() override {
		for (unsigned i = 0; i < 1 + extr(orig.pfb_config, 12, 1); i++) {
			printf("%08x %08x %08x PIXEL[%d]\n", pixel[i], epixel[i], rpixel[i], i);
		}
		MthdTest::print_fail();
	}
public:
	RopSolidTest(hwtest::TestOptions &opt, uint32_t seed) : MthdTest(opt, seed) {}
};

class RopBlitTest : public MthdTest {
	int x, y, sx, sy;
	uint32_t addr[2], saddr[2], pixel[2], epixel[2], rpixel[2], spixel[2];
	int repeats() override { return 100000; };
	void adjust_orig_mthd() override {
		x = rnd() & 0x1ff;
		y = rnd() & 0xff;
		sx = (rnd() & 0x3ff) + 0x200;
		sy = rnd() & 0xff;
		orig.dst_canvas_min = 0;
		orig.dst_canvas_max = 0x01000400;
		/* XXX bits 8-9 affect rendering */
		/* XXX bits 12-19 affect xy_misc_4 clip status */
		orig.xy_misc_1[0] &= 0xfff00cff;
		/* avoid invalid ops */
		orig.ctx_switch[0] &= ~0x001f;
		if (rnd()&1) {
			int ops[] = {
				0x00, 0x0f,
				0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
				0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
				0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x17,
			};
			orig.ctx_switch[0] |= ops[rnd() % ARRAY_SIZE(ops)];
		} else {
			/* BLEND needs more testing */
			int ops[] = { 0x18, 0x19, 0x1a, 0x1b, 0x1c };
			orig.ctx_switch[0] |= ops[rnd() % ARRAY_SIZE(ops)];
			/* XXX Y8 blend? */
			orig.pfb_config |= 0x200;
		}
		orig.pattern_config = rnd()%3; /* shape 3 is a rather ugly hole in Karnough map */
		if (rnd()&1) {
			orig.xy_misc_4[0] &= ~0xf0;
			orig.xy_misc_4[1] &= ~0xf0;
		}
		orig.xy_misc_4[0] &= ~0xf000;
		orig.xy_misc_4[1] &= ~0xf000;
		orig.valid[0] &= ~0x11000000;
		orig.valid[0] |= 0xf10f;
		insrt(orig.access, 12, 5, 0x10);
		insrt(orig.pfb_config, 4, 3, 3);
		orig.vtx_x[0] = sx;
		orig.vtx_y[0] = sy;
		orig.vtx_x[1] = x;
		orig.vtx_y[1] = y;
		if (rnd()&1)
			insrt(orig.cliprect_min[rnd()&1], 0, 16, x);
		if (rnd()&1)
			insrt(orig.cliprect_min[rnd()&1], 16, 16, y);
		if (rnd()&1)
			insrt(orig.cliprect_max[rnd()&1], 0, 16, x);
		if (rnd()&1)
			insrt(orig.cliprect_max[rnd()&1], 16, 16, y);
		if (rnd()&1) {
			/* it's vanishingly rare for the chroma key to match perfectly by random, so boost the odds */
			uint32_t ckey = pgraph_to_a1r10g10b10(pgraph_expand_color(&orig, orig.misc32[0]));
			ckey ^= (rnd() & 1) << 30; /* perturb alpha */
			if (rnd()&1) {
				/* perturb it a bit to check which bits have to match */
				ckey ^= 1 << (rnd() % 30);
			}
			orig.chroma = ckey;
		}
		for (unsigned i = 0; i < 1 + extr(orig.pfb_config, 12, 1); i++) {
			addr[i] = nv01_pgraph_pixel_addr(&orig, x, y, i);
			saddr[i] = nv01_pgraph_pixel_addr(&orig, sx, sy, i);
			nva_wr32(cnum, 0x1000000+(addr[i]&~3), rnd());
			nva_wr32(cnum, 0x1000000+(saddr[i]&~3), rnd());
			pixel[i] = nva_rd32(cnum, 0x1000000+(addr[i]&~3)) >> (addr[i] & 3) * 8;
			spixel[i] = nva_rd32(cnum, 0x1000000+(saddr[i]&~3)) >> (saddr[i] & 3) * 8;
			pixel[i] &= bflmask(nv01_pgraph_cpp(orig.pfb_config)*8);
			spixel[i] &= bflmask(nv01_pgraph_cpp(orig.pfb_config)*8);
			if (sx >= 0x400)
				spixel[i] = 0;
			if (!pgraph_cliprect_pass(&orig, sx, sy) && (i == 0 || !extr(orig.canvas_config, 4, 1))) {
				spixel[i] = 0;
			}
		}
		if (!extr(orig.pfb_config, 12, 1))
			spixel[1] = spixel[0];
		val = 1 << 16 | 1;
	}
	void choose_mthd() override {
		cls = 0x10;
		mthd = 0x308;
	}
	void emulate_mthd() override {
		int bfmt = extr(exp.ctx_switch[0], 9, 4);
		int bufmask = (bfmt / 5 + 1) & 3;
		if (!extr(exp.pfb_config, 12, 1))
			bufmask = 1;
		bool cliprect_pass = pgraph_cliprect_pass(&exp, x, y);
		struct pgraph_color s = nv01_pgraph_expand_surf(&exp, spixel[extr(exp.ctx_switch[0], 13, 1)]);
		for (unsigned i = 0; i < 1 + extr(orig.pfb_config, 12, 1); i++) {
			if (bufmask & (1 << i) && (cliprect_pass || (i == 1 && extr(exp.canvas_config, 4, 1))))
				epixel[i] = nv01_pgraph_rop(&exp, x, y, pixel[i], s);
			else
				epixel[i] = pixel[i];
		}
		nv01_pgraph_vtx_fixup(&exp, 0, 2, 1, 1, 0, 2);
		nv01_pgraph_vtx_fixup(&exp, 1, 2, 1, 1, 0, 2);
		nv01_pgraph_vtx_fixup(&exp, 0, 3, 1, 1, 1, 3);
		nv01_pgraph_vtx_fixup(&exp, 1, 3, 1, 1, 1, 3);
		pgraph_bump_vtxid(&exp);
		pgraph_bump_vtxid(&exp);
		exp.valid[0] &= ~0xffffff;
		if (extr(exp.cliprect_ctrl, 8, 1)) {
			exp.intr |= 1 << 24;
			exp.access &= ~0x101;
		}
		if (extr(exp.canvas_config, 24, 1)) {
			exp.intr |= 1 << 20;
			exp.access &= ~0x101;
		}
		if (extr(exp.xy_misc_4[0], 4, 4) || extr(exp.xy_misc_4[1], 4, 4)) {
			exp.intr |= 1 << 12;
			exp.access &= ~0x101;
		}
		if (exp.intr & 0x01101000) {
			for (int i = 0; i < 2; i++) {
				epixel[i] = pixel[i];
			}
		}
	}
	bool other_fail() override {
		bool res = false;
		for (unsigned i = 0; i < 1 + extr(orig.pfb_config, 12, 1); i++) {
			rpixel[i] = nva_rd32(cnum, 0x1000000+(addr[i]&~3)) >> (addr[i] & 3) * 8;
			rpixel[i] &= bflmask(nv01_pgraph_cpp(exp.pfb_config)*8);
			if (rpixel[i] != epixel[i]) {
			
				printf("Difference in PIXEL[%d]: expected %08x real %08x\n", i, epixel[i], rpixel[i]);
				res = true;
			}
		}
		return res;
	}
	void print_fail() override {
		for (unsigned i = 0; i < 1 + extr(orig.pfb_config, 12, 1); i++) {
			printf("%08x %08x %08x PIXEL[%d]\n", pixel[i], epixel[i], rpixel[i], i);
		}
		for (unsigned i = 0; i < 1 + extr(orig.pfb_config, 12, 1); i++) {
			printf("%08x SPIXEL[%d]\n", spixel[i], i);
		}
		MthdTest::print_fail();
	}
public:
	RopBlitTest(hwtest::TestOptions &opt, uint32_t seed) : MthdTest(opt, seed) {}
};

}

namespace hwtest {
namespace pgraph {

bool PGraphMthdMiscTests::supported() {
	return chipset.card_type < 3;
}

Test::Subtests PGraphMthdMiscTests::subtests() {
	return {
		{"ctx_switch", new MthdCtxSwitchTest(opt, rnd())},
		{"notify", new MthdNotifyTest(opt, rnd())},
		{"invalid", new MthdInvalidTests(opt, rnd())},
	};
}


bool PGraphRopTests::supported() {
	return chipset.card_type < 3;
}

Test::Subtests PGraphRopTests::subtests() {
	return {
		{"solid", new RopSolidTest(opt, rnd())},
		{"blit", new RopBlitTest(opt, rnd())},
	};
}

}
}
