#ifndef ACCOUNTS_H
#define ACCOUNTS_H
#include <iostream>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <windows.h>
#include <stdlib.h>
#include <string>
#include <tchar.h>
#include <tlhelp32.h>
#include <vector>
#include <sstream>
#include <shellapi.h>
#include <math.h>
#include <algorithm>
#include <fstream>
#include <ctime>
#include <unistd.h>
#include <sstream>
#include <conio.h>
#include <dirent.h>
#include <bitset>
#include <mutex>
#include <random>
#include <list>
#include "../Shared code/Helpers.h"
#include "../json/single_include/nlohmann/json.hpp"
#include "../Shared code/GLOBALS.H"

using namespace std;
using json = nlohmann::json;

class CHARACTER {
public:
        string name;
        CHARACTER_CLASS char_class = CLASS_NONE;
        bool canRunAuto = false;
        bool canDG = false;

        byte warpNavigation = 3;

        CHARACTER();
        CHARACTER(json &data);
        bool Load(json data);
        json toJSON();
};

class ACCOUNT {
public:
        string name, password;
        list<CHARACTER> characters;
        bool isP2P = false, canRunAuto = true;
        AI_TASK TASK = TASK_NONE;

        ACCOUNT();
        ACCOUNT(json &data);
        bool Load(json data);
        json toJSON();
};
extern std::mutex mtx_accounts;
extern list<ACCOUNT> accounts;
namespace ACCOUNTS {
/** \brief Load accounts from file to global list */
void Load();
/** \brief Save the accounts data to file */
void Save();
/** \brief Add new account to the list and save the new list */
void Add(string name, string password);
bool AddCharacter(string accName, string charName);
}
#endif // ACCOUNTS_H
