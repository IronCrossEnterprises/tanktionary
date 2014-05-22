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


double averageHealth[10][5] = {
{113.333,     140,       0,       0,       0},
{162.368, 183.333,       0,     122,      80},
{240.909, 258.571,       0, 164.286,  133.75},
{  342.5,  348.75, 413.333,  268.75, 181.667},
{  467.5, 504.667, 673.333,  366.25, 251.667},
{588.333,   762.5,  943.75,     625,     280},
{    890, 1160.91,    1345,     880,     350},
{   1075, 1365.83,    1560,    1150,     414},
{      0,    1687, 1871.25, 1678.75,     460},
{      0,    1950,    2420, 1993.75,     516},
};

int frequency[10][5] = {
{ 6,  1,  0,  0,  0},
{19,  3,  0,  5,  5},
{22,  7,  0,  7,  8},
{12,  8,  3,  8,  6},
{ 4, 15,  9,  8,  6},
{ 6, 12,  8, 10,  5},
{ 5, 11,  8, 11,  6},
{ 2, 12, 15, 10,  5},
{ 0, 10,  8,  8,  5},
{ 0, 12, 10,  8,  5},
};

struct Target {
	double probability, range, time, angle, hp, armorThickness;
};

double lodgistics(double x) {
	return 1 / (1 + exp(x));
}

double computeGunHandling(const Tank& tank, double angle = 100) {
	const Turret& turret = tank.turret;
	double timeAtAngle = 0;
	for (double theta = 0, guntheta = 0; theta < angle; timeAtAngle += 0.01) {
		double x = exp(2 * timeAtAngle / tank.gun.aimTime) - 1;
		double gunrotate = min(x * .5 / tank.gun.dispersion.movement, tank.gun.rotationSpeed);
		if (2 * guntheta > turret.yawLimits.left + turret.yawLimits.right)
			gunrotate = 0;
		double tankrotate = (x - gunrotate * tank.gun.dispersion.movement) / tank.chassis.movementDispersion;
		guntheta += gunrotate * 0.01;
		theta += (gunrotate + tankrotate) * 0.01;
	}
	return timeAtAngle;
}

double getArmor(double tier) {
	return tier * (10.0 - tier * (1.0 - tier * .2));
}

double getHP(double tier) {
	auto x = std::min(11.0, std::max(.8, tier));
	return (80.0 - (4.0 - 2.0*x)*x)*x;
}

double getExpectedDamage(std::function<double(double)> fn, double armor) {
	double x = .2 * exp(-25.0);
	double dmg = fn(0) * x;
	for (double i = 1; i < 75.0; i++) {
		x *= 25.0 / i;
		dmg += fn(i) * x;
	}
	x = .8 * exp(-armor);
	dmg += x * fn(0);
	for (double i = 1, e = 3.0 * armor; i < e; i++) {
		x *= armor / i;
		dmg += fn(i) * x;
	}
	return dmg;
}

std::function<double(double)> APRound(double tier, double pen, double damage) {
	return [tier, pen, damage](double x) -> double {
		x *= 1.4 / pen;
		double dmg = std::min(1.0, getHP(tier) / damage);
		if (x < 0.75) return 1.0;
		if (x > 1.25) return 0.0;
		return 2.5 - x - x;
	};
}

std::function<double(double)> HERound(double tier, double pen, double damage) {
	return [tier, pen, damage](double x) -> double {
		x *= 1.4 / pen;
		double hit = 0;
		if (x < 0.75) hit = 1.0;
		else if (x > 1.25) hit = 0.0;
		else hit = 2.5 - x - x;
		double dmg = std::max(0.5 - 1.1 * x / (1.4 / pen * damage), 0.0);
		return hit * std::min(1.0, getHP(tier) / damage) + (1.0 - hit) * dmg;
	};
}

double getExpectedDamage(const Tank& tank, const Shell& shell) {
	if (shell.kind == Shell::HE)
		return getExpectedDamage(HERound(tank.hull.tier, shell.penAt350m, shell.damage), getArmor(tank.hull.tier));
	return getExpectedDamage(APRound(tank.hull.tier, shell.penAt350m, shell.damage), getArmor(tank.hull.tier));
}

#if 0
double getExpectedDamage(const Tank& tank, const Shell& ap, const Shell& he) {
	return getExpectedDamage([&](double x){return
		std::max(APRound(tank.hull.tier, ap.penAt350m, ap.damage),
		         HERound(tank.hull.tier, he.penAt350m, he.damage));}, getArmor(tank.hull.tier));
}
#endif

