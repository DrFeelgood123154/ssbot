#include "UNIVERSE.h"
list<DUNGEON> DUNGEONS;
list<SYSTEM> GALAXY;
///===LOCKOUTS
vector<LOCKOUTS::characterLockouts> lockouts;

std::mutex LOCKOUTS::mtx_lockouts;
void LOCKOUTS::Add(string charName, string target, time_t until) {
        mtx_lockouts.lock();
        characterLockouts *thisChar = nullptr;
        for(auto &el : lockouts) if(el.character == charName){ thisChar = &el; break; }
        if(thisChar == nullptr){
                lockouts.push_back(characterLockouts());
                lockouts.back().character = charName;
                thisChar = &lockouts.back();
        }
        lockout *targetLockout = nullptr;
        for(lockout &el : thisChar->lockouts) if(el.target == target){ targetLockout = &el; break; }
        if(targetLockout == nullptr){
                thisChar->lockouts.push_back(lockout(target, until));
        }else targetLockout->until = until;
        mtx_lockouts.unlock();
        Save();
}
bool LOCKOUTS::IsLocked(string charName, string from){
        mtx_lockouts.lock();
        for(characterLockouts &character : lockouts){
                if(character.character == charName){
                        for(lockout &lock : character.lockouts) if(lock.target == from){ mtx_lockouts.unlock(); return time(0) < lock.until; }
                        mtx_lockouts.unlock();
                        return false;
                }
        }
        mtx_lockouts.unlock();
        return false;
}
LOCKOUTS::lockout* LOCKOUTS::Get(string charName, string from){
        mtx_lockouts.lock();
        for(characterLockouts &character : lockouts){
                if(character.character == charName){
                        for(lockout &lock : character.lockouts) if(lock.target == from){ mtx_lockouts.unlock(); return &lock; }
                }
        }
        mtx_lockouts.unlock();
        return NULL;
}
void LOCKOUTS::Check() {
        auto charIt = lockouts.begin();
        while(charIt != lockouts.end()){
                auto lockoutIt = (*charIt).lockouts.begin();
                while (lockoutIt != (*charIt).lockouts.end()){
                        if((*lockoutIt).until < time(0)) lockoutIt = (*charIt).lockouts.erase(lockoutIt);
                        else ++lockoutIt;
                }
                if((*charIt).lockouts.size() == 0) charIt = lockouts.erase(charIt);
                else ++charIt;
        }
}
void LOCKOUTS::Save() {
        mtx_lockouts.lock();
        json data;
        for(characterLockouts &charLockouts : lockouts){
                json thisChar;
                thisChar["character"] = charLockouts.character.c_str();
                thisChar["locks"];
                for(lockout &thisLockout : charLockouts.lockouts) thisChar["locks"][thisLockout.target.c_str()] = thisLockout.until;
                data.push_back(thisChar);
        }
        ofstream file("data/lockouts.txt");
        file<<data.dump(1);
        mtx_lockouts.unlock();
}
void LOCKOUTS::Load() {
        mtx_lockouts.lock();
        lockouts.clear();
        std::ifstream file("data/lockouts.txt");
        std::string strdata((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if(strdata.length() == 0) { mtx_lockouts.unlock(); return; }
        json data;
        try {
                data = json::parse(strdata.c_str());
                for(auto &el : data){
                        lockouts.push_back(characterLockouts());
                        lockouts.back().character = el["character"].get<string>();
                        for(auto &el2 : el["locks"].items()) lockouts.back().lockouts.push_back(lockout(el2.key(), el2.value().get<int>()));
                }
        }catch(const exception &e){ printf(CC_YELLOW, "Error loading lockouts\n"); }
        mtx_lockouts.unlock();
}
///===DUNGEON
bool DUNGEON::Load(json& data) {
        try {
                name = data["name"];
                lastCleared = (data["lastCleared"].is_string())?stoi(data["lastCleared"].get<string>(), NULL):data["lastCleared"].get<int>();
                isExplored = data["isExplored"].get<bool>();
                levels.clear();
                for(auto &el : data["levels"]) {
                        DUNGEONLEVEL newLvl;
                        newLvl.name = el["name"];
                        newLvl.enemy = el["enemy"];
                        newLvl.enemyAmount = (el["enemies"].is_string())?stoi(el["enemies"].get<string>(), NULL):el["enemies"].get<int>();
                        newLvl.enemyLevel = (el["enemyLevel"].is_string())?stoi(el["enemyLevel"].get<string>(), NULL):el["enemyLevel"].get<int>();
                        newLvl.boss = el["boss"];
                        newLvl.bossLevel = (el["bossLevel"].is_string())?stoi(el["bossLevel"].get<string>(), NULL):el["bossLevel"].get<int>();
                        for(auto &el2 : el["wormholes"]) newLvl.wormholes.push_back(el2.get<string>());
                        levels.push_back(newLvl);
                }
        } catch(const exception &e) { printf(CC_RED, "Failed to load DG %s: %s\n", name.c_str(), e.what()); return false; }
        return true;
}
json DUNGEON::to_JSON() {
        json data;
        data["name"] = name;
        data["lastCleared"] = lastCleared;
        data["isExplored"] = isExplored;
        for(DUNGEONLEVEL &lvl : levels) {
                json newLvl{
                        {"name", lvl.name},
                        {"enemy", lvl.enemy},
                        {"enemies", lvl.enemyAmount},
                        {"enemyLevel", lvl.enemyLevel},
                        {"boss", lvl.boss},
                        {"bossLevel", lvl.bossLevel},
                };
                for(string &wh : lvl.wormholes) newLvl["wormholes"].push_back(wh);
                data["levels"].push_back(newLvl);
        }
        return data;
}
///===_DUNGEONS
bool _DUNGEONS::Load(json& data) {
        DUNGEONS.clear();
        try {
                for(auto& el : data) {
                        DUNGEONS.push_back(DUNGEON());
                        DUNGEONS.back().Load(el);
                }
        } catch(const exception &e) { printf(CC_RED, "Error loading dungeons list: %s\n", e.what()); return false; }
        return true;
}
void _DUNGEONS::LoadFromFile() {
        std::ifstream file("data/dungeons.txt");
        std::string strdata((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if(strdata.length() == 0) { printf(CC_YELLOW, "Dungeons file not found\n"); return; }
        json data;
        try {
                data = json::parse(strdata.c_str());
                if(!Load(data)) printf(CC_RED, "Error loading dungeons from text file\n");
        } catch(const exception &e) { printf(CC_RED, "Error loading dungeons from text file\n"); }
}
json _DUNGEONS::to_JSON() {
        json data;
        for(DUNGEON& dungeon : DUNGEONS) data.push_back(dungeon.to_JSON());
        return data;
}
void _DUNGEONS::Save() {
        string data = to_JSON().dump(1);
        if(data.length() < 5) return;
        ofstream file("data/dungeons.txt");
        file<<data.c_str();
}
bool _DUNGEONS::AddDungeon(json &data) {
        try {
                ///check if exists
                for(DUNGEON& dg : DUNGEONS) if(dg.name == data["name"]) return false;
                DUNGEON newDungeon = DUNGEON();
                if(!newDungeon.Load(data)) return false;
                DUNGEONS.push_back(newDungeon);
        } catch(const exception &e) { printf(CC_YELLOW, "Error adding dungeon: %s\n", e.what()); return false; }
        return true;
}
void _DUNGEONS::UpdateDungeonData(json &data) {
        try {
                for(DUNGEON &dungeon : DUNGEONS) {
                        if(dungeon.name == data["name"]) {
                                dungeon.Load(data);
                                _DUNGEONS::Save();
                                break;
                        }
                }
        } catch(const exception &e) { printf(CC_YELLOW, "Error updating dungeon data: %s\n", e.what()); }
}
DUNGEON* _DUNGEONS::FindDungeon(const string entrance) {
        for(DUNGEON &dungeon : DUNGEONS) if(dungeon.name == entrance) return &dungeon;
        return NULL;
}
DUNGEONLEVEL* _DUNGEONS::FindDungeonLevel(const string name, DUNGEON &in) {
        for(DUNGEONLEVEL &lvl : in.levels) if(lvl.name == name) return &lvl;
        return NULL;
}

struct DG_NODE {
        DUNGEONLEVEL* lvl;
        DG_NODE* prevNode;
        DG_NODE(DUNGEONLEVEL *l, DG_NODE *n) { lvl = l; prevNode = n; }
};
vector<string> _DUNGEONS::FindPathToNextSplit(const string from, DUNGEON &in) {
        vector<string> path;
        list<DG_NODE> nodes;
        for(DUNGEONLEVEL &lvl : in.levels) lvl.PATHFINDER_CHECKED = false; ///reset all gals to unchecked
        DUNGEONLEVEL *start = _DUNGEONS::FindDungeonLevel(from, in), *currentLvl = nullptr;
        if(start == NULL) { return path; }
        DG_NODE* currentNode = nullptr; // for making the path at the end
        start->PATHFINDER_CHECKED = true;
        DUNGEONLEVEL *tempLevel = nullptr;
        nodes.push_back(DG_NODE(start, nullptr));
        auto it = nodes.begin();
        while(it != nodes.end()) {
                currentLvl = (*it).lvl;
                if(!currentLvl->completed) {
                        currentNode = &(*it);
                        while(currentNode != nullptr) {
                                path.push_back(currentNode->lvl->name);
                                currentNode = currentNode->prevNode;
                        }
                        if(tempLevel != nullptr) delete(tempLevel);
                        std::reverse(path.begin(), path.end());
                        return path;
                }
                for(string &wormhole : currentLvl->wormholes) {
                        string thisName = UNIVERSE::GetSystemFromWormhole(wormhole);
                        if(!UNIVERSE::IsDungeon(thisName)) {
                                if(tempLevel == nullptr) {
                                        tempLevel = new DUNGEONLEVEL();
                                        tempLevel->name = UNIVERSE::GetSystemFromDG(in.name);
                                        tempLevel->wormholes.push_back("Gate to "+in.name);
                                        tempLevel->completed = true;
                                        tempLevel->PATHFINDER_CHECKED = false;
                                        nodes.push_back(DG_NODE(tempLevel, &(*it)));
                                        continue;
                                }
                        }
                        DUNGEONLEVEL *thisLvl = _DUNGEONS::FindDungeonLevel(thisName, in);
                        if(thisLvl == NULL || thisLvl->PATHFINDER_CHECKED) continue;
                        nodes.push_back(DG_NODE(thisLvl, &(*it)));
                        thisLvl->PATHFINDER_CHECKED = true;
                }
                ++it;
        }
        if(tempLevel != nullptr) delete(tempLevel);
        return path;
}

///===System
SYSTEM::SYSTEM(UINT p_id) { id = p_id; }
SYSTEM::SYSTEM(const string p_name) { name = p_name; id = 0; }

bool SYSTEM::Load(json& data) {
        try {
                id = (data["id"].is_string())?stoi(data["id"].get<string>(), NULL):data["id"].get<int>();
                name = data["name"];
                ownedBy = data["ownedBy"];
                layer = data["layer"];
                dangerFactor = (data["dangerFactor"].is_string())?stod(data["id"].get<string>(), NULL):data["dangerFactor"].get<double>();
                for(auto &el : data["wormholes"]) wormholes.push_back(el);
                planets = (data["planets"].is_string())?stoi(data["planets"].get<string>(), NULL):data["planets"].get<short>();
                moons = (data["moons"].is_string())?stoi(data["moons"].get<string>(), NULL):data["moons"].get<short>();
                stars = (data["stars"].is_string())?stoi(data["stars"].get<string>(), NULL):data["stars"].get<short>();
                asteroids = (data["asteroids"].is_string())?stoi(data["asteroids"].get<string>(), NULL):data["asteroids"].get<short>();
                aibases = (data["aibases"].is_string())?stoi(data["aibases"].get<string>(), NULL):data["aibases"].get<short>();
                playerbases = (data["playerbases"].is_string())?stoi(data["playerbases"].get<string>(), NULL):data["playerbases"].get<short>();
                lastProspected = (data["lastProspected"].is_string())?stoi(data["lastProspected"].get<string>(), NULL):data["lastProspected"].get<int>();
                lastCleared = (data["lastCleared"].is_string())?stoi(data["lastCleared"].get<string>(), NULL):data["lastCleared"].get<int>();
                try { isExplored = data["isExplored"].get<bool>(); } catch(const exception &e) { isExplored = false; }
        } catch(const exception &e) { printf(CC_RED, "Failed to load system %s: %s\n", name.c_str(), e.what()); return false; }
        return true;
}
json SYSTEM::to_JSON() {
        json data;
        data["id"] = id;
        data["name"] = name;
        data["ownedBy"] = ownedBy;
        data["layer"] = layer;
        data["dangerFactor"] = dangerFactor;
        for(string& wormhole : wormholes) data["wormholes"].push_back(wormhole);
        data["planets"] = planets;
        data["moons"] = moons;
        data["stars"] = stars;
        data["asteroids"] = asteroids;
        data["playerbases"] = playerbases;
        data["aibases"] = aibases;
        data["lastProspected"] = lastProspected;
        data["lastCleared"] = lastCleared;
        data["isExplored"] = isExplored;
        return data;
}
///===Universe
std::mutex UNIVERSE::mtx_AddSystem, UNIVERSE::mtx_findTarget;
bool UNIVERSE::Load(json& data) {
        GALAXY.clear();
        try {
                for(auto& el : data) {
                        GALAXY.push_back(SYSTEM((el["id"].is_string())?stoi(el["id"].get<string>(), NULL):el["id"].get<int>()));
                        GALAXY.back().Load(el);
                }
        } catch(const exception &e) { printf(CC_RED, "Failed to load universe map: %s\n", e.what()); return false; }
        return true;
}
void UNIVERSE::LoadFromFile() {
        std::ifstream file("data/universe.txt");
        std::string strdata((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if(strdata.length() == 0) { printf(CC_YELLOW, "Universe file not found\n"); return; }
        json data;
        try {
                data = json::parse(strdata.c_str());
                if(!Load(data)) printf(CC_RED, "Error loading Universe from text file\n");
        } catch(const exception &e) { printf(CC_RED, "Error loading Universe from text file\n"); }
}
json UNIVERSE::to_JSON() {
        json data;
        for(SYSTEM& system : GALAXY) data.push_back(system.to_JSON());
        return data;
}
void UNIVERSE::Save() {
        string data = to_JSON().dump(1);
        if(data.length() < 5) return;
        ofstream file("data/universe.txt");
        file<<data.c_str();
        _DUNGEONS::Save();
}
bool UNIVERSE::AddUnexploredSystem(const string name) { // IFCRASH add lock but watch out for recursions
        if(name == "") return false;
        if(IsDungeon(name)) {
                for(DUNGEON &dungeon : DUNGEONS) if(dungeon.name == name) return false;
                DUNGEON newDungeon;
                newDungeon.name = name;
                newDungeon.isExplored = false;
                DUNGEONS.push_back(newDungeon);
                printf("Added new unexplored DG '%s'\n", name.c_str());
                return true;
        }
        for(SYSTEM &sys : GALAXY) if(sys.name == name) { return false; }
        GALAXY.push_back(SYSTEM(name));
        printf("Added new unexplored system '%s'\n", name.c_str());
        return true;
}
bool UNIVERSE::AddSystem(json& data) {
        mtx_AddSystem.lock();
        try {
                ///check if exists
                SYSTEM newSystem(SYSTEM((data["id"].is_string())?stoi(data["id"].get<string>(), NULL):data["id"].get<int>()));
                for(SYSTEM &sys : GALAXY) if(sys.name == data["name"] && !sys.isExplored) { // update data
                                sys.Load(data);
                                for(string &wh : sys.wormholes) AddUnexploredSystem(GetSystemFromWormhole(wh));
                                mtx_AddSystem.unlock();
                                return true;
                        }
                if(!newSystem.Load(data)) { mtx_AddSystem.unlock(); return false; }
                for(string &wh : newSystem.wormholes) AddUnexploredSystem(GetSystemFromWormhole(wh));
                GALAXY.push_back(newSystem);
        } catch(const exception &e) { printf(CC_YELLOW, "Error adding system: %s\n", e.what()); mtx_AddSystem.unlock(); return false; }
        mtx_AddSystem.unlock();
        return true;
}
bool UNIVERSE::AddSystem(SYSTEM &sys) {
        if(sys.name == "") return false;
        mtx_AddSystem.lock();
        for(SYSTEM& index : GALAXY) if(!index.isExplored && index.name == sys.name) {
                        index = sys;
                        for(string &wh : sys.wormholes) AddUnexploredSystem(GetSystemFromWormhole(wh));
                        mtx_AddSystem.unlock();
                        return true;
                }
        for(string &wh : sys.wormholes) AddUnexploredSystem(GetSystemFromWormhole(wh));
        GALAXY.push_back(sys);
        mtx_AddSystem.unlock();
        return true;
}
void UNIVERSE::AddSystems(json& data) {
        for(auto& el : data) AddSystem(el);
}

string UNIVERSE::GetSystemFromWormhole(string &wh) {
        if(wh.find("Gate to") != string::npos) return wh.substr(8);
        return wh;
}
string UNIVERSE::GetSystemFromDG(string &dg) {
        string buff = GetSystemFromWormhole(dg);
        if(buff.find("DG ") == string::npos) return buff;
        size_t pos = buff.find_last_of(' ');
        if(pos == string::npos || pos-3 < 0) return buff;
        return buff.substr(3, pos-3);
}

UINT UNIVERSE::PROSPECT_SYSTEM_COOLDOWN = 60*60*24;
std::mutex systemListLock;
vector<string> UNIVERSE::ignoreSystems, UNIVERSE::prospectorIgnoreSystems, UNIVERSE::instances, UNIVERSE::p2psystems, UNIVERSE::warp4systems;
vector<string> UNIVERSE::transwarpSystems;
void UNIVERSE::LoadIgnoreSystems(string loadFrom) {
        if(loadFrom == "") {
                std::ifstream file("data/ignore_systems.txt");
                loadFrom = string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        }
        ignoreSystems = String::split(loadFrom, "\n");
}
void UNIVERSE::AddIgnoredSystem(string name) {
        systemListLock.lock();
        if(std::find(ignoreSystems.begin(), ignoreSystems.end(), name) == ignoreSystems.end()) ignoreSystems.push_back(name);
        systemListLock.unlock();
}
void UNIVERSE::LoadProspectorIgnoreSystems(string loadFrom) {
        if(loadFrom == "") {
                std::ifstream file("data/prospector_ignore_systems.txt");
                loadFrom = string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        }
        prospectorIgnoreSystems = String::split(loadFrom, "\n");
}
void UNIVERSE::LoadInstances(string loadFrom) {
        if(loadFrom == "") {
                std::ifstream file("data/instances.txt");
                loadFrom = string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        }
        instances = String::split(loadFrom, "\n");
}
void UNIVERSE::LoadP2PSystems(string loadFrom) {
        if(loadFrom == "") {
                std::ifstream file("data/p2psystems.txt");
                loadFrom = string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        }
        p2psystems = String::split(loadFrom, "\n");
}
void UNIVERSE::AddP2PSystem(string name) {
        systemListLock.lock();
        if(std::find(p2psystems.begin(), p2psystems.end(), name) == p2psystems.end()) p2psystems.push_back(name);
        systemListLock.unlock();
}
void UNIVERSE::LoadWarp4Systems(string loadFrom) {
        if(loadFrom == "") {
                std::ifstream file("data/warp4systems.txt");
                loadFrom = string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        }
        warp4systems = String::split(loadFrom, "\n");
}
void UNIVERSE::AddWarp4System(string name) {
        systemListLock.lock();
        if(std::find(warp4systems.begin(), warp4systems.end(), name) == warp4systems.end()) warp4systems.push_back(name);
        systemListLock.unlock();
}
void UNIVERSE::LoadTranswarpSystems(string loadFrom) {
        if(loadFrom == "") {
                std::ifstream file("data/transwarpsystems.txt");
                loadFrom = string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        }
        transwarpSystems = String::split(loadFrom, "\n");
}
void UNIVERSE::SaveLists() {
        ofstream newFile;
        newFile.open("data/ignore_systems.txt");
        for(string& str : ignoreSystems) newFile<<str.c_str()<<"\n";
        newFile.close();
        newFile.open("data/prospector_ignore_systems.txt");
        for(string& str : prospectorIgnoreSystems) newFile<<str.c_str()<<"\n";
        newFile.close();
        newFile.open("data/instances.txt");
        for(string& str : instances) newFile<<str.c_str()<<"\n";
        newFile.close();
        newFile.open("data/p2psystems.txt");
        for(string& str : p2psystems) newFile<<str.c_str()<<"\n";
        newFile.close();
        newFile.open("data/warp4systems.txt");
        for(string& str : warp4systems) newFile<<str.c_str()<<"\n";
        newFile.close();
        newFile.open("data/transwarpsystems.txt");
        for(string& str : transwarpSystems) newFile<<str.c_str()<<"\n";
        newFile.close();
}

SYSTEM* UNIVERSE::FindSystem(const string name) {
        for(SYSTEM& sys : GALAXY) if(sys.name == name) return &sys;
        return NULL;
}
SYSTEM* UNIVERSE::FindSystem(const UINT id) {
        for(SYSTEM& sys : GALAXY) if(sys.id == id) return &sys;
        return NULL;
}

inline bool UNIVERSE::IsWarp4(string name) {
        return (name.find("Juxtaposition") != string::npos || name.find("Concourse") != string::npos || name.find("Subspace") == 0);
}
inline bool UNIVERSE::IsDungeon(string name) {
        return (name.find("DG ") != string::npos && name.find(".") != string::npos);
}

struct PATH_NODE {
        SYSTEM* sys;
        PATH_NODE* prevNode;
        PATH_NODE(SYSTEM *s, PATH_NODE *n) { sys = s; prevNode = n; }
};
vector<string> UNIVERSE::FindPath(const string from, const string to, bool useShortcuts) {
        vector<string> path;
        list<PATH_NODE> nodes;
        mtx_AddSystem.lock();
        for(SYSTEM &sys : GALAXY) sys.PATHFINDER_CHECKED = false; ///reset all gals to unchecked
        ///do stuff
        SYSTEM *start = UNIVERSE::FindSystem(from), *destination = UNIVERSE::FindSystem(to), *currentSystem = nullptr;
        PATH_NODE* currentNode = nullptr; /// used only at the end
        if(start == NULL || destination == NULL) { mtx_AddSystem.unlock(); return path; }
        start->PATHFINDER_CHECKED = true;
        nodes.push_back(PATH_NODE(start, nullptr));
        auto it = nodes.begin();
        while(it != nodes.end()) {
                currentSystem = (*it).sys;
                if(currentSystem == destination) {
                        currentNode = &(*it);
                        while(currentNode != nullptr) {
                                path.push_back(currentNode->sys->name);
                                currentNode = currentNode->prevNode;
                        }
                        std::reverse(path.begin(), path.end());
                        mtx_AddSystem.unlock();
                        return path;
                }
                for(string &wormhole : currentSystem->wormholes) {
                        SYSTEM *thisSystem = UNIVERSE::FindSystem(GetSystemFromWormhole(wormhole));
                        if(thisSystem == NULL || thisSystem->PATHFINDER_CHECKED) continue;
                        if(!useShortcuts && IsWarp4(GetSystemFromWormhole(thisSystem->name))) continue;
                        nodes.push_back(PATH_NODE(thisSystem, &(*it)));
                        thisSystem->PATHFINDER_CHECKED = true;
                }
                ++it;
        }
        mtx_AddSystem.unlock();
        return path;
}
bool UNIVERSE::IsValidExploreTarget(SYSTEM *system){
        return (time(0) > system->timeTakenByExplorer + 600  && time(0) > system->timeTakenByProspector + 600 // 10 minutes
                                && !system->isExplored
                                && std::find(ignoreSystems.begin(), ignoreSystems.end(), system->name) == ignoreSystems.end()
                                && std::find(instances.begin(), instances.end(), system->name) == instances.end()
                                && !IsDungeon(system->name));
}
pair<SYSTEM*, SYSTEM*> UNIVERSE::FindNearestUnexplored(const string from, byte maxWarpLevel, vector<string> *exclude, bool isp2p) {
        list<PATH_NODE> nodes;
        mtx_AddSystem.lock();
        for(SYSTEM &sys : GALAXY) sys.PATHFINDER_CHECKED = false; ///reset all gals to unchecked
        SYSTEM *start = UNIVERSE::FindSystem(from), *currentSystem = nullptr;
        if(start == NULL) { mtx_AddSystem.unlock(); return make_pair(nullptr, nullptr); }
        start->PATHFINDER_CHECKED = true;
        nodes.push_back(PATH_NODE(start, nullptr));
        auto it = nodes.begin();
        while(it != nodes.end()) {
                currentSystem = (*it).sys;
                if(IsValidExploreTarget(currentSystem)
                                && (exclude == NULL || (std::find(exclude->begin(), exclude->end(), currentSystem->name) == exclude->end()))
                                && (isp2p || std::find(p2psystems.begin(), p2psystems.end(), currentSystem->name) == p2psystems.end())
                                && (maxWarpLevel >= 4 || (!IsWarp4(currentSystem->name) && std::find(warp4systems.begin(), warp4systems.end(), currentSystem->name) == warp4systems.end()))) {
                        mtx_AddSystem.unlock(); //IFCRASH: probably of this dumb unlock
                        //confirm that a path can be found in case they mapped different parts of map
                        vector<string> buffer = FindPath(from, currentSystem->name, maxWarpLevel == 4);
                        if(buffer.size() != 0) return make_pair(currentSystem, nullptr);
                        else mtx_AddSystem.lock();
                }
                for(string &wormhole : currentSystem->wormholes) {
                        SYSTEM *thisSystem = UNIVERSE::FindSystem(GetSystemFromWormhole(wormhole));
                        // This is commented out because now all systems are auto added from wormholes as unexplored
                        /*if(thisSystem == NULL){ // not on the systems list
                                printf("this wh %s not found, check if ok\n", wormhole.c_str());
                                string thisWhSystem = GetSystemFromWormhole(wormhole);
                                if((exclude != NULL && (std::find(exclude->begin(), exclude->end(), thisWhSystem) != exclude->end()))
                                                       || std::find(ignoreSystems.begin(), ignoreSystems.end(), thisWhSystem) != ignoreSystems.end()
                                                       || std::find(instances.begin(), instances.end(), thisWhSystem) != instances.end()
                                                       || (maxWarpLevel < 4 && IsWarp4(GetSystemFromWormhole(thisWhSystem)))
                                                        || IsDungeon(thisWhSystem)) continue;
                                mtx_AddSystem.unlock();
                                printf("return this wh %s\n", wormhole.c_str());
                                return make_pair(thisWhSystem, currentSystem);
                        }*/
                        if(thisSystem == NULL || thisSystem->PATHFINDER_CHECKED) continue;
                        nodes.push_back(PATH_NODE(thisSystem, &(*it)));
                        thisSystem->PATHFINDER_CHECKED = true;
                }
                ++it;
        }
        mtx_AddSystem.unlock();
        return make_pair(nullptr, nullptr);
}
bool UNIVERSE::IsValidProspectTarget(SYSTEM *system){
        return (time(0) > system->lastProspected+PROSPECT_SYSTEM_COOLDOWN
                                && time(0) > system->timeTakenByProspector + 600
                                && (system->planets > 0 || system->stars > 0 || system->moons > 0)
                                && std::find(ignoreSystems.begin(), ignoreSystems.end(), system->name) == ignoreSystems.end()
                                && std::find(prospectorIgnoreSystems.begin(), prospectorIgnoreSystems.end(), system->name) == prospectorIgnoreSystems.end()
                                && std::find(instances.begin(), instances.end(), system->name) == instances.end()
                                && system->name != "Sol" && system->name != "Capella"
                                && !IsWarp4(system->name));
}
SYSTEM* UNIVERSE::FindNearestProspectable(const string from, bool useShortcuts, vector<string> layers, vector<string> *exclude, bool isp2p) {
        list<PATH_NODE> nodes;
        mtx_AddSystem.lock();
        for(SYSTEM &sys : GALAXY) sys.PATHFINDER_CHECKED = false; ///reset all gals to unchecked
        SYSTEM *start = UNIVERSE::FindSystem(from), *currentSystem = nullptr;
        if(start == NULL) { mtx_AddSystem.unlock(); return nullptr; }
        start->PATHFINDER_CHECKED = true;
        nodes.push_back(PATH_NODE(start, nullptr));
        auto it = nodes.begin();
        while(it != nodes.end()) {
                currentSystem = (*it).sys;
                if(IsValidProspectTarget(currentSystem)
                                && (exclude == NULL ||  (exclude != NULL && std::find(exclude->begin(), exclude->end(), currentSystem->name) == exclude->end()))
                                && (isp2p || std::find(p2psystems.begin(), p2psystems.end(), currentSystem->name) == p2psystems.end())
                                && std::find(layers.begin(), layers.end(), currentSystem->layer) != layers.end()
                                && std::find(warp4systems.begin(), warp4systems.end(), currentSystem->name) == warp4systems.end()) {
                        mtx_AddSystem.unlock(); //IFCRASH: this lock
                        //confirm that a path can be found in case they mapped different parts of map
                        vector<string> buffer = FindPath(from, currentSystem->name, useShortcuts);
                        if(buffer.size() != 0) return currentSystem;
                        else mtx_AddSystem.lock();
                }
                for(string &wormhole : currentSystem->wormholes) {
                        SYSTEM *thisSystem = UNIVERSE::FindSystem(GetSystemFromWormhole(wormhole));
                        if(thisSystem == NULL || thisSystem->PATHFINDER_CHECKED) continue;
                        nodes.push_back(PATH_NODE(thisSystem, &(*it)));
                        thisSystem->PATHFINDER_CHECKED = true;
                }
                ++it;
        }
        mtx_AddSystem.unlock();
        return nullptr;
}
DUNGEON* UNIVERSE::FindNearestAvailableDG(string from, double minDF, double maxDF, vector<string> layers, vector<string> *exclude, string characterName, bool lowerDF) {
        if(IsDungeon(from)) from = GetSystemFromDG(from);
        list<PATH_NODE> nodes;
        mtx_AddSystem.lock();
        for(SYSTEM &sys : GALAXY) sys.PATHFINDER_CHECKED = false; ///reset all gals to unchecked
        SYSTEM *start = UNIVERSE::FindSystem(from), *currentSystem = nullptr;
        if(start == NULL) { mtx_AddSystem.unlock(); return nullptr; }
        start->PATHFINDER_CHECKED = true;
        nodes.push_back(PATH_NODE(start, nullptr));
        auto it = nodes.begin();
        while(it != nodes.end()) {
                currentSystem = (*it).sys;
                size_t jumps = 0;
                PATH_NODE *currentNode = &(*it);
                if(minDF > 0.1 && start->dangerFactor > 0.3){
                        while(currentNode != nullptr) {
                                currentNode = currentNode->prevNode;
                                jumps++;
                        }
                        if(jumps > 10 && lowerDF){
                                mtx_AddSystem.unlock();
                                return FindNearestAvailableDG(from, 0, minDF*0.70, layers, exclude, characterName, false);
                        }
                }
                if(currentSystem->dangerFactor >= minDF && currentSystem->dangerFactor <= maxDF && std::find(layers.begin(), layers.end(), currentSystem->layer) != layers.end()) {
                        for(string &wh : currentSystem->wormholes) {
                                string name = GetSystemFromWormhole(wh);
                                if(!IsDungeon(name)) continue;
                                DUNGEON* thisDungeon = _DUNGEONS::FindDungeon(name);
                                if(thisDungeon == NULL){
                                        printf(CC_PINK, "Adding dungeon '%s'\n", name.c_str());
                                        AddUnexploredSystem(name);
                                        thisDungeon = _DUNGEONS::FindDungeon(name);
                                        if(thisDungeon == NULL) continue;
                                        _DUNGEONS::Save();
                                }
                                if(time(0) < thisDungeon->lastCleared+30*60 || time(0) < thisDungeon->lastTaken+30*60) continue;
                                if(characterName != "" && LOCKOUTS::IsLocked(characterName, thisDungeon->name)) continue;
                                //TODO: add some exclude for dgs that cant be cleared by given character

                                //check if reachable
                                mtx_AddSystem.unlock(); //IFCRASH: this lock
                                vector<string> buffer = FindPath(from, currentSystem->name, 3);
                                if(buffer.size() != 0) { mtx_AddSystem.unlock(); return thisDungeon; }
                                else mtx_AddSystem.lock();
                        }
                }
                for(string &wormhole : currentSystem->wormholes) {
                        SYSTEM *thisSystem = UNIVERSE::FindSystem(GetSystemFromWormhole(wormhole));
                        if(thisSystem == NULL || thisSystem->PATHFINDER_CHECKED) continue;
                        nodes.push_back(PATH_NODE(thisSystem, &(*it)));
                        thisSystem->PATHFINDER_CHECKED = true;
                }
                ++it;
        }
        mtx_AddSystem.unlock();
        if(minDF != 0) return FindNearestAvailableDG(from, 0, minDF, layers, exclude);
        return NULL;
}
SYSTEM* UNIVERSE::FindRandom(string from, double minDF, double maxDF, vector<string> layers, size_t atJumps) {
        list<PATH_NODE> nodes;
        mtx_AddSystem.lock();
        for(SYSTEM &sys : GALAXY) sys.PATHFINDER_CHECKED = false; ///reset all gals to unchecked
        SYSTEM *start = UNIVERSE::FindSystem(from), *currentSystem = nullptr;
        if(start == NULL) { mtx_AddSystem.unlock(); return nullptr; }
        start->PATHFINDER_CHECKED = true;
        nodes.push_back(PATH_NODE(start, nullptr));
        auto it = nodes.begin();
        while(it != nodes.end()) {
                currentSystem = (*it).sys;
                size_t jumps = 0;
                PATH_NODE *currentNode = &(*it);
                while(currentNode != nullptr) {
                        currentNode = currentNode->prevNode;
                        jumps++;
                }
                if(jumps >= atJumps && currentSystem->dangerFactor >= minDF && currentSystem->dangerFactor <= maxDF && std::find(layers.begin(), layers.end(), currentSystem->layer) != layers.end()) {
                        //check if reachable
                        mtx_AddSystem.unlock(); //IFCRASH: this lock
                        vector<string> buffer = FindPath(from, currentSystem->name, false);
                        if(buffer.size() != 0) { mtx_AddSystem.unlock(); return currentSystem; }
                        else mtx_AddSystem.lock();
                }
                vector<string> wormholes = currentSystem->wormholes;
                std::random_shuffle(wormholes.begin(), wormholes.end());
                for(string &wormhole : wormholes) {
                        SYSTEM *thisSystem = UNIVERSE::FindSystem(GetSystemFromWormhole(wormhole));
                        if(thisSystem == NULL || thisSystem->PATHFINDER_CHECKED) continue;
                        nodes.push_back(PATH_NODE(thisSystem, &(*it)));
                        thisSystem->PATHFINDER_CHECKED = true;
                }
                ++it;
        }
        mtx_AddSystem.unlock();
        if(atJumps >= 1) return UNIVERSE::FindRandom(from, minDF, maxDF, layers, floor(atJumps/2));
        return NULL;
}
SYSTEM* UNIVERSE::FindNearestTranswarpSystem(const string from, short maxDistance) {
        list<PATH_NODE> nodes;
        mtx_AddSystem.lock();
        for(SYSTEM &sys : GALAXY) sys.PATHFINDER_CHECKED = false; ///reset all gals to unchecked
        SYSTEM *start = UNIVERSE::FindSystem(from), *currentSystem = nullptr;
        if(start == NULL) { mtx_AddSystem.unlock(); return nullptr; }
        start->PATHFINDER_CHECKED = true;
        nodes.push_back(PATH_NODE(start, nullptr));
        short thisDistance = 0;
        auto it = nodes.begin();
        while(it != nodes.end()) {
                currentSystem = (*it).sys;
                if(std::find(transwarpSystems.begin(), transwarpSystems.end(), currentSystem->name) != transwarpSystems.end()) {
                        mtx_AddSystem.unlock();
                        return currentSystem;
                }
                for(string &wormhole : currentSystem->wormholes) {
                        SYSTEM *thisSystem = UNIVERSE::FindSystem(GetSystemFromWormhole(wormhole));
                        if(thisSystem == NULL || thisSystem->PATHFINDER_CHECKED) continue;
                        nodes.push_back(PATH_NODE(thisSystem, &(*it)));
                        thisSystem->PATHFINDER_CHECKED = true;
                }
                ++it;
                thisDistance++;
                if(thisDistance >= maxDistance) return nullptr;
        }
        mtx_AddSystem.unlock();
        return nullptr;
}
SYSTEM* UNIVERSE::FindNearestAIBase(const string from, byte maxWarpLevel, bool isp2p) {
        list<PATH_NODE> nodes;
        mtx_AddSystem.lock();
        for(SYSTEM &sys : GALAXY) sys.PATHFINDER_CHECKED = false; ///reset all gals to unchecked
        SYSTEM *start = UNIVERSE::FindSystem(from), *currentSystem = nullptr;
        if(start == NULL) { mtx_AddSystem.unlock(); return nullptr; }
        start->PATHFINDER_CHECKED = true;
        nodes.push_back(PATH_NODE(start, nullptr));
        auto it = nodes.begin();
        while(it != nodes.end()) {
                currentSystem = (*it).sys;
                if(currentSystem->aibases > 0
                                && (isp2p || std::find(p2psystems.begin(), p2psystems.end(), currentSystem->name) == p2psystems.end())
                                && (maxWarpLevel >= 4 || (!IsWarp4(currentSystem->name) && std::find(warp4systems.begin(), warp4systems.end(), currentSystem->name) == warp4systems.end()))) {
                        mtx_AddSystem.unlock(); //IFCRASH: this lock
                        //confirm that a path can be found in case they mapped different parts of map
                        vector<string> buffer = FindPath(from, currentSystem->name, maxWarpLevel == 4);
                        if(buffer.size() != 0) return currentSystem;
                        else mtx_AddSystem.lock();
                }
                for(string &wormhole : currentSystem->wormholes) {
                        SYSTEM *thisSystem = UNIVERSE::FindSystem(GetSystemFromWormhole(wormhole));
                        if(thisSystem == NULL || thisSystem->PATHFINDER_CHECKED) continue;
                        nodes.push_back(PATH_NODE(thisSystem, &(*it)));
                        thisSystem->PATHFINDER_CHECKED = true;
                }
                ++it;
        }
        mtx_AddSystem.unlock();
        return nullptr;
}
