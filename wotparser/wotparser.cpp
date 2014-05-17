// WoT Parser 2

#include <string>
#include <vector>
#include <map>
#include <conio.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "tank.h"

using std::string;
using std::vector;
using std::map;
using std::pair;
using std::make_pair;

namespace {
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
Nation current_nation;
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

struct ShellElement : Element {
	struct PriceElement : Element {
		PriceElement(ShellElement& shell) : shell(shell) {}
		void ParseSelf() override { int i; Case(i) && (shell.price = i, 1) || Fail(); }
		void ParseTag() override {
			Ignore("gold") && (shell.price *= 400, shell.isPremium = true) || Fail();
		}
		ShellElement& shell;
	};

	ShellElement() : explosionRadius(0), isPremium(false), piercingPowerLossFactorByDistance(0.0), notInShop(false) {}

	struct DamageElement : Element {
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
		Case(PriceElement(*this), "price") ||
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
	DamageElement damage;
	bool notInShop;
	// From shot!
	double defaultPortion;
	int speed;
	double gravity;
	int maxDistance;
	double piercingPower_at_100;
	double piercingPower_at_max;
};

static struct ShellsListElement : Element {
	void ParseTag() override {
		ShellElement shell;
		Ignore("icons") ||
		Case(shell) && (list[shell.label] = shell, 1);
	}

	map<string, ShellElement> list;
} shellsList;

//==============================================================================
// Helpers
//==============================================================================

struct UnlocksElement : Element {
	struct UnlockElement : Element {
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
		list.push_back(UnlockElement());
		Case(list.back()) || Fail();
	}

	vector<UnlockElement> list;
};

struct ComponentElement : Element {
	ComponentElement() : notInShop(false), weight(0) {}

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
	UnlocksElement unlocks;
	double weight; // should be int but...
	double maxHealth;
	double maxRegenHealth; // apparently fractional health is a thing
	double repairCost;
};

//==============================================================================
// Radios
//==============================================================================

struct RadioElement : ComponentElement {
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

static struct RadiosListElement : Element {
	void ParseTag() override {
		RadioElement radio;
		Ignore("ids") ||
		Case(*this, "shared") ||
		Case(radio) && (list[radio.label] = radio, 1);
	}

	map<string, RadioElement> list;
} radiosList;

//==============================================================================
// Engines
//==============================================================================

struct EngineElement : ComponentElement {
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

static struct EnginesListElement : Element {
	void ParseTag() override {
		EngineElement engine;
		Ignore("ids") ||
		Case(*this, "shared") ||
		Case(engine) && (list[engine.label] = engine, 1);
	}

	map<string, EngineElement> list;
} enginesList;

//==============================================================================
// FuelTanks
//==============================================================================

struct FuelTankElement : Element {
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
		FuelTankElement fuelTank;
		Ignore("ids") ||
		Case(*this, "shared") ||
		Case(fuelTank) && (list[fuelTank.label] = fuelTank, 1);
	}

	map<string, FuelTankElement> list;
} fuelTanksList;

//==============================================================================
// Guns
//==============================================================================

struct GunElement : ComponentElement {
	struct RecoilElement : Element {
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

	struct ClipBurstElement : Element {
		ClipBurstElement() : count(0), rate(0) {}
		void ParseTag() override {
			Case(count, "count") ||
			Case(rate, "rate") ||
			Fail();
		}
		int count;
		int rate;
	};

	struct ShotDispersionFactorsElement : Element {
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

	struct ShotsElement : Element {
		void ParseTag() override {
			list.push_back(shellsList.list.find(tag)->second);
			Case(list.back()) || Fail();
		}
		vector<ShellElement> list;
	};