double getEffectiveReloadRate(const Gun& gun) {
	return ((gun.magazine.size - 1) * gun.magazine.delay + gun.reloadTime) / gun.magazine.size;
}

double getEffectiveNumShots(const Gun& gun) {
	double shots = 0;
	for (double t = 0.0; t < 60; t += gun.reloadTime - gun.magazine.delay)
		for (int i = 0; i < (int)gun.magazine.size; t += gun.magazine.delay, i++)
			shots += lodgistics(3.0 * (t - 2)) * 0.6 + lodgistics(.25 * (t - 15)) * 0.4;
	return shots;
}

double getChanceToHit(const Gun& gun, const Shell& shell) {
	double scope = gun.dispersion.base;
	if (shell.kind == Shell::HE || shell.kind == Shell::HEAT)
		scope *= 1.25;
	double time = 350.0 / shell.speed;
	return lodgistics(4 * (0.5 - 0.12 / (scope * scope))) * lodgistics(5 * time - 5) * .6 + .4;
}

double getExpectedDamageOnTargetInTime(const Tank& tank, const Gun& gun, const Shell& shell) {
	return shell.damage * getExpectedDamage(tank, shell) * getEffectiveNumShots(gun) * getChanceToHit(gun, shell);
}


struct XShell : Shell {
	double utility;
};

struct XGun : Gun {
	double effectiveReloadTime;
	double effectiveAccuracy;
	double timeAt100deg;
	double utility;
};

struct XChassis : Chassis {
	double effectiveAcceleration;
	double effectiveRotationSpeed;
};


#if 0

