// WoT Parser 2

#include <string>
#include <vector>
#include <map>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <algorithm>
#include <conio.h>
#include <math.h>

#include <iostream>
#include <iomanip>

using std::string;
using std::vector;
using std::map;
using std::pair;
using std::make_pair;

enum Nation { CHINA, FRANCE, GERMANY, JAPAN, UK, USA, USSR } nation;
enum TagKind { TAG_ELEMENT, TAG_STRING, TAG_NUMBER, TAG_FLOATS, TAG_BOOLEAN, TAG_HEX64 };

struct DataDescriptor {
	unsigned end : 28;
	unsigned kind : 4;
};

struct ElementDescriptor {
	unsigned dictionaryIndex;
	DataDescriptor dataDesc;
};

vector<const char*> dictionary;
const char* tag;
const char* stream;
unsigned size;
unsigned kind;

template<typename T>
T Consume() { T x = *(T*)stream; stream += sizeof(T); return x; }

bool Fail() {
	fprintf(stderr, "Uncaught tag [%d]: %s\n", kind, tag);
	stream += size;
	return false;
};

bool Ignore(const char* query = 0) {
	if (query && strcmp(query, tag))
		return false;
	stream += size;
	return true;
}

bool Case(int& data, const char* query = 0) {
	if (kind != TAG_NUMBER || query && strcmp(query, tag))
		return false;
	switch (size) {
	case 0: data = 0; break;
	case 1: data = *(char*)stream; break;
	case 2: data = *(short*)stream; break;
	case 4: data = *(int*)stream; break;
	default:
		fprintf(stderr, "Unknown size of number %d!\n", size);
		exit(1);
	}
	stream += size;
	return true;
}

bool Case(float* data, unsigned space, const char* query = 0) {
	if (kind != TAG_FLOATS || query && strcmp(query, tag) || space != size)
		return false;
	memcpy(data, stream, size);
	stream += size;
	return true;
}

bool Case(bool& data, const char* query = 0) {
	if (kind != TAG_BOOLEAN || query && strcmp(query, tag))
		return false;
	data = size != 0;
	stream += size;
	return true;
}

bool Case(string& data, const char* query = 0) {
	if (query && strcmp(query, tag))
		return false;
	if (kind == TAG_STRING) {
		data = string(stream, stream + size);
		stream += size;
		return true;
	}
	if (kind != TAG_HEX64)
		return false;
	if (size % 3) {
		fprintf(stderr, "Invalid base64 size!");
		exit(1);
	}
	int len = size / 3 * 4;
	data = string(len, ' ');
	for (int i = 0; i != len; i += 4) {
		static const char table[] = {
			'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
			'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
			'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
			'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
			'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
			'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
			'w', 'x', 'y', 'z', '0', '1', '2', '3',
			'4', '5', '6', '7', '8', '9', '+', '/',
		};
		int x = Consume<unsigned char>() << 16 |
		        Consume<unsigned char>() << 8 |
		        Consume<unsigned char>();
		data[i + 0] = table[x >> 18];
		data[i + 1] = table[(x >> 12) & 0x3f];
		data[i + 2] = table[(x >> 6) & 0x3f];
		data[i + 3] = table[x & 0x3f];
	}
	return true;
}

bool Case(double& data, const char* query = 0) {
	int i;
	string s;
	float f;
	return
	Case(i, query) && (data = i, 1) ||
	Case(&f, sizeof(f), query) && (data = f, 1) ||
	Case(s, query) && (data = atof(s.c_str()), 1);
}

struct Element {
	virtual void ParseSelf() { kind == TAG_STRING && size == 0 || Fail(); }
	virtual void ParseTag() { Fail(); }
	Nation nation;
	string label;
};

bool Case(Element& element, const char* query = 0) {
	if (kind != TAG_ELEMENT || query && strcmp(query, tag))
		return false;
	vector<ElementDescriptor> elements(Consume<unsigned short>());
	DataDescriptor desc = Consume<DataDescriptor>();
	for (auto i = elements.begin(), e = elements.end(); i != e; ++i) {
		i->dictionaryIndex = Consume<unsigned short>();
		i->dataDesc = Consume<DataDescriptor>();
	}
	size = desc.end;
	kind = desc.kind;
	element.label = tag;
	element.nation = nation;
	element.ParseSelf();
	unsigned priorEnd = desc.end;
	for (auto i = elements.begin(), e = elements.end(); i != e; ++i) {
		tag = dictionary[i->dictionaryIndex];
		size = i->dataDesc.end - priorEnd;
		kind = i->dataDesc.kind;
		element.ParseTag();
		priorEnd = i->dataDesc.end;
	}
	return true;
}

double ParseNums(const char* p, int i) {
	for (; i; --i) {
		auto q = strstr(p, " ");
		if (!q) return 0;
		p = q + 1;
	}
	return atof(p);
}

//==============================================================================
// Shells
//==============================================================================

struct Shell : Element {
	struct Price : Element {
		Price(Shell& shell) : shell(shell) {}
		void ParseSelf() override { int i; Case(i) && (shell.price = i, 1) || Fail(); }
		void ParseTag() override {
			Ignore("gold") && (shell.price *= 400, shell.isPremium = true) || Fail();
		}
		Shell& shell;
	};

	Shell() : explosionRadius(0), isPremium(false), piercingPowerLossFactorByDistance(0.0), notInShop(false) {}

	struct Damage : Element {
		void ParseTag() override {
			Case(armor, "armor") ||
			Case(devices, "devices") ||
			Fail();
		}
		int armor;
		int devices;
	};

	void ParseTag() override {
		string temp;
		Ignore("id") ||
		Case(price, "price") ||
		Case(Price(*this), "price") ||
		Case(kind, "kind") ||
		Case(icon, "icon") ||
		Case(userString, "userString") ||
		Ignore("effects") ||
		Case(damage, "damage") ||
		Case(explosionRadius, "explosionRadius") ||
		Case(caliber, "caliber") ||
		Case(piercingPowerLossFactorByDistance, "piercingPowerLossFactorByDistance") ||
		Case(isTracer, "isTracer") ||
		Case(notInShop, "notInShop") ||
		// From shot!
		Case(defaultPortion, "defaultPortion") ||
		Case(speed, "speed") ||
		Case(gravity, "gravity") ||
		Case(maxDistance, "maxDistance") ||
		Case(temp, "piercingPower") && handlePiercingPower(temp) ||
		Fail();
	}

	bool handlePiercingPower(const string& str) {
		piercingPower_at_100 = atof(str.c_str());
		piercingPower_at_max = atof(str.c_str() + str.find(" ") + 1);
		return true;
	}

	string userString;
	string icon;
	string kind;
	int price;
	bool isPremium;
	double caliber;
	bool isTracer;
	double explosionRadius;
	double piercingPowerLossFactorByDistance;
	Damage damage;
	bool notInShop;
	// From shot!
	double defaultPortion;
	int speed;
	double gravity;
	int maxDistance;
	double piercingPower_at_100;
	double piercingPower_at_max;
};

static struct ShellsList : Element {
	void ParseTag() override {
		Shell shell;
		Ignore("icons") ||
		Case(shell) && (list[shell.label] = shell, 1);
	}

	map<string, Shell> list;
} shellsList;

//==============================================================================
// Helpers
//==============================================================================

