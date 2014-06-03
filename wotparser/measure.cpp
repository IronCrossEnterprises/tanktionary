#include "tank.h"
#include <math.h>
#include <algorithm>
#include <map>
#include <vector>
#include <conio.h>
#include <functional>

using std::string;
using std::map;
using std::vector;

#if 0
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
#endif

double lodgistics(double x) {
	return 1 / (1 + exp(x));
}

double getGunHandling(Tank& tank, double speed, double angle) {
	const Turret& turret = tank.turret;
	const Gun& gun = tank.gun;
	const Chassis& chassis = tank.chassis;
	double timeAtAngle = 0;
	for (double theta = 0, guntheta = 0; theta < angle; timeAtAngle += 0.01) {
		double x = exp(2 * timeAtAngle / gun.aimTime) - 1;
		double gunrotate = std::min(x * .5 / gun.dispersion.movement, gun.rotationSpeed);
		if (guntheta > 0.5 * (turret.yawLimits.left + turret.yawLimits.right))
			gunrotate = 0;
		double tankrotate = (x - gunrotate * gun.dispersion.movement) / chassis.rotation.dispersion;
		guntheta += gunrotate * 0.01;
		theta += (gunrotate + tankrotate) * 0.01;
	}
	double timeAtSpeed = gun.aimTime * 0.5 * log(1.0 + speed * chassis.movementDispersion * (speed * chassis.movementDispersion));
	return timeAtAngle + timeAtSpeed;
}

double getArmor(double tier) {
	return tier * (10.0 - tier * (1.0 - tier * .2));
}

double getHP(double tier) {
	auto x = std::min(11.0, std::max(.8, tier));
	return (80.0 - (4.0 - 2.0*x)*x)*x;
}

double getMMTier(Tank& tank) {
	auto tier = tank.hull.tier;
	if (tank.hull.tags.find("scout") != string::npos)
		tier += 1.5;
	if (tank.hull.name == "M24_Chaffee") tier += 0.5;
	return tier;
}

double getExpectedDamage(Tank& tank, const Shell& shell, double scope, double armor, double range) {
	auto maxDamage = getHP(getMMTier(tank));
	if (shell.kind == Shell::HE || shell.kind == Shell::HEAT)
		scope *= 1.2;
	double time = range / shell.speed;
	double hitChance = lodgistics(4.0 * (0.4 - 25.0 / (range * scope * scope))) * lodgistics(5.0 * time - 5.0) * 0.6 + 0.4;
	double pen = 0;
	if (range <= shell.maxRange)
		pen = std::max(range - 100.0, 0.0) * (1.0 / 250.0) * (shell.penAt350m - shell.penAt100m) + shell.penAt100m;
	double penChance = std::min(1.0, std::max(0.0, 2.5 - armor * (2.0 * 1.4) / pen));
	double damage = penChance * std::min(maxDamage, shell.damage);
	if (shell.kind == Shell::HE)
		damage += (1.0 - penChance) * std::min(1.0, std::max(0.0, 2.5 - armor * (1.1 * 2.0 / 0.5) / shell.damage)) * std::min(maxDamage, shell.damage * 0.5 - armor * 1.1);
	return damage * hitChance;
}

double getExpectedDamage(Tank& tank, double scope, double armor, double range) {
	double damage[] = {0.0, 0.0, 0.0};
	double efficiency[] = {0.0, 0.0, 0.0};
	double max = 0.0;
	size_t best = 0;
	for (size_t i = 0; i < 3; i++) {
		auto& shell = tank.shells[i];
		if (!shell.kind)
			continue;
		damage[i] = getExpectedDamage(tank, shell, scope, armor, range);
		double sunkCost = (tank.hull.health + tank.turret.health) * tank.hull.repairCost * damage[i] / getHP(tank.hull.tier) * 0.3;
		efficiency[i] = damage[i] * damage[i] / (shell.price + sunkCost);
		if (efficiency[i] > max) {
			max = efficiency[i];
			best = i;
		}
	}
	return damage[best];
}

double getExpectedDamage(Tank& tank, double scope, double armor) {
	double mean = 30.0; // range
	double w = exp(-mean);
	double x = w * getExpectedDamage(tank, scope, armor, 0.0);
	for (int i = 1; i < 48; i++) {
		w *= mean / i;
		x += getExpectedDamage(tank, scope, armor, i * (200.0 / 30.0)) * w;
	}
	return x;
}

