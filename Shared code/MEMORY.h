#ifndef MEMORY_H
#define MEMORY_H
#include <iostream>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <winsock2.h>
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
#include <thread>
#include <dirent.h>
#include <bitset>
#include <mutex>
#include <random>
#include <chrono>
#include <future>
#include <list>
#include "../json/single_include/nlohmann/json.hpp"
#include "Helpers.h"
using namespace std;
using json = nlohmann::json;

namespace OFFSETS {
struct Offset {
        string name;
        DWORD offset;
        Offset(string n, DWORD o) { name = n; offset = o; }
};
extern std::mutex mtx;
extern bool ok;
extern list<Offset> offsets;
DWORD Get(const string name);
void SetFromFile();
bool Set(json data);
json to_JSON();
void Save();
///---Offsets
extern DWORD EntityID, EntityName, EntityPosX, EntityPosY, EntityPosX2, EntityPosY2, EntityLevel, HullUsed, HullMax, EntityShield, EntityShieldRegen, EntityEnergy, EntityEnergyRegen;
extern DWORD EntityEnergyMax, EntityShieldMax, EntityShipName, EntityShipType, EntityTeamName, EntityRotation, EntityRotation2, EntityDroneLevel, EntityOwner;
extern DWORD EntityIsTractored, EntityStasisType, Inventory, IsPlayer, IsFriend, EntitySize, Keyboard_S, Keyboard_W, Keyboard_Shoot, Keyboard_F, IsSquadMember;
extern DWORD Keyboard_1, Keyboard_X, Keyboard_G, Keyboard_C, EntitySpeed, EntitySpeedMax, EntityMoveVectorX, Keyboard_R, EntityIsVisible, Keyboard_K;

extern DWORD Item_Name, Item_Slot, Item_Type, Item_Tech, Item_Weight, Item_Size, Item_Rarity, Item_ID, Item_ID2, Item_Mods, Item_Equipped, Item_Quantity;
extern DWORD Item_MaxCharge, Item_CurrentCharge, Item_Durability;
}

namespace POINTERS {
struct Pointer {
        string name;
        vector<DWORD> path;
};
extern std::mutex mtx;
extern list<Pointer> pointers;
Pointer* Get(const string name);
void SetFromFile();
bool Set(json data);
json to_JSON();
void Save();
}

/** \brief Part of the Client class, contains calculated addresses */
class MEMORY {
public:
        HANDLE processHandle = NULL;
        bool ok = false; /// set to false on pointer error and initial, set true when loaded ok
        DWORD PlayerName, PlayerData, LoggedInAccount, PlayerSpeed, PlayerLocation;
        DWORD IsLoggedIn, IsCharSelected, IsDocked, IsShooting;
        DWORD ObjListBegin, SelectedTargetID, AutopilotState, Keyboard, StopShip, AutopilotTargetPosX, AutopilotTargetPosY, AutopilotJumps, ThrustState, DangerFactor, ChatMessagesBegin;
        DWORD ChatInput, IsChatOpen, ChatToggle, ChatTabList, ChatTargetTabID, ChatInputStructure, UniverseLayer, AutopilotDestination, Hotkey1Item;
        DWORD IsTargetOptionsOpen, TargetOptions, AutopilotOn, RadarOn, SelectedWeaponID, SelectedWeaponRange, InventoryWindow, InventoryItemOptions, StationWindow;
        DWORD Credits, AccountLogin, AccountPassword, AccountTargetLogin, MCWindow, ClientResolutionX, VisibilityModifier;
        MEMORY();
        void SetHandle(UINT PID);
        void Update();
        string ReadString(DWORD address);
        string ReadStringDirect(DWORD address, size_t length);
        wstring ReadWString(DWORD address);
        wstring ReadWStringDirect(DWORD address, size_t length);

        void Calculate(const string ptrName, DWORD &receiver, bool addLast = true);
        bool Calculate2(const string ptrName, DWORD &receiver, bool addLast = true); // this doesnt throw errors or set ok = false
private:
        DWORD64 moduleBase;
        void GetModuleBaseAddress(UINT PID);
};

#endif // MEMORY_H