struct Unlocks : Element {
	struct Unlock : Element {
		enum Kind { VEHICLE, GUN, TURRET, ENGINE, CHASSIS, RADIO };
		void ParseSelf() override {
			if (!strcmp(tag, "vehicle")) kind = VEHICLE;
			if (!strcmp(tag, "gun")) kind = GUN;
			if (!strcmp(tag, "turret")) kind = TURRET;
			if (!strcmp(tag, "engine")) kind = ENGINE;
			if (!strcmp(tag, "chassis")) kind = CHASSIS;
			if (!strcmp(tag, "radio")) kind = RADIO;
			Case(target) || Fail();
		}

		void ParseTag() override {
			Case(cost, "cost") || Fail();
		}

		string target;
		Kind kind;
		double cost; // should be int, but they don't sanatize their data
	};

	void ParseTag() override {
		list.push_back(Unlock());
		Case(list.back()) || Fail();
	}

	vector<Unlock> list;
};

struct Component : Element {
	Component() : notInShop(false), weight(0) {}

	bool ParseComponent() {
		return
		Case(userString, "userString") ||
		Case(shortUserString, "shortUserString") ||
		Case(Element(), "tags") ||
		Case(tags, "tags") ||
		Case(level, "level") ||
		Case(price, "price") ||
		Case(notInShop, "notInShop") ||
		Case(unlocks, "unlocks") ||
		Case(weight, "weight") ||
		Case(maxHealth, "maxHealth") ||
		Case(maxRegenHealth, "maxRegenHealth") ||
		Case(repairCost, "repairCost") ||
		Ignore("camouflage") ||
		Ignore("emblemSlots") ||
		Fail();
	}

	string userString;
	string shortUserString;
	string tags;
	double level; // should be int but...
	double price; // should be int but...
	bool notInShop;
	Unlocks unlocks;
	double weight; // should be int but...
	double maxHealth;
	double maxRegenHealth; // apparently fractional health is a thing
	double repairCost;
};

//==============================================================================
// Radios
//==============================================================================

struct Radio : Component {
	void ParseSelf() override {
		string shared;
		Case(shared) && (shared == "" || shared == "shared") || Fail();
	}

	void ParseTag() override {
		Case(distance, "distance") ||
		ParseComponent();
	}

	int distance;
};

static struct RadiosList : Element {
	void ParseTag() override {
		Radio radio;
		Ignore("ids") ||
		Case(*this, "shared") ||
		Case(radio) && (list[radio.label] = radio, 1);
	}

	map<string, Radio> list;
} radiosList;

//==============================================================================
// Engines
//==============================================================================

struct Engine : Component {
	void ParseSelf() override {
		string shared;
		Case(shared) && (shared == "" || shared == "shared") || Fail();
	}

	void ParseTag() override {
		Case(sound, "sound") ||
		Case(power, "power") ||
		Case(fireStartingChance,"fireStartingChance") ||
		ParseComponent();
	}

	string sound;
	int power;
	double fireStartingChance;
};

static struct EnginesList : Element {
	void ParseTag() override {
		Engine engine;
		Ignore("ids") ||
		Case(*this, "shared") ||
		Case(engine) && (list[engine.label] = engine, 1);
	}

	map<string, Engine> list;
} enginesList;

//==============================================================================
// FuelTanks
//==============================================================================

struct FuelTank : Element {
	void ParseSelf() override {
		string shared;
		Case(shared) && (shared == "" || shared == "shared") || Fail();
	}

	void ParseTag() override {
		Case(tags, "tags") ||
		Case(price, "price") ||
		Case(weight, "weight") ||
		Case(maxHealth, "maxHealth") ||
		Case(maxRegenHealth, "maxRegenHealth") ||
		Case(repairCost, "repairCost") ||
		Fail();
	}

	string tags;
	int price;
	double weight;
	int maxHealth;
	int maxRegenHealth;
	double repairCost;
};

static struct FuelTanksList : Element {
	void ParseTag() override {
		FuelTank fuelTank;
		Ignore("ids") ||
		Case(*this, "shared") ||
		Case(fuelTank) && (list[fuelTank.label] = fuelTank, 1);
	}

	map<string, FuelTank> list;
} fuelTanksList;

//==============================================================================
// Guns
//==============================================================================

struct Gun : Component {
	struct Recoil : Element {
		void ParseTag() override {
			Case(lodDist, "lodDist") ||
			Case(amplitude, "amplitude") ||
			Case(backoffTime, "backoffTime") ||
			Case(returnTime, "returnTime") ||
			Fail();
		}
		string lodDist;
		double amplitude;
		double backoffTime;
		double returnTime;
	};

	struct ClipBurst : Element {
		ClipBurst() : count(0), rate(0) {}
		void ParseTag() override {
			Case(count, "count") ||
			Case(rate, "rate") ||
			Fail();
		}
		int count;
		int rate;
	};

	struct ShotDispersionFactors : Element {
		void ParseTag() override {
			Case(turretRotation, "turretRotation") ||
 			Case(afterShot, "afterShot") ||
			Case(whileGunDamaged, "whileGunDamaged") ||
			Fail();
		}
		double turretRotation;
		double afterShot;
		double whileGunDamaged;
	};

	struct Shots : Element {
		void ParseTag() override {
			list.push_back(shellsList.list.find(tag)->second);
			Case(list.back()) || Fail();
		}
		vector<Shell> list;
	};

	struct ExtraPitchLimits : Element {
		void ParseTag() override {
			Case(front, "front") ||
			Case(back, "back") ||
			Case(transition, "transition") ||
			Fail();
		}

		string front;
		string back;
		double transition;
	};

	void ParseSelf() override {
		string shared;
		Case(shared) && (shared == "" || shared == "shared") || Fail();
	}

	void ParseTag() override {
		Case(impulse, "impulse") ||
		Case(recoil, "recoil") ||
		Case(effects, "effects") ||
		Case(pitchLimits, "pitchLimits") ||
		Case(turretYawLimits, "turretYawLimits") ||
		Case(rotationSpeed, "rotationSpeed") ||
		Case(reloadTime, "reloadTime") ||
		Case(maxAmmo, "maxAmmo") ||
		Case(aimingTime, "aimingTime") ||
		Case(clip, "clip") ||
		Case(burst , "burst") ||
		Case(shotDispersionRadius, "shotDispersionRadius") ||
		Case(shotDispersionFactors, "shotDispersionFactors") ||
		Case(shots, "shots") ||
		Ignore("groundWave") ||
		Ignore("models") ||
		Ignore("hitTester") ||
		Ignore("armor") || //< both gun and mantlet armor
		Case(extraPitchLimits, "extraPitchLimits") ||
		ParseComponent();
	}

	double impulse;
	Recoil recoil;
	string effects;
	string pitchLimits;
	string turretYawLimits;
	double rotationSpeed;
	double reloadTime;
	int maxAmmo;
	double aimingTime;
	ClipBurst clip;
	ClipBurst burst;
	double shotDispersionRadius;
	ShotDispersionFactors shotDispersionFactors;
	Shots shots;
	ExtraPitchLimits extraPitchLimits;
};

static struct GunsList : Element {
	void ParseTag() override {
		Gun gun;
		Ignore("ids") ||
		Case(*this, "shared") ||
		Case(gun) && (list[gun.label] = gun, 1);

	}
	map<string, Gun> list;
} gunsList;

//==============================================================================
// Tanks
//==============================================================================

struct ArmorValue : Element {
	ArmorValue() : value(-1.0), vehicleDamageFactor(1.0) {}
	void ParseSelf() override { Case(value) || Fail(); }
	void ParseTag() override { Case(vehicleDamageFactor, "vehicleDamageFactor") || Fail(); }
	double value;
	double vehicleDamageFactor;
};

