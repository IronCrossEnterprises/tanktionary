#pragma once
#include <string>

enum Nation { CHINA, FRANCE, GERMANY, JAPAN, UK, USA, USSR };
enum Skills {
  UNUSED_TANKER_SLOT = 0, COMMANDER = 1, GUNNER = 2, DRIVER = 4,
  RADIOMAN = 8, LOADER = 16, ALSO_GUNNER = 32, ALSO_RADIOMAN = 64,
  ALSO_LOADER = 128
};

struct Device { double health, regenHealth, repairCost, chanceToHit; };

struct Component : Device {
  std::string name, tags;
  double tier, price, weight;
};

struct Hull : Component { struct { double front, side, back; } armor; };

struct Chassis : Component {
  struct { double left, right; } armor;
  double maxLoad, movementDispersion;
  struct { double hard, medium, soft; } terrainResistance;
  struct { double forward, backward; } speedLimits;
  struct { double speed, dispersion; bool isCentered; } rotation;
};

struct Turret : Component {
  double rotationSpeed, visionRadius;
  Device rotator, optics;
  struct { double front, side, rear; } armor;
  struct { double left, right; } yawLimits;
};

struct Engine : Component { double power, chanceOfFire; };

struct Radio : Component { double signalRadius; };

struct Gun : Component {
  double rotationSpeed, reloadTime, ammo, aimTime;
  struct { double size, burst, delay; } magazine;
  struct { double base, movement, shot, damaged; } dispersion;
  struct { struct { double up, down; } basic, front, back; } pitchLimits;
};

struct Shell {
  enum Kind { UNUSED_SLOT_SHELL, AP, APCR, HEAT, HE } kind;
  std::string name;
  double price, caliber, explosionRadius, damage, moduleDamage, speed, gravity, maxRange, penAt100m, penAt720m;
  bool isPremium;
};

struct Tank {
  Hull hull;
  Chassis chassis;
  Turret turret;
  Engine engine;
  Radio radio;
  Gun gun;
  Device ammoBay;
  Component fuelTank;
  Shell shells[3];
  double stockPowerToWeight;
  char crew[8];
  bool isPremium;
  Nation nation;
};