double getExpectedDamage(Tank& tank, double scope) {
	double mean = 10.0;
	double w = 0.2 * exp(-mean);
	double x = w * getExpectedDamage(tank, scope, 0.0);
	for (double i = 1; i < mean || w > 0.001; i++) {
		w *= mean / i;
		x += getExpectedDamage(tank, scope, i * 2.0) * w;
	}
	double armor = getArmor(tank.hull.tier) / 30.0;
	mean = 30.0;
	w = 0.8 * exp(-mean);
	for (int i = 1; i < 48; i++) {
		w *= mean / i;
		x += getExpectedDamage(tank, scope, i * armor) * w;
	}
	return x;
}

double getTargetIsValid(double t) {
	return lodgistics(2.0 * (t - 3.0)) * 0.6 + lodgistics(.25 * (t - 15)) * 0.4;
}

double getExpectedDamage(Tank& tank) {
	const Gun& gun = tank.gun;
	double shotDispersion = sqrt(1.0 + gun.dispersion.shot * gun.dispersion.shot);
	double reaimTime = gun.aimTime * log(shotDispersion);
	shotDispersion *= gun.dispersion.base;
	double t = 0.0;
	double scale = 1.0 / getTargetIsValid(t);
	double damage = getExpectedDamage(tank, gun.dispersion.base);
	bool notEnoughAimTime = false;
	for (int i = 0; scale * getTargetIsValid(t) > 0.001; i++) {
		double delta = ((i + 1) % (int)gun.magazine.size ? gun.magazine.delay : gun.reloadTime);
		double scope = std::max(gun.dispersion.base, shotDispersion * exp(-delta / gun.aimTime));
		double temp = scale * getTargetIsValid(t + delta) * getExpectedDamage(tank, scope);
		if (reaimTime > delta && (!((i + 1) % (int)gun.magazine.size) || gun.magazine.burst == 1.0)) {
			double scope = std::max(gun.dispersion.base, shotDispersion * exp(-reaimTime / gun.aimTime));
			double temp2 = scale * getTargetIsValid(t + delta) * getExpectedDamage(tank, scope);
			if (temp2 > temp) {
				delta = reaimTime;
				temp = temp2;
				notEnoughAimTime = true;
			}
		}
		t += delta;
		damage += temp;
	}
	if (notEnoughAimTime)
		tank.notes += " NOT_ENOUGH_AIM_TIME";
	return damage;
}

double getAgility(Tank& tank) {
	double mass = (tank.hull.weight +
		tank.chassis.weight +
		tank.fuelTank.weight +
		tank.turret.weight +
		tank.gun.weight +
		tank.engine.weight +
		tank.radio.weight);
	double powerToWeight = tank.engine.power / mass;
	double speed = tank.chassis.speedLimits.forward + tank.chassis.speedLimits.backward * 2.4 + tank.chassis.rotation.speed * powerToWeight / tank.stockPowerToWeight;
	double acceleration = powerToWeight * pow(tank.chassis.terrainResistance.hard * 0.2 + tank.chassis.terrainResistance.medium * 0.7 + tank.chassis.terrainResistance.soft * 0.1, -1.2);
	return speed * acceleration;
	//return pow(pow(, 0.412) *
	//       pow(, 0.588) - 0.16, 0.58);
}