struct ArmorSet : Element {
	void ParseTag() override {
		Case(armor[0].value, "armor_1") || Case(armor[0], "armor_1") ||
		Case(armor[1].value, "armor_2") || Case(armor[1], "armor_2") ||
		Case(armor[2].value, "armor_3") || Case(armor[2], "armor_3") ||
		Case(armor[3].value, "armor_4") || Case(armor[3], "armor_4") ||
		Case(armor[4].value, "armor_5") || Case(armor[4], "armor_5") ||
		Case(armor[5].value, "armor_6") || Case(armor[5], "armor_6") ||
		Case(armor[6].value, "armor_7") || Case(armor[6], "armor_7") ||
		Case(armor[7].value, "armor_8") || Case(armor[7], "armor_8") ||
		Case(armor[8].value, "armor_9") || Case(armor[8], "armor_9") ||
		Case(armor[9].value, "armor_10") || Case(armor[9], "armor_10") ||
		Case(armor[10].value, "armor_11") || Case(armor[10], "armor_11") ||
		Case(armor[11].value, "armor_12") || Case(armor[11], "armor_12") ||
		Case(armor[12].value, "armor_13") || Case(armor[12], "armor_13") ||
		Case(armor[13].value, "armor_14") || Case(armor[13], "armor_14") ||
		Case(armor[14].value, "armor_15") || Case(armor[14], "armor_15") ||
		Case(armor[15].value, "armor_16") || Case(armor[15], "armor_16") ||
		Case(surveyingDevice, "surveyingDevice") ||
		Ignore("commander") ||
		Ignore("gunner_1") ||
		Ignore("gunner_2") ||
		Ignore("driver") ||
		Ignore("loader_1") ||
		Ignore("loader_2") ||
		Ignore("radioman_1") ||
		Ignore("radioman_2") ||
		Fail();
	}
	ArmorValue armor[16];
	double surveyingDevice;
};

struct DeviceHealth : Element {
	DeviceHealth(double chanceToHit) : chanceToHit(chanceToHit) {}
	void ParseTag() override {
		Case(maxHealth, "maxHealth") ||
		Case(maxRegenHealth, "maxRegenHealth") ||
		Case(repairCost, "repairCost") ||
		Case(chanceToHit, "chanceToHit") ||
		Fail();
	}
	double maxHealth;
	double maxRegenHealth;
	double repairCost;
	double chanceToHit;
};

struct Turret : Component {
	Turret() : invisibilityFactor(-1.0f), turretRotatorHealth(.45), surveyingDeviceHealth(.45) {}

	struct Guns : Element {
		void ParseTag() override {
			list.push_back(gunsList.list.find(tag)->second);
			Case(list.back()) || Fail();
		}

		vector<Gun> list;
	};

	void ParseTag() override {
		Ignore("icon") ||
		Ignore("models") ||
		Ignore("hitTester") ||
		Ignore("gunPosition") ||
		Case(armor, "armor") ||
		Case(primaryArmor, "primaryArmor") ||
		Case(rotationSpeed, "rotationSpeed") ||
		Case(turretRotatorHealth, "turretRotatorHealth") ||
		Case(circularVisionRadius, "circularVisionRadius") ||
		Case(surveyingDeviceHealth, "surveyingDeviceHealth") ||
		Case(guns, "guns") ||
		Ignore("emblemSlots") ||
		Ignore("showEmblemsOnGun") ||
		Ignore("physicsShape") ||
		Ignore("turretRotatorSoundManual") ||
		Ignore("turretRotatorSoundGear") ||
		Case(invisibilityFactor, "invisibilityFactor") ||
		//Case(yawLimits, "yawLimits") && (printf("<%s>", label.c_str()), 1) ||
		ParseComponent();
	}
	ArmorSet armor;
	string primaryArmor;
	double rotationSpeed;
	DeviceHealth turretRotatorHealth;
	double circularVisionRadius;
	DeviceHealth surveyingDeviceHealth;
	Guns guns;
	//string yawLimits;
	double invisibilityFactor;
};

struct Tanker {
	enum Role { NONE, COMMANDER = 1, GUNNER = 2, DRIVER = 4, RADIOMAN = 8, LOADER = 16 } roles[4];
	void Set(Role primary, const string& extra) {
		roles[0] = primary;
		Role* next = roles + 1;
		for (size_t i = 0, e = extra.length(); i < e; i += 5) { // +5 for separating sequence 13 10 9 9 9
			if (extra.find("gunner", i) == i) { *next++ = GUNNER; i += strlen("gunner"); }
			else if (extra.find("radioman", i) == i) { *next++ = RADIOMAN; i += strlen("radioman"); }
			else if (extra.find("driver", i) == i) { *next++ = DRIVER; i += strlen("driver"); }
			else if (extra.find("loader", i) == i) { *next++ = LOADER; i += strlen("loader"); }
			else {
				fprintf(stderr, "Unparsable role string! : (%s)\n", extra.c_str());
				exit(1);
			}
		}
		while (next < roles + 4)
			*next++ = NONE;
	}
};

struct Chassis : Component {
	struct ChassisArmor: Element {
		void ParseTag() override {
			Case(leftTrack, "leftTrack") ||
			Case(rightTrack, "rightTrack") ||
			Fail();
		}
		int leftTrack;
		int rightTrack;
	};

	struct ShotDispersionFactors : Element {
		void ParseTag() override {
			Case(vehicleMovement, "vehicleMovement") ||
			Case(vehicleRotation, "vehicleRotation") ||
			Fail();
		}
		double vehicleMovement;
		double vehicleRotation;
	};

	void ParseTag() override {
		Ignore("icon") || // rare
		Ignore("models") ||
		Ignore("traces") ||
		Ignore("tracks") ||
		Ignore("wheels") ||
		Ignore("drivingWheels") ||
		Ignore("effects") ||
		Ignore("sound") ||
		Ignore("hullPosition") ||
		Ignore("hitTester") ||
		Case(armor, "armor") ||
		Ignore("topRightCarryingPoint") ||
		Case(maxClimbAngle, "maxClimbAngle") ||
		Case(navmeshGirth, "navmeshGirth") ||
		Case(maxLoad, "maxLoad") ||
		Case(terrainResistance, "terrainResistance") ||
		Case(brakeForce, "brakeForce") ||
		Case(rotationIsAroundCenter, "rotationIsAroundCenter") ||
		Case(rotationSpeed, "rotationSpeed") ||
		Case(shotDispersionFactors, "shotDispersionFactors") ||
		Case(bulkHealthFactor, "bulkHealthFactor") ||
		Ignore("splineDesc") ||
		Ignore("groundNodes") ||
		Ignore("trackThickness") ||
		Ignore("trackNodes") ||
		ParseComponent();
	}
	ChassisArmor armor;
	int maxClimbAngle;
	double navmeshGirth;
	int maxLoad;
	string terrainResistance;
	int brakeForce;
	bool rotationIsAroundCenter;
	double rotationSpeed;
	ShotDispersionFactors shotDispersionFactors;
	double bulkHealthFactor;
};

struct Tank : Element {
	struct Crew : Element {

		vector<Tanker> tankers;
		void ParseTag() override {
			std::string extra;
			Tanker::Role role = Tanker::NONE;
			Case(extra, "commander") ||
			Case(extra, "gunner") ||
			Case(extra, "radioman") ||
			Case(extra, "driver") ||
			Case(extra, "loader") ||
			Fail();
			if (role == Tanker::NONE)
				return;
			tankers.push_back(Tanker());
			tankers.back().Set(role, extra);
		}
	};

