#include "tank.h"
#include <math.h>
#include <algorithm>
#include <map>
#include <vector>
#include <conio.h>
#include <functional>

using std::min;
using std::max;
using std::string;
using std::map;
using std::vector;

struct XShell : Shell {
	double penAt350m;
};

struct XGun : Gun {
	XGun(const Gun& gun);
	XShell ap;
	XShell he;
	double apStart, apEnd, heEnd;
	double damage6, damage12, damage60;
	double timeAt100d;
};

struct Point {
	double t, ap, he;
	bool operator <(const Point& x) const { return t < x.t; }
};

Point Cross(Point a, Point b) {
	double t = (a.he - a.ap) / ((b.ap - a.ap) - (b.he - a.he)) + a.t;
	Point x = { t, (b.ap - a.ap) * (t - a.t) + a.ap, (b.he - a.he) * (t - a.t) + a.he };
	return x;
}

struct FunAP {
	struct Point { double t, dmg; } points[3];
	FunAP(const Shell& shell) {
		double pen = (shell.kind == Shell::HEAT ? 0.97 : 1.0) * shell.penAt350m * 0.7;
		if (shell.kind == Shell::HE) exit(1);
		points[0].t = 10;
		points[0].dmg = shell.damage;
		points[1].t = pen * 0.75;
		points[1].dmg = shell.damage;
		points[2].t = pen * 1.25;
		points[2].dmg = 0;
	}
	double operator ()(double t) const {
		for (int i = 0; i < 2; i++)
			if (points[i].t <= t && t <= points[i + 1].t)
				return (points[i + 1].dmg - points[i].dmg) / (points[i + 1].t - points[i].t) * (t - points[i].t) + points[i].dmg;
		return 0;
	}
	double area() const {
		double x = 0;
		for (int i = 0; i < 2; i++)
			x += (points[i + 1].dmg + points[i].dmg) * (points[i + 1].t - points[i].t);
		return x * 0.5;
	}
};

struct FunHE {
	struct Point { double t, dmg; } points[5];
	FunHE(const Shell& shell) {
		double pen = 0.7 * shell.penAt350m;
		if (shell.kind != Shell::HE) exit(1);
		points[0].t = 10;
		points[0].dmg = shell.damage;
		points[1].t = pen * 0.75;
		points[1].dmg = shell.damage;
		points[2].t = pen * 1.25;
		points[2].dmg = shell.damage * 0.5 - 1.1 * pen * 1.25;
		points[3].t = shell.damage * 0.5 / 1.1 * 0.75;
		points[3].dmg = shell.damage * 0.5 * 0.25;
		points[4].t = shell.damage * 0.5 / 1.1 * 1.25;
		points[4].dmg = 0;
	}
	double operator ()(double t) const {
		for (int i = 0; i < 4; i++)
			if (points[i].t <= t && t <= points[i + 1].t)
				return (points[i + 1].dmg - points[i].dmg) / (points[i + 1].t - points[i].t) * (t - points[i].t) + points[i].dmg;
		return 0;
	}
	double area() const {
		double x = 0;
		for (int i = 0; i < 4; i++)
			x += (points[i + 1].dmg + points[i].dmg) * (points[i + 1].t - points[i].t);
		return x * 0.5;
	}
};

struct Fun2 {
	struct Point { double t, ap, he; } points[16];
	int size;
	Fun2(const FunAP& ap, const FunHE& he) {
		int a = 1, h = 1, i = 1;
		points[0].t = 10;
		points[0].ap = ap.points[0].dmg;
		points[0].he = he.points[0].dmg;
		while (a < 3 || h < 5) {
			Point next;
			if (h == 5 || a < 3 && ap.points[a].t < he.points[h].t) {
				next.t = ap.points[a].t;
				next.ap = ap.points[a].dmg;
				next.he = he(next.t);
				a++;
			} else {
				next.t = he.points[h].t;
				next.he = he.points[h].dmg;
				next.ap = ap(next.t);
				h++;
			}
			Point cross = Cross(points[i - 1], next);
			if (points[i - 1].t < cross.t && cross.t < next.t)
				points[i++] = cross;
			points[i++] = next;
			while (a < 3 && ap.points[a].t < points[i - 1].t) a++;
			while (h < 5 && he.points[h].t < points[i - 1].t) h++;
		}
		for (; a < 3; a++) {
			Point next = { ap.points[a].t, ap.points[a].dmg, he(ap.points[a].t) };
			points[i++] = next;
		}
		for (; h < 5; h++) {
			Point next = { he.points[h].t, ap(he.points[h].t), he.points[h].dmg };
			points[i++] = next;
		}
		size = i - 1;
	}
	Point Cross(Point a, Point b) {
		double t = (a.he - a.ap) / ((b.ap - a.ap) - (b.he - a.he)) + a.t;
		Point x = { t, (b.ap - a.ap) / (b.t - a.t) * (t - a.t) + a.ap, (b.he - a.he) / (b.t - a.t) * (t - a.t) + a.he };
		return x;
	}
	double ap(double t) {
		for (int i = 0; i < size; i++)
			if (points[i].t <= t && t <= points[i + 1].t)
				return (points[i + 1].ap - points[i].ap) / (points[i + 1].t - points[i].t) * (t - points[i].t) + points[i].ap;
	}
	double he(double t) {
		for (int i = 0; i < size; i++)
			if (points[i].t <= t && t <= points[i + 1].t)
				return (points[i + 1].he - points[i].he) / (points[i + 1].t - points[i].t) * (t - points[i].t) + points[i].he;
	}
	double area() const {
		double x = 0;
		for (int i = 0; i < size; i++)
			x += max(points[i + 1].ap + points[i].ap, points[i + 1].he + points[i].he) * (points[i + 1].t - points[i].t);
		return x * 0.5;
	}
};