namespace {

double getPenetration(double tier) {
	auto x = min(10.0, max(.5, tier));
	return (23.0 - (3.0 - 0.47*x)*x)*x;
}

double getArmorThickness(double tier) {
	return getPenetration(tier - 0.8);
}

double getPenetration(const Shell& shell, double range) {
	auto x = min(shell.maxRange - 100, max(0.0, range - 100));
	return shell.penAt100m + (shell.penAt350m - shell.penAt100m) * x / 250;
}

struct XTank : Tank {
	XTank(const Tank& tank, Target target);
	struct XShell : Shell {
		XShell() {}
		XShell(const Shell& shell, Target target, const Tank& tank);
		double getEffectiveDamage(const Shell& shell, Target target);
		XShell& operator +=(const XShell& x);
		XShell& operator *=(double x);
		double effPenetration, effDamage, utility, accuracy;
	} xshells[3];
	struct XGun : Gun {
		XGun() {}
		XGun(const Gun& gun, Target target);
		XGun& operator +=(const XGun& x);
		XGun& operator *=(double x);
		double effReloadTime, effShots, utility;
	} xgun;
	XTank& operator +=(const XTank& x);
	XTank& operator *=(double x);
	double effDPM, effDamage, effBurstDamage, premiumUtility, timeAtAngle;
};

XTank::XShell::XShell(const Shell& shell, Target target, const Tank& tank) : Shell(shell) {
	accuracy = lodgistics(4 * target.range / speed - 4) * .6 + .4;
	effPenetration = getPenetration(shell, target.range);
	double x = lodgistics(10 * (target.armorThickness - effPenetration) / effPenetration);
	effDamage = x * accuracy * min(target.hp, shell.damage);
	if (shell.kind == Shell::HE)
		effDamage += (1 - x) * accuracy * min(target.hp, max(0.0, shell.damage * 0.5 - 0.7 * target.armorThickness));
	double costRatio = effDamage * effDamage * effDamage / max(price, min(shell.damage, tank.hull.health + tank.turret.health) * tank.hull.repairCost * 0.25);
	utility = costRatio * costRatio;
}

XTank::XShell& XTank::XShell::operator +=(const XShell& x) {
	effPenetration += x.effPenetration;
	effDamage += x.effDamage;
	utility += x.utility;
	accuracy += x.accuracy;
	return *this;
}

XTank::XShell& XTank::XShell::operator *=(double x) {
	effPenetration *= x;
	effDamage *= x;
	utility *= x;
	accuracy *= x;
	return *this;
}

XTank::XGun::XGun(const Gun& gun, Target target) : Gun(gun) {
	effReloadTime = ((magazine.size - 1) * magazine.delay + reloadTime) / magazine.size;
	effShots = 0;
	for (double t = 0.0; t < target.time; t += reloadTime - magazine.delay)
		for (int i = 0; i < (int)magazine.size; t += magazine.delay, i++)
			effShots += lodgistics(t - target.time);
}

XTank::XGun& XTank::XGun::operator +=(const XGun& x) {
	utility += x.utility;
	return *this;
}

XTank::XGun& XTank::XGun::operator *=(double x) {
	utility *= x;
	return *this;
}

XTank::XTank(const Tank& tank, Target target) : Tank(tank), xgun(tank.gun, target) {
	for (auto i = 0; i < 3; i++)
		if (shells[i].kind)
			xshells[i] = XShell(shells[i], target, tank);
	double totalUtility = 0;
	for (auto i = 0; i < 3; i++)
		if (shells[i].kind)
			totalUtility += xshells[i].utility;
	for (auto i = 0; i < 3; i++)
		if (shells[i].kind)
			xshells[i].utility /= totalUtility;
	effDPM = 0;
	for (auto i = 0; i < 3; i++)
		if (shells[i].kind)
			effDPM += 60.0 * xshells[i].effDamage / xgun.effReloadTime * xshells[i].utility;
	effDamage = 0;
	for (auto i = 0; i < 3; i++)
		if (shells[i].kind)
			effDamage += xshells[i].effDamage * xshells[i].utility;
	effBurstDamage = effDamage * xgun.effShots;
	premiumUtility = 0;
	for (auto i = 0; i < 3; i++)
		if (shells[i].kind && shells[i].isPremium)
			premiumUtility += xshells[i].utility;
	timeAtAngle = 0;
	for (double theta = 0, guntheta = 0; theta < target.angle; timeAtAngle += 0.01) {
		double x = exp(2 * timeAtAngle / tank.gun.aimTime) - 1;
		double gunrotate = min(x * .5 / tank.gun.dispersion.movement, tank.gun.rotationSpeed);
		if (2 * guntheta > turret.yawLimits.left + turret.yawLimits.right)
			gunrotate = 0;
		double tankrotate = (x - gunrotate * tank.gun.dispersion.movement) / tank.chassis.movementDispersion;
		guntheta += gunrotate * 0.01;
		theta += (gunrotate + tankrotate) * 0.01;
	}
}

XTank& XTank::operator +=(const XTank& x) {
	for (int i = 0; i < 3; i++)
		if (xshells[i].kind)
			xshells[i] += x.xshells[i];
	xgun += x.xgun;
	effDPM += x.effDPM;
	effDamage += x.effDamage;
	effBurstDamage += x.effBurstDamage;
	premiumUtility += x.premiumUtility;
	timeAtAngle += x.timeAtAngle;
	return *this;
}

XTank& XTank::operator *=(double x) {
	for (int i = 0; i < 3; i++)
		if (xshells[i].kind)
			xshells[i] *= x;
	xgun *= x;
	effDPM *= x;
	effDamage *= x;
	effBurstDamage *= x;
	premiumUtility *= x;
	timeAtAngle *= x;
	return *this;
}

XTank Evaluate(const Tank& tank, double range, double time, double angle) {
	double flankingBonus = 0;
	Target target[] = {
		{ 0.05, range, time, angle, getHP(tank.hull.tier / 2), getArmorThickness(tank.hull.tier - flankingBonus - 8) }, // arty
		{ 0.06, range, time, angle, getHP(tank.hull.tier - 2), getArmorThickness(tank.hull.tier - flankingBonus - 4) }, // scouts
		{ 0.09, range, time, angle, getHP(tank.hull.tier - 2), getArmorThickness(tank.hull.tier - flankingBonus - 2) },
		{ 0.20, range, time, angle, getHP(tank.hull.tier - 1), getArmorThickness(tank.hull.tier - flankingBonus - 1) },
		{ 0.30, range, time, angle, getHP(tank.hull.tier    ), getArmorThickness(tank.hull.tier - flankingBonus    ) },
		{ 0.20, range, time, angle, getHP(tank.hull.tier + 1), getArmorThickness(tank.hull.tier - flankingBonus + 1) },
		{ 0.10, range, time, angle, getHP(tank.hull.tier + 2), getArmorThickness(tank.hull.tier - flankingBonus + 2) },
	};
	if (tank.hull.tier == 1) {
		target[0].probability = target[1].probability = target[2].probability = target[3].probability = target[6].probability = 0;
		target[4].probability = .75;
		target[5].probability = .25;
	} else if (tank.hull.tier == 2) {
		target[1].probability = target[2].probability = target[6].probability = 0;
		target[3].probability = .20;
		target[4].probability = .5;
		target[5].probability = .25;
	} else if (tank.hull.tier == 3) {
		target[1].probability = target[2].probability = 0;
		target[5].probability = .25;
		target[6].probability = .15;
	} else if (tank.hull.tier == 4) {
		target[1].probability = target[2].probability = 0;
		target[4].probability = .35;
		target[5].probability = .25;
	} else if (tank.hull.tier == 9) {
		target[4].probability = .35;
		target[5].probability = .25;
		target[6].probability = 0;
	} else if (tank.hull.tier == 10) {
		target[3].probability = .30;
		target[4].probability = .45;
		target[5].probability = .05;
		target[6].probability = 0;
	}
	XTank xtank(tank, target[0]);
	xtank *= target[0].probability;
	for (int i = 1; i < 7; i++) {
		XTank x(tank, target[i]);
		x *= target[i].probability;
		xtank += x;
	}
	return xtank;
}
} // nameapce
#endif