	struct SpeedLimits : Element {
		void ParseTag() override {
			Case(forward, "forward") || Case(backward, "backward") || Fail();
		}
		double forward;
		double backward;
	};

	struct Hull : Element {
		Hull() : ammoBayHealth(.27) {}
		void ParseTag() override {
			Ignore("models") ||
			Ignore("swinging") ||
			Ignore("exhaust") ||
			Ignore("fakeTurrets") ||
			Ignore("turretPositions") ||
			Ignore("hitTester") ||
			Case(armor, "armor") ||
			Case(primaryArmor, "primaryArmor") ||
			Case(weight, "weight") ||
			Case(maxHealth, "maxHealth") ||
			Case(ammoBayHealth, "ammoBayHealth") ||
			Ignore("camouflage") ||
			Ignore("emblemSlots") ||
			Fail();
		}
		ArmorSet armor;
		string primaryArmor;
		double weight;
		int maxHealth;
		DeviceHealth ammoBayHealth;
	};

	struct Chassiss : Element {
		void ParseTag() override {
			list.push_back(Chassis());
			Case(list.back()) || Fail();
		}
		vector<Chassis> list;
	};

	struct Turrets : Element {
		void ParseTag() override {
			list.push_back(Turret());
			Case(list.back()) || Fail();
		}
		vector<Turret> list;
	};

	struct Engines : Element {
		void ParseTag() override {
			list.push_back(enginesList.list.find(tag)->second);
			Case(list.back()) ||
			Case(string()) ||
			Fail();
		}

		vector<Engine> list;
	};

	struct FuelTanks : Element {
		void ParseTag() override {
			list.push_back(fuelTanksList.list.find(tag)->second);
			Case(list.back()) ||
			Case(string()) ||
			Fail();
		}

		vector<FuelTank> list;
	};

	struct Radios : Element {
		void ParseTag() override {
			list.push_back(radiosList.list.find(tag)->second);
			Case(list.back()) ||
			Case(string()) ||
			Fail();
		}

		vector<Radio> list;
	};

	struct Price : Element {
		Price(Tank& tank) : tank(tank) {}
		void ParseSelf() override { int i; Case(i) && (tank.price = i, 1) || Fail(); }
		void ParseTag() override {
			Ignore("gold") && (tank.premium = true, 1) || Fail();
		}
		Tank& tank;
	};

	Tank() : notInShop(false), premium(false) {}

	void ParseTag() override {
		Ignore("id") ||
		Case(userString, "userString") ||
		Case(userString, "shortUserString") ||
		Case(description, "description") ||
		Case(price, "price") ||
		Case(Price(*this), "price") ||
		Case(notInShop, "notInShop") ||
		Case(tags, "tags") ||
		Case(level, "level") ||
		Case(crew, "crew") ||
		Case(speedLimits, "speedLimits") ||
		Case(repairCost, "repairCost") ||
		Case(crewXpFactor, "crewXpFactor") ||
		Ignore("effects") ||
		Ignore("camouflage") ||
		Ignore("emblems") ||
		Ignore("horns") ||
		Case(hull, "hull") ||
		Case(chassiss, "chassis") ||
		Case(turrets, "turrets0") ||
		Case(engines, "engines") ||
		Case(fuelTanks, "fuelTanks") ||
		Case(radios, "radios") ||
		Ignore("chassis") ||
		Ignore("turrets0") ||
		Ignore("engines") ||
		Ignore("fuelTanks") ||
		Ignore("radios") ||
		Fail();
	}

	string userString;
	string shortUserString;
	string description;
	int price;
	bool notInShop;
	bool premium;
	string tags;
	int level;
	Crew crew;
	SpeedLimits speedLimits;
	double repairCost;
	double crewXpFactor;
	Hull hull;
	Chassiss chassiss;
	Turrets turrets;
	Engines engines;
	FuelTanks fuelTanks;
	Radios radios;
};

//==============================================================================
// Extra
//==============================================================================

// A helper for handling MS files.
struct FileView {
	void* file;
	void* map;
	const char* data;
	const char* err;
	FileView(const char* name)
		: file(INVALID_HANDLE_VALUE), map(0), data(0), err(0) {
		file = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		if (file == INVALID_HANDLE_VALUE) {
			err = "CreateFileA failed";
			return;
		}
		map = CreateFileMapping(file, 0, PAGE_READONLY, 0, 0, 0);
		if (!map) {
			err = "CreateFileMapping failed";
			return;
		}
		data = (const char*)MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);
		if (!data)
			err = "MapViewOfFile failed";
	}
	~FileView() {
		if (data) UnmapViewOfFile(data);
		if (map) CloseHandle(map);
		if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
	}
	FileView(const FileView&);
	FileView& operator =(const FileView&);
};

void ReadHeader(const char* data) {
	stream = data;
	if (*(int*)stream != 0x62A14E45) {
		fprintf(stderr, "Invalid magic number!");
		exit(1);
	}
	stream += 5; // Read the magic number and a null byte.
	// Read the dictionary.
	dictionary.clear();
	while (*stream) {
		dictionary.push_back(stream);
		stream += strlen(stream) + 1;
	}
	stream++; // Read the dictionary-terminating null byte.
	// Read the root element (and all others recursively).
	kind = TAG_ELEMENT;
}

static struct TanksList : Element {
	void ParseTag() override {
		list.push_back(Tank());
		Case(list.back());
	}

	vector<Tank> list;
} tanksList;

void ReadNation(const string& path) {
	FileView shellsFile((path + "components\\shells.xml").c_str());
	if (shellsFile.err) {
		fprintf(stderr, "File open error: %s\n", shellsFile.err);
		return;
	}
	ReadHeader(shellsFile.data);
	tag = "shells.xml";
	Case(shellsList);

	FileView radiosFile((path + "components\\radios.xml").c_str());
	if (radiosFile.err) {
		fprintf(stderr, "File open error: %s\n", radiosFile.err);
		return;
	}
	ReadHeader(radiosFile.data);
	tag = "radios.xml";
	Case(radiosList);

	FileView enginesFile((path + "components\\engines.xml").c_str());
	if (enginesFile.err) {
		fprintf(stderr, "File open error: %s\n", enginesFile.err);
		return;
	}
	ReadHeader(enginesFile.data);
	tag = "engines.xml";
	Case(enginesList);

	FileView fuelTanksFile((path + "components\\fueltanks.xml").c_str());
	if (fuelTanksFile.err) {
		fprintf(stderr, "File open error: %s\n", fuelTanksFile.err);
		return;
	}
	ReadHeader(fuelTanksFile.data);
	tag = "fueltanks.xml";
	Case(fuelTanksList);

	FileView gunsFile((path + "components\\guns.xml").c_str());
	if (gunsFile.err) {
		fprintf(stderr, "File open error: %s\n", gunsFile.err);
		return;
	}
	ReadHeader(gunsFile.data);
	tag = "guns.xml";
	Case(gunsList);

	FileView tanksFile((path + "list.xml").c_str());
	if (tanksFile.err) {
		fprintf(stderr, "File open error: %s\n", tanksFile.err);
		return;
	}
	ReadHeader(tanksFile.data);
	tag = path.c_str();
	Case(tanksList);
	for (auto i = tanksList.list.begin(), e = tanksList.list.end(); i != e; ++i)
		if (i->nation == nation) {
			FileView tankFile((path + i->label + ".xml").c_str());
			if (tankFile.err) {
				fprintf(stderr, "File open error: %s\n", tankFile.err);
				continue;
			}
			ReadHeader(tankFile.data);
			//printf("parsing : %s\n", i->label.c_str());
			tag = i->label.c_str();
			Case(*i);
		}
}










