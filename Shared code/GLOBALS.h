#ifndef GLOBALS_H_INCLUDED
#define GLOBALS_H_INCLUDED
#include "../json/single_include/nlohmann/json.hpp"
#include <list>
#include <string>
#include <Helpers.h>

#define PI 3.141592654
#define MSG_END_DELIM "{MSGEND}"
#define USHORT unsigned short
#define DRONE_MAX_PREVIOUS_LOCATIONS 10

using namespace std;
using json = nlohmann::json;
enum CHARACTER_CLASS { CLASS_NONE, CLASS_FC, CLASS_GUNNER, CLASS_ZERK, CLASS_SNIPER, CLASS_SEER, CLASS_SD, CLASS_SHM, CLASS_ENGI};
CHARACTER_CLASS toCharacterClass(int i);

namespace OBJECT_TYPES{
        extern bool ok;
        extern USHORT STAR, PLANET, MOON, ASTEROID, SHIP, DRONE, AIBASE, PLAYERBASE, AUG, DEBRIS, WORMHOLE, CREDITS, UNKNOWN;
        bool Load(json& data);
        void LoadFromFile();
        void Save();
        json to_JSON();
        bool IsUnknown(USHORT id);
        string GetType(USHORT id);
}

enum AI_TASK { TASK_NONE, TASK_IDLE, TASK_DG, TASK_FOLLOW, TASK_PROSPECT, TASK_SEARCH, TASK_EXPLORE, TASK_SPECIAL };
NLOHMANN_JSON_SERIALIZE_ENUM( AI_TASK, {
    {TASK_NONE, nullptr},
    {TASK_NONE, "AITASK_NONE"},
    {TASK_IDLE, "AITASK_IDLE"},
    {TASK_DG, "AITASK_DG"},
    {TASK_FOLLOW, "AITASK_FOLLOW"},
    {TASK_PROSPECT, "AITASK_PROSPECT"},
    {TASK_SEARCH, "AITASK_SEARCH"},
    {TASK_EXPLORE, "AITASK_EXPLORE"},
    {TASK_SPECIAL, "AITASK_SPECIAL"},
})

struct APP_TASK{
        std::string task;
        json data;
        APP_TASK(const std::string t, json d = NULL){ task = t; data = d; }
};
extern std::list<APP_TASK> APP_TASK_QUEUE;

#endif // CONNECTION_GLOBALS_H_INCLUDED