void PrintSkills(char tanker) {
	if (tanker & COMMANDER) printf("commander");
	if (tanker & GUNNER) printf("gunner");
	if (tanker & DRIVER) printf("driver");
	if (tanker & RADIOMAN) printf("radioman");
	if (tanker & LOADER) printf("loader");
	if (tanker & ALSO_GUNNER) printf(", gunner");
	if (tanker & ALSO_RADIOMAN) printf(", radioman");
	if (tanker & ALSO_LOADER) printf(", loader");
}

void ProcessTanks(const Tank* tanks, size_t size) {

	struct Entry {
		bool operator <(const Entry& x) const {
			return key < x.key ||
				key == x.key && tank.hull.name < x.tank.hull.name ||
				key == x.key && tank.hull.name == x.tank.hull.name && tank.gun.name < x.tank.gun.name;
		}
		Tank tank;
		double key;
	};

	vector<Entry> entires;
	for (size_t i = 0; i < size; i++) {
		auto& tank = tanks[i];
		auto& gun = tank.gun;
		auto& shell = tank.shells[0];
		if (!shell.kind)
			continue;
		Entry x = { tank, -getExpectedDamageOnTargetInTime(tank, gun, shell) / pow(getHP(tank.hull.tier), .5) * 10 };
		entires.push_back(x);
	}
	std::sort(entires.begin(), entires.end());
	int j = 0;
	auto f = fopen("out.txt", "wb");
	for (auto i = entires.begin(), e = entires.end(); i != e && ++j < 5000; ++i)
		fprintf(f, "%4.0f : %5.1f-%-5.1f : %2d : %-14s : %s\n", -i->key, i->tank.shells[0].penAt100m, i->tank.shells[0].penAt350m, (int)i->tank.hull.tier, i->tank.hull.name.c_str(), i->tank.gun.name.c_str());
	return;

#if 0
	struct Entry {
		bool operator <(const Entry& x) const {
			return key < x.key ||
				key == x.key && xtank.hull.name < x.xtank.hull.name ||
				key == x.key && xtank.hull.name == x.xtank.hull.name && xtank.gun.name < x.xtank.gun.name;
		}
		XTank xtank;
		double key;
	};
	vector<XTank> xtanks;
	for (size_t i = 0; i < size; i++)
		xtanks.push_back(Evaluate(tanks[i], 350, 6, 100));
	map<string, vector<XTank*>> groupByGun;
	for (size_t i = 0; i < size; i++)
		groupByGun[xtanks[i].hull.name].push_back(&xtanks[i]);
	for (auto i = groupByGun.begin(), e = groupByGun.end(); i != e; i++) {
		double maxUtility = 0;
		for (auto j = 0; j < i->second.size(); j++)
			maxUtility = max(maxUtility, pow(i->second[j]->effDPM + 15 * i->second[j]->effBurstDamage, 2));
		for (auto j = 0; j < i->second.size(); j++) {
			i->second[j]->xgun.utility = pow(pow(i->second[j]->effDPM + 15 * i->second[j]->effBurstDamage, 2) / maxUtility, 3);
			i->second[j]->premiumUtility *= i->second[j]->xgun.utility;
			for (int k = 0; k < 3; k++)
				if (i->second[j]->xshells[k].kind)
					i->second[j]->xshells[k].utility *= i->second[j]->xgun.utility;
		}
	}
	vector<Entry> entries;
	for (size_t i = 0; i < size; i++) {
		Entry e = { xtanks[i], xtanks[i].xgun.utility };
		entries.push_back(e);
	}
	std::sort(entries.begin(), entries.end());
	for (auto i = entries.begin(), e = entries.end(); i != e; ++i)
		//printf("%10g : %-20s : %s\n", , i->xtank.hull.name.c_str(), i->xtank.gun.name.c_str());
		//if (i->xtank.hull.name.find("GB11") != string::npos)
		printf("%10g : %-20s : %s\n", i->xtank.xgun.utility, i->xtank.hull.name.c_str(), i->xtank.gun.name.c_str());
#endif
}