struct TankInstance {
  enum Skills { COMMANDER = 1, GUNNER = 2, DRIVER = 4, RADIO_OPERATOR = 8, LOADER = 16 };
  struct Device { double health, regenHealth, repairCost, chanceToHit; };
  struct Component : Device {
    string name, tags;
    double tier, price, weight;
  };
  struct Hull : Component { struct { double front, side, back; } armor; } hull;
  struct Chassis : Component {
    struct { double left, right; } armor;
    double maxLoad, movementDispersion;
    struct { double hard, medium, soft; } terrainResistance;
    struct { double forward, backward; } speedLimits;
    struct { double speed, dispersion; bool isCentered; } rotation;
  } chassis;
  struct Turret : Component {
    double rotationSpeed, visionRadius;
    Device rotator, optics;
    struct { double front, side, rear; } armor;
    struct { double left, right; } yawLimits;
  } turret;
  struct Engine : Component { double power, chanceOfFire; } engine;
  struct Radio : Component { double signalRadius; } radio;
  struct Gun : Component {
    double rotationSpeed, reloadTime, ammo, aimTime;
    struct { double size, burst, delay; } magazine;
    struct { double base, movement, shot, damaged; } dispersion;
    struct { struct { double up, down; } basic, front, back; } pitchLimits;
  } gun;
  struct Shell : Component {
    enum Kind { SLOT_UNUSED, AP, APCR, HEAT, HE };
    bool isPremium;
    double caliber, explosionRadius, damage, moduleDamage, speed, gravity, maxRange, penAt100m, penAt720m;
  } shells[3];
  double stockPowerToWeight;
  Device ammoBay;
  Component fuelTank;
  char crew[8];
  bool isPremium;
  Nation nation;

  TankInstance(const Tank& h, const ::Turret& t, const ::Gun& g) {
    const ::FuelTank& f = h.fuelTanks.list.back();
    const ::Chassis& c = h.chassiss.list.back();
    const ::Engine& e = h.engines.list.back();
    const ::Radio& r = h.radios.list.back();
    hull.health = h.hull.maxHealth;
    hull.regenHealth = 0;
    hull.repairCost = h.repairCost;
    hull.chanceToHit = 1;
    hull.name = h.label;
    hull.tags = h.tags;
    hull.tier = h.level;
    hull.price = h.price;
    hull.weight = h.hull.weight;
    hull.armor.front = 0; // FIXME
    hull.armor.side = 0; // FIXME
    hull.armor.back = 0; // FIXME
    chassis.health = c.maxHealth;
    chassis.regenHealth = c.maxRegenHealth;
    chassis.repairCost = c.repairCost;
    chassis.chanceToHit = 1;
    chassis.name = c.label;
    chassis.tags = c.tags;
    chassis.tier = c.level;
    chassis.price = c.price;
    chassis.weight = c.weight;
    chassis.armor.left = c.armor.leftTrack;
    chassis.armor.right = c.armor.rightTrack;
    chassis.maxLoad = c.maxLoad;
    chassis.movementDispersion = c.shotDispersionFactors.vehicleMovement;
    chassis.terrainResistance.hard = ParseNums(c.terrainResistance.c_str(), 0);
    chassis.terrainResistance.medium = ParseNums(c.terrainResistance.c_str(), 1);
    chassis.terrainResistance.soft = ParseNums(c.terrainResistance.c_str(), 2);
    chassis.speedLimits.forward = h.speedLimits.forward;
    chassis.speedLimits.backward = h.speedLimits.backward;
    chassis.rotation.speed = c.rotationSpeed;
    chassis.rotation.dispersion = c.shotDispersionFactors.vehicleRotation;
    chassis.rotation.isCentered = c.rotationIsAroundCenter;
    turret.health = t.maxHealth;
    turret.regenHealth = 0;
    turret.repairCost = t.repairCost;
    turret.chanceToHit = 1;
    turret.name = t.label;
    turret.tags = t.tags;
    turret.tier = t.level;
    turret.price = t.price;
    turret.weight = t.weight;
    turret.rotationSpeed = t.rotationSpeed;
    turret.visionRadius = t.circularVisionRadius;
    turret.rotator.health = t.turretRotatorHealth.maxHealth;
    turret.rotator.regenHealth = t.turretRotatorHealth.maxRegenHealth;
    turret.rotator.repairCost = t.turretRotatorHealth.repairCost;
    turret.rotator.chanceToHit = t.turretRotatorHealth.chanceToHit;
    turret.optics.health = t.surveyingDeviceHealth.maxHealth;
    turret.optics.regenHealth = t.surveyingDeviceHealth.maxRegenHealth;
    turret.optics.repairCost = t.surveyingDeviceHealth.repairCost;
    turret.optics.chanceToHit = t.surveyingDeviceHealth.chanceToHit;
    turret.armor.front = 0; // FIXME
    turret.armor.side = 0; // FIXME
    turret.armor.rear = 0; // FIXME
    turret.yawLimits.left = ParseNums(g.turretYawLimits.c_str(), 0);
    turret.yawLimits.right = ParseNums(g.turretYawLimits.c_str(), 1);
    engine.health = e.maxHealth;
    engine.regenHealth = e.maxRegenHealth;
    engine.repairCost = e.repairCost;
    engine.chanceToHit = .45;
    engine.name = e.label;
    engine.tags = e.tags;
    engine.tier = e.level;
    engine.price = e.price;
    engine.weight = e.weight;
    engine.power = e.power;
    engine.chanceOfFire = e.fireStartingChance;
    radio.health = r.maxHealth;
    radio.regenHealth = r.maxRegenHealth;
    radio.repairCost = r.repairCost;
    radio.chanceToHit = .45;
    radio.name = r.label;
    radio.tags = r.tags;
    radio.tier = r.level;
    radio.price = r.price;
    radio.weight = r.weight;
    radio.signalRadius = r.distance;
    gun.health = g.maxHealth;
    gun.regenHealth = g.maxRegenHealth;
    gun.repairCost = g.repairCost;
    gun.chanceToHit = .33;
    gun.name = g.label;
    gun.tags = g.tags;
    gun.tier = g.level;
    gun.price = g.price;
    gun.weight = g.weight;
    gun.rotationSpeed = g.rotationSpeed;
    gun.reloadTime = g.reloadTime;
    gun.ammo = g.maxAmmo;
    gun.aimTime = g.aimingTime;
    gun.magazine.size = g.clip.count;
    gun.magazine.burst = g.burst.count;
    gun.magazine.delay = 60.0 / g.burst.rate;
    gun.dispersion.base = g.shotDispersionRadius;
    gun.dispersion.movement = g.shotDispersionFactors.turretRotation;
    gun.dispersion.shot = g.shotDispersionFactors.afterShot;
    gun.dispersion.damaged = g.shotDispersionFactors.whileGunDamaged;
    gun.pitchLimits.basic.up = -ParseNums(g.pitchLimits.c_str(), 0);
    gun.pitchLimits.basic.down = ParseNums(g.pitchLimits.c_str(), 1);
    gun.pitchLimits.front.up = g.extraPitchLimits.front.size() ? -ParseNums(g.extraPitchLimits.front.c_str(), 0) : gun.pitchLimits.basic.up;
    gun.pitchLimits.front.down = g.extraPitchLimits.front.size() ? ParseNums(g.extraPitchLimits.front.c_str(), 1) : gun.pitchLimits.basic.down;
    gun.pitchLimits.back.up = g.extraPitchLimits.back.size() ? -ParseNums(g.extraPitchLimits.back.c_str(), 0) : gun.pitchLimits.basic.up;
    gun.pitchLimits.back.down = g.extraPitchLimits.back.size() ? ParseNums(g.extraPitchLimits.back.c_str(), 1) : gun.pitchLimits.basic.down;
    ammoBay.health = h.hull.ammoBayHealth.maxHealth;
    ammoBay.regenHealth = h.hull.ammoBayHealth.maxRegenHealth;
    ammoBay.repairCost = h.hull.ammoBayHealth.repairCost;
    ammoBay.chanceToHit = h.hull.ammoBayHealth.chanceToHit;
    fuelTank.health = f.maxHealth;
    fuelTank.regenHealth = f.maxRegenHealth;
    fuelTank.repairCost = f.repairCost;
    fuelTank.chanceToHit = .45;
    fuelTank.name = f.label;
    fuelTank.tags = f.tags;
    fuelTank.tier = f.maxHealth * 2 / 25 - 6;
    fuelTank.price = f.price;
    fuelTank.weight = f.weight;
    stockPowerToWeight = (double)h.engines.list.begin()->power / (
        h.hull.weight +
        h.chassiss.list.begin()->weight +
        h.fuelTanks.list.begin()->weight +
        h.turrets.list.begin()->weight +
        h.turrets.list.begin()->guns.list.begin()->weight +
        h.engines.list.begin()->weight +
        h.radios.list.begin()->weight);
    for (size_t tanker = 0; tanker < 8; tanker++) {
      crew[tanker] = 0;
      for (size_t role = 0; role < 4; role++)
        crew[tanker] |= h.crew.tankers[tanker].roles[role];
    }
    isPremium = h.premium;
    nation = h.nation;
  }
};





