#pragma once
#include <vector>
#include <cstdint>

enum class Mode { Cube, Ship, Ball, UFO, Wave, Robot, Spider, Swing };

struct ObjSpec {
	int id;
	float x;
	float y;
	float rot = 0.f;
};

// Один участок уровня. Его можно задать случайно или получить от нейросети.
struct Segment {
	Mode mode = Mode::Cube;
	int speed = 1;      // 0=0.5x 1=1x 2=2x 3=3x 4=4x
	int beats = 12;     // длина участка в битах
	int density = 60;   // 10..90, сколько препятствий
};

struct LayoutParams {
	int songID = 0;
	float bpm = 120.f;
	int lengthSec = 60;
	std::vector<Mode> modes;   // разрешённые режимы
	std::vector<int> speeds;   // разрешённые скорости
	uint32_t seed = 0;
};

std::vector<Segment> randomPlan(LayoutParams const& p);
std::vector<ObjSpec> buildLayout(LayoutParams const& p, std::vector<Segment> const& plan);