double getGunHandlingNormalized(Tank& tank) {
	return 0;
	//double x = getGunHandling(tank, 20, 20) * 0.3 + getGunHandling(tank, 20, 100) * 0.7;
	//return pow(2.0 / x - 0.15, 0.97);
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

Tank applySkillsAndEquipment(const Tank& in, const string& equipment) {
	Tank tank = in;
	
	int gunners = 0;
	int loaders = 0;
	int commanderGunner = 0;
	int commanderLoader = 0;
	for (int i = 0; i < 8; i++) {
		if (tank.crew[i] & (GUNNER | ALSO_GUNNER)) {
			gunners++;
			if (tank.crew[i] & COMMANDER)
				commanderGunner = 1;
		}
		if (tank.crew[i] & (LOADER | ALSO_LOADER)) {
			loaders++;
			if (tank.crew[i] & COMMANDER)
				commanderLoader = 1;
		}
	}
	if (commanderGunner) tank.notes += " COMMANDER_IS_GUNNER";
	if (commanderLoader) tank.notes += " COMMANDER_IS_LOADER";
	double commanderSkill = 100.0;

	if (equipment.find("antifragmentationLining_light") != string::npos)
		tank.hull.weight += 250.0;
	if (equipment.find("antifragmentationLining") != string::npos)
		tank.hull.weight += 500.0;
	if (equipment.find("antifragmentationLining_heavy") != string::npos)
		tank.hull.weight += 1000.0;
	if (equipment.find("antifragmentationLining_superheavy") != string::npos)
		tank.hull.weight += 1500.0;
	if (equipment.find("mediumCaliberTankRammer") != string::npos) {
		tank.gun.reloadTime *= 0.9;
		tank.hull.weight += 200.0;
	}
	if (equipment.find("largeCaliberTankRammer") != string::npos) {
		tank.gun.reloadTime *= 0.9;
		tank.hull.weight += 400.0;
	}
	if (equipment.find("mediumCaliberHowitzerRammer") != string::npos) {
		tank.gun.reloadTime *= 0.9;
		tank.hull.weight += 300.0;
	}
	if (equipment.find("largeCaliberHowitzerRammer") != string::npos) {
		tank.gun.reloadTime *= 0.9;
		tank.hull.weight += 500.0;
	}
	if (equipment.find("enhancedAimDrives") != string::npos) {
		tank.gun.aimTime *= 0.91;
		tank.hull.weight += 100.0;
	}
	if (equipment.find("aimingStabilizer_Mk1") != string::npos) {
		tank.gun.dispersion.shot *= 0.8;
		tank.gun.dispersion.movement *= 0.8;
		tank.chassis.movementDispersion *= 0.8;
		tank.hull.weight += 100.0;
	}
	if (equipment.find("aimingStabilizer_Mk2") != string::npos) {
		tank.gun.dispersion.shot *= 0.8;
		tank.gun.dispersion.movement *= 0.8;
		tank.chassis.movementDispersion *= 0.8;
		tank.hull.weight += 200.0;
	}
	if (equipment.find("grousers") != string::npos) {
		tank.chassis.terrainResistance.soft *= 0.909;
		tank.chassis.terrainResistance.medium *= 0.952;
		tank.chassis.weight += 1000.0;
	}
	if (equipment.find("improvedVentilation_class1") != string::npos) {
		commanderSkill += 5.0;
		tank.hull.weight += 100.0;
	}
	if (equipment.find("improvedVentilation_class2") != string::npos) {
		commanderSkill += 5.0;
		tank.hull.weight += 150.0;
	}
	if (equipment.find("improvedVentilation_class6") != string::npos) {
		commanderSkill += 5.0;
		tank.hull.weight += 200.0;
	}
	if (equipment.find("carbonDioxide") != string::npos) {
		tank.fuelTank.health *= 1.5;
	}
	if (equipment.find("filterCyclone") != string::npos) {
		tank.engine.health *= 1.5;
	}
	if (equipment.find("wetCombatPack_class") != string::npos) {
		tank.ammoBay.health *= 1.5;
		tank.hull.weight += 0.01 * (tank.hull.weight + tank.chassis.weight + tank.fuelTank.weight +
			tank.turret.weight + tank.turret.weight + tank.engine.weight + tank.radio.weight);
	}

	if (equipment.find("lendLeaseOil") != string::npos || equipment.find("qualityOil") != string::npos)
		tank.engine.power *= 1.05;
	if (equipment.find("gasoline100") != string::npos) {
		tank.engine.power *= 1.05;
		tank.turret.rotationSpeed *= 1.05;
	}
	if (equipment.find("gasoline105") != string::npos) {
		tank.engine.power *= 1.1;
		tank.turret.rotationSpeed *= 1.1;
	}
	if (equipment.find("chocolate") != string::npos ||
		equipment.find("cocacola") != string::npos ||
		equipment.find("ration") != string::npos ||
		equipment.find("hotCoffee") != string::npos)
		commanderSkill += 10.0;

	double gunnerSkill = commanderSkill + commanderSkill * 0.1 * (gunners - commanderGunner) / gunners;
	double loaderSkill = commanderSkill + commanderSkill * 0.1 * (loaders - commanderLoader) / loaders;
	double driverSkill = commanderSkill * 1.1;

	tank.gun.aimTime *= 0.875 / (0.5 + 0.00375 * gunnerSkill);
	tank.gun.dispersion.base *= 0.875 / (0.5 + 0.00375 * gunnerSkill);
	tank.gun.reloadTime *= 0.875 / (0.5 + 0.00375 * loaderSkill);
	tank.chassis.terrainResistance.hard *= 0.875 / (0.5 + 0.00375 * driverSkill);
	tank.chassis.terrainResistance.medium *= 0.875 / (0.5 + 0.00375 * driverSkill);
	tank.chassis.terrainResistance.soft *= 0.875 / (0.5 + 0.00375 * driverSkill);

	return tank;
}

double normalizeExpectedDamage(double damage, double tier) {
	return pow((damage * pow(tier + 4, -1.6) - 0.45) / 14.5, 0.8);
}

double normalizeGunHandling(double handling) {
	return pow((pow(handling, -0.85) - 0.07) / 0.386, 0.88);
}

double normalizeAgility(double agility) {
	return pow((agility - 0.18) / 6.63, 0.44);
}

void ProcessTanks(const Tank* tanks, size_t size) {
	struct Stats { double damage, handling, agility, health; };
	map<string, Stats> statsMap;
	auto fin = fopen("in.txt", "rb");
	while (!feof(fin)) {
		char name[64];
		float a[4];
		fscanf(fin, "%s %f %f %f %f", name, &a[0], &a[1], &a[2], &a[3]);
		Stats s = { a[0], a[1], a[2], a[3] };
		statsMap[name] = s;
	}

#if 0
	for (auto i = 0; i < size; i++)
		if (tanks[i].hull.name.find("Waffentrager_IV") != string::npos) {
		//if (tanks[i].hull.name.find("T-28") != string::npos) {
			auto& gun = tanks[i].gun;
			auto reAimTime = getReAimTime(gun);
		//if (tanks[i].hull.name.find("Vickers") != string::npos || tanks[i].hull.name.find("Car") != string::npos)
			printf("%26s : %f\n", tanks[i].gun.name.c_str(), getExpectedDamageOnTargetInTime(tanks[i], gun, tanks[i].shells[0]));
			//printf("%26s : %f : %f : %f, %f\n", tanks[i].hull.name.c_str(), gun.reloadTime, reAimTime, sqrt(17.0)*exp(-gun.reloadTime/gun.aimTime), exp((reAimTime - gun.reloadTime)/gun.aimTime));
		}
	//return;
#endif

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
		auto tank = applySkillsAndEquipment(tanks[i], "");
		//Entry x = { tank, getExpectedDamage(tank) * pow(tank.hull.tier + 4.0, -1.8) };
		//Entry x = { tank, getManeuverability(tank) };
		//Entry x = { tank, getGunHandling(tank, 10, 100) };
		//Entry x = { tank, pow((statsMap[tank.hull.name + tank.gun.name].agility - 0.18) / 6.63, 0.44) };
		//Entry x = { tank, (int)(1 + 10 * normalizeAgility(statsMap[tank.hull.name + tank.gun.name].agility)) };
		Entry x = { tank, (int)(1 + 10 * normalizeGunHandling(statsMap[tank.hull.name + tank.gun.name].handling)) };
		//Entry x = { tank, (int)(1 + 10 * normalizeExpectedDamage(statsMap[tank.hull.name + tank.gun.name].damage, tank.hull.tier)) };
		//Entry x = { tank, normalizeAgility(statsMap[tank.hull.name + tank.gun.name].agility) +
		//normalizeGunHandling(statsMap[tank.hull.name + tank.gun.name].handling) +
		//3 * normalizeExpectedDamage(statsMap[tank.hull.name + tank.gun.name].damage, tank.hull.tier) };
		//printf("%7.2f : %s\n", x.key, x.tank.hull.name.c_str());
		entires.push_back(x);
	}
	std::sort(entires.begin(), entires.end());
	auto f = fopen("out.txt", "wb");
	for (auto i = entires.begin(), e = entires.end(); i != e; ++i)
		//fprintf(f, "%5.3f %s%s\n", i->key, i->tank.hull.name.c_str(), i->tank.gun.name.c_str());
		fprintf(f, "%2d %2d %2d %s : %s\n",
			(int)(1 + 10 * normalizeExpectedDamage(statsMap[i->tank.hull.name + i->tank.gun.name].damage, i->tank.hull.tier)),
			(int)(1 + 10 * normalizeGunHandling(statsMap[i->tank.hull.name + i->tank.gun.name].handling)),
			(int)(1 + 10 * normalizeAgility(statsMap[i->tank.hull.name + i->tank.gun.name].agility)),
			i->tank.hull.name.c_str(), i->tank.gun.name.c_str());
		//fprintf(f, "%5.3f : %2d : %25s : %s\n", i->key, (int)i->tank.hull.tier, i->tank.hull.name.c_str(), i->tank.gun.name.c_str());
		//fprintf(f, "%5.3f : %s\n", (double)(int)(10 * i->key + 0.5), i->tank.hull.name.c_str());
		//fprintf(f, "%4.0f : %5.1f-%-5.1f : %2d : %-14s : %s\n", -i->key, i->tank.shells[0].penAt100m, i->tank.shells[0].penAt350m, (int)i->tank.hull.tier, i->tank.hull.name.c_str(), i->tank.gun.name.c_str());

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