	struct ExtraPitchLimitsElement : Element {
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
	RecoilElement recoil;
	string effects;
	string pitchLimits;
	string turretYawLimits;
	double rotationSpeed;
	double reloadTime;
	int maxAmmo;
	double aimingTime;
	ClipBurstElement clip;
	ClipBurstElement burst;
	double shotDispersionRadius;
	ShotDispersionFactorsElement shotDispersionFactors;
	ShotsElement shots;
	ExtraPitchLimitsElement extraPitchLimits;
};

static struct GunsListElement : Element {
	void ParseTag() override {
		GunElement gun;
		Ignore("ids") ||
		Case(*this, "shared") ||
		Case(gun) && (list[gun.label] = gun, 1);

	}
	map<string, GunElement> list;
} gunsList;

//==============================================================================
// Tanks
//==============================================================================

struct ArmorValueElement : Element {
	ArmorValueElement() : value(-1.0), vehicleDamageFactor(1.0) {}
	void ParseSelf() override { Case(value) || Fail(); }
	void ParseTag() override { Case(vehicleDamageFactor, "vehicleDamageFactor") || Fail(); }
	double value;
	double vehicleDamageFactor;
};

struct ArmorSetElement : Element {
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
	ArmorValueElement armor[16];
	double surveyingDevice;
};

struct DeviceHealthElement : Element {
	DeviceHealthElement(double chanceToHit) : chanceToHit(chanceToHit) {}
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

struct TurretElement : ComponentElement {
	TurretElement() : invisibilityFactor(-1.0f), turretRotatorHealth(.45), surveyingDeviceHealth(.45) {}
	struct GunsElement : Element {
		void ParseTag() override {
			list.push_back(gunsList.list.find(tag)->second);
			Case(list.back()) || Fail();
		}
		vector<GunElement> list;
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
	ArmorSetElement armor;
	string primaryArmor;
	double rotationSpeed;
	DeviceHealthElement turretRotatorHealth;
	double circularVisionRadius;
	DeviceHealthElement surveyingDeviceHealth;
	GunsElement guns;
	//string yawLimits;
	double invisibilityFactor;
};


struct ChassisElement : ComponentElement {
	struct ChassisArmorElement : Element {
		void ParseTag() override {
			Case(leftTrack, "leftTrack") ||
			Case(rightTrack, "rightTrack") ||
			Fail();
		}
		int leftTrack;
		int rightTrack;
	};

	struct ShotDispersionFactorsElement : Element {
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
	ChassisArmorElement armor;
	int maxClimbAngle;
	double navmeshGirth;
	int maxLoad;
	string terrainResistance;
	int brakeForce;
	bool rotationIsAroundCenter;
	double rotationSpeed;
	ShotDispersionFactorsElement shotDispersionFactors;
	double bulkHealthFactor;
};

struct TankElement : Element {
	struct CrewElement : Element {
		vector<char> tankers;
		void ParseTag() override {
			string extra;
			char tanker = 0;
			Case(extra, "commander") && (tanker |= COMMANDER) ||
			Case(extra, "gunner") && (tanker |= GUNNER) ||
			Case(extra, "driver") && (tanker |= DRIVER) ||
			Case(extra, "radioman") && (tanker |= RADIOMAN) ||
			Case(extra, "loader") && (tanker |= LOADER) ||
			Fail();
			for (size_t i = 0, e = extra.length(); i < e;) {
				if (extra.find("gunner", i) == i) { tanker |= ALSO_GUNNER; i += strlen("gunner"); }
				else if (extra.find("driver", i) == i) { tanker |= DRIVER; i += strlen("driver"); } // only Karl...
				else if (extra.find("radioman", i) == i) { tanker |= ALSO_RADIOMAN; i += strlen("radioman"); }
				else if (extra.find("loader", i) == i) { tanker |= ALSO_LOADER; i += strlen("loader"); }
				else {
					fprintf(stderr, "Unparsable role string! : (%s) %d\n", extra.c_str(), (int)i);
					exit(1);
				}
				for (; i < e && extra[i] < 'a'; i++); // consume inconsistent whitespace
			}
			tankers.push_back(tanker);
		}
	};

