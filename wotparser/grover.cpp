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