void ProcessTanks3(const Tank* tanks, size_t size) {
	for (size_t i = 0; i < size; i++)
		if (tanks[i].gun.name.find("_57") == 0) {
			double ap_start = tanks[i].shells[0].penAt350m * 0.7 * 0.75;
			double ap_end = tanks[i].shells[0].penAt350m * 0.7 * 1.25;
			double cr_start = tanks[i].shells[1].penAt350m * 0.7 * 0.75;
			double cr_end = tanks[i].shells[1].penAt350m * 0.7 * 1.25;
			double he_start = tanks[i].shells[2].penAt350m * 0.7 * 0.75;
			double he_end = tanks[i].shells[2].penAt350m * 0.7 * 1.25;
			double he_low = tanks[i].shells[2].damage * 0.5 / 1.1 * .75;
			double he_hi = tanks[i].shells[2].damage * 0.5 / 1.1 * 1.25;
			
			//printf("%s : %g %g\n", tanks[i].shells[0].name.c_str(), ap_start, ap_end);
			////printf("%s : %g %g\n", tanks[i].shells[1].name.c_str(), cr_start, cr_end);
			//printf("%s : %g %g %g %g\n", tanks[i].shells[2].name.c_str(), he_start, he_end, he_low, he_hi);

			if (tanks[i].shells[0].kind == Shell::HE || tanks[i].shells[2].kind != Shell::HE)
				continue;
			FunAP ap(tanks[i].shells[0]);
			FunHE he(tanks[i].shells[2]);
			Fun2 fun2(ap, he);
			//for (int i = 0; i <= 2; i++)
			//	printf("   (%10g : %10g)\n", ap.points[i].t, ap.points[i].dmg);
			//printf("\n");
			//for (int i = 0; i <= 4; i++)
			//	printf("   (%10g : %10g)\n", he.points[i].t, he.points[i].dmg);
			//printf("\n");
			//for (int i = 0; i <= fun2.size; i++)
			//	printf("   (%10g : %10g, %10g : %10g, %10g)\n", fun2.points[i].t, ap(fun2.points[i].t), he(fun2.points[i].t), fun2.points[i].ap, fun2.points[i].he);
			printf("%20s : %20s : %g\n", tanks[i].hull.name.c_str(), tanks[i].gun.name.c_str(), fun2.area() / ap.area());
			//break;
		}
}

void printAverages(const Tank* tanks, size_t size) {
	double data[10][5] = {};
	int count[10][5] = {};
	for (size_t i = 0; i < size; i++) {
		const Tank& tank = tanks[i];
		int kind;
		if (tank.hull.tags.find("lightTank") == 0) kind = 0;
		else if (tank.hull.tags.find("mediumTank") == 0) kind = 1;
		else if (tank.hull.tags.find("heavyTank") == 0) kind = 2;
		else if (tank.hull.tags.find("AT-SPG") == 0) kind = 3;
		else if (tank.hull.tags.find("SPG") == 0) kind = 4;
		else exit(1);
		int tier = (int)tank.hull.tier;
		data[tier - 1][kind] += tank.hull.health + tank.turret.health;
		count[tier - 1][kind]++;
	}
	for (int tier = 0; tier < 10; tier++) {
		printf("{");
		for (int kind = 0; kind < 5; kind++)
			printf("%8g,", count[tier][kind] ? data[tier][kind] / count[tier][kind] : 0);
		printf("},\n");
	}
	for (int tier = 0; tier < 10; tier++) {
		printf("{");
		for (int kind = 0; kind < 5; kind++)
			printf("%2d, ", count[tier][kind]);
		printf("},\n");
	}
}

void ProcessTanks2(const Tank* tanks, size_t size) {
	printAverages(tanks, size);
	return;
	struct Test {
		const Tank* tank;
		const Shell* shell;
		double key;
		bool operator <(const Test& x) const { return key < x.key || key == x.key && shell->name < x.shell->name; }
		bool operator ==(const Test& x) const { return key == x.key && shell->name == x.shell->name; }
	};
	vector<Test> tests;
	for (size_t i = 0; i < size; i++)
		for (size_t j = 0; j < 3; j++) {
			const Shell& shell = tanks[i].shells[j];
			if (shell.kind)
				if (shell.kind != Shell::HE && shell.kind != Shell::HEAT) {
					Test x = { &tanks[i], &shell, (shell.penAt100m - shell.penAt350m) / shell.penAt100m };
					tests.push_back(x);
				}
		}
	if (tests.empty())
		return;
	std::stable_sort(tests.begin(), tests.end());
	vector<Test> filter;
	filter.push_back(tests[0]);
	for (auto i = tests.begin(), e = tests.end(); i != e; i++)
		if (!(*i == filter.back()))
			filter.push_back(*i);
	for (auto i = filter.begin(), e = filter.end(); i != e; i++)
		printf("%10g : %10g : %4s : %s, %s\n", i->shell->penAt100m - i->shell->penAt350m, (i->shell->penAt100m - i->shell->penAt350m) / i->shell->penAt100m, i->shell->kind == Shell::AP ? "AP" : "APCR", i->shell->name.c_str(), i->tank->hull.name.c_str());
}