	struct SpeedLimitsElement : Element {
		void ParseTag() override {
			Case(forward, "forward") || Case(backward, "backward") || Fail();
		}
		double forward;
		double backward;
	};

	struct HullElement : Element {
		HullElement() : ammoBayHealth(.27) {}
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
		ArmorSetElement armor;
		string primaryArmor;
		double weight;
		int maxHealth;
		DeviceHealthElement ammoBayHealth;
	};

	struct ChassissElement : Element {
		void ParseTag() override {
			list.push_back(ChassisElement());
			Case(list.back()) || Fail();
		}
		vector<ChassisElement> list;
	};

	struct TurretsElement : Element {
		void ParseTag() override {
			list.push_back(TurretElement());
			Case(list.back()) || Fail();
		}
		vector<TurretElement> list;
	};

	struct EnginesElement : Element {
		void ParseTag() override {
			list.push_back(enginesList.list.find(tag)->second);
			Case(list.back()) ||
			Case(string()) ||
			Fail();
		}

		vector<EngineElement> list;
	};

	struct FuelTanksElement : Element {
		void ParseTag() override {
			list.push_back(fuelTanksList.list.find(tag)->second);
			Case(list.back()) ||
			Case(string()) ||
			Fail();
		}

		vector<FuelTankElement> list;
	};

	struct RadiosElement : Element {
		void ParseTag() override {
			list.push_back(radiosList.list.find(tag)->second);
			Case(list.back()) ||
			Case(string()) ||
			Fail();
		}

		vector<RadioElement> list;
	};

	struct PriceElement : Element {
		PriceElement(TankElement& tank) : tank(tank) {}
		void ParseSelf() override { int i; Case(i) && (tank.price = i, 1) || Fail(); }
		void ParseTag() override {
			Ignore("gold") && (tank.premium = true, 1) || Fail();
		}
		TankElement& tank;
	};

	TankElement() : notInShop(false), premium(false) {}

	void ParseSelf() override {
		nation = current_nation;
		Element::ParseSelf();
	}

