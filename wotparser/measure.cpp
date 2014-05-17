#include "tank.h"
#include <math.h>
#include <algorithm>
#include <map>
#include <vector>
#include <conio.h>

using std::min;
using std::max;
using std::string;
using std::map;
using std::vector;

namespace {

double lodgistics(double x) {
	return 1 / (1 + exp(x));
}

double getPenetration(double tier) {
	auto x = min(10.0, max(.5, tier));
	return (23.0 - (3.0 - 0.47*x)*x)*x;
}

double getArmorThickness(double tier) {
	return getPenetration(tier - 0.8);
}

double getHP(double tier) {
	auto x = min(11.0, max(.8, tier));
	return (80.0 - (4.0 - 2.0*x)*x)*x;
}

double getPenetration(const Shell& shell, double range) {
	auto x = min(shell.maxRange - 100, max(0.0, range - 100));
	return shell.penAt100m + (shell.penAt720m - shell.penAt100m) * x / (shell.maxRange - 100);
}

struct Target {
	double probability, range, time, angle, hp, armorThickness;
};

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
	accuracy = lodgistics(4 * target.range / speed - 4);
	effPenetration = getPenetration(shell, target.range);
	double x = lodgistics(10 * (target.armorThickness - effPenetration) / effPenetration);
	effDamage = x * accuracy * min(target.hp, shell.damage);
	if (shell.kind  == Shell::HE)
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
		bool operator <(const Entry& x) {
			return key < x.key ||
				key == x.key && xtank.hull.name < x.xtank.hull.name ||
				key == x.key && xtank.hull.name == x.xtank.hull.name && xtank.gun.name < x.xtank.gun.name;
		}
		XTank xtank;
		double key;
	};
	vector<XTank> xtanks;
	for (size_t i = 0; i < size; i++)
		xtanks.push_back(Evaluate(tanks[i], 100, 6, 100));
	map<string, vector<XTank*>> groupByGun;
	for (size_t i = 0; i < size; i++)
		groupByGun[xtanks[i].hull.name].push_back(&xtanks[i]);
	for (auto i = groupByGun.begin(), e = groupByGun.end(); i != e; i++) {
		double maxUtility = 0;
		for (auto j = 0; j < i->second.size(); j++)
			maxUtility = max(maxUtility, pow(i->second[j]->effDPM + i->second[j]->effBurstDamage, 2));
		for (auto j = 0; j < i->second.size(); j++) {
			i->second[j]->xgun.utility = pow(pow(i->second[j]->effDPM + i->second[j]->effBurstDamage, 2) / maxUtility, 3);
			i->second[j]->premiumUtility *= i->second[j]->xgun.utility;
			for (int k = 0; k < 3; k++)
				if (i->second[j]->xshells[k].kind)
					i->second[j]->xshells[k].utility *= i->second[j]->xgun.utility;
		}
	}
	vector<Entry> entries;
	for (size_t i = 0; i < size; i++) {
		Entry e = { xtanks[i], -xtanks[i].timeAtAngle };
		entries.push_back(e);
	}
	std::sort(entries.begin(), entries.end());
	for (auto i = entries.begin(), e = entries.end(); i != e; ++i)
		//printf("%10g : %-20s : %s\n", , i->xtank.hull.name.c_str(), i->xtank.gun.name.c_str());
		if (i->xtank.gun.name.find("_122mm") != string::npos || i->xtank.gun.name.find("_122-mm") != string::npos)
		printf("%10g : %-20s : %s\n", i->xtank.timeAtAngle, i->xtank.hull.name.c_str(), i->xtank.gun.name.c_str());
}
