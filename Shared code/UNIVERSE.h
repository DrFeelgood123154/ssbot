#ifndef UNIVERSE_H
#define UNIVERSE_H
#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <sys/types.h>
#include "../json/single_include/nlohmann/json.hpp"
#include <Helpers.h>

using namespace std;
using json = nlohmann::json;

namespace LOCKOUTS{
        extern std::mutex mtx_lockouts;
        struct lockout{ // using pairs was a bit messy
                string target;
                time_t until;
                lockout(string t, time_t u){ target = t; until = u; }
        };
        struct characterLockouts{
                string character;
                vector<lockout> lockouts;
        };
        void Add(string charName, string target, time_t until);
        bool IsLocked(string charName, string from);
        lockout* Get(string charName, string from);
        void Check();
        void Save();
        void Load();
}
extern vector<LOCKOUTS::characterLockouts> lockouts;

struct DUNGEONLEVEL {
        string name;
        string enemy;
        int enemyAmount = 0, enemyLevel = 0;
        string boss;
        int bossLevel = 0;
        vector<string> wormholes;
        bool completed = false, PATHFINDER_CHECKED = false;
};

class DUNGEON {
public:
        string name;
        list<DUNGEONLEVEL> levels;
        time_t lastCleared = 0, lastTaken = 0;
        bool isExplored = false, hasUpdatedData = false;

        bool Load(json &data);
        json to_JSON();
};
extern list<DUNGEON> DUNGEONS;

namespace _DUNGEONS { /// shit naming pls
bool Load(json &data);
void LoadFromFile();
json to_JSON();
void Save();
bool AddDungeon(json &data);
void UpdateDungeonData(json &data);
DUNGEON* FindDungeon(const string entranceName);
DUNGEONLEVEL* FindDungeonLevel(const string name, DUNGEON &in);
vector<string> FindPathToNextSplit(const string from, DUNGEON &in);
}

class SYSTEM {
public:
        UINT id=0;
        string name="", ownedBy="", layer="";
        double dangerFactor=0;
        vector<string> wormholes;
        short planets=0, stars=0, moons=0, asteroids=0, aibases=0, playerbases=0;
        time_t lastProspected=0, lastCleared=0;
        bool isExplored = false, PATHFINDER_CHECKED = false;

        SYSTEM(UINT p_id);
        SYSTEM(const string p_name);

        bool Load(json& data);
        json to_JSON();
        // Data for coordination
        time_t timeTakenByProspector=0, timeTakenByExplorer=0;
};

extern list<SYSTEM> GALAXY;
namespace UNIVERSE {
extern std::mutex mtx_AddSystem, mtx_findTarget; // findTarget is used only by drones
bool Load(json& data);
void LoadFromFile();
json to_JSON();
void Save();
bool AddUnexploredSystem(const string name);
bool AddSystem(json& data);
bool AddSystem(SYSTEM &sys);
void AddSystems(json& data);

extern UINT PROSPECT_SYSTEM_COOLDOWN;
extern vector<string> ignoreSystems, prospectorIgnoreSystems, instances, p2psystems, warp4systems; /// first one is for general, second for prospectors
extern vector<string> transwarpSystems;
void LoadIgnoreSystems(string loadFrom = "");
void AddIgnoredSystem(string name);
void LoadProspectorIgnoreSystems(string loadFrom = "");
void LoadInstances(string loadFrom = "");
void LoadP2PSystems(string loadFrom = "");
void AddP2PSystem(string name);
void LoadWarp4Systems(string loadFrom = "");
void AddWarp4System(string name);
void LoadTranswarpSystems(string loadFrom = "");
void SaveLists();

bool IsWarp4(string name);
bool IsDungeon(string name);
string GetSystemFromWormhole(string &wh);
string GetSystemFromDG(string &dg);
SYSTEM* FindSystem(const string name);
SYSTEM* FindSystem(const UINT id);
vector<string> FindPath(const string from, const string to, bool useShortcuts = false);
bool IsValidExploreTarget(SYSTEM* system);
pair<SYSTEM*, SYSTEM*> FindNearestUnexplored(const string from, byte maxWarpLevel, vector<string> *exclude = NULL, bool isp2p = false);
bool IsValidProspectTarget(SYSTEM* system);
SYSTEM* FindNearestProspectable(const string from, bool includeShortcuts, vector<string> layers, vector<string> *exclude = NULL, bool isp2p = false);
DUNGEON* FindNearestAvailableDG(string from, double minDF, double maxDF, vector<string> layers, vector<string> *exclude = NULL, string characterName = "", bool lowerDF = true);
SYSTEM* FindRandom(string from, double minDF, double maxDF, vector<string> layers, size_t atJumps);
SYSTEM* FindNearestTranswarpSystem(const string from, short maxDistance = 5);
SYSTEM* FindNearestAIBase(const string from, byte maxWarpLevel = 3, bool isp2p = false);
}
#endif // UNIVERSE_H