struct MeasuredShell : Shell {
	enum Kind { HE, AP, APCR, HEAT } kind_enum;
	MeasuredShell(
		const Shell& shell,
		double key = 0.0,
		const Tank* tank = 0,
		const Turret* turret = 0,
		const Gun* gun = 0) : Shell(shell), key(key), tank(tank), turret(turret), gun(gun) {
		if (kind == "HIGH_EXPLOSIVE") {
			kind_enum = HE;
			tier = pow(pow(getTier(damage.armor * 0.175), 2.5) + pow(getTier(piercingPower_at_100), 2.5), 1.0/2.5);
		} else if (kind == "ARMOR_PIERCING") {
			kind_enum = AP;
			tier = getTier(piercingPower_at_100);
		} else if (kind == "ARMOR_PIERCING_CR") {
			kind_enum = APCR;
			tier = getTier(piercingPower_at_100);
		} else if (kind == "HOLLOW_CHARGE") {
			kind_enum = HEAT;
			tier = getTier(piercingPower_at_100 * 0.96);
		}
		cost_per_damage = price / (double)damage.armor;
	}

	static double getPen(double x) {
		return (23.0 - (3.0 - 0.47*x)*x)*x;
	}

	static double getTier(double a) {
		auto x = 0.025*a + 1;
		x -= (getPen(x) - a)/(23 - 6*x + 1.41*x*x);
		x -= (getPen(x) - a)/(23 - 6*x + 1.41*x*x);
		x -= (getPen(x) - a)/(23 - 6*x + 1.41*x*x);
		x -= (getPen(x) - a)/(23 - 6*x + 1.41*x*x);
		return x;
	}

	double tier;
	double cost_per_damage;
	double key;
	const Tank* tank;
	const Turret* turret;
	const Gun* gun;
	bool operator <(const MeasuredShell& a) const { return key < a.key || key == a.key && label < a.label; }
	bool operator ==(const MeasuredShell& a) const { return key == a.key && label == a.label; }
};

//static double getTier(double a) {
//	auto x = 0.025*a + 1;
//	x -= (getPen(x) - a)/(23 - 6*x + 1.41*x*x);
//	x -= (getPen(x) - a)/(23 - 6*x + 1.41*x*x);
//	x -= (getPen(x) - a)/(23 - 6*x + 1.41*x*x);
//	x -= (getPen(x) - a)/(23 - 6*x + 1.41*x*x);
//	return x;
//}

struct MeasuredGun : Gun {
	MeasuredGun(
		const Gun& gun,
		const Tank* tank,
		const Turret* turret,
		double key = 0) : Gun(gun), key(key), tank(tank), turret(turret) {
		double armor_tier = tank->level;
	}

	static double getPen(double x) {
		x = min(max(x, .5), 10);
		return (23.0 - (3.0 - 0.47*x)*x)*x;
	}

	static double getHP(double x) {
		x = min(max(x, .8), 10);
		return (80.0 - (4.0 - 2.0*x)*x)*x;
	}

	static double getEffectiveDamage(double hp_tier, double armor_tier, const Shell* shell, double range = 100) {
		double armor_thickness = getPen(armor_tier - 0.8);
		double piercingPower = shell->piercingPower_at_100 + (shell->piercingPower_at_max - shell->piercingPower_at_100) * (max(100, min(shell->maxDistance, range)) - 100) / (shell->maxDistance - 100);
		double x = 1 / (1 + exp(10*(armor_thickness - piercingPower) / piercingPower));
		double damage = x * min((double)shell->damage.armor, getHP(hp_tier) * 0.8);
		if (shell->kind  == "HIGH_EXPLOSIVE")
			damage += (1 - x) * min(getHP(hp_tier) * 0.8, max(0.0, shell->damage.armor * 0.5 - 1.1 * armor_thickness));
		return damage;
	}

	static double getEffectiveDamage(double hp_tier, double armor_tier, const vector<Shell>& list, double range = 100) {
		double damage = 0;
		double ddpcr = 0;
		for (auto i = list.begin(), e = list.end(); i != e; ++i) {
			double temp = getEffectiveDamage(hp_tier, armor_tier, &*i, range);
			double costRatio = temp * temp / i->price;
			damage += temp * costRatio * costRatio;
			ddpcr += costRatio * costRatio;
		}
		return damage / ddpcr;
	}

	double getEffectiveDamage(double range = 100, double time = 0, double armor_tier_delta = 0) {
		struct { double p, hp_tier, armor_tier; } target[] = {
			{ 0.05, tank->level * 0.5, tank->level + armor_tier_delta - 8 }, // arty
			{ 0.06, tank->level - 2, tank->level + armor_tier_delta - 4 }, // scouts
			{ 0.09, tank->level - 2, tank->level + armor_tier_delta - 2 },
			{ 0.20, tank->level - 1, tank->level + armor_tier_delta - 1 },
			{ 0.30, tank->level    , tank->level + armor_tier_delta     },
			{ 0.20, tank->level + 1, tank->level + armor_tier_delta + 1 },
			{ 0.10, tank->level + 2, tank->level + armor_tier_delta + 2 },
		};
		if (tank->level == 1) {
			target[0].p = target[1].p = target[2].p = target[3].p = target[6].p = 0;
			target[4].p = .75;
			target[5].p = .25;
		} else if (tank->level == 2) {
			target[1].p = target[2].p = target[6].p = 0;
			target[3].p = .20;
			target[4].p = .5;
			target[5].p = .25;
		} else if (tank->level == 3) {
			target[1].p = target[2].p = 0;
			target[5].p = .25;
			target[6].p = .15;
		} else if (tank->level == 4) {
			target[1].p = target[2].p = 0;
			target[4].p = .35;
			target[5].p = .25;
		}

		double effDamage = 0; 
		for (size_t i = 0; i < 7; i++)
			effDamage += target[i].p * getEffectiveDamage(target[i].hp_tier, target[i].armor_tier, shots.list, range);
		if (!time)
			return effDamage;
		double damage = 0;
		if (clip.count)
			for (int burst = 0; burst < 10; burst++)
				for (int i = 0; i < clip.count; i++)
					damage += effDamage / (1 + exp((6.0 / time) * (i * 60.0 / clip.rate + burst * reloadTime - time)));
		else
			for (int i = 0; i < 100; i++)
				damage += effDamage / (1 + exp(i * reloadTime - time));
		return damage;
	}