	void ParseTag() override {
		Ignore("id") ||
		Case(userString, "userString") ||
		Case(userString, "shortUserString") ||
		Case(description, "description") ||
		Case(price, "price") ||
		Case(PriceElement(*this), "price") ||
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

	Nation nation;
	string userString;
	string shortUserString;
	string description;
	int price;
	bool notInShop;
	bool premium;
	string tags;
	int level;
	CrewElement crew;
	SpeedLimitsElement speedLimits;
	double repairCost;
	double crewXpFactor;
	HullElement hull;
	ChassissElement chassiss;
	TurretsElement turrets;
	EnginesElement engines;
	FuelTanksElement fuelTanks;
	RadiosElement radios;
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
		list.push_back(TankElement());
		Case(list.back());
	}
	vector<TankElement> list;
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
	for (auto i = tanksList.list.begin(), e = tanksList.list.end(); i != e; ++i) {
		if (i->nation != current_nation)
			continue;
		FileView tankFile((path + i->label + ".xml").c_str());
		if (tankFile.err) {
			fprintf(stderr, "File open error: %s\n", tankFile.err);
			continue;
		}
		ReadHeader(tankFile.data);
		tag = i->label.c_str();
		//printf("%s\n", i->label.c_str());
		Case(*i);
	}
}

void TankInit(Tank& o, const TankElement& h, const TurretElement& t, const GunElement& g) {
	const FuelTankElement& f = h.fuelTanks.list.back();
	const ChassisElement& c = h.chassiss.list.back();
	const EngineElement& e = h.engines.list.back();
	const RadioElement& r = h.radios.list.back();
	o.hull.health = h.hull.maxHealth;
	o.hull.regenHealth = 0;
	o.hull.repairCost = h.repairCost;
	o.hull.chanceToHit = 1;
	o.hull.name = h.label;
	o.hull.tags = h.tags;
	o.hull.tier = h.level;
	o.hull.price = h.price;
	o.hull.weight = h.hull.weight;
	o.hull.armor.front = 0; // FIXME
	o.hull.armor.side = 0; // FIXME
	o.hull.armor.back = 0; // FIXME
	o.chassis.health = c.maxHealth;
	o.chassis.regenHealth = c.maxRegenHealth;
	o.chassis.repairCost = c.repairCost;
	o.chassis.chanceToHit = 1;
	o.chassis.name = c.label;
	o.chassis.tags = c.tags;
	o.chassis.tier = c.level;
	o.chassis.price = c.price;
	o.chassis.weight = c.weight;
	o.chassis.armor.left = c.armor.leftTrack;
	o.chassis.armor.right = c.armor.rightTrack;
	o.chassis.maxLoad = c.maxLoad;
	o.chassis.movementDispersion = c.shotDispersionFactors.vehicleMovement;
	o.chassis.terrainResistance.hard = ParseNums(c.terrainResistance.c_str(), 0);
	o.chassis.terrainResistance.medium = ParseNums(c.terrainResistance.c_str(), 1);
	o.chassis.terrainResistance.soft = ParseNums(c.terrainResistance.c_str(), 2);
	o.chassis.speedLimits.forward = h.speedLimits.forward;
	o.chassis.speedLimits.backward = h.speedLimits.backward;
	o.chassis.rotation.speed = c.rotationSpeed;
	o.chassis.rotation.dispersion = c.shotDispersionFactors.vehicleRotation;
	o.chassis.rotation.isCentered = c.rotationIsAroundCenter;
	o.turret.health = t.maxHealth;
	o.turret.regenHealth = 0;
	o.turret.repairCost = t.repairCost;
	o.turret.chanceToHit = 1;
	o.turret.name = t.label;
	o.turret.tags = t.tags;
	o.turret.tier = t.level;
	o.turret.price = t.price;
	o.turret.weight = t.weight;
	o.turret.rotationSpeed = t.rotationSpeed;
	o.turret.visionRadius = t.circularVisionRadius;
	o.turret.rotator.health = t.turretRotatorHealth.maxHealth;
	o.turret.rotator.regenHealth = t.turretRotatorHealth.maxRegenHealth;
	o.turret.rotator.repairCost = t.turretRotatorHealth.repairCost;
	o.turret.rotator.chanceToHit = t.turretRotatorHealth.chanceToHit;
	o.turret.optics.health = t.surveyingDeviceHealth.maxHealth;
	o.turret.optics.regenHealth = t.surveyingDeviceHealth.maxRegenHealth;
	o.turret.optics.repairCost = t.surveyingDeviceHealth.repairCost;
	o.turret.optics.chanceToHit = t.surveyingDeviceHealth.chanceToHit;
	o.turret.armor.front = 0; // FIXME
	o.turret.armor.side = 0; // FIXME
	o.turret.armor.rear = 0; // FIXME
	o.turret.yawLimits.left = -ParseNums(g.turretYawLimits.c_str(), 0);
	o.turret.yawLimits.right = ParseNums(g.turretYawLimits.c_str(), 1);
	o.engine.health = e.maxHealth;
	o.engine.regenHealth = e.maxRegenHealth;
	o.engine.repairCost = e.repairCost;
	o.engine.chanceToHit = .45;
	o.engine.name = e.label;
	o.engine.tags = e.tags;
	o.engine.tier = e.level;
	o.engine.price = e.price;
	o.engine.weight = e.weight;
	o.engine.power = e.power;
	o.engine.chanceOfFire = e.fireStartingChance;
	o.radio.health = r.maxHealth;
	o.radio.regenHealth = r.maxRegenHealth;
	o.radio.repairCost = r.repairCost;
	o.radio.chanceToHit = .45;
	o.radio.name = r.label;
	o.radio.tags = r.tags;
	o.radio.tier = r.level;
	o.radio.price = r.price;
	o.radio.weight = r.weight;
	o.radio.signalRadius = r.distance;
	o.gun.health = g.maxHealth;
	o.gun.regenHealth = g.maxRegenHealth;
	o.gun.repairCost = g.repairCost;
	o.gun.chanceToHit = .33;
	o.gun.name = g.label;
	o.gun.tags = g.tags;
	o.gun.tier = g.level;
	o.gun.price = g.price;
	o.gun.weight = g.weight;
	o.gun.rotationSpeed = g.rotationSpeed;
	o.gun.reloadTime = g.reloadTime;
	o.gun.ammo = g.maxAmmo;
	o.gun.aimTime = g.aimingTime;
	o.gun.magazine.size = g.clip.count ? g.clip.count : 1;
	o.gun.magazine.burst = g.burst.count ? g.burst.count : 1;
	o.gun.magazine.delay = g.burst.rate ? 60.0 / g.burst.rate : 0;
	o.gun.dispersion.base = g.shotDispersionRadius;
	o.gun.dispersion.movement = g.shotDispersionFactors.turretRotation;
	o.gun.dispersion.shot = g.shotDispersionFactors.afterShot;
	o.gun.dispersion.damaged = g.shotDispersionFactors.whileGunDamaged;
	o.gun.pitchLimits.basic.up = -ParseNums(g.pitchLimits.c_str(), 0);
	o.gun.pitchLimits.basic.down = ParseNums(g.pitchLimits.c_str(), 1);
	o.gun.pitchLimits.front.up = g.extraPitchLimits.front.size() ? -ParseNums(g.extraPitchLimits.front.c_str(), 0) : o.gun.pitchLimits.basic.up;
	o.gun.pitchLimits.front.down = g.extraPitchLimits.front.size() ? ParseNums(g.extraPitchLimits.front.c_str(), 1) : o.gun.pitchLimits.basic.down;
	o.gun.pitchLimits.back.up = g.extraPitchLimits.back.size() ? -ParseNums(g.extraPitchLimits.back.c_str(), 0) : o.gun.pitchLimits.basic.up;
	o.gun.pitchLimits.back.down = g.extraPitchLimits.back.size() ? ParseNums(g.extraPitchLimits.back.c_str(), 1) : o.gun.pitchLimits.basic.down;
	o.ammoBay.health = h.hull.ammoBayHealth.maxHealth;
	o.ammoBay.regenHealth = h.hull.ammoBayHealth.maxRegenHealth;
	o.ammoBay.repairCost = h.hull.ammoBayHealth.repairCost;
	o.ammoBay.chanceToHit = h.hull.ammoBayHealth.chanceToHit;
	o.fuelTank.health = f.maxHealth;
	o.fuelTank.regenHealth = f.maxRegenHealth;
	o.fuelTank.repairCost = f.repairCost;
	o.fuelTank.chanceToHit = .45;
	o.fuelTank.name = f.label;
	o.fuelTank.tags = f.tags;
	o.fuelTank.tier = f.maxHealth * 2 / 25 - 6;
	o.fuelTank.price = f.price;
	o.fuelTank.weight = f.weight;
	for (size_t i = 0; i < 3; i++)
		o.shells[i].kind = Shell::UNUSED_SLOT_SHELL;
	for (size_t i = 0, end = g.shots.list.size(); i < end; i++) {
		const ShellElement& e = g.shots.list[i];
		if (e.kind == "ARMOR_PIERCING") o.shells[i].kind = Shell::AP;
		if (e.kind == "ARMOR_PIERCING_CR") o.shells[i].kind = Shell::APCR;
		if (e.kind == "HOLLOW_CHARGE") o.shells[i].kind = Shell::HEAT;
		if (e.kind == "HIGH_EXPLOSIVE") o.shells[i].kind = Shell::HE;
		o.shells[i].name = e.label;
		o.shells[i].price = e.price;
		o.shells[i].caliber = e.caliber;
		o.shells[i].explosionRadius = e.explosionRadius;
		o.shells[i].damage = e.damage.armor;
		o.shells[i].moduleDamage = e.damage.devices;
		o.shells[i].speed = e.speed;
		o.shells[i].gravity = e.gravity;
		o.shells[i].maxRange = e.maxDistance;
		o.shells[i].penAt100m = e.piercingPower_at_100;
		o.shells[i].penAt720m = e.piercingPower_at_max;
		o.shells[i].isPremium = e.isPremium;
	}
	o.stockPowerToWeight = (double)h.engines.list.begin()->power / (
		h.hull.weight +
		h.chassiss.list.begin()->weight +
		h.fuelTanks.list.begin()->weight +
		h.turrets.list.begin()->weight +
		h.turrets.list.begin()->guns.list.begin()->weight +
		h.engines.list.begin()->weight +
		h.radios.list.begin()->weight);
	for (size_t i = 0; i < 8; i++)
		o.crew[i] = 0;
	for (size_t i = 0; i < h.crew.tankers.size(); i++)
		o.crew[i] = h.crew.tankers[i];
	o.isPremium = h.premium;
	o.nation = h.nation;
}
} // namespace

extern void ProcessTanks(const Tank* tanks, size_t size);

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
	for (current_nation = CHINA; current_nation <= USSR; current_nation = (Nation)(current_nation + 1))
		ReadNation((path + nations[current_nation] + "\\").c_str());

