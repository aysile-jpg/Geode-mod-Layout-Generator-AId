#include "Generator.hpp"
#include <random>
#include <algorithm>
#include <cmath>

namespace {
	// Высота центра блока, стоящего на земле. Если в редакторе объекты
	// появятся не на земле, поменяй на 15.f (или подбери значение).
	constexpr float GROUND = 105.f;
	constexpr float B = 30.f;
	constexpr int ROWS = 10;

	const float UPS[] = {251.16f, 311.58f, 387.42f, 468.0f, 576.0f};
	const int SPEED_PORTAL[] = {200, 201, 202, 203, 1334};

	int portalFor(Mode m) {
		switch (m) {
			case Mode::Cube: return 12;
			case Mode::Ship: return 13;
			case Mode::Ball: return 47;
			case Mode::UFO: return 111;
			case Mode::Wave: return 660;
			case Mode::Robot: return 745;
			case Mode::Spider: return 1331;
			case Mode::Swing: return 1933;
		}
		return 12;
	}

	int gapFor(Mode m) {
		switch (m) {
			case Mode::Ship: return 7;
			case Mode::UFO: return 7;
			case Mode::Wave: return 5;
			case Mode::Swing: return 6;
			case Mode::Ball: return 3;
			default: return 0;
		}
	}
}

std::vector<Segment> randomPlan(LayoutParams const& p) {
	std::vector<Segment> plan;
	if (p.modes.empty() || p.speeds.empty()) return plan;
	std::mt19937 rng(p.seed ^ (uint32_t)(p.songID * 2654435761u));
	auto rnd = [&](int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng); };
	float bpm = std::clamp(p.bpm, 40.f, 300.f);
	float t = 0.f;
	while (t < (float)p.lengthSec) {
		Segment s;
		s.mode = p.modes[rnd(0, (int)p.modes.size() - 1)];
		s.speed = p.speeds[rnd(0, (int)p.speeds.size() - 1)];
		s.beats = rnd(8, 16);
		s.density = 60;
		plan.push_back(s);
		t += s.beats * 60.f / bpm;
	}
	return plan;
}

std::vector<ObjSpec> buildLayout(LayoutParams const& p, std::vector<Segment> const& plan) {
	std::vector<ObjSpec> out;
	if (plan.empty()) return out;

	std::mt19937 rng(p.seed ^ 0x9e3779b9u);
	auto rnd = [&](int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng); };
	auto add = [&](int id, float x, float y, float rot = 0.f) { out.push_back({id, x, y, rot}); };

	float bpm = std::clamp(p.bpm, 40.f, 300.f);
	float t = 0.f;
	float x = 150.f;
	bool first = true;
	Mode prev = Mode::Cube;
	int curSpeed = -1;
	int b = 0;

	for (auto const& seg : plan) {
		if (t >= (float)p.lengthSec) break;

		Mode m = seg.mode;
		int s = std::clamp(seg.speed, 0, 4);
		int density = std::clamp(seg.density, 10, 90);
		float ups = UPS[s];
		float beatU = ups * 60.f / bpm;
		int beats = std::clamp(seg.beats, 4, 32);
		int cols = std::max(4, (int)std::lround(beats * beatU / B));
		int gap = gapFor(m);
		bool corridor = gap > 0;

		float portalY = corridor ? GROUND + (gap - 1) * B * 0.5f : GROUND + B;
		if (first || s != curSpeed) add(SPEED_PORTAL[s], x, portalY);
		if (first || m != prev) add(portalFor(m), x + 60.f, portalY);

		x += 180.f;
		t += 180.f / ups;

		float minGap = ups * 0.5f;
		int step = std::max(1, (int)std::ceil(minGap / beatU));
		float everyU = step * beatU;
		float nextObs = x + everyU;
		int target = 0;
		bool floorSpike = true;

		for (int c = 0; c < cols; c++) {
			float cx = x + c * B;
			bool onBeat = cx >= nextObs;

			if (corridor) {
				if (onBeat) {
					nextObs += everyU;
					if (m == Mode::Ball) {
						if (rnd(0, 99) < density) {
							float sy = floorSpike ? GROUND : GROUND + (gap - 1) * B;
							add(8, cx, sy, floorSpike ? 0.f : 180.f);
							floorSpike = !floorSpike;
						}
					} else if (rnd(0, 99) < density) {
						target = rnd(0, ROWS - gap);
					}
				}
				if (m != Mode::Ball) {
					if (b < target) b++;
					else if (b > target) b--;
				}
				if (b > 0) add(1, cx, GROUND + (b - 1) * B);
				add(1, cx, GROUND + (b + gap) * B);
			} else if (onBeat) {
				nextObs += everyU;
				if (rnd(0, 99) < density) {
					switch (rnd(0, 3)) {
						case 0: add(8, cx, GROUND); break;
						case 1: add(8, cx, GROUND); add(8, cx + B, GROUND); break;
						case 2: add(8, cx, GROUND); add(8, cx + B, GROUND); add(8, cx + 2 * B, GROUND); break;
						default:
							add(1, cx, GROUND);
							add(1, cx + B, GROUND);
							add(8, cx + 3 * B, GROUND);
							break;
					}
				}
			}
		}
		x += cols * B;
		t += cols * B / ups;

		while (corridor && b > 0) {
			b--;
			if (b > 0) add(1, x, GROUND + (b - 1) * B);
			add(1, x, GROUND + (b + gap) * B);
			x += B;
			t += B / ups;
		}

		prev = m;
		curSpeed = s;
		first = false;
	}
	return out;
}