	double getEffReloadTime() {
		return clip.count
			? ((clip.count - 1) * (60.0 / clip.rate) + reloadTime) / clip.count
			: reloadTime;
	}

	double getEffectiveDPM(double range = 100, double armor_tier_delta = 0) {
		return getEffectiveDamage(range, 0.0, armor_tier_delta) * 60.0 / getEffReloadTime();
	}

	double getIdealTimeAtAngle(double angle) {
		//return 
	}

	double key;
	const Tank* tank;
	const Turret* turret;
	bool operator <(const MeasuredGun& a) const { return key < a.key || key == a.key && label < a.label; }
	bool operator ==(const MeasuredGun& a) const { return key == a.key && label == a.label; }
};

double shotsTier(const vector<Shell>& shots) {
	double total = 0;
	double size = 0;
	for (auto i = shots.begin(), e = shots.end(); i != e; ++i) {
		if (i->isPremium)
			continue;
		total += pow(MeasuredShell(*i).tier, 2.0);
		size += 1.0f;
	}
	return pow(total * (1.0 / pow(size, 0.25)), 1.0/2.0);
}










int main(int argc, char* argv[]) {
	string path = "C:\\Games\\World_of_Tanks\\res\\scripts\\item_defs\\vehicles\\";
	char buffer[512];
	long size = sizeof(buffer);
	if (RegQueryValue(HKEY_CLASSES_ROOT, "Applications\\WOTLauncher.exe\\shell\\open\\command\\", buffer, &size) == ERROR_SUCCESS) {
		auto begin = strstr(buffer, ":\\");
		auto end = strstr(buffer, "WOTLauncher.exe");
		if (begin != 0 || end != 0)
			path = string(begin - 1, end) + "res\\scripts\\item_defs\\vehicles\\";
	}
	static const char* const nations[] = { "china", "france", "germany", "japan", "uk", "usa", "ussr" };
	for (nation = CHINA; nation <= USSR; nation = (Nation)(nation + 1)) {
		//printf("%s\n", nations[nation]);
		ReadNation((path + nations[nation] + "\\").c_str());
		//break;
	}

#if 0



#if defined(GROVER)
{
const char * fmtH = "%45s  |  %20s %10s %10s %10s %10s %10s %10s %10s %10
 const char * fmtR = "%45s  |  %20s %10d %10d %10.1f %10.1f %10d %10d %10d
 printf(fmtH, "Shell", "Kind", "Price", "Premium", "ExpRadius", "PierceFal
 for (auto i = shellsList.list.begin(), e = shellsList.list.end(); i != e;
printf(fmtR
 , i->second.userString.c_str()
 , i->second.kind.c_str()
 , i->second.price
 , (int)i->second.isPremium
 , (float)i->second.explosionRadius
 , (float)i->second.piercingPowerLossFactorByDistance
 , i->second.damage.armor
 , i->second.speed
 , i->second.maxDistance
 , (float)i->second.piercingPower_at_100
 , (float)i->second.piercingPower_at_max
 );
}

}
printf("\n");
{
const char * fmtH = "%45s  |  %10s   %10s   %10s   %10s   %10s   %10s   %
 const char * fmtR = "%45s  |  %10.1f %10.1f %10.1f %10.1f %10.1f %10.1f %
 printf(fmtH, "Gun", "Impulse", "Recoil.amplitude", "Recoil.backoffTime",
for (auto k = gunsList.list.begin(), g = gunsList.list.end(); k != g; ++k
 printf(fmtR
 , k->second.label.c_str()
 , k->second.impulse
 , k->second.recoil.amplitude
 , k->second.recoil.backoffTime
 , k->second.recoil.returnTime
 , k->second.rotationSpeed
 , k->second.reloadTime
 , k->second.maxAmmo
 , k->second.aimingTime
 , k->second.shotDispersionRadius
 );
}
printf("\n");
{

cout.precision(1);
cout
 << setw(50) << "Tank |"
 << setw(10) << "Price"
 << setw(10) << "Premium"
 << setw(10) << "Level"
 << setw(14) << "SpeedLimits"
 << setw(12) << "RepairCost"
 << setw(25) << "Hull.Armor.primaryArmor"
 << setw(10) << "Hull.Weight"
 << setw(16) << "Hull.MaxHealth"
 << std::endl;

for (auto k = tanksList.list.begin(); k != tanksList.list.end(); ++k)
 cout
 << setw(50) << k->userString
 << setw(10) << k->price
 << setw(10) << k->premium
 << setw(10) << k->level
 << setw(14) << k->speedLimits.forward
 << setw(12) << k->repairCost
 << setw(25) << k->hull.primaryArmor
 << setw(10) << k->hull.weight
 << setw(16) << k->hull.maxHealth
 << std::endl;
}
#endif





#if 0
 struct ShellTest {
double key;
Shell* value;
ShellTest(double key, Shell* value) : key(key), value(value) {}
bool operator <(const ShellTest& x) { return key > x.key; }

};
vector<ShellTest> shellTest;
for (auto i = shellsList.list.begin(), e = shellsList.list.end(); i != e
 if (!i->second.notInShop && i->second.premium && i->second.kind
 shellTest.push_back(ShellTest(i->second.damage.armor / (
}
std::sort(shellTest.begin(), shellTest.end());
for (int i = 0; i < 80 && i < shellTest.size(); i++)
 printf("%28s : %-17s : %-10g : %-4d : %-4d\n", shellTest[i].valu
 #endif

#if 0
 struct ShellTest {
double key;
Shell* value;
ShellTest(double key, Shell* value) : key(key), value(value) {}
bool operator <(const ShellTest& x) { return key > x.key; }

  };
vector<ShellTest> shellTest;
for (auto i = tanksList.list.begin(), e = tanksList.list.end(); i != e;
for (auto j = i->turrets.list.begin(), f = i->turrets.list.end()
 for (auto k = j->guns.list.begin(), g = j->guns.list.end
 for (auto l = k->shots.list.begin(), h = k->shot
 if (!l->notInShop && l->kind == "HIGH_EX
                                               //printf("%d : %g\n", l->damage.
shellTest.push_back(ShellTest(l -
}
std::sort(shellTest.begin(), shellTest.end());
for (int i = 0; i < 250 && i < shellTest.size(); i++)
 printf("%28s : %-17s : %-10g\n", shellTest[i].value->label.c_str
 #endif

#if 0
 struct ShellTest {
double key;
Shell* value;
Tank* tank;
ShellTest(double key, Shell* value, Tank* tank) : key(key), valu
 bool operator <(const ShellTest& x) { return key < x.key; }

    };
vector<ShellTest> shellTest;
for (auto i = tanksList.list.begin(), e = tanksList.list.end(); i != e;
for (auto k = i->turrets.list.back().guns.list.begin(), g = i->t
 for (auto l = k->shots.list.begin(), h = k->shots.list.e
 if (i->notInShop)
 continue;
if (l->kind == "HIGH_EXPLOSIVE")
 shellTest.push_back(ShellTest(HETier(ato
 else if (l->kind == "ARMOR_PIERCING")
 shellTest.push_back(ShellTest(APTier(ato
 else if (l->kind == "ARMOR_PIERCING_CR")
 shellTest.push_back(ShellTest(APTier(ato
 else if (l->kind == "HOLLOW_CHARGE")
 shellTest.push_back(ShellTest(APTier(ato
 else
 printf("%s\n", l->kind.c_str());
}
std::sort(shellTest.begin(), shellTest.end());
int printed = 0;
for (int i = 0; i < shellTest.size(); i++) {
if (i != 0 && shellTest[i].value->label == shellTest[i - 1].value -
continue;
if (printed++ % 5 && shellTest[i].value->label.find("HESH") == s
 continue;
printf("%20s : %-17s : %-10g\n", shellTest[i].value->label.c_str
               //printf("%20s : %-17s : %-17s : %-10g\n", shellTest[i].value->l

    }
#endif

#if defined(BASIC_SHELL_TEST) || 0
 struct ShellTest {
double key;
Shell* value;
Tank* tank;
ShellTest(double key, Shell* value, Tank* tank) : key(key), valu
 bool operator <(const ShellTest& x) {
            return key > x.key || key

        };
vector<ShellTest> shellTest;
for (auto i = tanksList.list.begin(), e = tanksList.list.end(); i != e;
if (!i->notInShop)
 for (auto k = i->turrets.list.back().guns.list.begin(), g = i->t
 for (auto l = k->shots.list.begin(), h = k->shots.list.e
 if (!l->isPremium)
 continue;
                               //auto t = l->speed * sqrt(0.5) / l->gravity;
                               //auto x = l->speed * sqrt(0.5) * t;
                               //auto x = l->speed * l->speed / l->gravity;
                               //printf("%d %g\n", l->maxDistance, x);
auto x = MeasuredShell(*l).cost_per_damage;
if (x == 0)
 continue;
shellTest.push_back(ShellTest(x, &*l, &*i));

    }
std::sort(shellTest.begin(), shellTest.end());
int printed = 0;
for (int i = 0; i < shellTest.size(); i++) {
if (i != 0 && shellTest[i].value->label == shellTest[i - 1].value -
continue;
               //if (printed++ % 5 && shellTest[i].value->label.find("HESH") ==
               //      continue;
               //printf("%20s : %-17s : %-10g\n", shellTest[i].value->label.c_s
printf("%20s : %-17s : %-17s : %-10g\n", shellTest[i].value->lab

    }
#endif

#if 0
 struct TankTest {
double key;
Gun* value;
Tank* tank;
TankTest(double key, Gun* value, Tank* tank) : key(key), value(v
 bool operator <(const TankTest& x) { return key > x.key; }

    };
vector<TankTest> tankTest;
for (auto i = tanksList.list.begin(), e = tanksList.list.end(); i != e;
for (auto k = i->turrets.list.back().guns.list.begin(), g = i->t
 if (i->notInShop)
 continue;
tankTest.push_back(TankTest(shotsTier(k->shots.l
 }
std::sort(tankTest.begin(), tankTest.end());
int printed = 0;
for (int i = 0; i < tankTest.size() && i < 400; i++) {
if (i != 0 && tankTest[i].value->label == tankTest[i - 1].value->l
 continue;
               //if (printed++ % 3)
               //      continue;
printf("%20s : %20s : %-10g\n", tankTest[i].tank->label.c_str(),
               //printf("%20s : %-17s : %-17s : %-10g\n", shellTest[i].value->l

      }
#endif

#if 1
 struct GunTest {
double key;
Gun* value;
Tank* tank;
GunTest(double key, Gun* value, Tank* tank) : key(key), value(va
 bool operator <(const GunTest& x) { return key > x.key; }

      };
vector<GunTest> gunTest;
for (auto i = tanksList.list.begin(), e = tanksList.list.end(); i != e;
for (auto j = i->turrets.list.begin(), f = i->turrets.list.end()
 for (auto k = j->guns.list.begin(), g = j->guns.list.end
 if (!i->notInShop && !k->burst.count) {
double r = k->clip.count ? 60.0 / k->cli
 double t = k->shotDispersionFactors.afte
 t = log(sqrt(1.0 + t*t)) * k->aimingTime
 if (t > r)
 gunTest.push_back(GunTest(t / r,

      }
std::sort(gunTest.begin(), gunTest.end());
for (int i = 0; i < 200 && i < gunTest.size(); i++) {
auto& gun = *gunTest[i].value;
printf("%23s : %d : %3g : %-23s : %-6g : %-4g\n", gun.label.c_st

      }
#endif

       //for (auto i = tanksList.list.begin(), e = tanksList.list.end(); i != e
       //      if (!i->notInShop && i->label.find("T-") != string::npos)
       //              for (auto j = i->turrets.list.begin(), f = i->turrets.li
       //                      for (auto k = j->guns.list.begin(), g = j->guns.
       //                              printf("%20s : %-20s\n", k->label.c_str(
       //                              MeasuredGun::EffectiveDamage(i->level, k
       //                      }
       //_getch();
       //return 0;

#if 0
   //defined(GUN_QUALITY_TEST) || 1
vector<MeasuredGun> gunTest;
for (auto i = tanksList.list.begin(), e = tanksList.list.end(); i != e;
if (!i->notInShop)// && i->label.find("GB32") != string::npos)
 for (auto j = i->turrets.list.begin(), f = i->turrets.list.end()
 for (auto k = j->guns.list.begin(), g = j->guns.list.end
 MeasuredGun a(*k, &*i, &*j);
gunTest.push_back(MeasuredGun(*k, &*i, &*j, -a.g
 }
std::sort(gunTest.begin(), gunTest.end());
for (int i = 0; i < 250 && i < gunTest.size(); i++) {
if (i && gunTest[i] == gunTest[i - 1])
 continue;
auto& gun = gunTest[i];
printf("%20s : %-20s : %-5g\n", gun.label.c_str(), gun.tank->lab
               //printf("%20s : %-20s : %-2d - %-2d : %-5g\n", gun.label.c_str(

        }
#endif

       //for (auto i = gunsList.list.begin(), e = gunsList.list.end(); i != e;

#if 0
 for (auto i = tanksList.list.begin(), e = tanksList.list.end(); i != e;
if (i->turrets.list.size() == 0)
 printf("%s\n", i->label.c_str());
else {
printf("%s : %g\n", i->label.c_str(), i->turrets.list[0]
                       //Tank::Turret& t = i->turrets.list[0];
                       //printf("%s\n", i->turrets.list[0].label.c_str());
                       //printf("%s : %f\n", i->turrets.list[0].maxHealth);

        }
               //fprintf(stdout, "%s %d\n", i->label.c_str(), i->radios.list.si

       //auto f = fopen("out.csv", "rb");
       //for (auto i = tanksList.list.begin(), e = tanksList.list.end(); i != e
               //printf("%s\n", i->label ? i->label : "???");
               //fprintf(stdout, "%s %d\n", i->label, 0);//i->turrets.list[0].g
       //}
#endif
#endif


	_getch();
}