	vector<Tank> tanks;
	for (auto h = tanksList.list.begin(), he = tanksList.list.end(); h != he; ++h)
		//for (auto t = h->turrets.list.begin(), te = h->turrets.list.end(); t != te; ++t)
			//for (auto g = t->guns.list.begin(), ge = t->guns.list.end(); g != ge; ++g) {
			for (auto g = h->turrets.list.back().guns.list.begin(), ge = h->turrets.list.back().guns.list.end(); g != ge; ++g) {
				if (h->notInShop)
					continue;
  //166.413 : T23
  //185.503 : PzIV
  //211.091 : VK7201
  // 283.49 : T44_122
  // 310.76 : M6A2E1
  //320.609 : Ch03_WZ-111
  //326.517 : Ch01_Type59
  //326.517 : Ch01_Type59_Gold
  //328.508 : KV-5
  //351.102 : M60
  //  362.9 : T95_E6
  //363.347 : PzVIB_Tiger_II_training
  //378.863 : Ch04_T34_1_training
  //378.869 : PzV_training
  //441.858 : Object_907
  //496.035 : _105_leFH18B2
  //498.772 : T44_85
  //503.176 : PzIII_training
  //510.081 : T7_Combat_Car
  //528.729 : GB76_Mk_VIC
  //552.629 : Ch02_Type62
  //575.735 : PzII_J
  //583.264 : Sexton_I
  // 586.43 : G100_Gtraktor_Krupp
  //604.599 : A-32
  //619.817 : M4A3E8_Sherman_training
  // 648.42 : T_50_2
  // 648.42 : T_50_2
  //656.403 : T-34-85_training
  //657.113 : M3_Stuart_LL
  //674.785 : KV
  //674.785 : KV
  //713.262 : Observer
  //770.143 : T23E3
  //807.787 : KV-220
  //807.787 : KV-220_action
  //808.609 : LTP
  //834.857 : Ke_Ni_B
  //835.935 : B-1bis_captured
  // 849.04 : BT-SV
  //850.318 : M4A2E4
  //869.252 : PzV_PzIV
  //869.252 : PzV_PzIV_ausf_Alfa
  //896.037 : PzIV_Hydro
  // 919.77 : MTLS-1G14
  //932.436 : H39_captured
  //951.818 : SU_85I
  //957.566 : T1_E6
  //962.013 : Tetrarch_LL
  //1068.09 : SU76I
  //   2340 : Karl
				tanks.push_back(Tank());
				//TankInit(tanks.back(), *h, *t, *g);
				TankInit(tanks.back(), *h, h->turrets.list.back(), *g);
			}
	ProcessTanks(tanks.data(), tanks.size());
	_getch();
}

