#include <GLOBALS.h>
std::list<APP_TASK> APP_TASK_QUEUE;

bool OBJECT_TYPES::ok = false;
USHORT OBJECT_TYPES::STAR, OBJECT_TYPES::PLANET, OBJECT_TYPES::MOON, OBJECT_TYPES::ASTEROID, OBJECT_TYPES::SHIP;
USHORT OBJECT_TYPES::DRONE, OBJECT_TYPES::AIBASE, OBJECT_TYPES::PLAYERBASE, OBJECT_TYPES::AUG, OBJECT_TYPES::DEBRIS, OBJECT_TYPES::UNKNOWN;
USHORT OBJECT_TYPES::WORMHOLE, OBJECT_TYPES::CREDITS;
bool OBJECT_TYPES::Load(json& data) {
        try {
                OBJECT_TYPES::STAR = (data["star"].is_string())?(USHORT)strtoul(data["star"].get<string>().c_str(), NULL, 16):(USHORT)data["star"].get<int>();
                OBJECT_TYPES::PLANET = (data["planet"].is_string())?(USHORT)strtoul(data["planet"].get<string>().c_str(), NULL, 16):(USHORT)data["planet"].get<int>();
                OBJECT_TYPES::MOON = (data["moon"].is_string())?(USHORT)strtoul(data["moon"].get<string>().c_str(), NULL, 16):(USHORT)data["moon"].get<int>();
                OBJECT_TYPES::ASTEROID = (data["asteroid"].is_string())?(USHORT)strtoul(data["asteroid"].get<string>().c_str(), NULL, 16):(USHORT)data["asteroid"].get<int>();
                OBJECT_TYPES::SHIP = (data["ship"].is_string())?(USHORT)strtoul(data["ship"].get<string>().c_str(), NULL, 16):(USHORT)data["ship"].get<int>();
                OBJECT_TYPES::DRONE = (data["drone"].is_string())?(USHORT)strtoul(data["drone"].get<string>().c_str(), NULL, 16):(USHORT)data["drone"].get<int>();
                OBJECT_TYPES::AIBASE = (data["aibase"].is_string())?(USHORT)strtoul(data["aibase"].get<string>().c_str(), NULL, 16):(USHORT)data["aibase"].get<int>();
                OBJECT_TYPES::PLAYERBASE = (data["playerbase"].is_string())?(USHORT)strtoul(data["playerbase"].get<string>().c_str(), NULL, 16):(USHORT)data["playerbase"].get<int>();
                OBJECT_TYPES::AUG = (data["augmenter"].is_string())?(USHORT)strtoul(data["augmenter"].get<string>().c_str(), NULL, 16):(USHORT)data["augmenter"].get<int>();
                OBJECT_TYPES::DEBRIS = (data["debris"].is_string())?(USHORT)strtoul(data["debris"].get<string>().c_str(), NULL, 16):(USHORT)data["debris"].get<int>();
                OBJECT_TYPES::WORMHOLE = (data["wormhole"].is_string())?(USHORT)strtoul(data["wormhole"].get<string>().c_str(), NULL, 16):(USHORT)data["wormhole"].get<int>();
                OBJECT_TYPES::CREDITS = (data["credits"].is_string())?(USHORT)strtoul(data["credits"].get<string>().c_str(), NULL, 16):(USHORT)data["credits"].get<int>();
        } catch(const exception &e) { printf(CC_RED, "Failed to update object types: %s\n", e.what()); return false; }
        ok = true;
        return true;
}
void OBJECT_TYPES::LoadFromFile() {
        std::ifstream file("data/objecttypes.txt");
        std::string strdata((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if(strdata.length() == 0) { printf(CC_RED, "Object types file not found\n"); return; }
        json data;
        try {
                data = json::parse(strdata.c_str());
                if(!Load(data)) printf(CC_RED, "Error loading object types from text file\n");
        } catch(const exception &e) { printf(CC_RED, "Error loading object types from text file\n"); }
}
void OBJECT_TYPES::Save() {
        ofstream file("data/objecttypes.txt");
        file<<to_JSON().dump(2);
}
json OBJECT_TYPES::to_JSON() {
        json data{
                {"star", STAR},
                {"planet", PLANET},
                {"moon", MOON},
                {"asteroid", ASTEROID},
                {"ship", SHIP},
                {"drone", DRONE},
                {"aibase", AIBASE},
                {"playerbase", PLAYERBASE},
                {"augmenter", AUG},
                {"debris", DEBRIS},
                {"wormhole", WORMHOLE},
                {"credits", CREDITS},
        };
        return data;
}
bool OBJECT_TYPES::IsUnknown(USHORT id) {
        return id != STAR && id != PLANET && id != MOON && id != ASTEROID && id != SHIP && id != DRONE && id != AIBASE && id != PLAYERBASE && id != AUG && id != DEBRIS && id != WORMHOLE && id != CREDITS;
}

string OBJECT_TYPES::GetType(USHORT id) {
        if(id == STAR) return "STAR";
        else if(id == PLANET) return "PLANET";
        else if(id == MOON) return "MOON";
        else if(id == ASTEROID) return "ASTEROID";
        else if(id == SHIP) return "SHIP";
        else if(id == DRONE) return "DRONE";
        else if(id == AIBASE) return "AIBASE";
        else if(id == PLAYERBASE) return "PLAYERBASE";
        else if(id == AUG) return "AUGMENTER";
        else if(id == DEBRIS) return "DEBRIS";
        else if(id == WORMHOLE) return "WORMHOLE";
        else if(id == CREDITS) return "CREDITS";
        return "UNKNOWN";
}

CHARACTER_CLASS toCharacterClass(int i) {
        switch(i) {
        case 0: return CLASS_NONE;
        case 1: return CLASS_FC;
        case 2: return CLASS_GUNNER;
        case 3: return CLASS_ZERK;
        case 4: return CLASS_SNIPER;
        case 5: return CLASS_SEER;
        case 6: return CLASS_SD;
        case 7: return CLASS_SHM;
        case 8: return CLASS_ENGI;
        }
        return CLASS_NONE;
}
