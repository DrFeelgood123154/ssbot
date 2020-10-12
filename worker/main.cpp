#define NTDDI_VERSION NTDDI_VISTA
#define WINVER _WIN32_WINNT_VISTA
#define _WIN32_WINNT _WIN32_WINNT_VISTA
#include <Ws2tcpip.h>
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
#include <iomanip>
#include "../Shared code/Helpers.h"
#include "../Shared code/WebServer.h"
#include "../json/single_include/nlohmann/json.hpp"
#include "ACCOUNTS.h"
#include "MEMORY.h"
#include "UNIVERSE.h"

#define TASK_MESSAGE_TIMEOUT 60
#define WEAPON_ENERGY 53
#define WEAPON_LASER 54
#define WEAPON_HEAT 55
#define WEAPON_MINING 56
#define WEAPON_SURGICAL 57
#define WEAPON_PHYSICAL 58
#define WEAPON_RADIATION 59
#define WEAPON_TRANSFERENCE 60
#define ITEM_FIGHTER 33
#define ITEM_TWEAKGEN 61
#define ITEM_CLOAK 11
#define ITEM_COMMODITY 0
#define ITEM_AUGMENTER 13
#define ITEM_ORE 16
#define ITEM_ALLOY 17

using namespace std;
using json = nlohmann::json;

///Global variables
string gameClientPath = "";
bool APP_PAUSE = false;
const time_t PROCESSBEGINTIME = time(0);
bool IS_INDEPENDENT = false, CONNECT_TO_MASTER = true, REPORT_TO_WEBSERVER = true, PLAY_SOUNDS = true;
bool _IS_CONNECTED = false;
bool* IS_CONNECTED = &_IS_CONNECTED; /// this shouldnt be here?
DWORD TEST_OFFSET;
list<string> ConnectionMessageQueue; /// for sending to controller
clock_t WebServerMessageCooldown = 0;
list<string> WebServerMessageQueue;
HWND LastActiveWindow = NULL;

class BOT;
std::mutex mtx_bots, mtx_openClient;
vector<BOT*> bots;
struct TaskMessage { /// so they can be removed after time if no one reads them
        time_t receivedAt;
        json data;
        TaskMessage(json &d) { receivedAt = time(0); data = d; }
};
mutex mtx_ReceivedMessageQueue;
list<TaskMessage> ReceivedMessageQueue; /// for bot task data etc

struct Point {
        double x, y;
        Point():x(0), y(0) {}
        Point(double px, double py) : x(px), y(py) {}
        void Zero() { x=0; y=0; }
        inline double Distance(double _x, double _y) { return sqrt((_x - x)*(_x - x) + (_y - y)*(_y - y)); }

        bool operator ==(const Point rhs) const { return (x == rhs.x && y == rhs.y); }
        bool operator !=(const Point rhs) const { return (x != rhs.x && y != rhs.y); }
        Point operator +(const double val) const { return Point(x + val, y + val); }
        Point operator -(const double val) const { return Point(x - val, y - val); }
        Point operator +(const Point &val) const { return Point(x + val.x, y + val.y); }
        Point operator -(const Point &val) const { return Point(x - val.x, y - val.y); }
        Point operator *(const double val) const { return Point(x * val, y * val); }
        Point operator /(const double val) const { return Point(x / val, y / val); }
};

///Functions
void GetConfig() {
        vector<string> CONFIGFILE;
        CONFIGFILE = GetFileLines("config.txt");
        if(CONFIGFILE.size() == 0) {
                printf(CC_RED, "\t@No config file. Get it and set the IP of web server.\n");
                getchar();
                exit(0);
        }
        ///webserver
        if(!GetLineByKey(WebServer::address, "webaddress", CONFIGFILE, "")) printf(CC_YELLOW, "\t@WARNING: WEB ADDRESS NOT FOUND IN CONFIG\n");
        string buffer;
        GetLineByKey(buffer, "webport", CONFIGFILE, "80");
        WebServer::port = atoi(buffer.c_str());
        WebServer::useragent = "SS Bot Drone";
        if(GetLineByKey(buffer, "independent", CONFIGFILE, "0")) IS_INDEPENDENT = atoi(buffer.c_str());
        if(GetLineByKey(buffer, "connect_to_master", CONFIGFILE, "1")) CONNECT_TO_MASTER = atoi(buffer.c_str());
        if(GetLineByKey(buffer, "report_to_webserver", CONFIGFILE, "1")) REPORT_TO_WEBSERVER = atoi(buffer.c_str());
        if(GetLineByKey(buffer, "play_sounds", CONFIGFILE, "1")) PLAY_SOUNDS = atoi(buffer.c_str());
        if(GetLineByKey(buffer, "prospect_system_cooldown", CONFIGFILE, "24")) UNIVERSE::PROSPECT_SYSTEM_COOLDOWN = atoi(buffer.c_str());
        UNIVERSE::PROSPECT_SYSTEM_COOLDOWN = UNIVERSE::PROSPECT_SYSTEM_COOLDOWN*60*60;
        if(GetLineByKey(buffer, "TEST_OFFSET", CONFIGFILE, "1")) TEST_OFFSET = strtoul(buffer.c_str(), NULL, 16);
        GetLineByKey(gameClientPath, "client_path", CONFIGFILE, "");
        ///
}

void SoundWarning(string sound){
        if(!PLAY_SOUNDS) return;
        string path = string(APPLOCATION)+"\\"+sound;
        mciSendString(string("open \""+path+"\" type mpegvideo alias sound").c_str(), NULL, 0, NULL);
        mciSendString(string("play "+sound+" from 0").c_str(), NULL, 0, NULL);
        mciSendString("close sound", NULL, 0, NULL);
}
///
struct IgnoredDebris {
        DWORD id = 0;
        clock_t since = clock();
        string where = "";
        IgnoredDebris(DWORD i, const string w) { id = i; where = w; }
};
struct PlayerEncounter {
        string name;
        string where;
        int jumpsAway;
        PlayerEncounter(string &n, string &w){ name = n; where = w; jumpsAway = 0;}
};

/** \brief For handling game GUI */
class GUIElement {
public:
        // C4 = is active?
        // 624 = item option element size?
        // 65C = is selected for input?
        MEMORY* memory = nullptr;
        bool ok = false;
        DWORD address;
        Point position;
        string text1, text2, name;
        bool isToggled, isEnabled;
        byte isSelected;

        GUIElement(MEMORY* pmemory) { memory = pmemory; }

        void Load(DWORD add) {
                DWORD buffer;
                wstring wsBuffer;
                int length = 0;
                address = add;
                char memoryChunk[256];
                size_t bytesRead = 0;
                ReadProcessMemory(memory->processHandle, LongToPtr(address), &memoryChunk, 256, &bytesRead);
                if(bytesRead == 0) {
                        ReadProcessMemory(memory->processHandle, LongToPtr(address), &memoryChunk, 4096 - (address & 0xFFF), &bytesRead);
                }
                length = *reinterpret_cast<int*>((char*)memoryChunk+0x38);
                position.x = (double)length;
                length = *reinterpret_cast<int*>((char*)memoryChunk+0x3C);
                position.y = (double)length;

                length = *reinterpret_cast<int*>((char*)memoryChunk+0xA4);
                buffer = *reinterpret_cast<DWORD*>((char*)memoryChunk+0x9C);
                wsBuffer = memory->ReadWStringDirect(buffer, length);
                text1 = String::ws2s(wsBuffer);

                length = *reinterpret_cast<int*>((char*)memoryChunk+0xB4);
                buffer = *reinterpret_cast<DWORD*>((char*)memoryChunk+0xAC);
                wsBuffer = memory->ReadWStringDirect(buffer, length);
                text2 = String::ws2s(wsBuffer);

                isToggled = *reinterpret_cast<bool*>((char*)memoryChunk+0x98);
                //isEnabled = *reinterpret_cast<bool*>((char*)memoryChunk+0x5E8);
                ReadProcessMemory(memory->processHandle, LongToPtr(address+0x5E8), &isEnabled, 1, NULL);
                ReadProcessMemory(memory->processHandle, LongToPtr(address+0x65C), &isSelected, 1, NULL);

                ReadProcessMemory(memory->processHandle, LongToPtr(address+0x494), &memoryChunk, 12, NULL);
                length = *reinterpret_cast<int*>((char*)memoryChunk+0x8);
                name = memory->ReadStringDirect(*reinterpret_cast<DWORD*>((char*)memoryChunk), length);
                ok = true;
        }

        inline bool IsSelected() { return isSelected == 2; }
        static GUIElement FindInList(DWORD listPointer, const string findPart, MEMORY* memory) {
                GUIElement result(memory);
                DWORD buffer = 0;
                size_t maxElements = 0;
                ReadProcessMemory(memory->processHandle, LongToPtr(listPointer+0x8), &maxElements, 4, NULL);
                ReadProcessMemory(memory->processHandle, LongToPtr(listPointer), &buffer, 4, NULL);
                char memoryChunk[1024];
                ReadProcessMemory(memory->processHandle, LongToPtr(buffer), &memoryChunk, 1024, NULL);
                for(size_t i = 0; i<256 && i<maxElements; i++) {
                        buffer = *reinterpret_cast<DWORD*>((char*)memoryChunk+i*4);
                        result.Load(buffer);
                        if(result.text2.find(findPart) != string::npos) {
                                result.ok = true;
                                return result;
                        }
                }
                result.ok = false;
                return result;
        }
        static DWORD FindChild(DWORD start, const string childName, MEMORY* memory, short maxDepth = 1, short currentDepth = 0) {
                DWORD addbuffer, result = 0;
                GUIElement elbuffer(memory);
                int elements = 0;
                char memoryChunk[1024];
                ReadProcessMemory(memory->processHandle, LongToPtr(start+0xC), &memoryChunk, 12, NULL);
                addbuffer = *reinterpret_cast<DWORD*>((char*)memoryChunk);
                if(addbuffer == 0) return result;
                elements = *reinterpret_cast<int*>((char*)memoryChunk+8);
                size_t bytesToRead = elements*4;
                if(bytesToRead > 1024) bytesToRead = 1024;
                ReadProcessMemory(memory->processHandle, LongToPtr(addbuffer), &memoryChunk, bytesToRead, NULL);
                for(int i = 0; i<elements; i++) {
                        addbuffer = *reinterpret_cast<DWORD*>((char*)memoryChunk+i*4);
                        elbuffer.Load(addbuffer);
                        if(elbuffer.name == childName) return addbuffer;
                        if(currentDepth != maxDepth) result = FindChild(addbuffer, childName, memory, maxDepth, currentDepth+1);
                        if(result != 0) return result;
                }
                return result;
        }
        static DWORD IterateChildren(DWORD start, const string childName, MEMORY* memory, short maxDepth = 1, short currentDepth = 0) {
                DWORD addbuffer, result = 0;
                GUIElement elbuffer(memory);
                int elements = 0;
                char memoryChunk[1024];
                ReadProcessMemory(memory->processHandle, LongToPtr(start+0xC), &memoryChunk, 12, NULL);
                addbuffer = *reinterpret_cast<DWORD*>((char*)memoryChunk);
                if(addbuffer == 0) return result;
                elements = *reinterpret_cast<int*>((char*)memoryChunk+8);
                size_t bytesToRead = elements*4;
                printf("Elements: %i\n", elements);
                if(bytesToRead > 1024) bytesToRead = 1024;
                ReadProcessMemory(memory->processHandle, LongToPtr(addbuffer), &memoryChunk, bytesToRead, NULL);
                for(int i = 0; i<elements; i++) {
                        printf("Element %i of %i\n", i+1, elements);
                        printf("Address %X, offset C->%X\n", start, i*4);
                        addbuffer = *reinterpret_cast<DWORD*>((char*)memoryChunk+i*4);
                        printf("Target: %X\n", addbuffer);
                        elbuffer.Load(addbuffer);
                        if(elbuffer.name != "") printf("Name: %s\n", elbuffer.name.c_str());
                        else if(elbuffer.text1 != "") printf("Text1: %s\n", elbuffer.text1.c_str());
                        else if(elbuffer.text2 != "") printf("Text2: %s\n", elbuffer.text2.c_str());
                        if(elbuffer.name == childName) return addbuffer;
                        if(currentDepth != maxDepth) result = FindChild(addbuffer, childName, memory, maxDepth, currentDepth+1);
                        if(result != 0) return result;
                }
                return result;
        }
};

/** \brief For keeping track of ingame bots */
struct CombatBot {
        DWORD id = 0;
        string name, shipType = "";
        string lastSeenAt = "";
        time_t lastSeenTime = time(0);
        bool isOK = false, isWild = false, isLost = false, isDead = false;
        short lastDurability = 100;
        double lastShieldPercent = 1;
};

class Entity {
public:
        DWORD structurePtr, id;
        USHORT type;
        short _size;
        string name, shipname, shiptype, teamname, owner;
        double shield, shieldMax, shieldRegen, speed, speedMax, energy, energyRegen, rotation;
        float isVisible;
        UINT energyMax, level;
        Point pos, moveVector;
        bool isTractored = false, isSquadMember = false;
        bitset<10> what; // what - contains whether player bot/player/ai
        byte stasisType, relation;
        Entity() {};
        Entity(DWORD addr, MEMORY* memory) { GetData(addr, memory); };
        void GetData(DWORD addr, MEMORY* memory) {
                /// TODO: can optimize with direct string reads
                structurePtr = addr;
                char* memoryChunk[4096];
                ZeroMemory(memoryChunk, 4096);
                size_t bytesRead;
                ReadProcessMemory(memory->processHandle, LongToPtr(addr), &memoryChunk, 4096, &bytesRead);
                if(bytesRead == 0) {
                        ReadProcessMemory(memory->processHandle, LongToPtr(addr), &memoryChunk, 4096 - (addr & 0xFFF), &bytesRead);
                }
                id = *reinterpret_cast<DWORD*>((char*)memoryChunk+OFFSETS::EntityID);
                type = *reinterpret_cast<USHORT*>((char*)memoryChunk);
                name = memory->ReadString(addr+OFFSETS::EntityName);
                teamname = memory->ReadString(addr+OFFSETS::EntityTeamName);
                owner = memory->ReadString(addr+OFFSETS::EntityOwner);
                speed = *reinterpret_cast<double*>((char*)memoryChunk+OFFSETS::EntitySpeed);
                speedMax = *reinterpret_cast<double*>((char*)memoryChunk+OFFSETS::EntitySpeedMax);
                isVisible = *reinterpret_cast<float*>((char*)memoryChunk+OFFSETS::EntityIsVisible);
                moveVector.x = *reinterpret_cast<double*>((char*)memoryChunk+OFFSETS::EntityMoveVectorX);
                moveVector.y = *reinterpret_cast<double*>((char*)memoryChunk+OFFSETS::EntityMoveVectorX+0x8);
                shield = *reinterpret_cast<double*>((char*)memoryChunk+OFFSETS::EntityShield);
                shieldMax = *reinterpret_cast<double*>((char*)memoryChunk+OFFSETS::EntityShieldMax);
                shieldRegen = *reinterpret_cast<double*>((char*)memoryChunk+OFFSETS::EntityShieldRegen);
                _size = *reinterpret_cast<short*>((char*)memoryChunk+OFFSETS::EntitySize);
                pos.x = *reinterpret_cast<double*>((char*)memoryChunk+OFFSETS::EntityPosX2);
                pos.y = *reinterpret_cast<double*>((char*)memoryChunk+OFFSETS::EntityPosY2);
                energy = *reinterpret_cast<double*>((char*)memoryChunk+OFFSETS::EntityEnergy);
                energyRegen = *reinterpret_cast<double*>((char*)memoryChunk+OFFSETS::EntityEnergyRegen);
                energyMax = *reinterpret_cast<int*>((char*)memoryChunk+OFFSETS::EntityEnergyMax);
                rotation = *reinterpret_cast<double*>((char*)memoryChunk+OFFSETS::EntityRotation);
                level = *reinterpret_cast<int*>((char*)memoryChunk+OFFSETS::EntityLevel);
                stasisType = (int)*reinterpret_cast<byte*>((char*)memoryChunk+OFFSETS::EntityStasisType);
                what = *reinterpret_cast<bitset<10>*>((char*)memoryChunk+OFFSETS::EntityIsTractored);
                isTractored = what[2] == 1;
                if(type == OBJECT_TYPES::SHIP || type == OBJECT_TYPES::DRONE) {
                        shipname = memory->ReadString(addr+OFFSETS::EntityShipName);
                        shiptype = memory->ReadString(addr+OFFSETS::EntityShipType);
                        what = *reinterpret_cast<bitset<10>*>((char*)memoryChunk+OFFSETS::IsPlayer);
                        isSquadMember = *reinterpret_cast<bool*>((char*)memoryChunk+OFFSETS::IsSquadMember);
                }

                relation = *reinterpret_cast<byte*>((byte*)memoryChunk+OFFSETS::IsFriend);
        }

        bool IsPlayer() { return what[1] == 1; }
        bool IsPlayerBot() { return what[8] == 1; }
        bool IsMyBot() { return what[9] == 1; }
        bool IsFriend() { return relation >= 2; }
        bool IsHostile() { return relation == 1; }
        bool IsNeutral() { return relation == 0; }
        bool IsFighter() { return (shiptype == "Light Fighter" && shipname.find("Fighter") != string::npos); }
        bool IsMissile() { return (shiptype == "Light Fighter" && shipname.find("Missile") != string::npos); }
        bool IsRadded() { return (shield <= 10000 && level == 0); }
        bool IsVisible() { return (isVisible != 0); }
};

/** \brief Contains player data and ship data */
class Player {
public:
        string loggedAccount, name = "", location = "", universeLayer = "";
        int hullUsed, hullMax;
        Entity ship;

        Player() {};

};

/** \brief Character settings */
class Settings {
public:
        int followRange = 200, followRangeMax = 1000, minHullToUnload = 200;
        double minDF = 1.30, maxDF = 2.30, tractorRange = 1200, prospectTractorRange = 1000, scoopRange = 450, maxLootDistance = 2000, surfaceScannerRange = 500;
        bool useCombatBots = true, waitForSlaves = true, canScoopLoots = true, canRevive = true, canRepair = true, canMarkAsEnemy = false;
        bool canHeal = true, canAttack = true, canUseFighters = true, canTractorEnemy = true;
        string homeSystem = "Explicit", unloadLoots_system = "Explicit", unloadLoots_base = "NoGnome";
        vector<string> dgLayers{"EarthForce"};
        vector<string> prospectLayers{"EarthForce", "Wild Space"};
        vector<string> prospectIgnoreResources{"Metals", "Nuclear Waste", "Silicon", "Pyrite"};

        Settings() {}
};

/** \brief For handling chat messages, part of client */
struct ChatMessage {
        string tab, sender, text;
        ChatMessage() {};
        ChatMessage(const string _tab, const string _sender, const string _text) : tab(_tab), sender(_sender), text(_text) {};
};
class Chat {
public:
        UINT MSG_SIZE = 0x2C, messagesRead = 0;
        MEMORY* memory;
        DWORD lastMessage = 0, firstMessage = 0;
        list<ChatMessage> newMessages;

        void Update() {
                if(lastMessage == 0) {
                        ReadProcessMemory(memory->processHandle, LongToPtr(memory->ChatMessagesBegin), &firstMessage, 4, NULL);
                        ReadProcessMemory(memory->processHandle, LongToPtr(memory->ChatMessagesBegin+0x4), &lastMessage, 4, NULL); /// get the chat end
                        messagesRead = (lastMessage - firstMessage)/MSG_SIZE;
                        return;
                }
                ///check if moved
                DWORD thisFirst;
                ReadProcessMemory(memory->processHandle, LongToPtr(memory->ChatMessagesBegin), &thisFirst, 4, NULL);
                if(thisFirst != firstMessage)/// moved, recaculate
                        lastMessage = thisFirst+(messagesRead*MSG_SIZE);
                firstMessage = thisFirst;
                ///read new
                newMessages.clear();
                DWORD msgEndAt;
                ReadProcessMemory(memory->processHandle, LongToPtr(memory->ChatMessagesBegin+0x4), &msgEndAt, 4, NULL);
                UINT toRead = msgEndAt - lastMessage;
                if(toRead != 0) {
                        if(toRead > 4096) toRead = 4096;
                        char memoryChunk[4096];
                        ReadProcessMemory(memory->processHandle, LongToPtr(lastMessage), &memoryChunk, toRead, NULL);
                        UINT bytesRead = 0;
                        while(bytesRead < toRead && toRead - bytesRead >= MSG_SIZE) {
                                newMessages.push_back(ChatMessage());
                                ///get tab
                                DWORD add;
                                int length = *reinterpret_cast<int*>((char*)memoryChunk+bytesRead+0x10);
                                int maxLength = *reinterpret_cast<int*>((char*)memoryChunk+bytesRead+0x14);
                                if(length < 0 || length > 100) {
                                        printf("%X = %i\n", memory->processHandle, length);
                                        memory->Update();
                                        return;
                                }
                                if(maxLength >= 16) {
                                        add = *reinterpret_cast<DWORD*>((char*)memoryChunk+bytesRead);
                                        newMessages.back().tab = memory->ReadStringDirect(add, length);
                                } else newMessages.back().tab = string(memoryChunk+bytesRead, length);
                                ///get text
                                length = *reinterpret_cast<int*>((char*)memoryChunk+bytesRead+0x1C);
                                add = *reinterpret_cast<DWORD*>((char*)memoryChunk+bytesRead+0x18);
                                wstring wsbuffer = memory->ReadWStringDirect(add, length);
                                string textBuffer = String::ws2s(wsbuffer);
                                ///split to sender/text
                                if(textBuffer[1] == '(') {
                                        size_t pos = textBuffer.find(")");
                                        if(pos != string::npos) {
                                                newMessages.back().sender = textBuffer.substr(2, pos-2);
                                                newMessages.back().text = textBuffer.substr(pos+1);
                                        }
                                } else newMessages.back().text = textBuffer;
                                bytesRead+=MSG_SIZE;
                                messagesRead++;
                        }
                        lastMessage = msgEndAt;
                }
        }
        DWORD GetTabByName(const string tabName) {
                DWORD start, next, buffer; /// buffer for holding address to string
                wchar_t wsReadBuffer[50];
                wstring wsName;
                string nameHolder;
                ReadProcessMemory(memory->processHandle, LongToPtr(memory->ChatTabList), &start, 4, NULL);
                next = start;
                char memoryChunk[512];
                size_t control = 0;
                do {
                        ReadProcessMemory(memory->processHandle, LongToPtr(next), &memoryChunk, 512, NULL);
                        buffer = *reinterpret_cast<DWORD*>(memoryChunk+0x08);
                        ReadProcessMemory(memory->processHandle, LongToPtr(buffer), &wsReadBuffer, 50, NULL);
                        wsName = wsReadBuffer;
                        nameHolder = String::ws2s(wsName);
                        if(tabName == nameHolder.c_str()) {
                                buffer = *reinterpret_cast<DWORD*>(memoryChunk+0x0C);
                                return buffer;
                        }
                        next = *reinterpret_cast<DWORD*>(memoryChunk);
                        control++;
                } while(next != start && control < 100 );
                if(control >= 100) {
                        printf(CC_YELLOW, "WARNING: control iterator reached 100 in GetTabByName\n");
                        memory->Update();
                }
                return 0;
        }
        bool SelectTab(const string name) {
                DWORD id = GetTabByName(name);
                if(id == 0) return false;
                WriteProcessMemory(memory->processHandle, LongToPtr(memory->ChatTargetTabID), &id, 4, NULL);
                return true;
        }
        DWORD GetSelectedTab() {
                DWORD ret;
                ReadProcessMemory(memory->processHandle, LongToPtr(memory->ChatTargetTabID), &ret, 4, NULL);
                return ret;
        }
        ChatMessage* FindMessage(const string msgPart, const string inTab = "") {
                for(ChatMessage &msg : newMessages) if(msg.text.find(msgPart) != string::npos && (inTab == "" || msg.tab == inTab)) return &msg;
                return NULL;
        }
};

struct Weapon {
        string name;
        DWORD id;
        short type, id2;
        double range;
};

class InventoryItem {
public:
        DWORD address = 0, id;
        MEMORY* memory;
        string name;
        byte tech, rarity, slot;
        short type, id2;
        int weight, _size, quantity, durability;
        bool isEquipped;
        double charge, chargeMax;

        InventoryItem(DWORD a, MEMORY* m) { address = a; memory = m; }

        void GetData() {
                DWORD buffer;
                char memoryChunk[200];
                ReadProcessMemory(memory->processHandle, LongToPtr(address), &memoryChunk, 200, NULL);
                int length = *reinterpret_cast<int*>((char*)memoryChunk+OFFSETS::Item_Name+0x10);
                if(length > 100) {
                        memory->Update();
                        return;
                }
                int maxLength = *reinterpret_cast<int*>((char*)memoryChunk+OFFSETS::Item_Name+0x14);
                if(maxLength >= 16) {
                        buffer = *reinterpret_cast<DWORD*>((char*)memoryChunk+OFFSETS::Item_Name);
                        name = memory->ReadStringDirect(buffer, length);
                } else name = string(memoryChunk+OFFSETS::Item_Name, length);
                slot = *reinterpret_cast<byte*>((char*)memoryChunk+OFFSETS::Item_Slot);
                rarity = *reinterpret_cast<byte*>((char*)memoryChunk+OFFSETS::Item_Rarity);
                tech = *reinterpret_cast<byte*>((char*)memoryChunk+OFFSETS::Item_Tech);
                type = *reinterpret_cast<short*>((char*)memoryChunk+OFFSETS::Item_Type);
                id2 = *reinterpret_cast<short*>((char*)memoryChunk+OFFSETS::Item_ID2);
                weight = *reinterpret_cast<int*>((char*)memoryChunk+OFFSETS::Item_Weight);
                _size = *reinterpret_cast<int*>((char*)memoryChunk+OFFSETS::Item_Size);
                quantity = *reinterpret_cast<int*>((char*)memoryChunk+OFFSETS::Item_Quantity);
                durability = *reinterpret_cast<int*>((char*)memoryChunk+OFFSETS::Item_Durability);
                if(durability != 0) durability /= 100;
                id = *reinterpret_cast<DWORD*>((char*)memoryChunk+OFFSETS::Item_ID);
                bitset<8> buffer2;
                buffer2 = *reinterpret_cast<bitset<8>*>((char*)memoryChunk+OFFSETS::Item_Equipped);
                isEquipped = buffer2[0] == 1;
                charge = *reinterpret_cast<double*>((char*)memoryChunk+OFFSETS::Item_CurrentCharge);
                chargeMax = *reinterpret_cast<double*>((char*)memoryChunk+OFFSETS::Item_MaxCharge);
        }
        inline double GetCharge() {
                if(chargeMax == 0) return 1;
                return charge / chargeMax;
        }
        inline bool IsCharged() {
                if(chargeMax == 0) return 1;
                return charge >= chargeMax;
        }
        void UseItem() {
                if(address == 0) return;
                WriteProcessMemory(memory->processHandle, LongToPtr(memory->Hotkey1Item), &id2, 2, NULL);
                WriteProcessMemory(memory->processHandle, LongToPtr(memory->Hotkey1Item+0x4), &id, 4, NULL);
                bool buff = true;
                WriteProcessMemory(memory->processHandle, LongToPtr(memory->Keyboard+OFFSETS::Keyboard_1), &buff, 1, NULL);
                Sleep(20);
                buff = false;
                WriteProcessMemory(memory->processHandle, LongToPtr(memory->Keyboard+OFFSETS::Keyboard_1), &buff, 1, NULL);
                GetData();
        }
};

class Inventory {
public:
        std::future<void> ASYNC_USEITEM;
        std::unique_ptr<std::mutex> mtx_inventory;
        list<InventoryItem> items;
        MEMORY* memory = 0;
        DWORD playerData;
        InventoryItem *transwarp = nullptr, *amplifier = nullptr, *enlightement = nullptr, *travelfield = nullptr, *fighters = nullptr, *shieldTweakGen = nullptr;
        InventoryItem *surfaceScanner = nullptr, *systemScanner = nullptr, *prospectTractor = nullptr, *cloak = nullptr, *torch = nullptr, *advancedBlocker = nullptr;

        Inventory() { mtx_inventory = std::unique_ptr<std::mutex>(new std::mutex()); }

        void Update() {
                mtx_inventory->lock();
                items.clear();
                transwarp = nullptr;
                amplifier = nullptr;
                enlightement = nullptr;
                travelfield = nullptr;
                fighters = nullptr;
                shieldTweakGen = nullptr;
                surfaceScanner = nullptr;
                systemScanner = nullptr;
                prospectTractor = nullptr;
                cloak = nullptr;
                torch = nullptr;
                advancedBlocker = nullptr;
                DWORD inventory = 0;
                ReadProcessMemory(memory->processHandle, LongToPtr(playerData+OFFSETS::Inventory), &inventory, 4, NULL);
                DWORD next = inventory, currentItem = 0;
                ReadProcessMemory(memory->processHandle, LongToPtr(inventory), &next, 4, NULL);
                while(next != inventory) {
                        char addressChunk[12];
                        ReadProcessMemory(memory->processHandle, LongToPtr(next), &addressChunk, 12, NULL);
                        next = *reinterpret_cast<DWORD*>((char*)addressChunk);
                        currentItem = *reinterpret_cast<DWORD*>((char*)addressChunk+0x8);
                        items.push_back(InventoryItem(currentItem, memory));
                        items.back().GetData();
                        if(items.back().name == "Blue Photon Armada Transwarp") transwarp = &items.back();
                        else if(items.back().name == "Amplified Field Amplification Device") amplifier = &items.back();
                        else if(items.back().name == "Commander's Enlightenment") enlightement = &items.back();
                        else if(items.back().name.find("Traveling Field") != string::npos) {
                                if(travelfield == nullptr) travelfield = &items.back();
                                else if(items.back().tech > travelfield->tech) travelfield = &items.back();
                        } else if(items.back().type == ITEM_FIGHTER && items.back().durability > 0 &&
                                        (items.back().name == "Panther Kitten Fighter" || items.back().name == "Qokuji'qii")) {
                                if(fighters == nullptr) fighters = &items.back();
                                else if(items.back().tech > fighters->tech) fighters = &items.back();
                        } else if(items.back().name.find("Shield Service") != string::npos && items.back().type == ITEM_TWEAKGEN) {
                                if(items.back().isEquipped) shieldTweakGen = &items.back();
                        } else if(items.back().name == "Assayer's Scanner") {
                                surfaceScanner = &items.back();
                        } else if(items.back().name == "EDVAC Intensive Prospecting Scanner") {
                                systemScanner = &items.back();
                        } else if(items.back().name.find("Drilling Beam") != string::npos) {
                                prospectTractor = &items.back();
                        } else if(items.back().type == ITEM_CLOAK) {
                                cloak = &items.back();
                        } else if(items.back().name.find("Torch") != string::npos && items.back().isEquipped) {
                                if(torch == nullptr) torch = &items.back();
                                else if(items.back().tech > torch->tech) torch = &items.back();
                        }  else if(items.back().name.find("Advanced Blockade") != string::npos && items.back().type == ITEM_TWEAKGEN) {
                                if(items.back().isEquipped) advancedBlocker = &items.back();
                        }
                }
                mtx_inventory->unlock();
        }

        InventoryItem* GetItem(const string name) {
                InventoryItem* ret = nullptr;
                for(InventoryItem &item : items) if(item.name == name) {
                                ret = &item;
                                if(ret->isEquipped) return ret;
                        }
                return ret;
        }
        InventoryItem* GetItemByPart(const string name) {
                InventoryItem* ret = nullptr;
                for(InventoryItem &item : items) if(item.name.find(name) != string::npos) {
                                if(ret == nullptr) ret = &item;
                                if(ret->isEquipped) return ret;
                                if(item.tech > ret->tech) ret = &item;
                        }
                return ret;
        }

        void EquipItem(InventoryItem *item, bool intoState) {
                if(item == nullptr) return;
                if(!ASYNC_USEITEM.valid() || IsThreadFinished(ASYNC_USEITEM)) {
                        ASYNC_USEITEM = std::async(std::launch::async, [](Inventory *inventory, InventoryItem *item, bool intoState) {
                                if(item->address == 0) return;
                                inventory->mtx_inventory->lock();
                                clock_t startTime = clock();
                                while(item->isEquipped != intoState && clock() < startTime + 3000) {
                                        WriteProcessMemory(inventory->memory->processHandle, LongToPtr(inventory->memory->Hotkey1Item), &item->id2, 2, NULL);
                                        WriteProcessMemory(inventory->memory->processHandle, LongToPtr(inventory->memory->Hotkey1Item+0x4), &item->id, 4, NULL);
                                        bool buff = true;
                                        WriteProcessMemory(inventory->memory->processHandle, LongToPtr(inventory->memory->Keyboard+OFFSETS::Keyboard_1), &buff, 1, NULL);
                                        Sleep(20);
                                        buff = false;
                                        WriteProcessMemory(inventory->memory->processHandle, LongToPtr(inventory->memory->Keyboard+OFFSETS::Keyboard_1), &buff, 1, NULL);
                                        Sleep(500);
                                        item->GetData();
                                }
                                inventory->mtx_inventory->unlock();
                        }, this, item, intoState);
                }
        }
};

/** \brief For reading game client's memory and such */
class GameClient {
public:
        std::unique_ptr<std::mutex> mtx_playerData, mtx_objectsList;
        time_t clientBeginTime;
        MEMORY memory;
        Player* player = 0;
        Chat chat;
        Inventory inventory;
        HWND clientWindow = NULL;
        clock_t lastCrashCheck = 0;
        ///--- CLIENT DATA
        DWORD playerData, objListBegin, objListEnd;
        bool isLoggedIn = false, isCharSelected = false, isDocked = false;
        list<Entity> entities;
        vector<Entity*> enemies, hostiles, neutrals, friends, players, admins;
        vector<IgnoredDebris> ignoredEnemies;
        ///---
        GameClient() {
                mtx_playerData = std::unique_ptr<std::mutex>(new std::mutex());
                mtx_objectsList = std::unique_ptr<std::mutex>(new std::mutex());
        };
        GameClient(Player* p) {
                player = p;
                mtx_playerData = std::unique_ptr<std::mutex>(new std::mutex());
                mtx_objectsList = std::unique_ptr<std::mutex>(new std::mutex());
        }

        void SetClient(UINT _PID, bool waitForClient = false) {
                PID = _PID;
                if(waitForClient) sleep(5);
                GetWindowHandle();
                if(clientWindow == NULL) {
                        sleep(5);
                        GetWindowHandle();
                }
                clientBeginTime = time(0);
                memory.SetHandle(PID);
                clock_t start = clock();
                while(clock() < start+10*CLOCKS_PER_SEC && !memory.Calculate2("universe_layer", memory.UniverseLayer)) Sleep(100);
                if(waitForClient) sleep(4);
                memory.Update();
                chat.memory = &memory;
                chat.lastMessage = 0;
                inventory.memory = &memory;
        }
        bool Update() {
                if(player == NULL || !memory.ok || !OFFSETS::ok) return false;
                mtx_playerData->lock();
                if(player->name == "") {
                        player->loggedAccount = "";
                        player->loggedAccount = memory.ReadString(memory.LoggedInAccount);
                        player->name = memory.ReadString(memory.PlayerName);
                }
                ReadProcessMemory(memory.processHandle, LongToPtr(memory.PlayerData), &playerData, 4, NULL);
                ReadProcessMemory(memory.processHandle, LongToPtr(memory.IsLoggedIn), &isLoggedIn, 1, NULL);
                ReadProcessMemory(memory.processHandle, LongToPtr(memory.IsCharSelected), &isCharSelected, 1, NULL);
                ReadProcessMemory(memory.processHandle, LongToPtr(memory.ObjListBegin), &objListBegin, 4, NULL);
                ReadProcessMemory(memory.processHandle, LongToPtr(memory.ObjListBegin+0x4), &objListEnd, 4, NULL);
                if(isCharSelected) {
                        wstring buffer;
                        ReadProcessMemory(memory.processHandle, LongToPtr(memory.IsDocked), &isDocked, 1, NULL);
                        player->ship.GetData(playerData, &memory);
                        player->location = "";
                        player->location = memory.ReadString(memory.PlayerLocation);
                        buffer = memory.ReadWString(memory.UniverseLayer);
                        player->universeLayer = "";
                        player->universeLayer = String::ws2s(buffer);
                        ReadProcessMemory(memory.processHandle, LongToPtr(playerData+OFFSETS::HullUsed), &player->hullUsed, 4, NULL);
                        ReadProcessMemory(memory.processHandle, LongToPtr(playerData+OFFSETS::HullMax), &player->hullMax, 4, NULL);
                        inventory.playerData = playerData;
                        mtx_playerData->unlock();
                        chat.Update();
                } else mtx_playerData->unlock();
                GetObjectList();
                return true;
        }
        bool UpdateAll() {
                memory.Update();
                if(player == NULL || !memory.ok || !OFFSETS::ok) return false;
                player->loggedAccount = "";
                player->loggedAccount = memory.ReadString(memory.LoggedInAccount);
                player->name = "";
                player->name = memory.ReadString(memory.PlayerName);
                return true;
        }
        bool IsAttached() { return PID != 0; }
        UINT GetPID() { return PID; }
        ///--- CLIENT DATA
        void GetObjectList() {
                mtx_objectsList->lock();
                entities.clear();
                enemies.clear();
                hostiles.clear();
                neutrals.clear();
                friends.clear();
                players.clear();
                admins.clear();
                vector<Entity*> tempDrones;
                char* memoryChunk[16384];
                DWORD toRead = objListEnd - objListBegin;
                if(toRead > 16384) toRead = 16384;
                ReadProcessMemory(memory.processHandle, LongToPtr(objListBegin), &memoryChunk, toRead, NULL);
                DWORD ptr = 0;
                for(DWORD i=0x0; i<toRead; i+=0x4) {
                        ptr = *reinterpret_cast<DWORD*>((char*)memoryChunk+i);
                        if(ptr == 0) continue;
                        entities.push_back(Entity(ptr, &memory));
                        if(entities.back().type == OBJECT_TYPES::SHIP || entities.back().type == OBJECT_TYPES::DRONE) {
                                if(entities.back().IsFriend()) friends.push_back(&entities.back());
                                else if(entities.back().IsHostile()) hostiles.push_back(&entities.back());
                                else neutrals.push_back(&entities.back());
                                if(entities.back().IsPlayer() && entities.back().id != player->ship.id) players.push_back(&entities.back());
                                if(!entities.back().IsFriend()) { // potential attack targets
                                        if((entities.back().IsPlayer() || entities.back().IsPlayerBot()) && player->universeLayer == "EarthForce") continue;
                                        if(entities.back().type == OBJECT_TYPES::DRONE) { tempDrones.push_back(&entities.back()); continue; }
                                        if(entities.back().shipname.find("Fighter") != string::npos || entities.back().shipname.find("Missile") != string::npos) continue;
                                        if(entities.back().shipname == "Pax Astralogica") continue;
                                        if(entities.back().teamname.find("Earth Force Patrol") != string::npos) continue;
                                        if(entities.back().teamname.find("Earth Force Task") != string::npos || entities.back().teamname.find("Blue Photon") != string::npos) continue;
                                        if(entities.back().teamname == "Admins" || entities.back().shipname == "Administrator" || entities.back().shipname == "Developer" || entities.back().name == "Blue Dwarf"
                                           || entities.back().name == "ssRyan" || entities.back().name == "The Voomy One") admins.push_back(&entities.back());
                                        bool isIgnored = false;
                                        for(IgnoredDebris &ignored : ignoredEnemies) if(ignored.id == entities.back().id) { isIgnored = true; break; }
                                        if(isIgnored) continue;
                                        enemies.push_back(&entities.back());
                                }
                        }
                }
                for(Entity *drone : tempDrones) {
                        bool isPlayer = false;
                        for(Entity *ePlayer : players) if(drone->owner == ePlayer->name) { isPlayer = true; break; }
                        if(!isPlayer) enemies.push_back(drone);
                }
                mtx_objectsList->unlock();
        }

        DWORD GetSelectedTarget() {
                DWORD buff;
                ReadProcessMemory(memory.processHandle, LongToPtr(memory.SelectedTargetID), &buff, 4, NULL);
                return buff;
        }
        bool CheckCrash() {
                lastCrashCheck = clock();
                if(PID == 0 || clientWindow == NULL) return true;
                bool crashed = false;
                EnumWindowsGetClientStruct data = EnumWindowsGetClientStruct(PID);
                EnumWindows(EnumWindowsGetClient, (LPARAM)&data);;
                if(data.hwnd == NULL) crashed = true;
                if(!crashed) {
                        data.hwnd = NULL;
                        EnumWindows(EnumWindowsCrashCheck, (LPARAM)&data);
                        if(data.pid == 0) crashed = true;
                }
                if(crashed) {
                        printf("%s client crashed\n", player->name.c_str());
                        PID = 0;
                        clientWindow = NULL;
                        TerminateProcess(memory.processHandle, 1);
                        memory.processHandle = NULL;
                        return true;
                }
                return false;
        }
private:
        /**< Process ID */
        UINT PID = 0;

        /// WINDOW HANDLE
        static bool IsValidClientWindow(HWND &hwnd) {
                if(!IsWindowVisible(hwnd)) return false;
                char windowClass[100];
                GetClassName(hwnd, windowClass, sizeof(windowClass));
                string buffer = windowClass;
                if(buffer.find("TeamViewer") != string::npos) return false;
                char buffer2[256];
                GetWindowText(hwnd, (LPSTR)buffer2, 255);
                if(strcmp(buffer2, "Crash Dump") == 0) return false;
                return true;
        }
        struct EnumWindowsGetClientStruct {
                UINT pid;
                HWND hwnd = NULL;
                EnumWindowsGetClientStruct(UINT p) { pid = p; }
        };
        static BOOL CALLBACK EnumWindowsGetClient(HWND hwnd, LPARAM lparam) {
                EnumWindowsGetClientStruct *_data = (EnumWindowsGetClientStruct*)lparam;
                DWORD windowPID = 0;
                GetWindowThreadProcessId(hwnd, &windowPID);

                if(windowPID == _data->pid && IsValidClientWindow(hwnd)) {
                        _data->hwnd = hwnd;
                        return false;
                }
                return true;
        }
        static BOOL CALLBACK EnumWindowsCrashCheck(HWND hwnd, LPARAM lparam) {
                EnumWindowsGetClientStruct *_data = (EnumWindowsGetClientStruct*)lparam;
                DWORD windowPID = 0;
                GetWindowThreadProcessId(hwnd, &windowPID);
                char buff[256];
                if(windowPID == _data->pid) {
                        GetWindowText(hwnd, (LPSTR)buff, 255);
                        if(strcmp(buff, "Crash Dump") == 0) { _data->pid = 0; return false; }
                }
                return true;
        }
        void GetWindowHandle() {
                if(PID == 0) return;
                clientWindow = NULL;
                clock_t start = clock();
                while(clientWindow == NULL && clock() < start+60*CLOCKS_PER_SEC) {
                        EnumWindowsGetClientStruct data = EnumWindowsGetClientStruct(PID);
                        EnumWindows(EnumWindowsGetClient, (LPARAM)&data);
                        clientWindow = data.hwnd;
                }
        }
};
/** \brief Variables contain account data, character data and client data (for memory), the class itself contains AI */
class BOT {
public:
        time_t beginTime;
        thread THREAD;
        std::mutex mtx, mtx_debris;
        bool TERMINATE = false, PAUSE = false;
        ACCOUNT account;
        CHARACTER character;
        GameClient client;
        Player player;
        Settings settings;
        bool active = false; // has an assigned account from controller?
        clock_t lastPointersUpdate = 0, lastTaskUpdateRequest = 0, adminReportTime = 0;
        time_t taskNoneReceived = 0; // set when bot receives TASK_NONE, if idle for too long log out/close client
        ///DEBUG
        vector<string> THIS_LOOP_ACTIONS;
        ///TASK RELATED
        AI_TASK TASK;
        string task_followTargetName, task_targetLocation, task_adjacentLocation;
        bool task_followPause = false;
        vector<string> followers;
        ///GLOBAL AI VARIABLES
        std::future<void> ASYNC_EXPLORESYSTEM, ASYNC_TOGGLECHATINPUT, ASYNC_MOUSEMOVE, ASYNC_CLICKLEFT, ASYNC_CLICKRIGHT;
        std::future<void> ASYNC_THRUST, ASYNC_SCOOP, ASYNC_AUTOPILOT, ASYNC_WARP, ASYNC_SHOOT;
        vector<Weapon> weapons;
        short thisDungeonLevel;
        bool isDead = false, isInDungeon = false, isInInstance = false;
        bool wasLoggedIn = false, wasDead = false, wasDocked = false;
        bool isReady = false, isBossKilled = false, isUnderAttack = false, isRetrievingCombatBot = false;
        bool isAttacking = false, isScooping = false, isRegening = false, isTranswarping = false, isUnloadingLoots = false, isRetreating = false, isWaitingForBots = false, isRepairing = false;
        bool returnToBase = false, isReturningToBase = false, finishDGAndWait = false, hasReportedDGWait = false;
        clock_t systemEnterTime = 0, idleUntil = 0, underAttackTrigger = 0, errorReportTime1 = 0;
        bool hasExploredThisSystem = false, hasEnteredTargetDungeon = false, hasReportedPlayer = false, hasReportedStuck = false;
        bool hasInit_prospector = false;
        bool canUseTorch = true;
        UINT thisSystemID = 0;
        int hullUsedOnDock = 0;
        list<InventoryItem> itemsOnDock;
        string lastLocation, systemOwnedBy, repairInSystem = "";
        vector<string> previousLocations, currentPath;
        vector<PlayerEncounter> playerEncounters;
        DWORD targetWormholeID = 0;
        Entity* targetWormhole = NULL;
        DUNGEON thisDungeon;
        DUNGEONLEVEL* targetDungeonLevel = nullptr;
        UINT dgsCompleted = 0;

        DWORD targetID = 0, followTargetID = 0, scoopTargetID = 0;
        clock_t lastTargetTime, lastFollowTargetTime, scoopTargetTime = 0, scoopTargetTime_LongDistance = 0, targetTime = 0;
        Entity lastTarget, lastFollowTarget, lastScoopTarget;
        Entity *target = NULL, *followTarget = NULL, *scoopTarget = NULL;

        Point autopilotTarget;
        double shieldPercent, energyPercent, shieldLostOverTime = 0, timeToDeath = 0;
        clock_t nextCombatBotCheckTime = 0, nextInventoryCheck = 0, nextDebrisCheck = 0, nextTravelFieldCheck = 0, nextTranswarpCheck = 0, nextShieldCheck = 0;
        vector<CombatBot> combatBots;
        vector<Entity*> debris, credits;
        vector<IgnoredDebris> ignoredDebris;
        long long creditsOnDGStart = 0;
        clock_t dgStartTime = 0;
        vector<double> shieldWatch;

        BOT() {
                beginTime = time(0);
                player = Player();
                client = GameClient(&player);
                for(size_t i = 0; i<DRONE_MAX_PREVIOUS_LOCATIONS; i++) previousLocations.push_back("");
        }
        void Init(UINT PID = 0, bool hasJustOpenedClient = false) {
                ///if given a pid, find and attach the client
                if(PID != 0) {
                        client.SetClient(PID, hasJustOpenedClient);
                        client.Update();
                        if(account.name == "") account.name = player.loggedAccount;
                }
                THREAD = thread(BOT::loop, this);

                //TEMP
                character.warpNavigation = 3;
                account.isP2P = false;
                TASK = TASK_DG;
                task_followTargetName = "Gate to Sol";
                settings.useCombatBots = true;
        }
        bool RequestTaskUpdate(json &data) {
                if(clock() < lastTaskUpdateRequest + 5*1000) return false;
                lastTaskUpdateRequest = clock();
                data["action"] = "RequestTaskUpdate";
                data["task"] = TASK;
                data["character"] = player.name;
                ConnectionMessageQueue.push_back(data.dump());
                return true;
        }
        void ReportTaskFail(json &data) {
                if(*IS_CONNECTED && !IS_INDEPENDENT) {
                        data["action"] = "TaskFail";
                        data["task"] = TASK;
                        data["character"] = player.name;
                        ConnectionMessageQueue.push_back(data.dump());
                }
        }
        void ProcessTaskUpdates() { /// This is for general tasks, specific may be read by certain AI functions
                mtx_ReceivedMessageQueue.lock();
                auto it = ReceivedMessageQueue.begin();
                while(it != ReceivedMessageQueue.end()) {
                        json* data = &(*it).data;
                        try {
                                if((*data)["receiver"] == player.name || (*data)["receiver"] == character.name) {
                                        if(!(*data)["setTask"].empty()) {
                                                if((*data)["setTask"] == "NONE") { UnsetTask(); }
                                        }
                                        if(!(*data)["type"].empty()) {
                                                if((*data)["type"] == "NewExploreTarget") {
                                                        task_targetLocation = (*data)["target"];
                                                        printf("%s received new target '%s' from controller\n", player.name.c_str(), task_targetLocation.c_str());
                                                }
                                                if((*data)["type"] == "NewDGTarget") {
                                                        task_targetLocation = (*data)["target"];
                                                        if(task_targetLocation == "") {
                                                                printf("No available DGs left, %s going idle for 5 minutes\n", player.name.c_str());
                                                                idleUntil = clock()+300*CLOCKS_PER_SEC;
                                                        } else {
                                                                DUNGEON* targetDungeon = _DUNGEONS::FindDungeon(task_targetLocation);
                                                                if(targetDungeon != NULL) thisDungeon = *targetDungeon;
                                                                printf(CC_DARKGREEN, "%s received new DG target '%s' from controller\n", player.name.c_str(), task_targetLocation.c_str());
                                                        }
                                                }
                                        }
                                        //once read, delete it
                                        it = ReceivedMessageQueue.erase(it);
                                }
                        } catch(const exception &e) { printf(CC_YELLOW, "Error receiving new task from controller, removing this task\n"); it = ReceivedMessageQueue.erase(it); }
                        ++it;
                }
                mtx_ReceivedMessageQueue.unlock();
        }
        void UnsetTask() {
                TASK = TASK_NONE;
                taskNoneReceived = time(0);
        }
        inline void Report(string msg){ if(REPORT_TO_WEBSERVER) WebServerMessageQueue.push_back(msg); }
        /// --- DUMPS
        void DumpData() {
                mtx.lock();
                stringstream fileName;
                fileName<<"dumps\\DATA DUMP - "<<player.name.c_str()<<".txt";
                ofstream logFile;
                logFile.open(fileName.str().c_str());
                logFile<<"===DATA DUMP FOR "<<player.name.c_str()<<endl;
                logFile<<"Is connected to controller; \t"<<*IS_CONNECTED<<endl;
                logFile<<"Is independent; \t"<<IS_INDEPENDENT<<endl;
                logFile<<"Attached to process: \t"<<client.GetPID()<<endl;
                logFile<<"Attached to client: \t"<<client.clientWindow<<endl;
                logFile<<"Is client crashed: \t"<<client.CheckCrash()<<endl;
                logFile<<"Memory: \t"<<client.memory.ok<<endl;
                logFile<<"Offsets: \t"<<OFFSETS::ok<<endl;
                logFile<<"Assigned account: \t"<<account.name.c_str()<<endl;
                logFile<<"Account: \t"<<player.loggedAccount.c_str()<<endl;
                logFile<<"Player data: \t"<<hex<<client.playerData<<dec<<endl;
                logFile<<"Logged in:\t"<<client.isLoggedIn<<endl;
                logFile<<"In game:\t"<<client.isCharSelected<<endl;
                logFile<<"Shield:\t"<<shieldPercent*100<<"\%"<<endl;
                logFile<<"Energy:\t"<<energyPercent*100<<"\%"<<endl;
                logFile<<"Speed:\t"<<GetSpeed()<<endl;
                logFile<<"Current location:\t'"<<player.location.c_str()<<"' in layer '"<<player.universeLayer.c_str()<<"'"<<endl;
                logFile<<"Has explored this system:\t"<<hasExploredThisSystem<<endl;
                logFile<<"Time spent in this system:\t"<<((clock()-systemEnterTime)/1000)<<"s"<<endl;
                logFile<<"Last location:\t"<<lastLocation.c_str()<<endl;
                logFile<<"Previous locations: ";
                for(string &str : previousLocations) logFile<<str.c_str()<<", ";
                logFile<<"Shield lost over last 5s: \t"<<shieldLostOverTime<<endl;
                logFile<<"Time to death: \t"<<timeToDeath<<endl;
                logFile<<endl;
                logFile<<"DG level: \t"<<thisDungeonLevel<<endl;
                logFile<<"Has reached next gate: \t"<<hasReachedNextGate<<endl;
                logFile<<"Is ready: \t"<<isReady<<endl;
                logFile<<"Are combat bots ready: \t"<<AreCombatBotsOK()<<endl;
                logFile<<"Farthest out bot: \t"<<GetFarthestOutCombatBot()<<endl;
                logFile<<"Entities: \t"<<client.entities.size()<<endl;
                logFile<<"Enemies: \t"<<client.enemies.size()<<endl;
                logFile<<"Hostiles: \t"<<client.hostiles.size()<<endl;
                logFile<<"Neutrals: \t"<<client.neutrals.size()<<endl;
                logFile<<"Friends: \t"<<client.friends.size()<<endl;
                logFile<<"Players: \t"<<client.players.size()<<endl;
                logFile<<"Debris: \t"<<debris.size()<<endl;
                logFile<<"Time left to ignore target debris: \t"<<(scoopTargetTime-clock())/1000<<"s"<<endl;
                logFile<<"Ignored debris: \t"<<ignoredDebris.size()<<endl;
                logFile<<"Scoop target ID: \t"<<scoopTargetID<<endl;
                logFile<<"Is dead: \t"<<isDead<<endl;
                logFile<<"Is in dungeon: \t"<<isInDungeon<<endl;
                logFile<<"Is in instance: \t"<<isInInstance<<endl;
                logFile<<"Is attacking: \t"<<isAttacking<<endl;
                logFile<<"Is under attack: \t"<<isUnderAttack<<endl;
                logFile<<"Is looting: \t"<<isScooping<<endl;
                logFile<<"Is regening: \t"<<isRegening<<endl;
                logFile<<"Is unloading loots: \t"<<isUnloadingLoots<<endl;
                logFile<<"Is repairing: \t"<<isRepairing<<endl;
                logFile<<"Return to base: \t"<<returnToBase<<endl;
                logFile<<"Is returning to base: \t"<<isReturningToBase<<endl;
                logFile<<"Is retrieving lost combat bot: \t"<<isRetrievingCombatBot<<endl;
                logFile<<"Is waiting for bots: \t"<<isWaitingForBots<<endl;
                logFile<<"Followers:"<<endl<<"-";
                for(string &str : followers) logFile<<str.c_str()<<", ";
                logFile<<endl;
                logFile<<"Travel path:"<<endl;
                for(string &str : currentPath) logFile<<str.c_str()<<"->";
                logFile<<endl;
                logFile<<"Jumps left: \t"<<JumpsLeft()<<endl;
                logFile<<endl;
                logFile<<"====="<<endl;
                logFile<<"Has travel field: \t"<<(client.inventory.travelfield != nullptr);
                if(client.inventory.travelfield != nullptr) logFile<<", equipped: "<<client.inventory.travelfield->isEquipped;
                logFile<<endl;
                logFile<<"Has fighters: \t"<<(client.inventory.fighters != nullptr)<<endl;
                logFile<<"Has shield tweak gen: \t"<<(client.inventory.shieldTweakGen != nullptr)<<endl;
                logFile<<"Has torch: \t"<<(client.inventory.torch != nullptr)<<endl;
                logFile<<"====="<<endl;
                logFile<<"Task: \t"<<TASK<<endl;
                logFile<<"Time since no task: \t"<<((taskNoneReceived == 0)?0:(time(0)-taskNoneReceived))<<"s"<<endl;
                if(idleUntil != 0) logFile<<"Idle time left: \t"<<floor((clock() - idleUntil)/CLOCKS_PER_SEC)<<"s"<<endl;
                else logFile<<"Idle time left: \t0"<<endl;
                logFile<<"Target: "<<lastTarget.name.c_str()<<", ID "<<hex<<targetID<<dec<<endl;
                logFile<<"Target time: "<<(clock()-targetTime)/1000<<"s"<<endl;
                logFile<<"Follow target: "<<lastFollowTarget.name.c_str()<<", ID "<<hex<<followTargetID<<dec<<endl;
                logFile<<"Target wormhole: "<<hex<<targetWormholeID<<dec<<endl;
                logFile<<"Task follow target: \t"<<task_followTargetName.c_str()<<endl;
                logFile<<"Task target location: \t"<<task_targetLocation.c_str()<<endl;
                logFile<<"Task target adjacent system: \t"<<task_adjacentLocation.c_str()<<endl;
                logFile<<"====="<<endl;
                logFile<<"Last action sequence:"<<endl<<"-";
                for(string &act : THIS_LOOP_ACTIONS) logFile<<act.c_str()<<", ";
                logFile<<endl;
                logFile<<"Last target: "<<lastTarget.name.c_str()<<" ("<<hex<<lastTarget.structurePtr<<") ID "<<lastTarget.id<<dec<<endl;
                logFile<<"Last follow target: "<<lastFollowTarget.name.c_str()<<" ("<<hex<<lastFollowTarget.structurePtr<<") ID "<<lastFollowTarget.id<<dec<<endl;
                logFile<<"====="<<endl;
                logFile<<"Combat bots:"<<combatBots.size()<<endl;
                for(CombatBot &combatbot : combatBots) {
                        logFile<<"-"<<combatbot.name.c_str()<<"("<<combatbot.shipType.c_str()<<"), last shield%: "<<combatbot.lastShieldPercent<<", dead: "<<combatbot.isDead<<", lost: "<<combatbot.isLost<<endl;
                }
                logFile<<"====="<<endl;
                logFile<<"Equipped weapons:"<<weapons.size()<<endl;
                for(Weapon &weapon : weapons) {
                        logFile<<"-"<<weapon.name.c_str()<<": type "<<weapon.type<<", range: "<<weapon.range<<endl;
                }
                logFile<<"===== PROSPECTOR"<<endl;
                logFile<<"Has scanned system: "<<prospector_hasScannedSystem<<endl;
                logFile<<"Resources in system: "<<prospector_resourcesInSystem.size()<<endl;
                logFile<<"Resources on target: "<<prospector_resourcesOnTarget.size()<<endl;
                logFile<<"Target ID: "<<hex<<prospector_scanTarget<<dec<<endl;
                logFile<<"Scanned solarbodies: "<<prospector_scannedEntities.size()<<endl;
                logFile<<"Time since last scan: "<<(clock()-prospector_scanTime)/1000<<endl;
                mtx.unlock();
        }
        void DumpObjects() {
                mtx.lock();
                stringstream fileName;
                fileName<<"dumps\\OBJECT DUMP - "<<player.name.c_str()<<".txt";
                ofstream logFile;
                logFile.open(fileName.str().c_str());
                logFile<<"List start: "<<hex<<uppercase<<client.objListBegin<<", end: "<<hex<<uppercase<<client.objListEnd<<", total(dec): "<<client.objListEnd - client.objListBegin<<"\n";
                logFile<<dec<<nouppercase;
                logFile<<"Total objects: "<<client.entities.size()<<"\n";
                UINT _count = 0;
                for(Entity &entity : client.entities) { if(entity.type == OBJECT_TYPES::AIBASE) _count++; }
                logFile<<"AI base: "<<_count<<"\n";
                _count = 0;
                for(Entity &entity : client.entities) { if(entity.type == OBJECT_TYPES::ASTEROID) _count++; }
                logFile<<"Asteroid: "<<_count<<"\n";
                _count = 0;
                for(Entity &entity : client.entities) { if(entity.type == OBJECT_TYPES::AUG) _count++; }
                logFile<<"Augmenter: "<<_count<<"\n";
                _count = 0;
                for(Entity &entity : client.entities) { if(entity.type == OBJECT_TYPES::DEBRIS) _count++; }
                logFile<<"Debris: "<<_count<<"\n";
                _count = 0;
                for(Entity &entity : client.entities) { if(entity.type == OBJECT_TYPES::DRONE) _count++; }
                logFile<<"Drone: "<<_count<<"\n";
                _count = 0;
                for(Entity &entity : client.entities) { if(entity.type == OBJECT_TYPES::MOON) _count++; }
                logFile<<"Moon: "<<_count<<"\n";
                _count = 0;
                for(Entity &entity : client.entities) { if(entity.type == OBJECT_TYPES::PLANET) _count++; }
                logFile<<"Planet: "<<_count<<"\n";
                _count = 0;
                for(Entity &entity : client.entities) { if(entity.type == OBJECT_TYPES::PLAYERBASE) _count++; }
                logFile<<"Playerbase: "<<_count<<"\n";
                _count = 0;
                for(Entity &entity : client.entities) { if(entity.type == OBJECT_TYPES::SHIP) _count++; }
                logFile<<"Ship: "<<_count<<"\n";
                _count = 0;
                for(Entity &entity : client.entities) { if(entity.type == OBJECT_TYPES::STAR) _count++; }
                logFile<<"Star: "<<_count<<"\n";
                _count = 0;
                for(Entity &entity : client.entities) { if(entity.type == OBJECT_TYPES::WORMHOLE) _count++; }
                logFile<<"Wormhole: "<<_count<<"\n";
                _count = 0;
                for(Entity &entity : client.entities) { if(!OBJECT_TYPES::IsUnknown(entity.type)) _count++; }
                logFile<<"Unknown: "<<_count<<"\n";
                for(Entity &entity : client.entities) {
                        logFile<<"====="<<entity.name.c_str()<<", "<<hex<<uppercase<<entity.structurePtr<<" ("<<entity.id<<")\n"<<nouppercase<<dec;
                        logFile<<"Type "<<entity.type<<" ("<<OBJECT_TYPES::GetType(entity.type).c_str()<<")\n";
                        logFile<<"x"<<entity.pos.x<<", y"<<entity.pos.y<<"\n";
                        logFile<<"shield: "<<entity.shield<<"/"<<entity.shieldMax<<"\n";
                        logFile<<"ship: '"<<entity.shipname.c_str()<<"'\n";
                        logFile<<"team: '"<<entity.teamname.c_str()<<"'\n";
                        logFile<<"owner: '"<<entity.owner.c_str()<<"'\n";
                        logFile<<"level: "<<entity.level<<"\n";
                        logFile<<"istractored: "<<entity.isTractored<<"\n";
                        logFile<<"stasistype: "<<entity.stasisType<<"\n";
                        logFile<<"isPlayer: "<<entity.IsPlayer()<<"\n";
                        logFile<<"isPlayerBot: "<<entity.IsPlayerBot()<<"\n";
                        logFile<<"isFriend: "<<entity.IsFriend()<<"\n";
                        logFile<<"isHostile: "<<entity.IsHostile()<<"\n";
                        logFile<<"isNeutral: "<<entity.IsNeutral()<<"\n";
                        logFile<<"\n";
                }
                logFile.close();
                mtx.unlock();
        }
        void DumpFriends() {
                mtx.lock();
                stringstream fileName;
                fileName<<"dumps\\OBJECT DUMP - "<<player.name.c_str()<<".txt";
                ofstream logFile;
                logFile.open(fileName.str().c_str());
                logFile<<"FRIENDS dump\n";
                for(Entity &entity : client.entities) {
                        if(!entity.IsFriend()) continue;
                        logFile<<"====="<<entity.name.c_str()<<", "<<hex<<uppercase<<entity.structurePtr<<" ("<<entity.id<<")\n"<<nouppercase<<dec;
                        logFile<<"x"<<entity.pos.x<<", y"<<entity.pos.y<<"\n";
                        logFile<<"shield: "<<entity.shield<<"/"<<entity.shieldMax<<"\n";
                        logFile<<"ship: '"<<entity.shipname.c_str()<<"'\n";
                        logFile<<"team: '"<<entity.teamname.c_str()<<"'\n";
                        logFile<<"owner: '"<<entity.owner.c_str()<<"'\n";
                        logFile<<"level: "<<entity.level<<"\n";
                        logFile<<"istractored: "<<entity.isTractored<<"\n";
                        logFile<<"stasistype: "<<entity.stasisType<<"\n";
                        logFile<<"isFriend: "<<entity.IsFriend()<<"\n";
                        logFile<<"isPlayer: "<<entity.IsPlayer()<<"\n";
                }
                logFile.close();
                mtx.unlock();
        }
        void DumpPlayers() {
                mtx.lock();
                stringstream fileName;
                fileName<<"dumps\\OBJECT DUMP - "<<player.name.c_str()<<".txt";
                ofstream logFile;
                logFile.open(fileName.str().c_str());
                logFile<<"PLAYERS dump\n";
                for(Entity &entity : client.entities) {
                        if(!entity.IsPlayer()) continue;
                        logFile<<"====="<<entity.name.c_str()<<", "<<hex<<uppercase<<entity.structurePtr<<" ("<<entity.id<<")\n"<<nouppercase<<dec;
                        logFile<<"x"<<entity.pos.x<<", y"<<entity.pos.y<<"\n";
                        logFile<<"shield: "<<entity.shield<<"/"<<entity.shieldMax<<"\n";
                        logFile<<"ship: '"<<entity.shipname.c_str()<<"'\n";
                        logFile<<"team: '"<<entity.teamname.c_str()<<"'\n";
                        logFile<<"owner: '"<<entity.owner.c_str()<<"'\n";
                        logFile<<"level: "<<entity.level<<"\n";
                        logFile<<"istractored: "<<entity.isTractored<<"\n";
                        logFile<<"stasistype: "<<entity.stasisType<<"\n";
                        logFile<<"isFriend: "<<entity.IsFriend()<<"\n";
                        logFile<<"isPlayer: "<<entity.IsPlayer()<<"\n";
                }
                logFile.close();
                mtx.unlock();
        }
        void DumpTest() {
                mtx.lock();
                stringstream fileName;
                fileName<<"dumps\\OBJECT DUMP - "<<player.name.c_str()<<".txt";
                ofstream logFile;
                logFile.open(fileName.str().c_str());
                logFile<<"TEST dump\n";
                logFile.close();
                mtx.unlock();
        }
        void DumpItems() {
                mtx.lock();
                stringstream fileName;
                fileName<<"dumps\\DATA DUMP - "<<player.name.c_str()<<".txt";
                ofstream logFile;
                logFile.open(fileName.str().c_str());
                logFile<<"===INVENTORY DUMP FOR "<<player.name.c_str()<<endl;
                for(InventoryItem &item : client.inventory.items) {
                        logFile<<"====="<<endl;
                        logFile<<"Name: "<<item.name.c_str()<<endl;
                        logFile<<"Type: "<<item.type<<endl;
                        logFile<<"ID1: "<<hex<<item.id<<endl;
                        logFile<<"ID2: "<<hex<<item.id2<<endl;
                        logFile<<dec<<endl;
                        logFile<<"Equipped: "<<item.isEquipped<<endl;
                }
                mtx.unlock();
        }
private:
        /// --- HERE GOES AI
        void loop() {
                srand((unsigned)time(NULL)+std::random_device()());
                while(!TERMINATE) {
                        ProcessTaskUpdates();
                        mtx.lock();
                        if(clock() > lastPointersUpdate+60*CLOCKS_PER_SEC && client.clientWindow != NULL && time(0) > client.clientBeginTime+5) client.memory.Update();
                        if(APP_PAUSE || PAUSE) goto LOOP_END;
                        if(!TERMINATE && clock() > client.lastCrashCheck+5000) if(client.CheckCrash()) goto LOOP_END;
                        /// CLIENT AND LOGIN
                        if(client.clientWindow != NULL) {
                                if(client.clientWindow != LastActiveWindow) client.chat.SelectTab("Event");
                                if(!client.IsAttached() || !client.Update()) { mtx.unlock(); continue; }
                                if(!client.isCharSelected && account.name != "") LogIn();
                        } else if(client.GetPID() == 0) {
                                mtx_openClient.lock();
                                DWORD newProcess = LaunchExe(gameClientPath);
                                client.SetClient(newProcess, true);
                                Sleep(1000);
                                client.Update();
                                mtx_openClient.unlock();
                                mtx.unlock();
                                continue;
                        }
                        /// LOOP BEGIN
                        if(player.name == "" || time(0) < client.clientBeginTime+3 || client.playerData == 0 || !client.memory.ok) { mtx.unlock(); continue; }
                        THIS_LOOP_ACTIONS.clear();
                        canUseTorch = true;
                        /// DATA
                        shieldPercent = player.ship.shield / player.ship.shieldMax;
                        energyPercent = player.ship.energy / player.ship.energyMax;
                        isDead = (player.ship.shipname.find("Escape Pod") != string::npos || player.ship.shipname == "Spirit" || player.ship.shipname == "Pod");
                        thisDungeonLevel = GetDungeonLevel(player.location);
                        isInDungeon = thisDungeonLevel != -1;
                        // Admins
                        if((clock() > adminReportTime+30*CLOCKS_PER_SEC || adminReportTime == 0) && client.admins.size() > 0){
                                SoundWarning("sound.mp3");
                                SoundWarning("sound.mp3"); // double volume idk
                                printf(CC_RED, "%s has found an admin '%s' in %s, doing objects dump\n", player.name.c_str(), client.admins.back()->name.c_str(), player.location.c_str());
                                DumpObjects();
                                DumpData();
                                stringstream ss;
                                ss<<player.name.c_str()<<" has found an admin '"<<client.admins.back()->name.c_str()<<"' in "<<player.location.c_str();
                                Report(ss.str());
                                adminReportTime = clock();
                        }
                        // Combat bots
                        if(wasLoggedIn && settings.useCombatBots && clock() > nextCombatBotCheckTime) UpdateCombatBots();
                        // Inventory / ignored entities
                        client.inventory.Update();
                        if(!isDead) {
                                if(clock() > nextInventoryCheck) {
                                        if(GetAutopilotMode() == 0) AI_TRAVELFIELD();
                                        nextInventoryCheck = clock() + 5*CLOCKS_PER_SEC;
                                        // Durability check
                                        if(settings.canRepair) {
                                                for(InventoryItem &item : client.inventory.items) {
                                                        if(item.type == ITEM_FIGHTER || !item.isEquipped) continue;
                                                        if((item.durability < 6000 && !isInDungeon) || (item.durability < 5000 && isInDungeon)) isRepairing = true;
                                                }
                                        }
                                }
                                if(clock() > nextDebrisCheck) {
                                        auto it = ignoredDebris.begin();
                                        while(it != ignoredDebris.end()) {
                                                if(clock() > (*it).since+5*60*CLOCKS_PER_SEC) it = ignoredDebris.erase(it);
                                                else if((*it).where == player.location && GetEntity((*it).id) == nullptr) it = ignoredDebris.erase(it);
                                                else it++;
                                        }
                                        it = client.ignoredEnemies.begin();
                                        while(it != client.ignoredEnemies.end()) {
                                                if(clock() > (*it).since+5*60*CLOCKS_PER_SEC) it = client.ignoredEnemies.erase(it);
                                                else if((*it).where == player.location && GetEntity((*it).id) == nullptr) it = client.ignoredEnemies.erase(it);
                                                else it++;
                                        }
                                        nextDebrisCheck = clock() + 30*CLOCKS_PER_SEC;
                                }
                                if(settings.canScoopLoots) GetDebrisList();
                                isReady = (shieldPercent > 0.75 && energyPercent > 0.3 && !isDead);
                                if(shieldPercent <= 0.5) isRegening = true;
                                else if(shieldPercent >= 0.8) isRegening = false;
                        }
                        // Shield watch
                        if(clock() > nextShieldCheck) {
                                if(player.ship.shield > player.ship.shieldMax) shieldWatch.insert(shieldWatch.begin(), player.ship.shieldMax);
                                else shieldWatch.insert(shieldWatch.begin(), player.ship.shield);
                                if(shieldWatch.size() > 10) shieldWatch.pop_back(); // last 5 seconds

                                shieldLostOverTime = 0;
                                for(size_t i=shieldWatch.size(); i>1; i--) shieldLostOverTime += shieldWatch[i]-shieldWatch[i-1];
                                if(shieldLostOverTime != 0) shieldLostOverTime /= 10;
                                if(shieldLostOverTime > player.ship.shieldMax * 0.02) underAttackTrigger = clock();
                                if(underAttackTrigger != 0 && clock() < underAttackTrigger+5000) isUnderAttack = true;
                                else isUnderAttack = false;
                                timeToDeath = player.ship.shield / shieldLostOverTime;
                                if(timeToDeath < 0) timeToDeath = 1e9; // lol
                                nextShieldCheck = clock() + 500;
                        }
                        // Player reports
                        if(!hasReportedPlayer && (GetDangerFactor() > 0.5 || isInDungeon)){
                                vector<Entity*> toReport;
                                for(Entity *entity : client.players) if(!entity->IsFriend()) {
                                        bool isListed = false;
                                        for(Entity *inList : toReport) if(inList->name == entity->name){ isListed = true; break; }
                                        if(isListed) continue;
                                        toReport.push_back(entity);
                                }
                                for(Entity* entity : toReport){
                                        printf(CC_YELLOW, "%s has found a player(%s from %s) lvl%i in %s DF %.0f\n", player.name.c_str(), entity->name.c_str(), entity->teamname.c_str(), entity->level, player.location.c_str(), GetDangerFactor()*100);
                                        printf(CC_YELLOW, "   Pos x%.0f y%.0f, distance %.0f, ship %s, visible: %i\n", entity->pos.x, entity->pos.y, DistanceTo(entity), entity->shipname.c_str(), entity->IsVisible());
                                        hasReportedPlayer = true;
                                        playerEncounters.push_back(PlayerEncounter(entity->name, player.location));
                                }
                                vector<string> checkedPlayers;
                                /*for(PlayerEncounter &encounter : playerEncounters){
                                        if(std::find(checkedPlayers.begin(), checkedPlayers.end(), encounter.name) != checkedPlayers.end()) continue;
                                        checkedPlayers.push_back(encounter.name);
                                        size_t times = 0;
                                        for(PlayerEncounter &enc : playerEncounters) if(enc.name == encounter.name) times++;
                                        if(times >= 3){
                                                string targetTeam = "";
                                                Entity *encounterPlayer = GetEntity(encounter.name);
                                                if(encounterPlayer != nullptr) targetTeam = encounterPlayer->teamname;
                                                printf(CC_RED, "%s is being followed by %s from team '%s'\n", player.name.c_str(), encounter.name.c_str(), targetTeam.c_str());
                                                SoundWarning("sound.mp3");
                                                stringstream ss;
                                                if(encounterPlayer == nullptr) ss<<player.name.c_str()<<" is being followed by "<<encounter.name.c_str()<<" in "<<player.location.c_str();
                                                else{
                                                        ss<<player.name.c_str()<<" is being followed by "<<encounter.name.c_str()<<" from team '"<<encounterPlayer->teamname.c_str()<<"'\n";
                                                        ss<<"Ship: "<<encounterPlayer->shipname.c_str()<<", distance: "<<DistanceTo(encounterPlayer)<<", isVisible: "<<encounterPlayer->IsVisible();
                                                        ss<<"\nSystems: ";
                                                        for(PlayerEncounter &encAgain : playerEncounters) if(encAgain.name == encounter.name) ss<<encAgain.where.c_str();
                                                }
                                                Report(ss.str());
                                        }
                                }*/
                        }
                        // Stucks
                        if(clock() > systemEnterTime+10*60*1000 && !hasReportedStuck && TASK != TASK_NONE && clock() < idleUntil) {
                                hasReportedStuck = true;
                                printf(CC_YELLOW, "%s has been in %s since 10 minutes\n", player.name.c_str(), player.location.c_str());
                                stringstream ss;
                                ss<<player.name.c_str()<<" has been in "<<player.location.c_str()<<" since 10 minutes";
                                Report(ss.str());
                        }
                        /// ON EVENTS
                        if(wasLoggedIn && !client.isLoggedIn) { OnLogOut(); goto LOOP_END; }
                        if(!wasLoggedIn && client.isLoggedIn) { OnLogIn(); goto LOOP_END; }
                        if(!wasDead && isDead) OnDeath();
                        if(wasDead && !isDead) OnRevive();
                        if(!wasDocked && client.isDocked) OnDock();
                        if(wasDocked && !client.isDocked) OnUndock();
                        if(player.location != lastLocation) OnSystemEnter();
                        ProcessChatMessages();
                        /// IF DEAD
                        if(isDead) {
                                AI_REVIVE();
                                goto LOOP_END;
                        }
                        /// COMMON LOGIC
                        if(settings.waitForSlaves) WaitForCombatBots();
                        if(client.isDocked && !isUnloadingLoots) Undock();
                        if(shieldPercent <= 0.8 && client.inventory.shieldTweakGen != nullptr && client.inventory.shieldTweakGen->GetCharge() == 1) client.inventory.shieldTweakGen->UseItem();
                        if(shieldLostOverTime > 0.1*player.ship.shieldMax && client.inventory.advancedBlocker != nullptr && client.inventory.advancedBlocker->GetCharge() == 1) client.inventory.advancedBlocker->UseItem();
                        /// AI TASKS
                        if(isRetrievingCombatBot) THIS_LOOP_ACTIONS.push_back("GET_THAT_LOST_SLAVE");
                        if(!isUnloadingLoots && !isRetrievingCombatBot && time(0) > PROCESSBEGINTIME+5 && !isReturningToBase) { // Give some time to connect and stuff
                                if(TASK == TASK_FOLLOW && player.name != task_followTargetName) AI_FOLLOW();
                                else if(TASK == TASK_EXPLORE) AI_EXPLORE();
                                else if(TASK == TASK_DG) AI_DG();
                                else if(TASK == TASK_PROSPECT) AI_PROSPECT();
                                else if(TASK == TASK_SEARCH) AI_SEARCH();
                                else if(TASK == TASK_NONE) AI_NOTASK();
                        } else if(!isUnloadingLoots && isReturningToBase && settings.homeSystem != "") {
                                if(player.location != settings.homeSystem) MoveTo(settings.homeSystem);
                                else {
                                        if(!hasReportedReturn) {
                                                printf(CC_GREEN, "%s has returned to base\n", player.name.c_str());
                                                hasReportedReturn = true;
                                        }
                                        ToggleAnchor(true);
                                }
                        }
                        /// REPAIR
                        //if(isRepairing) AI_REPAIR();
                        /// COMMON LOGIC, continued
                        if(isUnloadingLoots) AI_UNLOADLOOTS();
                        //if(isTranswarping || clock() > nextTranswarpCheck) AI_TRANSWARP(); //TODO: this
                        AI_SPEEDBOOSTS();
                        if(GetAutopilotMode() == 3 || clock() > nextTravelFieldCheck) AI_TRAVELFIELD();
                        AI_AURAS();
LOOP_END:
                        /// FINISH UP LOOP
                        wasLoggedIn = client.isLoggedIn;
                        wasDead = isDead;
                        wasDocked = client.isDocked;
                        lastLocation = player.location;
                        if(target != NULL) {
                                lastTarget = *target;
                                lastTargetTime = clock();
                                targetID = target->id;
                        }else StopShooting();
                        if(followTarget != NULL) {
                                lastFollowTarget = *followTarget;
                                lastFollowTargetTime = clock();
                                followTargetID = followTarget->id;
                        }
                        target = NULL; followTarget = NULL; targetWormhole = NULL; scoopTarget = NULL;
                        debris.clear();
                        credits.clear();
                        isAttacking = false;
                        mtx.unlock();
                        Sleep(10);
                }
        }

        /// --- EVENTS
        void OnLogIn() {
                client.UpdateAll();
                printf("%s has logged in\n", player.name.c_str());
                if(player.name != "" && client.clientWindow != NULL) SetWindowText(client.clientWindow, player.name.c_str());
                ClickEventTab();
                OnUndock();
               /* if(player.name == "Gate to Sol"){
                        followers.clear();
                        settings.useCombatBots = false;
                        settings.minDF = 1.5;
                        settings.maxDF = 2.2;
                        settings.canMarkAsEnemy = true;
                        settings.canAttack = false;
                        settings.canUseFighters = false;
                        settings.canScoopLoots = false;
                        TASK = TASK_SEARCH;
                }*/
        }
        void OnLogOut() {
                if(!TERMINATE) client.CheckCrash();
                printf("%s has logged out\n", player.name.c_str());
        }
        void OnDeath() {
                printf(CC_YELLOW, "%s died in %s\n", player.name.c_str(), player.location.c_str());
                client.UpdateAll();
        }
        void OnRevive() {
                client.UpdateAll();
                if(weapons.size() == 0) GetWeapons();
        }
        void OnDock() {
                hullUsedOnDock = player.hullUsed;
                itemsOnDock = client.inventory.items;
        }
        void OnUndock() {
                hullUsedOnDock = 0;
                itemsOnDock.clear();
                GetWeapons();
                repairInSystem = "";
        }
        void OnSystemEnter() {
                systemEnterTime = clock();
                isBossKilled = false;
                hasReportedPlayer = false;
                hasReportedStuck = false;
                if(currentPath.size() != 0 && player.location == currentPath.back()) currentPath.clear();
                if(!isInDungeon) hasEnteredTargetDungeon = false;
                else if(player.location == task_targetLocation) hasEnteredTargetDungeon = true;
                hasExploredThisSystem = false;
                thisSystemID = GetSystemID();
                systemOwnedBy = "";
                isInInstance = false;
                previousLocations.insert(previousLocations.begin(), lastLocation);
                previousLocations.pop_back();
                auto it = playerEncounters.begin();
                while(it != playerEncounters.end()){
                        (*it).jumpsAway++;
                        if((*it).jumpsAway >= 6) it = playerEncounters.erase(it);
                        else ++it;
                }
                targetWormholeID = 0;
                hasReachedNextGate = false;
                hasReportedReturn = false;
                prospector_hasScannedSystem = false;
                prospector_resourcesInSystem.clear();
                prospector_scannedEntities.clear();
                prospector_resourcesOnTarget.clear();
                if(!ASYNC_EXPLORESYSTEM.valid() || IsThreadFinished(ASYNC_EXPLORESYSTEM)) {
                        UINT thisSystem = GetSystemID();
                        ASYNC_EXPLORESYSTEM = std::async(std::launch::async, [](BOT* bot, UINT thisSystem) {
                                std::this_thread::sleep_for(std::chrono::seconds(1));
                                clock_t start = clock();
                                bot->client.mtx_objectsList->lock();
                                while(bot->GetEntityClosestTo(Point(0, 0), OBJECT_TYPES::WORMHOLE) == nullptr && clock() < start+10*CLOCKS_PER_SEC)
                                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                                bot->client.mtx_objectsList->unlock();
                                if(bot->GetSystemID() == thisSystem) bot->CollectSystemData();
                        }, this, thisSystem);
                }
        }
        /// --- AI
        // --- No task
        bool hasReportedReturn = false;
        void AI_NOTASK() {
                /*if(player.location != settings.homeSystem) MoveTo(settings.homeSystem);
                else {
                        if(GetSpeed() > 0) ToggleAnchor(true);
                        else ToggleAnchor(false);
                        if(!hasReportedReturn) { printf(CC_GREEN, "%s has returned to base\n", player.name.c_str()); hasReportedReturn = true; }
                }*/
                if(player.name == "Seed Spreader") AI_ScoopLoots();
        }
        // --- Follow
        string follow_guessTargetLocation = "";
        bool follow_reportedLost = false;
        bool follow_hasCheckedGuess = false, follow_hasMadeGuess = false;
        void AI_FOLLOW() {
                if(task_followPause) return;
                followTarget = GetEntity(followTargetID);
                if(followTarget == NULL) {
                        followTarget = GetEntity(task_followTargetName);
                        if(GetAutopilotMode() != 3 && GetSpeed() > 0) ToggleAnchor(true);
                        else ToggleAnchor(false);
                }
                if(followTarget != NULL) {
                        follow_hasCheckedGuess = false;
                        follow_hasMadeGuess = false;
                        follow_reportedLost = false;
                        if(DistanceTo(followTarget) > settings.followRange) AutopilotTo(followTarget);
                        else{
                                /*Weapon *shieldTrans = nullptr;
                                for(Weapon &weapon : weapons) if(weapon.name.find("Transference") != string::npos){ shieldTrans = &weapon; break; }
                                if(shieldTrans != nullptr && settings.canHeal){
                                        if(DistanceTo(followTarget) > shieldTrans->range) AutopilotTo(followTarget);
                                        else{
                                                SelectTarget(followTarget);
                                                SelectWeapon(shieldTrans->id);
                                                Shoot();
                                                ToggleAnchor(true);
                                        }
                                }else ToggleAnchor(true);*/
                                ToggleAnchor(true);
                        }
                } else {
                        bool fail = false;
                        if(!follow_hasMadeGuess && lastFollowTarget.pos != Point(0, 0)) {
                                follow_hasMadeGuess = true;
                                targetWormhole = GetEntityClosestTo(lastFollowTarget.pos, OBJECT_TYPES::WORMHOLE);
                                if(targetWormhole != NULL && DistanceBetween(targetWormhole, &lastFollowTarget) < 1000) {
                                        follow_guessTargetLocation = UNIVERSE::GetSystemFromWormhole(targetWormhole->name);
                                        targetWormholeID = targetWormhole->id;
                                        follow_hasMadeGuess = true;
                                } else fail = true;
                        }
                        if(follow_hasMadeGuess) {
                                if(!follow_hasCheckedGuess) {
                                        if(player.location != follow_guessTargetLocation) {
                                                if(targetWormhole == NULL) targetWormhole = GetEntity(targetWormholeID);
                                                if(targetWormhole != NULL) WarpThrough(targetWormhole);
                                                else fail = true;
                                        } else follow_hasCheckedGuess = true;
                                } else if(player.location == follow_guessTargetLocation) { /// still not found
                                        if(clock() > systemEnterTime + 1000) {
                                                targetWormhole = GetEntity(targetWormholeID);
                                                if(targetWormhole == NULL) {
                                                        targetWormhole = GetWormholeTo(previousLocations[0]);
                                                        if(targetWormhole != NULL) targetWormholeID = targetWormhole->id;
                                                }
                                                if(targetWormhole != NULL) WarpThrough(targetWormhole);
                                        }
                                } else fail = true;
                        }
                        if(fail && !follow_reportedLost && clock() > systemEnterTime + 60000 && GetAutopilotMode() == 0) {
                                printf(CC_YELLOW, "Bot %s has lost it's follow target %s\n", player.name.c_str(), task_followTargetName.c_str());
                                follow_reportedLost = true;
                        }
                }
        }
        // --- Explore
        void AI_EXPLORE() {
                // Get new target
                if(hasExploredThisSystem && player.location == task_targetLocation) { task_targetLocation = ""; task_adjacentLocation = ""; }
                if(task_targetLocation == "" && hasExploredThisSystem) {
                        if(!*IS_CONNECTED || IS_INDEPENDENT) { // Find next target on it's own
                                THIS_LOOP_ACTIONS.push_back("EXP_FIND_NEW_TARGET");
                                pair<SYSTEM*, SYSTEM*> newTarget = UNIVERSE::FindNearestUnexplored(player.location, false);
                                if(newTarget.first != NULL) {
                                        newTarget.first->timeTakenByExplorer = time(0);
                                        if(newTarget.second != NULL) task_adjacentLocation = newTarget.second->name;
                                        else task_adjacentLocation = "";
                                        task_targetLocation = newTarget.first->name;
                                } else {
                                        printf(CC_PINK, "%s has not found any unexplored systems, setting task to none and returning to base\n", player.name.c_str());
                                        UnsetTask();
                                        returnToBase = true;
                                }
                        } else {
                                THIS_LOOP_ACTIONS.push_back("EXP_REQUEST_NEW_TARGET");
                                if(clock() > lastTaskUpdateRequest+5*CLOCKS_PER_SEC) {
                                        json msg{
                                                {"location", player.location},
                                                {"maxWarp", character.warpNavigation}
                                        };
                                        RequestTaskUpdate(msg);
                                }
                        }
                        return;
                }
                // Go to target
                if(task_targetLocation != "" && player.location != task_targetLocation) {
                        string apDestination = GetAutopilotDestination();
                        string thisMoveTarget = task_adjacentLocation;
                        if(thisMoveTarget == "") thisMoveTarget = task_targetLocation;
                        if(thisMoveTarget != "") {
                                if(player.location != task_adjacentLocation && player.location != task_targetLocation) {
                                        THIS_LOOP_ACTIONS.push_back("EXP_TRAVEL");
                                        if(!MoveTo(thisMoveTarget)) {
                                                printf(CC_YELLOW, "%s failed to find path to %s, unable to explore %s\n", player.name.c_str(), thisMoveTarget.c_str(), task_targetLocation.c_str());
                                                UNIVERSE::ignoreSystems.push_back(task_targetLocation);
                                                UNIVERSE::SaveLists();
                                                json errorData{
                                                        {"target", task_targetLocation},
                                                };
                                                ReportTaskFail(errorData);
                                                task_targetLocation = "";
                                                task_adjacentLocation = "";
                                        }
                                } else if(player.location == task_adjacentLocation) {
                                        THIS_LOOP_ACTIONS.push_back("EXP_WARP");
                                        targetWormhole = GetWormholeTo(task_targetLocation);
                                        if(targetWormhole != NULL) WarpThrough(targetWormhole);
                                }
                        } else { THIS_LOOP_ACTIONS.push_back("EXP_IDK2"); }
                } else if(task_targetLocation != "" && player.location == task_targetLocation) {
                        //Do whatever, onSystemEnter will gather data
                        THIS_LOOP_ACTIONS.push_back("EXP_WAIT");
                } else if(!hasExploredThisSystem) {
                        CollectSystemData();
                        sleep(1);
                } else {
                        THIS_LOOP_ACTIONS.push_back("EXP_IDK");
                }
        }
        // --- DG
        bool hasReachedNextGate = false;
        void AI_DG() {
                if(settings.canScoopLoots && (thisDungeonLevel != 0 || clock() > systemEnterTime+15*CLOCKS_PER_SEC) && (player.hullMax - player.hullUsed <= 50 ||
                                (client.inventory.fighters != nullptr && player.hullMax - player.hullUsed <= 100))) {
                        GoUnloadLoots();
                        return;
                }
                // Get target
                if(task_targetLocation == "" && clock() > idleUntil) {
                        if(finishDGAndWait) {
                                if(!hasReportedDGWait) {
                                        printf(CC_GREEN, "%s has finished DG and is waiting\n", player.name.c_str());
                                        hasReportedDGWait = true;
                                }
                                if(isInDungeon) LeaveDG();
                                else if(!AI_ScoopLoots(true)) ToggleAnchor(true);
                                return;
                        }
                        if(!*IS_CONNECTED || IS_INDEPENDENT) {
                                THIS_LOOP_ACTIONS.push_back("DG_FIND_NEW_TARGET");
                                UNIVERSE::mtx_findTarget.lock();
                                DUNGEON* newTarget = UNIVERSE::FindNearestAvailableDG(player.location, settings.minDF, settings.maxDF, settings.dgLayers, NULL, player.name);
                                if(newTarget != NULL) {
                                        task_targetLocation = newTarget->name;
                                        newTarget->lastTaken = time(0);
                                        printf(CC_DARKGREEN, "%s target DG set to '%s'\n", player.name.c_str(), task_targetLocation.c_str());
                                        thisDungeon = *newTarget;
                                        creditsOnDGStart = GetCredits();
                                        dgStartTime = clock();
                                } else {
                                        printf(CC_PINK, "No available DGs left, %s going idle for 5 minutes\n", player.name.c_str());
                                        idleUntil = clock()+300*CLOCKS_PER_SEC;
                                }
                                UNIVERSE::mtx_findTarget.unlock();
                        } else {
                                THIS_LOOP_ACTIONS.push_back("DG_REQUEST_NEW_TARGET");
                                //printf("%s requesting new DG target from controller\n", player.name.c_str());
                                json data{
                                        {"location", player.location},
                                        {"maxDF", settings.maxDF},
                                        {"minDF", settings.minDF}
                                };
                                for(string &layer : settings.dgLayers) data["layers"].push_back(layer);
                                RequestTaskUpdate(data);
                        }
                        return;
                } else if(clock() < idleUntil) {
                        THIS_LOOP_ACTIONS.push_back("DG_IDLE");
                        return;
                }
                //Get to target
                if(!isInDungeon) {
                        string targetSystem = UNIVERSE::GetSystemFromDG(task_targetLocation);
                        if(player.location != targetSystem) {
                                THIS_LOOP_ACTIONS.push_back("DG_MOVE_TO_TARGET");
                                if(AreFollowersHere()) MoveTo(targetSystem);
                        } else {
                                targetWormhole = GetWormholeTo(task_targetLocation);
                                if(targetWormhole != NULL && DistanceTo(targetWormhole) > 500 && !hasReachedNextGate) AutopilotTo(targetWormhole);
                                else if(targetWormhole != NULL && DistanceTo(targetWormhole) <= 500) hasReachedNextGate = true;
                                if(hasReachedNextGate) {
                                        if(settings.waitForSlaves && !AreCombatBotsOK()) {
                                                THIS_LOOP_ACTIONS.push_back("DG_WAIT_FOR_BOTS");
                                                isWaitingForBots = true;
                                                if(!AI_ScoopLoots()) AI_REGEN();
                                        } else if(!isReady) {
                                                THIS_LOOP_ACTIONS.push_back("DG_REGEN");
                                                if(!AI_ScoopLoots()) AI_REGEN();
                                        } else if(!AreFollowersWithinDistance(1000)) {
                                                THIS_LOOP_ACTIONS.push_back("DG_WAIT_FOR_FOLLOWERS");
                                                if(!AI_ScoopLoots()) AI_REGEN();
                                        } else {
                                                THIS_LOOP_ACTIONS.push_back("DG_ENTER_TARGET");
                                                isWaitingForBots = false;
                                                Entity* targetWormhole = GetWormholeTo(task_targetLocation);
                                                if(targetWormhole != NULL) WarpThrough(targetWormhole);
                                        }
                                }
                        }
                        return;
                }
                //If in wrong dg, leave
                if(UNIVERSE::GetSystemFromDG(player.location) != UNIVERSE::GetSystemFromDG(task_targetLocation) || !hasEnteredTargetDungeon) {
                        THIS_LOOP_ACTIONS.push_back("DG_LEAVE_WRONG_DG");
                        if(AI_ScoopLoots() == 0 && AreFollowersHere()) LeaveDG();
                        return;
                }
                //Kill and plunder, also start moving towards next gate
                //Get next wormhole
                size_t currentStep = GetCurrentPathStep();
                if(currentStep != 9999 && currentPath.size() >= 2 && currentStep < currentPath.size()-2) targetWormhole = GetWormholeTo(currentPath[currentStep+1]);
                if(targetWormhole == nullptr && clock() > systemEnterTime+100) targetWormhole = GetNextDGGate();
                if(targetWormhole == nullptr && thisDungeonLevel != 0 && clock() > systemEnterTime+1000) {
                        THIS_LOOP_ACTIONS.push_back("DG_IDK_NO_GATE1");
                        bool buff = AI_ScoopLoots();
                        buff = AI_Combat(buff);
                        if(!buff && clock() > systemEnterTime+30*CLOCKS_PER_SEC) {
                                if(AreFollowersHere()) LeaveDG();
                                currentPath.clear();
                        }
                        return;
                }
                //Move to next wormhole if not reached
                if(!hasReachedNextGate && thisDungeonLevel != 0 && targetWormhole != nullptr) {
                        THIS_LOOP_ACTIONS.push_back("DG_MOVE_TO_NEXT_GATE");
                        if(DistanceTo(targetWormhole) > 300) {
                                AI_Combat(false);
                                if(GetFarthestWithinDistance(debris, 10000, settings.tractorRange) != nullptr || debris.size() >= 2) AI_ScoopLoots(true);
                                else {
                                        AI_ScoopLoots(false);
                                        AutopilotTo(targetWormhole);
                                }
                                return;
                        } else hasReachedNextGate = true;
                }
                bool okToMove = false;
                Entity *nearestEnemy = GetNearestWithinDistance(client.enemies, 10000);
                int enemies = CountNormalEnemies();
                if(enemies == 0 || (enemies <= 2 && DistanceTo(nearestEnemy) > 1000) || GetNewTarget() == NULL) okToMove = true;
                if(nearestEnemy == nullptr) okToMove = true;
                if(!okToMove || (thisDungeonLevel == 0 && enemies > 0 && GetNewTarget() != NULL)) {
                        THIS_LOOP_ACTIONS.push_back("DG_KILL_AND_PLUNDER");
                        //Clear level
                        if(AI_ScoopLoots(true) != 1) {
                                /*if(targetWormhole != nullptr && DistanceTo(targetWormhole) > 2000) { AutopilotTo(targetWormhole); AI_Combat(false); }
                                else AI_Combat(true);*/
                                AI_Combat(true);
                        } else AI_Combat(false);
                } else if(okToMove) {
                        DUNGEONLEVEL *thisLvl = nullptr;
                        for(DUNGEONLEVEL &lvl : thisDungeon.levels) if(lvl.name == player.location) { if(!lvl.completed) lvl.completed = true; thisLvl = &lvl; break; }
                        // Go to next level
                        AI_Combat(false);
                        if(AI_ScoopLoots()) { THIS_LOOP_ACTIONS.push_back("DG_FINISH_PLUNDERING"); return; } // If nothing to scoop, continue
                        // Make sure this level data exists before moving on
                        if(thisLvl == nullptr || thisLvl->wormholes.size() == 0) {
                                if(thisLvl == nullptr) THIS_LOOP_ACTIONS.push_back("DG_NULL_LEVEL");
                                else THIS_LOOP_ACTIONS.push_back("DG_NO_WORMHOLE_DATA");
                                if(clock() > systemEnterTime+5000) LeaveDG();
                                if(thisLvl != nullptr && clock() > systemEnterTime+2000) {
                                        thisLvl->completed = false;
                                        CollectSystemData();
                                }
                                return;
                        }
                        if(targetDungeonLevel != nullptr && player.location != targetDungeonLevel->name && currentPath.size() != 0) {
                                if(isReady && (!settings.waitForSlaves || thisDungeonLevel == 0 || (thisDungeonLevel != 0 && AreCombatBotsOK())) && AreFollowersHere()) TraversePath();
                                else AI_REGEN();
                                return;
                        }
                        if(thisDungeonLevel != 0) {
                                if(isReady && (!settings.waitForSlaves || AreCombatBotsOK())) {
                                        THIS_LOOP_ACTIONS.push_back("DG_WARP_TO_NEXT");
                                        if(targetWormhole != NULL) {
                                                // If bots aren't close, wait
                                                if(AreFollowersWithinDistance(1000)
                                                    && (!settings.waitForSlaves || thisDungeonLevel != 1 || (thisDungeonLevel == 1 && (GetFarthestOutCombatBot() > 10000 || GetFarthestOutCombatBot() < 750))))
                                                        WarpThrough(targetWormhole);
                                                else if(DistanceTo(targetWormhole) > 300) AutopilotTo(targetWormhole);
                                                else ToggleAnchor(true);
                                        } else {
                                                THIS_LOOP_ACTIONS.push_back("DG_NO_GATE_IDK2");
                                                AI_Combat(AI_ScoopLoots(true));
                                        }
                                } else AI_REGEN();
                        } else if(clock() > systemEnterTime+1000) {
                                // Check if all levels are completed
                                bool allCompleted = true;
                                for(DUNGEONLEVEL &lvl : thisDungeon.levels) if(!lvl.completed) { allCompleted = false; break; }
                                // If not, find unfinished split, TODO: possibly optimize this? idk
                                currentPath = _DUNGEONS::FindPathToNextSplit(player.location, thisDungeon);
                                if(currentPath.size() != 0) targetDungeonLevel = _DUNGEONS::FindDungeonLevel(currentPath.back(), thisDungeon);
                                //--
                                if(!allCompleted && currentPath.size() == 0) {
                                        if(clock() > systemEnterTime+30*CLOCKS_PER_SEC) {
                                                printf(CC_YELLOW, "%s was unable to complete %s, going to next DG\n", player.name.c_str(), task_targetLocation.c_str());
                                                FinishDG();
                                                return;
                                        }
                                        printf(CC_RED, "Error3 on %s\n", player.name.c_str());
                                        sleep(1);
                                        return;
                                }
                                // If all completed
                                if(allCompleted) {
                                        dgsCompleted++;
                                        printf(CC_DARKGREEN, "%s has finished %i DG %s\n", player.name.c_str(), dgsCompleted, thisDungeon.name.c_str());
                                        size_t ignoredLoot = 0;
                                        for(IgnoredDebris &debris : ignoredDebris) if(debris.where == player.location) ignoredLoot++;
                                        if(ignoredLoot > 0) printf(CC_DARKGREEN, "\tIgnored %i debris in boss room, hull available: %i, time in system: %i\n", ignoredLoot, player.hullMax - player.hullUsed, floor((clock() - systemEnterTime)/1000));
                                        //printf("Credits gained: %'lld in %is\n", GetCredits() - creditsOnDGStart, (clock() - dgStartTime)/1000);
                                        thisDungeon.lastCleared = time(0);
                                        thisDungeon.isExplored = true;
                                        if(thisDungeon.hasUpdatedData) {
                                                json newMsg{
                                                        {"action", "UpdateDungeonData"},
                                                        {"data", thisDungeon.to_JSON()}
                                                };
                                                _DUNGEONS::UpdateDungeonData(newMsg["data"]);
                                                ConnectionMessageQueue.push_back(newMsg.dump());
                                        }
                                        LOCKOUTS::lockout *thisLockout = LOCKOUTS::Get(player.name, thisDungeon.name);
                                        UINT lockTime = 0;
                                        if(thisLockout != nullptr) lockTime = thisLockout->until;
                                        json newMsg{
                                                {"action", "DGCompleted"},
                                                {"character", player.name},
                                                {"target", thisDungeon.name},
                                                {"lockoutTime", lockTime}
                                        };
                                        ConnectionMessageQueue.push_back(newMsg.dump());
                                        FinishDG();
                                }
                        }
                }
        }
        void FinishDG() {
                DUNGEON *targetDg = _DUNGEONS::FindDungeon(thisDungeon.name);
                if(targetDg != nullptr) targetDg->lastCleared = time(0);
                thisDungeon = DUNGEON(); // reset
                thisDungeon.levels.clear();
                targetDungeonLevel = nullptr;
                task_targetLocation = "";
                hasEnteredTargetDungeon = false;
                if(settings.canScoopLoots && player.hullMax - player.hullUsed < 150) {
                        GoUnloadLoots();
                }
                if(returnToBase) isReturningToBase = true;
        }
        void GoUnloadLoots(){
                printf(CC_PINK, "%s going to unload loots\n", player.name.c_str());
                isUnloadingLoots = true;
                //if(followers.size() > 0) SendChatMessage("Team", "Going to unload loots");
        }
        // --- Combat
        clock_t fighterLaunchTime = 0;
        bool AI_Combat(bool canMove = true) {
                Entity* attackTarget = GetEntity(targetID);
                if(attackTarget == NULL) {
                        attackTarget = GetNewTarget();
                        if(attackTarget != NULL) {
                                targetID = attackTarget->id;
                                targetTime = clock();
                        } else targetID = 0;
                        isAttacking = false;
                } else {
                        for(Weapon &weapon : weapons) {
                                if(weapon.type != WEAPON_TRANSFERENCE && weapon.range >= DistanceTo(attackTarget) && weapon.name != "Prismatic Conversion") {
                                        SelectWeapon(weapon.id);
                                        break;
                                }
                        }
                        SelectTarget(attackTarget);
                        if(canMove) {
                                if(DistanceTo(attackTarget) > 500) AutopilotTo(attackTarget);
                                else ToggleAnchor(true);
                        }
                        if(settings.canTractorEnemy && !player.ship.isTractored) ToggleTractor();
                        Sleep(10);
                        if(settings.canAttack) Shoot();
                        //fighters
                        if(settings.canUseFighters){
                                if(client.chat.FindMessage("is at maximum capacity", "Event") != nullptr || client.chat.FindMessage("charge launch cap", "Event") != nullptr) fighterLaunchTime = clock();
                                if(energyPercent > 0.3 && client.inventory.fighters != nullptr && (clock() > fighterLaunchTime+65*CLOCKS_PER_SEC || fighterLaunchTime == 0)) client.inventory.fighters->UseItem();
                        }
                        isAttacking = true;

                        if(attackTarget->IsPlayer() || attackTarget->IsPlayerBot()) targetID = 0;
                        if(!attackTarget->IsVisible()) targetID = 0;
                        if(attackTarget->IsRadded() && clock() > targetTime+10*CLOCKS_PER_SEC) IgnoreTarget();
                }
                return isAttacking;
        }
        // --- Looting
        /** \brief Return 0 if not scooping, 1 if scooping, 2 if there was no need to move*/
        short AI_ScoopLoots(bool canMove = true, double maxDistance = 3000) {
                if(!settings.canScoopLoots) return 0;
                short ret = 0;
                bool hasNewTarget = true;
                scoopTarget = GetEntity(scoopTargetID);
                if(scoopTarget != nullptr){
                        hasNewTarget = false;
                        // Check if failed to scoop
                        if(!canMove) scoopTargetTime = clock() + 15*CLOCKS_PER_SEC;
                        if(scoopTargetTime != 0 && clock() > scoopTargetTime) IgnoreTargetLoot();
                }
                if(scoopTarget == NULL || DistanceTo(scoopTarget) > settings.scoopRange*0.9) scoopTarget = GetNearestWithinDistance(debris, settings.maxLootDistance, settings.scoopRange*0.9, &ignoredDebris);
                if(scoopTarget != NULL) {
                        if(DistanceTo(scoopTarget) < 50) { SelectTarget(scoopTarget); ScoopFast(); Sleep(10); }
                        else Scoop();
                        if(hasNewTarget){
                                scoopTargetID = scoopTarget->id;
                                UINT addTime = floor(DistanceTo(scoopTarget)/(player.ship.speedMax*1000))*CLOCKS_PER_SEC;
                                if(addTime < 0){
                                        addTime = 0;
                                        printf("error1\n");
                                }
                                scoopTargetTime = clock()+15*CLOCKS_PER_SEC+addTime;
                        }
                        Scoop();
                        if(canMove){
                                Point apTarget = scoopTarget->pos;
                                if(!IsMovingTowards(scoopTarget, player.ship.pos, 0.1)){
                                        apTarget = MovePoint(scoopTarget->pos, GetAngleTo(scoopTarget->pos, scoopTarget->pos+scoopTarget->moveVector), scoopTarget->speed*1000);
                                        apTarget = MovePoint(apTarget, GetAngleTo(player.ship.pos, apTarget), scoopTarget->speed*1000);
                                }
                                AutopilotTo(apTarget.x, apTarget.y);
                        }
                        ret = 1;
                }else{
                        scoopTargetTime = 0;
                        scoopTargetID = 0;
                }
                //Tractor loots that are far away
                bool hadTarget = scoopTarget != NULL;
                scoopTarget = GetFarthestWithinDistance(debris, settings.tractorRange, 0, &ignoredDebris);
                if(scoopTarget == NULL && thisDungeonLevel == 0 && credits.size() != 0) scoopTarget = GetNearestWithinDistance(credits, 2000, 0, &ignoredDebris);
                if(scoopTarget != NULL) {
                        if(!hadTarget) {
                                scoopTargetID = scoopTarget->id;
                                UINT addTime = floor(DistanceTo(scoopTarget)/(player.ship.speedMax*1000))*CLOCKS_PER_SEC;
                                if(addTime < 0){
                                        addTime = 0;
                                        printf("error2\n");
                                }
                                scoopTargetTime = clock()+15*CLOCKS_PER_SEC+addTime;
                                //scoopTargetTime_LongDistance = clock();
                        }
                        if(!scoopTarget->isTractored) {
                                SelectTarget(scoopTarget);
                                Sleep(20);
                                ToggleTractor();
                        }
                        if(DistanceTo(scoopTarget) > maxDistance) scoopTargetID = 0;
                        if(canMove && GetAutopilotMode() == 0) AutopilotTo(scoopTarget);
                        if(std::find_if(ignoredDebris.begin(), ignoredDebris.end(),
                                                               [this](const IgnoredDebris &ignored){ return scoopTarget->id == ignored.id && ignored.where == player.location; }) != ignoredDebris.end()){
                                                                        scoopTarget = nullptr;
                                                                        scoopTargetID = 0;
                                                               }
                        if(ret == 0) ret = 2;
                } else scoopTargetTime = 0;

                if(scoopTarget == NULL) StopScooping();
                isScooping = ret != 0;
                return ret;
        }
        // --- Regen
        void AI_REGEN() {
                THIS_LOOP_ACTIONS.push_back("REGEN");
                if(client.inventory.travelfield != nullptr && client.inventory.travelfield->isEquipped) client.inventory.EquipItem(client.inventory.travelfield, false);
                Entity *star = GetEntityClosestTo(player.ship.pos, OBJECT_TYPES::STAR);
                if(star != NULL && DistanceTo(star) < star->_size*0.5*1.1) {
                        Point moveTo = MovePoint(player.ship.pos, GetAngleTo(star->pos, player.ship.pos), (double)star->_size*0.5*1.5);
                        AutopilotTo(moveTo.x, moveTo.y);
                        return;
                }
                AI_ScoopLoots(true);
                if(!isScooping) {
                        ToggleAnchor(true);
                        AutopilotOff();
                }
                if(client.hostiles.size() > 0) AI_Combat();
        }
        // --- Speed boosts
        clock_t lastOBAUse = 0;
        void AI_SPEEDBOOSTS() {
                if(clock() < lastOBAUse+8*CLOCKS_PER_SEC) return;
                Point aptarget = GetAutopilotTarget();
                if(GetAutopilotMode() == 0) return;
                // Calculate time to reach destination
                double distanceLeft = DistanceTo(aptarget);
                double timeToTarget =  distanceLeft / (player.ship.speedMax*1000-1);
                if(client.inventory.torch != nullptr && (player.universeLayer != "Wild Space" || account.isP2P) && player.hullUsed <= player.hullMax) {
                        if(GetSpeed() < (player.ship.speedMax*1000)*2-1 && IsMovingTowards(&player.ship, aptarget, 1.108) && timeToTarget > 2 && IsFacing(aptarget, 1.0471) && !isAttacking
                                        && !IsAnchorOn() && clock() > systemEnterTime+500) {
                                SelectWeapon(client.inventory.torch->id);
                                SelectTarget((DWORD)0);
                                Shoot();
                        }
                }
                InventoryItem *obagen = client.inventory.GetItem("Oilheart's Best Supersonic Afterburner");
                if(obagen != nullptr && obagen->GetCharge() == 1 && (client.inventory.travelfield == nullptr || !client.inventory.travelfield->isEquipped) && !isScooping && clock() > systemEnterTime+1000) {
                        if(timeToTarget > 20 && IsFacing(aptarget, 0.0872) && IsMovingTowards(&player.ship, aptarget, 0.10943) && GetSpeed() < player.ship.speedMax*1000*3) {
                                obagen->UseItem();
                                lastOBAUse = clock();
                        }
                }
        }
        // --- Travel field
        void AI_TRAVELFIELD() {
                int jumpsLeft = JumpsLeft();
                if(client.inventory.travelfield != nullptr) {
                        if(client.inventory.travelfield->isEquipped && (isRegening || isInDungeon)) client.inventory.EquipItem(client.inventory.travelfield, false);
                        else if(jumpsLeft > 2 && !isInDungeon && !isInInstance
                                        && (TASK != TASK_DG || (player.location == UNIVERSE::GetSystemFromDG(task_targetLocation) && !isWaitingForBots)
                                            || player.location != UNIVERSE::GetSystemFromDG(task_targetLocation))) {
                                if(!client.inventory.travelfield->isEquipped && !isRegening) client.inventory.EquipItem(client.inventory.travelfield, true);
                        } else if(!isUnloadingLoots && client.inventory.travelfield->isEquipped && (GetAutopilotMode() == 0 || jumpsLeft == 0 || (jumpsLeft == 1 && IsMovingTowards(&player.ship, GetAutopilotTarget(), 0.261791)))) client.inventory.EquipItem(client.inventory.travelfield, false);
                        nextTravelFieldCheck = clock() + 1000;
                } else nextTravelFieldCheck = clock() + 10*CLOCKS_PER_SEC;
        }
        // --- Transwarp
        string transwarpTarget = "";
        void AI_TRANSWARP() {
                nextTranswarpCheck = clock() + 5*CLOCKS_PER_SEC;
                int jumpsLeft = GetAutopilotJumps();
                if(!isTranswarping && GetAutopilotMode() == 3 && jumpsLeft >= 15 && jumpsLeft <= 25 && client.inventory.transwarp != nullptr && client.inventory.transwarp->charge == 1 && shieldPercent >= 0.8) {
                        SYSTEM* target = UNIVERSE::FindNearestTranswarpSystem(player.location, 5);
                        if(target == nullptr) { transwarpTarget = ""; return; }
                        else transwarpTarget = target->name;
                        if(!settings.waitForSlaves) isTranswarping = true;
                        else {
                                isTranswarping = true;
                                for(CombatBot &combatbot : combatBots) if(combatbot.lastShieldPercent < 0.8) { isTranswarping = false; break; }
                        }
                }
                if(isTranswarping) {
                        // Check if didnt take damage etc
                        if(shieldPercent < 0.6) { isTranswarping = false; return; }
                        else if(settings.waitForSlaves) for(CombatBot &combatbot : combatBots) if(combatbot.lastShieldPercent < 0.6) { isTranswarping = false; return; }
                        bool ok = true;
                        for(Entity &entity : client.entities) {
                                if(entity.type == OBJECT_TYPES::ASTEROID || entity.type == OBJECT_TYPES::MOON || entity.type == OBJECT_TYPES::PLANET || entity.type == OBJECT_TYPES::STAR) {
                                        ok = false;
                                        break;
                                }
                        }
                        // Fly out
                        if(!ok) {
                                double angle = GetAngleTo(Point(0, 0), player.ship.pos);
                                Point where = MovePoint(player.ship.pos, angle, 3000);
                                AutopilotTo(where.x, where.y);
                                return;
                        }
                        if(settings.waitForSlaves && GetFarthestOutCombatBot() > 700) {
                                AI_ScoopLoots(false);
                                AI_REGEN();
                                return;
                        }
                        //////////if(!)
                }
        }
        // --- Unload loots
        void AI_UNLOADLOOTS() {
                client.memory.Update();
                THIS_LOOP_ACTIONS.push_back("UNLOAD_LOOTS");
                if(player.location != settings.unloadLoots_system) {
                        THIS_LOOP_ACTIONS.push_back("TRAVEL_TO_TARGET");
                        MoveTo(settings.unloadLoots_system);
                        return;
                }
                if(!client.isDocked) {
                        THIS_LOOP_ACTIONS.push_back("DOCK");
                        Entity *targetBase = nullptr;
                        for(Entity &entity : client.entities) if(entity.type == OBJECT_TYPES::PLAYERBASE && entity.teamname == player.ship.teamname
                                                && entity.name == settings.unloadLoots_base) { targetBase = &entity; break; }
                        if(targetBase != nullptr) {
                                if(DistanceTo(targetBase) > 50 && DistanceTo(targetBase) > player.ship._size / 2) AutopilotTo(targetBase);
                                else {
                                        StopShooting();
                                        if(targetBase->speed <= 10) ToggleAnchor(true);
                                        SelectTarget(targetBase);
                                        Dock();
                                }
                        } else {
                                ToggleAnchor(true);
                                THIS_LOOP_ACTIONS.push_back("CANT_FIND_BASE");
                        }
                        return;
                }
                if(player.hullUsed < hullUsedOnDock-5) {
                        FinishUnloadingLoots();
                        return;
                }
                THIS_LOOP_ACTIONS.push_back("TRANSFER_LOOTS");
                ToggleInventory(true);
                Sleep(50);
                int x, y;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.InventoryWindow+0x38), &x, 4, NULL);
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.InventoryWindow+0x3C), &y, 4, NULL);
                Point clickPoint((double)x+50, (double)(y+Random(130, 180)));
                GUIElement itemOptions(&client.memory);
                clock_t start = clock();
                do {
                        Click_Right(clickPoint);
                        Sleep(500);
                        itemOptions.Load(client.memory.InventoryItemOptions);
                } while(!itemOptions.isToggled && clock() < start+5*CLOCKS_PER_SEC);
                if(!itemOptions.isToggled) return;
                vector<string> options = String::split(itemOptions.text1, "|");
                String::trim(options);
                clickPoint.x = itemOptions.position.x+25;
                bool found = false;
                for(size_t i = 0; i<options.size(); i++) if(options[i].find("Transfer all") != string::npos || options[i] == "id_item_xfer_all_ship") { clickPoint.y = itemOptions.position.y+5+14*i; found = true; break; }
                if(!found) {
                        printf(CC_YELLOW, "%s could not find 'transfer all' when unloading loots\n", player.name.c_str());
                        printf("Item options: ");
                        for(string &str : options) printf("'%s', ", str.c_str());
                        printf("\n");
                        client.memory.Update();
                        return;
                }
                DWORD buffer = 0;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.InventoryItemOptions+0xC), &buffer, 4, NULL);
                ReadProcessMemory(client.memory.processHandle, LongToPtr(buffer+0xC), &itemOptions.address, 4, NULL);
                start = clock();
                do {
                        MouseMove(clickPoint);
                        Sleep(500);
                        itemOptions.Load(itemOptions.address);
                } while(!itemOptions.isToggled && clock() < start+5*CLOCKS_PER_SEC);
                if(!itemOptions.isToggled) return;
                clickPoint.x = itemOptions.position.x+10;
                options = String::split(itemOptions.text1, "|");
                String::trim(options);
                found = false;
                for(size_t i=0; i<options.size(); i++) if(options[i] == "Base") { clickPoint.y = itemOptions.position.y+5+i*14; found = true; break; }
                if(!found) {
                        printf(CC_YELLOW, "%s could not find 'base' when unloading loots\n", player.name.c_str());
                        client.memory.Update();
                        return;
                }
                start = clock();
                do {
                        MouseMove(clickPoint);
                        Sleep(50);
                        Click_Left(clickPoint);
                        Sleep(500);
                        itemOptions.Load(itemOptions.address);
                } while(itemOptions.isToggled && clock() < start+5*CLOCKS_PER_SEC);
                if(itemOptions.isToggled) return;
                Sleep(200);
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.playerData+OFFSETS::HullUsed), &player.hullUsed, 4, NULL);
                FinishUnloadingLoots();
        }
        void FinishUnloadingLoots() {
                // Log loots to file
                if(itemsOnDock.size() > 0) {
                        Sleep(1000);
                        // Get item list
                        client.inventory.Update();
                        vector<pair<string, int>> augs, commods, other;
                        bool written = false;
                        int transferred = 0;
                        for(InventoryItem &item : itemsOnDock) {
                                bool found = false;
                                for(InventoryItem &invItem : client.inventory.items) {
                                        if(item.name == invItem.name) {
                                                transferred = invItem.quantity - item.quantity;
                                                found = true;
                                                break;
                                        }
                                }
                                if(!found) transferred = item.quantity;
                                if(transferred != 0) {
                                        if(item.type == ITEM_AUGMENTER) augs.push_back(make_pair(item.name, transferred));
                                        else if(item.type == ITEM_COMMODITY || item.type == ITEM_ORE || item.type == ITEM_ALLOY) commods.push_back(make_pair(item.name, transferred));
                                        else other.push_back(make_pair(item.name, transferred));
                                        //logFile<<transferred<<" "<<item.name.c_str()<<endl;
                                        written = true;
                                }
                        }
                        // Sort
                        std::sort(augs.begin(), augs.end(), [](const pair<string, int> &left, const pair<string, int> &right) { return left.first < right.first; } );
                        std::sort(commods.begin(), commods.end(), [](const pair<string, int> &left, const pair<string, int> &right) { return left.first < right.first; } );
                        std::sort(other.begin(), other.end(), [](const pair<string, int> &left, const pair<string, int> &right) { return left.first < right.first; } );
                        // Save list
                        time_t ts = time(0);
                        char buffer[100];
                        strftime(buffer, 100, "%Y-%m-%d %H-%M-%S", localtime(&ts));
                        string fileName = string(buffer)+" "+player.name+".txt";
                        ofstream logFile("loot dumps/"+fileName);
                        if(commods.size() > 0) {
                                logFile<<"=== COMMODITIES"<<endl;
                                for(auto &item : commods) logFile<<item.second<<" "<<item.first.c_str()<<endl;
                        }
                        if(augs.size() > 0) {
                                logFile<<"=== AUGMENTERS"<<endl;
                                for(auto &item : augs) logFile<<item.second<<" "<<item.first.c_str()<<endl;
                        }
                        if(other.size() > 0) {
                                logFile<<"=== OTHERS"<<endl;
                                for(auto &item : other) logFile<<item.second<<" "<<item.first.c_str()<<endl;
                        }
                        logFile.close();
                        if(!written) remove(string("loot dumps/"+fileName).c_str());
                        else printf(CC_PINK, "%s has unloaded loots\n", player.name.c_str());
                }
                isUnloadingLoots = false;
                ToggleInventory(false);
                Undock();
        }
        // --- Revive
        void AI_REVIVE() {
                Entity *targetShip = nullptr;
                for(Entity &entity : client.entities) {
                        if(entity.name != player.name || entity.stasisType == 0) continue;
                        if(targetShip == nullptr) { targetShip = &entity; continue; }
                        if(DistanceTo(targetShip) > DistanceTo(&entity)) targetShip = &entity;
                }
                if(targetShip == nullptr) return;
                if(DistanceTo(targetShip) > targetShip->_size) AutopilotTo(targetShip);
                else if(targetShip->stasisType != 2) ToggleAnchor(true);
                else {
                        SelectTarget(targetShip);
                        Dock();
                        ToggleChatInput();
                        Sleep(500);
                        if(IsChatOpen()) ToggleChatInput();
                }
        }
        // --- Auras
        void AI_AURAS() {
                if((client.inventory.travelfield == nullptr || !client.inventory.travelfield->isEquipped) && client.inventory.enlightement != nullptr && !client.inventory.enlightement->isEquipped) client.inventory.EquipItem(client.inventory.enlightement, true);
        }
        // --- Prospector
        vector<string> prospector_resourcesInSystem, prospector_resourcesOnTarget;
        bool prospector_hasScannedSystem = false;
        vector<DWORD> prospector_scannedEntities;
        DWORD prospector_scanTarget = 0;
        clock_t prospector_scanTime = 0;
        void AI_PROSPECT() {
                if(client.inventory.cloak != nullptr && !IsCloakEnabled()) ToggleCloak();
                if(player.hullMax - player.hullUsed < 5) {
                        printf(CC_BLUE, "Prospector %s going to unload loots\n", player.name.c_str());
                        isUnloadingLoots = true;
                        return;
                }
                if(!hasInit_prospector) {
                        THIS_LOOP_ACTIONS.push_back("PROSPECTOR_INIT");
                        client.inventory.Update();
                        if(client.inventory.surfaceScanner == nullptr) {
                                printf(CC_YELLOW, "Prospector %s is missing surface scanner\n", player.name.c_str());
                                TASK = TASK_NONE;
                        }
                        if(client.inventory.systemScanner == nullptr) {
                                printf(CC_YELLOW, "Prospector %s is missing system scanner\n", player.name.c_str());
                                TASK = TASK_NONE;
                        }
                        if(client.inventory.prospectTractor == nullptr) {
                                printf(CC_YELLOW, "Prospector %s is missing prospector tractor\n", player.name.c_str());
                                TASK = TASK_NONE;
                        }
                        hasInit_prospector = true;
                        if(TASK == TASK_NONE) return;
                }
                // Get target
                if(task_targetLocation == "" && clock() > idleUntil) {
                        if(!*IS_CONNECTED || IS_INDEPENDENT) {
                                THIS_LOOP_ACTIONS.push_back("PROSPECTOR_FIND_NEW_TARGET");
                                UNIVERSE::mtx_findTarget.lock();
                                SYSTEM* newTarget = UNIVERSE::FindNearestProspectable(player.location, character.warpNavigation == 4, settings.prospectLayers, NULL, account.isP2P);
                                if(newTarget != NULL) {
                                        task_targetLocation = newTarget->name;
                                        newTarget->timeTakenByProspector = time(0);
                                        printf("%s prospect target set to '%s'\n", player.name.c_str(), task_targetLocation.c_str());
                                } else {
                                        printf(CC_PINK, "No available prospect targets left, %s task set to none and is returning to base\n", player.name.c_str());
                                        returnToBase = true;
                                        isUnloadingLoots = true;
                                        UnsetTask();
                                        idleUntil = clock()+300*CLOCKS_PER_SEC;
                                }
                                UNIVERSE::mtx_findTarget.unlock();
                        } else {
                                THIS_LOOP_ACTIONS.push_back("PROSPECT_REQUEST_NEW_TARGET");
                                json data{
                                        {"location", player.location}
                                };
                                for(string &layer : settings.prospectLayers) data["layers"].push_back(layer);
                                RequestTaskUpdate(data);
                        }
                        return;
                } else if(clock() < idleUntil) {
                        THIS_LOOP_ACTIONS.push_back("PROSPECTOR_IDLE");
                        if(player.location != settings.homeSystem) MoveTo(settings.homeSystem);
                        else if(GetSpeed() > 0) ToggleAnchor(true);
                        else ToggleAnchor(false);
                        return;
                }
                // Get to target
                if(player.location != task_targetLocation) {
                        THIS_LOOP_ACTIONS.push_back("PROSPECT_TRAVEL_TO_TARGET");
                        MoveTo(task_targetLocation);
                        return;
                }
                // If dying, run
                if(GetNearestWithinDistance(client.enemies, 3000, 0) != NULL &&  (timeToDeath < 5 || shieldPercent < 0.1)) {
                        THIS_LOOP_ACTIONS.push_back("PROSPECT_AVOID_DEATH");
                        isRegening = true;
                }
                if(isRegening) {
                        THIS_LOOP_ACTIONS.push_back("PROSPECT_REGEN");
                        Entity *nearestStar = GetEntityClosestTo(player.ship.pos, OBJECT_TYPES::STAR);
                        if(GetNearestWithinDistance(client.enemies, 3000, 0) != NULL || (nearestStar != nullptr && DistanceTo(nearestStar) < nearestStar->_size*0.5*1.1)) {
                                Point apTarget = MovePoint(player.ship.pos, GetAngleTo(Point(0, 0), player.ship.pos), 1000);
                                AutopilotTo(apTarget.x, apTarget.y);
                        } else if(GetSpeed() > 0) ToggleAnchor(true);
                        else ToggleAnchor(false);
                        return;
                }
                // Scan sysetm
                if(!prospector_hasScannedSystem) {
                        if(GetDangerFactor() > 1 && GetNearestWithinDistance(client.enemies, 1500, 0) != NULL && clock() > prospector_scanTime + 5*CLOCKS_PER_SEC) {
                                THIS_LOOP_ACTIONS.push_back("PROSPECT_GET_DISTANCE");
                                Point apTarget = MovePoint(player.ship.pos, GetAngleTo(Point(0, 0), player.ship.pos), 1000);
                                AutopilotTo(apTarget.x, apTarget.y);
                                return;
                        }
                        if(client.inventory.systemScanner != nullptr && client.inventory.systemScanner->IsCharged()) {
                                THIS_LOOP_ACTIONS.push_back("PROSPECT_SYSTEM_SCAN");
                                StopShooting();
                                clock_t start = clock();
                                while(GetSpeed() >= 40 && clock() < start+5*CLOCKS_PER_SEC) { ToggleAnchor(true); Sleep(10); }
                                if(GetSpeed() < 40) {
                                        ToggleAnchor(false);
                                        client.inventory.systemScanner->UseItem();
                                        prospector_scanTime = clock();
                                        Sleep(50);
                                }
                                return;
                        } else if(clock() > prospector_scanTime + 8*CLOCKS_PER_SEC && GetAutopilotMode() == 0) {
                                THIS_LOOP_ACTIONS.push_back("PROSPECT_WAIT_FOR_CHARGE");
                                Entity *nearestWormhole = GetEntityClosestTo(player.ship.pos, OBJECT_TYPES::WORMHOLE);
                                if(nearestWormhole != nullptr && DistanceTo(nearestWormhole) < 1500) {
                                        Point apTarget = MovePoint(player.ship.pos, GetAngleTo(Point(0, 0), player.ship.pos), DistanceBetween(Point(0, 0), nearestWormhole->pos)+1500);
                                        AutopilotTo(apTarget.x, apTarget.y);
                                }
                        }
                        return;
                }
                // If nothing left, find new target
                if(prospector_hasScannedSystem && prospector_resourcesInSystem.size() == 0) {
                        Prospector_FinishSystem();
                        return;
                }
                // Extract resources
                if(prospector_resourcesOnTarget.size() > 0) {
                        THIS_LOOP_ACTIONS.push_back("PROSPECTOR_EXTRACT");
                        target = GetEntity(prospector_scanTarget);
                        if(target != nullptr) {
                                canUseTorch = DistanceTo(target) > settings.prospectTractorRange*2;
                                if(DistanceTo(target) > settings.prospectTractorRange) {
                                        if(target->type == OBJECT_TYPES::STAR) {
                                                double dist = target->_size*0.5*1.3;
                                                if(dist > settings.prospectTractorRange*0.95) dist = settings.prospectTractorRange*0.95;
                                                Point apTarget = MovePoint(target->pos, GetAngleTo(target->pos, player.ship.pos), dist);
                                                AutopilotTo(apTarget.x, apTarget.y);
                                        } else AutopilotTo(target);
                                } else {
                                        if((!target->isTractored || !player.ship.isTractored) && energyPercent > 0.2) {
                                                SelectTarget(target);
                                                if(GetAutopilotMode() == 0) ToggleTractor();
                                                Sleep(200);
                                        }
                                        Point currentApTarget = GetAutopilotTarget();
                                        if(target->_size*0.5 < settings.prospectTractorRange*0.95 && (GetAutopilotMode() == 0 || DistanceTo(currentApTarget) < 50) && (GetNearestWithinDistance(client.enemies, 1000, 0) != NULL && isUnderAttack)) {
                                                Point apTarget;
                                                apTarget.x = Random((int)target->pos.x-settings.prospectTractorRange*0.5, (int)target->pos.x+settings.prospectTractorRange*0.5);
                                                apTarget.y = Random((int)target->pos.y-settings.prospectTractorRange*0.5, (int)target->pos.y+settings.prospectTractorRange*0.5);
                                                AutopilotTo(apTarget.x, apTarget.y);
                                        } else if(GetAutopilotMode() == 0 && GetSpeed() > 0) ToggleAnchor(true);
                                        else ToggleAnchor(false);
                                }
                        }
                        return;
                }
                // Scan solarbodies
                if(prospector_scanTarget == 0) {
                        THIS_LOOP_ACTIONS.push_back("PROSPECTOR_FIND_NEXT_SCAN_TARGET");
                        Entity *newTarget = nullptr;
                        for(Entity &entity : client.entities) {
                                if(entity.type != OBJECT_TYPES::STAR && entity.type != OBJECT_TYPES::PLANET && entity.type != OBJECT_TYPES::MOON) continue;
                                if(std::find(prospector_scannedEntities.begin(), prospector_scannedEntities.end(), entity.id) != prospector_scannedEntities.end()) continue;
                                if(newTarget == nullptr) { newTarget = &entity; continue; }
                                if(DistanceTo(&entity) < DistanceTo(newTarget)) newTarget = &entity;
                        }
                        if(newTarget != nullptr) prospector_scanTarget = newTarget->id;
                } else {
                        if(client.entities.size() == 0) return;
                        THIS_LOOP_ACTIONS.push_back("PROSPECTOR_SCAN_TARGET");
                        target = GetEntity(prospector_scanTarget);
                        if(target == nullptr) {
                                printf(CC_YELLOW, "%s failed to find entity ID %X for scan\n", player.name.c_str(), prospector_scanTarget);
                                prospector_scannedEntities.push_back(prospector_scanTarget);
                                prospector_scanTarget = 0;
                                return;
                        } else if(DistanceTo(target) > settings.surfaceScannerRange) {
                                if(target->type == OBJECT_TYPES::STAR) {
                                        double dist = target->_size*0.5*1.3;
                                        if(dist > settings.surfaceScannerRange*0.95) dist = settings.surfaceScannerRange*0.95;
                                        Point apTarget = MovePoint(target->pos, GetAngleTo(target->pos, player.ship.pos), dist);
                                        AutopilotTo(apTarget.x, apTarget.y);
                                } else AutopilotTo(target);
                        } else {
                                if(target->type == OBJECT_TYPES::STAR) {
                                        if(GetSpeed() > 0) ToggleAnchor(true);
                                        else ToggleAnchor(false);
                                }
                                SelectTarget(target);
                                AutopilotOff();
                                if(client.inventory.surfaceScanner != nullptr) {
                                        if(client.inventory.surfaceScanner->IsCharged()) {
                                                client.inventory.surfaceScanner->UseItem();
                                                Sleep(50);
                                        } else if(GetSpeed() > 0) ToggleAnchor(true);
                                        else ToggleAnchor(false);
                                }
                                Sleep(10);
                                return;
                        }
                }
                // If nothing found, abandon
                if(prospector_scanTarget == 0) {
                        printf(CC_YELLOW, "%s was unable to find resources in %s: ", player.name.c_str(), player.location.c_str());
                        for(string &str : prospector_resourcesInSystem) printf(CC_YELLOW, "%s, ", str.c_str());
                        printf("\n");
                        Prospector_FinishSystem();
                        prospector_resourcesInSystem.clear();
                }
        }
        clock_t prospector_lastUniverseSave = 0;
        void Prospector_FinishSystem() {
                if(!*IS_CONNECTED || IS_INDEPENDENT) {
                        SYSTEM *targetSystem = UNIVERSE::FindSystem(task_targetLocation);
                        if(targetSystem != NULL) {
                                targetSystem->lastProspected = time(0);
                                if(clock() > prospector_lastUniverseSave+15*CLOCKS_PER_SEC) {
                                        UNIVERSE::Save();
                                        prospector_lastUniverseSave = clock();
                                }
                        }
                } else {
                        //add controller msg idk TODO
                }
                task_targetLocation = "";
        }
        // --- Repair
        void AI_REPAIR() {
                /*if(repairInSystem == ""){
                        SYSTEM* targetSystem = UNIVERSE::FindNearestAIBase(player.location, character.warpNavigation, account.isP2P);
                        if(targetSystem == nullptr){
                                settings.canRepair = false;
                                printf(CC_RED, "%s was unable to find a system to repair at\n", player.name.c_str());
                        }else{
                                repairInSystem = targetSystem->name;
                                printf(CC_BLUE, "%s is going to repair in %s\n", player.name.c_str(), repairInSystem.c_str());
                        }
                        return;
                }
                if(player.location != repairInSystem){
                        MoveTo(repairInSystem);
                        return;
                }*/

        }
        // --- Search
        vector<DWORD> markedEnemies;
        void AI_SEARCH(){
                if(player.location != lastLocation) markedEnemies.clear(); // buzz
                // Get target
                size_t targets = 0;
                target = GetEntity(targetID);
                for(Entity *entity : client.enemies){
                        if(entity->level < 400) continue;
                        if(DistanceTo(entity) > 5000) continue;
                        targets++;
                        if(std::find(markedEnemies.begin(), markedEnemies.end(), entity->id) != markedEnemies.end()) continue;
                        if(target == nullptr) target = entity;
                        else if(DistanceTo(target) > DistanceTo(entity)) target = entity;
                }
                if(target != nullptr){
                        targetID = target->id;
                        if(settings.canMarkAsEnemy){
                                if(IsTargetOptionsOpen()) ToggleTargetOptions();
                                if(DistanceTo(target) > 1000) AutopilotTo(target);
                                else if(!target->IsVisible()) AutopilotTo(target);
                                SelectTarget(target);
                                Sleep(100);
                                if(!IsTargetOptionsOpen()) ToggleTargetOptions();
                                clock_t _start = clock();
                                while(clock() < _start+5*1000 && !IsTargetOptionsOpen()) { ToggleTargetOptions(); Sleep(500); }
                                Sleep(500);
                                if(!client.memory.Calculate2("target_options", client.memory.TargetOptions)) return;
                                GUIElement guielement(&client.memory);
                                guielement = GUIElement::FindInList(client.memory.TargetOptions, "Mark Team Enemy", &client.memory);
                                if(guielement.ok){
                                        ToggleGUIValue(guielement, true);
                                        markedEnemies.push_back(target->id);
                                }
                        }
                        if(DistanceTo(target) < settings.tractorRange && !target->isTractored) ToggleTractor();
                }
                // If no targets, move to next gal
                if(targets == 0 && AreFollowersHere()){
                        if(player.location != task_targetLocation && task_targetLocation != "") MoveTo(task_targetLocation);
                        else{
                                SYSTEM *newTarget = UNIVERSE::FindRandom(player.location, settings.minDF, settings.maxDF, settings.dgLayers, Random(3, 8));
                                if(newTarget != nullptr) task_targetLocation = newTarget->name;
                        }
                } else ToggleAnchor(true);
        }
        /// --- DATA COLLECTION - a bit more complex than GETS
        UINT CountObjects(USHORT type) {
                UINT ret = 0;
                for(Entity& entity : client.entities) if(entity.type == type) ret++;
                return ret;
        }
        void CollectSystemData() {
                client.mtx_objectsList->lock(); /// don't change objlist while collecting data
                if(!isInDungeon) {
                        UINT thisSystemID = GetSystemID();
                        for(SYSTEM &sys : GALAXY) if(sys.id == thisSystemID && sys.isExplored) { hasExploredThisSystem = true; client.mtx_objectsList->unlock(); return; }
                        printf("%s collecting system data for %s(%X)\n", player.name.c_str(), player.location.c_str(), thisSystemID);
                        SYSTEM newSystem(thisSystemID);
                        newSystem.name = player.location;
                        newSystem.dangerFactor = GetDangerFactor();
                        newSystem.lastCleared = 0;
                        newSystem.lastProspected = 0;
                        newSystem.ownedBy = systemOwnedBy;
                        newSystem.layer = player.universeLayer;
                        for(Entity& entity : client.entities) if(entity.type == OBJECT_TYPES::WORMHOLE) newSystem.wormholes.push_back(entity.name);
                        newSystem.stars = CountObjects(OBJECT_TYPES::STAR);
                        newSystem.planets = CountObjects(OBJECT_TYPES::PLANET);
                        newSystem.moons = CountObjects(OBJECT_TYPES::MOON);
                        newSystem.asteroids = CountObjects(OBJECT_TYPES::ASTEROID);
                        newSystem.aibases = CountObjects(OBJECT_TYPES::AIBASE);
                        newSystem.playerbases = CountObjects(OBJECT_TYPES::PLAYERBASE);
                        newSystem.isExplored = true;
                        if(UNIVERSE::AddSystem(newSystem)) {
                                if(CONNECT_TO_MASTER && *IS_CONNECTED) {
                                        json newMsg = {
                                                {"action", "AddSystem"},
                                                {"data", newSystem.to_JSON()}
                                        };
                                        try {
                                                printf(CC_BLUE, "Sending new system for '%s' data to controller\n", newSystem.name.c_str());
                                                ConnectionMessageQueue.push_back(newMsg.dump());
                                        } catch(const exception &e) { printf(CC_YELLOW, "Error sending new system data to controller: %s\n", e.what()); }
                                }
                                UNIVERSE::Save();
                                printf("New system data for %s saved\n", newSystem.name.c_str());
                        }
                } else {
                        if(thisDungeon.name != "" && hasEnteredTargetDungeon && UNIVERSE::GetSystemFromDG(player.location) == UNIVERSE::GetSystemFromDG(task_targetLocation)) {
                                DUNGEONLEVEL *thisLvl = nullptr;
                                for(DUNGEONLEVEL &lvl : thisDungeon.levels) if(lvl.name == player.location) { thisLvl = &lvl; break; }
                                if(thisLvl == nullptr) {
                                        thisDungeon.hasUpdatedData = true;
                                        thisDungeon.levels.push_back(DUNGEONLEVEL());
                                        thisLvl = &thisDungeon.levels.back();
                                        thisLvl->completed = false;
                                        thisLvl->name = player.location;
                                }
                                if(!thisLvl->completed) {
                                        thisLvl->name = player.location;
                                        vector<pair<Entity*, int>> enemies;
                                        for(Entity &entity : client.entities) {
                                                if(entity.type != OBJECT_TYPES::SHIP) continue;
                                                if(entity.teamname.find("DX") != 0) continue;
                                                if(!entity.IsFighter() && !entity.IsMissile()) {
                                                        auto it = std::find_if(enemies.begin(), enemies.end(), [&entity](const pair<Entity*, int> &element) { return element.first->shipname == entity.shipname; });
                                                        if(it != enemies.end()) (*it).second++;
                                                        else enemies.push_back(make_pair(&entity, 1));
                                                }
                                        }
                                        // set data on enemies
                                        pair<Entity*, int> *highestAmount = nullptr;
                                        for(auto &type : enemies) {
                                                if(highestAmount == nullptr) highestAmount = &type;
                                                if(type.second > highestAmount->second) highestAmount = &type;
                                        }
                                        if(highestAmount != nullptr && highestAmount->second > thisLvl->enemyAmount) {
                                                thisLvl->enemy = highestAmount->first->shipname;
                                                thisLvl->enemyLevel = highestAmount->first->level;
                                                thisLvl->enemyAmount = highestAmount->second;
                                                thisDungeon.hasUpdatedData = true;
                                        }
                                        // set data on boss
                                        if(thisDungeonLevel == 0 && thisLvl->boss == "") {
                                                for(auto &type : enemies) {
                                                        if(type.second == 1) {
                                                                thisLvl->boss = type.first->shipname;
                                                                thisLvl->bossLevel = type.first->level;
                                                                thisDungeon.hasUpdatedData = true;
                                                        }
                                                }
                                        }
                                        // set wormholes
                                        bool foundWh = false;
                                        if(thisLvl->wormholes.size() == 0) for(Entity &entity : client.entities) if(entity.type == OBJECT_TYPES::WORMHOLE) {
                                                                if(UNIVERSE::GetSystemFromDG(entity.name) != UNIVERSE::GetSystemFromDG(task_targetLocation)) continue;
                                                                foundWh = true;
                                                                thisDungeon.hasUpdatedData = true;
                                                                thisLvl->wormholes.push_back(entity.name);
                                                                if(!UNIVERSE::IsDungeon(entity.name)) continue;
                                                                string nextLvlName = UNIVERSE::GetSystemFromWormhole(entity.name);
                                                                bool listed = false;
                                                                for(DUNGEONLEVEL &lvl : thisDungeon.levels) if(lvl.name == nextLvlName) { listed = true; break; }
                                                                if(listed) continue;
                                                                DUNGEONLEVEL nextLvl;
                                                                nextLvl.completed = false;
                                                                nextLvl.name = nextLvlName;
                                                                thisDungeon.levels.push_back(nextLvl);
                                                        }
                                        if(!foundWh && thisLvl->wormholes.size() == 0) {
                                                printf("WH error on %s - pause now, time: %i\n", player.name.c_str(), clock() - systemEnterTime);
                                                PAUSE = true;
                                        }
                                }
                        }
                }
                client.mtx_objectsList->unlock();
                hasExploredThisSystem = true;
        }
        void ProcessChatMessages() {
                for(ChatMessage& msg : client.chat.newMessages) {
                        if(msg.text.find("and peace kept by") != string::npos) systemOwnedBy = msg.text.substr(msg.text.find("and peace kept by")+string("and peace kept by ").length());
                        else if(msg.text.find("Galaxy owned by") != string::npos) systemOwnedBy = msg.text.substr(string("Galaxy owned by ").length());
                        String::trim(systemOwnedBy);
                        if(msg.text.find("Entering instanced area") != string::npos) isInInstance = true;
                        else if(msg.text.find("leads to a premium only") != string::npos) {
                                Entity* thisWormhole = GetEntityClosestTo(player.ship.pos, OBJECT_TYPES::WORMHOLE);
                                if(thisWormhole != NULL && DistanceTo(thisWormhole) < 500) {
                                        string thisSystem = UNIVERSE::GetSystemFromWormhole(thisWormhole->name);
                                        for(string &step : currentPath) if(step == thisSystem) currentPath.clear();
                                        if(task_targetLocation == thisSystem) task_targetLocation = "";
                                        if(task_adjacentLocation == thisSystem) task_adjacentLocation = "";
                                        UNIVERSE::AddP2PSystem(thisSystem);
                                        UNIVERSE::SaveLists();
                                        json newMsg{
                                                {"action", "AddP2PSystem"},
                                                {"name", thisSystem}
                                        };
                                        ConnectionMessageQueue.push_back(newMsg.dump());
                                }
                        } else if(msg.text.find("wormhole requires a key") != string::npos || msg.text.find("wormhole will remain locked") != string::npos
                                        || msg.text.find("currently blocked by") != string::npos || msg.text.find("cannot be entered with an equipped engine") != string::npos) {
                                if(TASK == TASK_EXPLORE || TASK == TASK_PROSPECT) {
                                        Entity* thisWormhole = GetEntityClosestTo(player.ship.pos, OBJECT_TYPES::WORMHOLE);
                                        if(thisWormhole != NULL && DistanceTo(thisWormhole) < 500) {
                                                string thisSystem = UNIVERSE::GetSystemFromWormhole(thisWormhole->name);
                                                if(task_targetLocation == thisSystem) task_targetLocation = "";
                                                if(task_adjacentLocation == thisSystem) task_adjacentLocation = "";
                                                UNIVERSE::AddIgnoredSystem(thisSystem);
                                                printf(CC_YELLOW, "%s was unable to warp through through %s, adding %s to ignored systems\n", player.name.c_str(), thisWormhole->name.c_str(),
                                                       thisSystem.c_str());
                                                UNIVERSE::SaveLists();
                                                json newMsg{
                                                        {"action", "AddIgnoredSystem"},
                                                        {"name", thisSystem}
                                                };
                                                ConnectionMessageQueue.push_back(newMsg.dump());
                                        }
                                }
                        } else if(msg.text.find("Insufficient skill in Warp Navigation") != string::npos) {
                                if(TASK == TASK_EXPLORE || TASK == TASK_PROSPECT) {
                                        Entity* thisWormhole = GetEntityClosestTo(player.ship.pos, OBJECT_TYPES::WORMHOLE);
                                        if(thisWormhole != NULL && DistanceTo(thisWormhole) < 500) {
                                                string thisSystem = UNIVERSE::GetSystemFromWormhole(thisWormhole->name);
                                                if(task_targetLocation == thisSystem) task_targetLocation = "";
                                                if(task_adjacentLocation == thisSystem) task_adjacentLocation = "";
                                                UNIVERSE::AddWarp4System(thisSystem);
                                                printf(CC_YELLOW, "%s adding %s to warp4 systems\n", player.name.c_str(), thisSystem.c_str());
                                                UNIVERSE::SaveLists();
                                                json newMsg{
                                                        {"action", "AddWarp4System"},
                                                        {"name", thisSystem}
                                                };
                                                ConnectionMessageQueue.push_back(newMsg.dump());
                                        }
                                }
                        } else if(combatBots.size() > 0 && (msg.text.find("entering stasis") != string::npos || msg.text.find("was too wild to enter stasis") != string::npos)) {
                                for(CombatBot &combatbot : combatBots) if(msg.text.find(combatbot.name) != string::npos) {
                                                if(combatbot.isWild) printf(CC_YELLOW, "%s's wild bot '%s' exploded\n", player.name.c_str(), combatbot.shipType.c_str());
                                                combatbot.isDead = true;
                                                break;
                                        }
                        } else if(msg.text.find("now locked out") != string::npos) {
                                UINT lockoutTime = GetLockoutTime(msg.text);
                                if(thisDungeon.name != "") LOCKOUTS::Add(player.name, thisDungeon.name, time(0)+lockoutTime);
                                if(lockoutTime > 60*60) {
                                        bool getKillMessage = false;
                                        for(ChatMessage &msg2 : client.chat.newMessages) {
                                                if(msg2.text == msg.text) { getKillMessage = true; continue; }
                                                if(getKillMessage) {
                                                        size_t pos1, pos2;
                                                        pos1 = msg2.text.find("kill of");
                                                        if(pos1 != string::npos) pos1 += 7;
                                                        else{
                                                                pos1 = msg2.text.find("Killed ");
                                                                if(pos1 != string::npos) pos1 += 7;
                                                                else continue;
                                                        }
                                                        pos2 = msg2.text.find_first_of(",");
                                                        if(pos2 == string::npos) pos2 = msg2.text.find_first_of("(");
                                                        if(pos2 == string::npos) continue;
                                                        string killName = msg2.text.substr(pos1, pos2-pos1);
                                                        String::trim(killName);
                                                        printf(CC_CYAN, "%s has killed %s in %s and got a long lockout\n", player.name.c_str(), killName.c_str(), player.location.c_str());
                                                        ofstream logFile("lockout_kills.txt", std::ofstream::app);
                                                        logFile<<player.name.c_str()<<" has murdered "<<killName.c_str()<<" in "<<player.location.c_str()<<endl;
                                                        logFile.close();
                                                        break;
                                                }
                                        }
                                }
                                isBossKilled = true;
                        } else if(msg.text.find("prevent you from entering") != string::npos) {
                                if(thisDungeon.name != "") LOCKOUTS::Add(player.name, thisDungeon.name, time(0)+GetLockoutTime(msg.text));
                                bool finish = false;
                                // Find target lvl and mark it completed
                                Entity* thisWormhole = GetEntityClosestTo(player.ship.pos, OBJECT_TYPES::WORMHOLE);
                                if(thisWormhole != NULL && DistanceTo(thisWormhole) < 500) {
                                        string thisLvlName = UNIVERSE::GetSystemFromWormhole(thisWormhole->name);
                                        DUNGEONLEVEL *targetLvl = _DUNGEONS::FindDungeonLevel(thisLvlName, thisDungeon);
                                        if(targetLvl != NULL) targetLvl->completed = true;
                                        else finish = true;
                                } else finish = true;
                                if(targetDungeonLevel != nullptr) targetDungeonLevel->completed = true;
                                // Try to find unfinished
                                currentPath = _DUNGEONS::FindPathToNextSplit(player.location, thisDungeon);
                                if(currentPath.size() == 0) finish = true;
                                else targetDungeonLevel = _DUNGEONS::FindDungeonLevel(currentPath.back(), thisDungeon);
                                if(targetDungeonLevel == NULL) finish = true;
                                if(finish) FinishDG();
                        } else if(msg.text.find("Not enough space") != string::npos || msg.text.find("doesn't belong to you") != string::npos) {
                                IgnoreTargetLoot();
                        } else if(msg.text.find("Nothing valid or unequipped to transfer") != string::npos) {
                                isUnloadingLoots = false;
                        } else if(msg.tab == "Galaxy" && msg.sender != "" && player.location != "Sol" && msg.sender != player.name) {
                                printf(CC_YELLOW, "Message on %s:\n\t(%s) %s\n", player.name.c_str(), msg.sender.c_str(), msg.text.c_str());
                                SoundWarning("sound.mp3");
                                if((GetDangerFactor() > 0.3 || isInDungeon) && LastActiveWindow != client.clientWindow){
                                        stringstream ss;
                                        ss<<"System message in "<<player.location.c_str()<<" on "<<player.name.c_str()<<": ("<<msg.sender.c_str()<<") "<<msg.text.c_str();
                                        Report(ss.str());
                                }
                        } else if(msg.sender != "" && msg.tab != "Event" && msg.tab != "Galaxy" && msg.tab != "All" && msg.tab != "Trade" && msg.tab != "Moderator" && msg.tab != "Help"
                                  && msg.tab != "LFG" && msg.tab != "Team" && msg.tab != "Squad" && msg.sender != player.name) {
                                printf(CC_YELLOW, "%s received PM: \n\t(%s) %s\n", player.name.c_str(), msg.sender.c_str(), msg.text.c_str());
                                SoundWarning("sound.mp3");
                                if(LastActiveWindow != client.clientWindow){
                                        stringstream ss;
                                        ss<<player.name.c_str()<<" received PM: ("<<msg.sender.c_str()<<") "<<msg.text.c_str();
                                        Report(ss.str());
                                }
                        } else if(msg.tab == "Event" && TASK == TASK_PROSPECT) {
                                if(msg.text.find("nodes detected in scan") != string::npos) {
                                        size_t pos1, pos2;
                                        pos1 = msg.text.find_last_of(':');
                                        pos2 = msg.text.find_last_of('.');
                                        if(pos1 != string::npos && pos2 != string::npos) {
                                                string part = msg.text.substr(pos1+1, pos2-pos1-1);
                                                String::trim(part);
                                                if(part != "") prospector_resourcesInSystem = String::split(part, ",");
                                                for(string &str : prospector_resourcesInSystem) String::trim(str);
                                                if(prospector_resourcesInSystem.size() > 0) {
                                                        for(size_t i=prospector_resourcesInSystem.size()-1; i>= 0; i--) {
                                                                if(std::find(settings.prospectIgnoreResources.begin(), settings.prospectIgnoreResources.end(), prospector_resourcesInSystem[i]) != settings.prospectIgnoreResources.end())
                                                                        prospector_resourcesInSystem.erase(prospector_resourcesInSystem.begin()+i);
                                                                if(i == 0) break;
                                                        }
                                                }
                                        }
                                        prospector_hasScannedSystem = true;
                                } else if(msg.text.find("Nothing detected") != string::npos) {
                                        prospector_scannedEntities.push_back(prospector_scanTarget);
                                        prospector_scanTarget = 0;
                                } else if(msg.text.find("Nodes on") != string::npos) {
                                        size_t pos1, pos2;
                                        pos1 = msg.text.find_first_of(':');
                                        pos2 = msg.text.find_last_of('.');
                                        if(pos2 == string::npos) pos2 = msg.text.find_last_of(',');
                                        if(pos2 == string::npos) pos2 = msg.text.length();
                                        if(pos1 != string::npos && pos2 != string::npos) {
                                                string part = msg.text.substr(pos1+1, pos2-pos1-1);
                                                String::trim(part);
                                                if(part.length() > 1) prospector_resourcesOnTarget = String::split(part, ","); \
                                                size_t pos;
                                                for(size_t i = 0; i<prospector_resourcesOnTarget.size(); i++) {
                                                        pos = prospector_resourcesOnTarget[i].find(" of ");
                                                        if(pos != string::npos) prospector_resourcesOnTarget[i] = prospector_resourcesOnTarget[i].substr(pos+4);
                                                        pos = prospector_resourcesOnTarget[i].find("little");
                                                        if(pos != string::npos) prospector_resourcesOnTarget[i] = prospector_resourcesOnTarget[i].substr(pos+6);
                                                        String::trim(prospector_resourcesOnTarget[i]);
                                                }
                                                for(string &resource : prospector_resourcesOnTarget) {
                                                        if(std::find(settings.prospectIgnoreResources.begin(), settings.prospectIgnoreResources.end(), resource) != settings.prospectIgnoreResources.end()) {
                                                                prospector_resourcesOnTarget.clear();
                                                                break;
                                                        }
                                                }

                                        }
                                        if(prospector_resourcesOnTarget.size() == 0) {
                                                prospector_scannedEntities.push_back(targetID);
                                                prospector_scanTarget = 0;
                                        }
                                } else if(msg.text.find("No prospectable resources on") != string::npos) {
                                        for(string &str : prospector_resourcesOnTarget) {
                                                auto it = prospector_resourcesInSystem.begin();
                                                while(it != prospector_resourcesInSystem.end()) {
                                                        if((*it) == str) {
                                                                ofstream file("extracted_resources.txt", ofstream::app);
                                                                file<<GetTimestamp().c_str()<<" "<<player.name.c_str()<<" extracted "<<str.c_str()<<" in "<<player.location.c_str()<<endl;
                                                                file.close();
                                                                printf(CC_GREEN, "%s extracted %s in %s\n", player.name.c_str(), str.c_str(), player.location.c_str());
                                                                prospector_resourcesInSystem.erase(it);
                                                                break;
                                                        }
                                                        ++it;
                                                }
                                        }
                                        /*
                                        if(prospector_resourcesInSystem.size() > 0 && prospector_resourcesOnTarget.size() > 0){
                                                for(size_t i=prospector_resourcesInSystem.size()-1; i>=0; i--){
                                                        for(size_t r=prospector_resourcesOnTarget.size()-1; r>=0; r--){
                                                                if(prospector_resourcesOnTarget[r] == prospector_resourcesInSystem[i]){
                                                                        ofstream file("extracted_resources.txt", ofstream::app);
                                                                        file<<GetTimestamp().c_str()<<" "<<player.name.c_str()<<" extracted "<<prospector_resourcesOnTarget[r].c_str()<<" in "<<player.location.c_str()<<endl;
                                                                        file.close();
                                                                        printf(CC_GREEN, "%s extracted %s in %s\n", player.name.c_str(), prospector_resourcesOnTarget[r].c_str(), player.location.c_str());
                                                                        prospector_resourcesInSystem.erase(prospector_resourcesInSystem.begin()+i);
                                                                        prospector_resourcesOnTarget.erase(prospector_resourcesOnTarget.begin()+r);
                                                                        break;
                                                                }
                                                                if(r == 0) break;
                                                        }
                                                        if(i == 0) break;
                                                }
                                        }*/
                                        if(prospector_resourcesOnTarget.size() > 0) {
                                                printf("%s resources: ", player.name.c_str());
                                                for(string &str : prospector_resourcesOnTarget) printf("'%s', ", str.c_str());
                                                printf("\n");
                                        }
                                        prospector_resourcesOnTarget.clear();
                                } else if(msg.text.find("You must target") != string::npos) {
                                        Entity *target = GetEntity(prospector_scanTarget);
                                        if(target != nullptr) printf(CC_YELLOW, "%s was unable to target '%s' in %s\n", player.name.c_str(), target->name.c_str(), player.location.c_str());
                                        prospector_scannedEntities.push_back(prospector_scanTarget);
                                        prospector_scanTarget = 0;
                                }
                        } // prospect
                        else if(msg.sender == task_followTargetName){
                                if(msg.text == "Going to unload loots") task_followPause = true;
                                else if(msg.text == "Follow") task_followPause = false;
                        }
                }
        }
        void UpdateCombatBots() {
                for(Entity &entity : client.entities) {
                        if(!entity.IsMyBot()) continue;
                        if(GetPlayerNameFromBot(entity.name) != player.name) continue;
                        bool isListed = false;
                        for(CombatBot &combatbot : combatBots) if(combatbot.id == entity.id) { isListed = true; break; }
                        if(isListed) continue;
                        if(entity.name.find("WB") == 0 || entity.name.find("CB") == 0) {
                                SelectTarget(entity.id);
                                ToggleTargetOptions(); // to force update from server?
                                Sleep(500);
                                CombatBot newBot;
                                newBot.id = entity.id;
                                if(entity.name.find("WB") == 0) newBot.isWild = true;
                                newBot.name = entity.name;
                                newBot.shipType = entity.shipname;
                                newBot.lastSeenAt = player.location;
                                newBot.lastSeenTime = time(0);
                                clock_t _start = clock();
                                while(clock() < _start+5*1000 && !IsTargetOptionsOpen()) { ToggleTargetOptions(); Sleep(500); }
                                Sleep(1000);
                                if(!client.memory.Calculate2("target_options", client.memory.TargetOptions)) return;
                                GUIElement guielement(&client.memory);
                                //follow me
                                guielement = GUIElement::FindInList(client.memory.TargetOptions, "Follow Me", &client.memory);
                                if(!guielement.isEnabled) {
                                        SelectTarget(entity.id);
                                        ToggleGUIValue(guielement, true);
                                }
                                //fight enemies
                                guielement = GUIElement::FindInList(client.memory.TargetOptions, "Fight Enemies", &client.memory);
                                if(!guielement.isEnabled) {
                                        SelectTarget(entity.id);
                                        ToggleGUIValue(guielement, true);
                                }
                                //stay close
                                guielement = GUIElement::FindInList(client.memory.TargetOptions, "Stay Close", &client.memory);
                                if(guielement.isEnabled) {
                                        SelectTarget(entity.id);
                                        ToggleGUIValue(guielement, false);
                                }
                                //v formation
                                guielement = GUIElement::FindInList(client.memory.TargetOptions, "V Formation", &client.memory);
                                if(guielement.isEnabled) {
                                        SelectTarget(entity.id);
                                        ToggleGUIValue(guielement, false);
                                }
                                //attack my target
                                guielement = GUIElement::FindInList(client.memory.TargetOptions, "Attack My Target", &client.memory);
                                if(!guielement.isEnabled) {
                                        SelectTarget(entity.id);
                                        ToggleGUIValue(guielement, true);
                                }
                                newBot.isOK = true;
                                combatBots.push_back(newBot);
                        }
                }
                if(IsTargetOptionsOpen()) ToggleTargetOptions();
                for(CombatBot &combatbot : combatBots) {
                        Entity *entityBot = GetEntity(combatbot.id);
                        if(entityBot != NULL) {
                                combatbot.lastSeenTime = time(0);
                                combatbot.isLost = false;
                                combatbot.lastShieldPercent = GetEntityShieldPercent(entityBot);
                                combatbot.isDead = entityBot->stasisType != 0;
                        }else if(time(0) > combatbot.lastSeenTime + 300) combatbot.isLost = true;
                }
                nextCombatBotCheckTime = clock() + 30*CLOCKS_PER_SEC;
        }
        void IgnoreTargetLoot() {
                ignoredDebris.push_back(IgnoredDebris(scoopTargetID, player.location));
                scoopTargetID = 0;
                scoopTargetTime = 0;
                scoopTargetTime_LongDistance = 0;
                scoopTarget = nullptr;
        }
        void IgnoreTarget() {
                client.ignoredEnemies.push_back(IgnoredDebris(targetID, player.location));
                targetID = 0;
                targetTime = 0;
                target = nullptr;
        }
        void GetWeapons() {
                weapons.clear();
                ToggleRadar(true);
                if(client.inventory.items.size() == 0) client.inventory.Update();
                for(InventoryItem &item : client.inventory.items) {
                        if(item.isEquipped && item.type >= WEAPON_ENERGY && item.type <= WEAPON_TRANSFERENCE) {
                                bool isListed = false;
                                for(Weapon &weap : weapons) if(weap.name == item.name) { isListed = true; break; }
                                if(isListed) continue;
                                ToggleRadar(true);
                                Weapon newWep;
                                newWep.name = item.name;
                                newWep.id = item.id;
                                newWep.id2 = item.id2;
                                newWep.type = item.type;
                                if(GetSelectedWeaponID() != newWep.id) {
                                        double curRange = GetSelectedWeaponRange();
                                        SelectWeapon(newWep.id);
                                        clock_t start = clock();
                                        while(clock() < start+10*CLOCKS_PER_SEC && curRange == GetSelectedWeaponRange()) Sleep(25);
                                }
                                newWep.range = GetSelectedWeaponRange();
                                if(newWep.range == 0) continue;
                                weapons.push_back(newWep);
                        }
                }
        }
        /// --- GETS
        Entity* GetWormholeTo(string toSystem) {
                const string fullName = "Gate to "+toSystem;
                for(Entity &entity : client.entities) {
                        if(entity.type != OBJECT_TYPES::WORMHOLE) continue;
                        if(entity.name == fullName || entity.name == toSystem) return &entity;
                }
                return NULL;
        }
        Entity* GetEntity(const string name) { for(Entity &entity : client.entities) if(entity.name == name) return &entity; return NULL; }
        Entity* GetEntityByPart(const string namePart) { for(Entity &entity : client.entities) if(entity.name.find(namePart) != string::npos) return &entity; return NULL; }
        Entity* GetEntity(const DWORD id) {
                if(id == 0) return NULL;
                for(Entity& entity : client.entities) if(entity.id == id) return &entity;
                return NULL;
        }
        Entity* GetEntityClosestTo(Point to, USHORT targetType) {
                Entity* ret = NULL;
                for(Entity &entity : client.entities) {
                        if(entity.type != targetType) continue;
                        if(ret == NULL) { ret = &entity; continue; }
                        if(Distance(entity.pos.x, entity.pos.y, to.x, to.y) < Distance(ret->pos.x, ret->pos.y, to.x, to.y)) ret = &entity;
                }
                return ret;
        }
        double GetFarthestOutCombatBot() {
                double ret = 0;
                for(Entity &entity : client.entities) if(entity.type == OBJECT_TYPES::SHIP && entity.IsMyBot() && !entity.IsFighter() && !entity.IsMissile()) {
                                if(DistanceTo(&entity) > ret) ret = DistanceTo(&entity);
                        }
                return ret;
        }
        Entity* GetNewTarget() {
                Entity* newTarget = NULL;
                newTarget = GetBestTarget(client.hostiles);
                if(newTarget == NULL) newTarget = GetBestTarget(client.enemies);

                return newTarget;
        }
        Entity* GetBestTarget(vector<Entity*> &from) {
                Entity* newTarget = NULL;
                double lastTargetShieldPercent = 1, thisEntityShield = 1, thisDistance = 0;
                double mostRange = 0;
                for(Weapon &wep : weapons) if(wep.range > mostRange) mostRange = wep.range;
                for(Entity* entity : from) {
                        if(entity->IsPlayer() || entity->IsPlayerBot()) continue;
                        if(!entity->IsVisible()) continue;
                        /*Entity *nearestWormhole = GetEntityClosestTo(entity->pos, OBJECT_TYPES::WORMHOLE);
                        if(nearestWormhole != NULL && Distance(entity->pos.x, entity->pos.y, nearestWormhole->pos.x, nearestWormhole->pos.y) < nearestWormhole->_size*1.1) continue;*/
                        if(newTarget == NULL) { newTarget = entity; lastTargetShieldPercent = GetEntityShieldPercent(newTarget); continue; }
                        thisDistance = DistanceTo(entity);
                        thisEntityShield = GetEntityShieldPercent(entity);
                        if(thisDistance > mostRange) thisEntityShield *=  thisDistance / mostRange;
                        if(thisEntityShield < lastTargetShieldPercent) { newTarget = entity; lastTargetShieldPercent = thisEntityShield; }
                }
                return newTarget;
        }
        Entity* GetNextDGGate() {
                if(thisDungeonLevel == -1) return NULL;
                Entity* ret = nullptr;
                for(Entity &entity : client.entities) {
                        if(entity.type != OBJECT_TYPES::WORMHOLE) continue;
                        int thisGateLvl = GetDungeonLevel(entity.name);
                        if(thisGateLvl == -1) continue;
                        if(thisGateLvl < thisDungeonLevel) {
                                if(ret == nullptr) { ret = &entity; continue; }
                                DUNGEONLEVEL* dglvl = _DUNGEONS::FindDungeonLevel(UNIVERSE::GetSystemFromWormhole(entity.name), thisDungeon);
                                if(GetDungeonLevel(ret->name) >= thisGateLvl && (dglvl == nullptr || !dglvl->completed)) ret = &entity;
                        }
                }
                return ret;
        }
        Entity* GetPreviousDGGate() {
                if(thisDungeonLevel == -1) return NULL;
                for(Entity &entity : client.entities) {
                        if(entity.type != OBJECT_TYPES::WORMHOLE) continue;
                        if(GetDungeonLevel(entity.name) > thisDungeonLevel) return &entity;
                }
                return NULL;
        }
        template<typename T = Entity*, template<typename...> class Container>
        Entity* GetNearestWithinDistance(Container<Entity*> &container, double maxDistance = 99999, double minDistance = 0, vector<IgnoredDebris> *ignoreds = nullptr) {
                Entity* ret = nullptr;
                double lastDistance = maxDistance, thisDistance;
                for(Entity* entity : container) {
                        if(ignoreds != nullptr && std::find_if(ignoreds->begin(), ignoreds->end(),
                                                               [&entity, this](const IgnoredDebris &ignored){ return entity->id == ignored.id && ignored.where == player.location; }) != ignoreds->end()) continue;
                        thisDistance = DistanceTo(entity);
                        if(thisDistance < minDistance) continue;
                        if(thisDistance < lastDistance) { ret = entity; lastDistance = thisDistance; }
                }
                return ret;
        }
        template<typename T = Entity*, template<typename...> class Container>
        Entity* GetFarthestWithinDistance(Container<Entity*> &container, double maxDistance = 99999, double minDistance = 0, vector<IgnoredDebris> *ignoreds = nullptr) {
                Entity* ret = nullptr;
                double lastDistance = minDistance, thisDistance;
                for(Entity* entity : container) {
                       if(ignoreds != nullptr && std::find_if(ignoreds->begin(), ignoreds->end(),
                                                               [&entity, this](const IgnoredDebris &ignored){ return entity->id == ignored.id && ignored.where == player.location; }) != ignoreds->end()) continue;
                        thisDistance = DistanceTo(entity);
                        if(thisDistance < minDistance) continue;
                        if(thisDistance > lastDistance) { ret = entity; lastDistance = thisDistance; }
                }
                return ret;
        }
        void GetDebrisList() {
                mtx_debris.lock();
                debris.clear();
                credits.clear();
                for(Entity &entity : client.entities) if(entity.type == OBJECT_TYPES::AUG || entity.type == OBJECT_TYPES::DEBRIS || entity.type == OBJECT_TYPES::CREDITS) {
                                bool isIgnored = false;
                                for(IgnoredDebris &ignored : ignoredDebris) if(ignored.id == entity.id) { isIgnored = true; break; }
                                if(DistanceTo(&entity) > settings.maxLootDistance) continue;
                                if(!isIgnored) {
                                        if(entity.type == OBJECT_TYPES::CREDITS) credits.push_back(&entity);
                                        else {
                                                bool skipThis = false;
                                                for(Entity *_player : client.players) if(DistanceBetween(&entity, _player) < 50) { ignoredDebris.push_back(IgnoredDebris(entity.id, player.location)); skipThis = true; break; }
                                                if(skipThis) continue;
                                                debris.push_back(&entity);
                                        }
                                }
                        }
                mtx_debris.unlock();
        }
        bool IsShooting() {
                bool buff, buff2;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.IsShooting), &buff, 1, NULL);
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.Keyboard+OFFSETS::Keyboard_Shoot), &buff2, 1, NULL);
                return (buff || buff2);
        }
        bool IsCloakEnabled() {
                double buff;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.VisibilityModifier), &buff, 8, NULL);
                return buff != 0;
        }
        UINT GetSpeed() {
                UINT buffer;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.PlayerSpeed), &buffer, 4, NULL);
                return buffer;
        }
        bool IsAnchorOn() {
                bool buff;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.StopShip), &buff, sizeof(buff), NULL);
                return buff;
        }
        Point GetAutopilotTarget() {
                int buff;
                Point ret;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.AutopilotTargetPosX), &buff, 4, NULL);
                ret.x = buff;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.AutopilotTargetPosY), &buff, 4, NULL);
                ret.y = buff;
                return ret;
        }
        int GetAutopilotJumps() {
                int ret;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.AutopilotJumps), &ret, 4, NULL);
                return ret;
        }
        byte GetAutopilotMode() {
                byte buff;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.AutopilotState), &buff, 1, NULL);
                return buff;
        }
        int GetDungeonLevel(string &galaxyName) {
                string numbers = "";
                size_t pos = galaxyName.rfind(" "), pos2 = galaxyName.rfind(".");
                if(pos == string::npos || pos2 == string::npos || pos2 < pos) return -1;
                return atoi(galaxyName.substr(pos+1, pos2-(pos+1)).c_str());
        }
        UINT GetSystemID() {
                UINT buff;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.PlayerLocation-0x4), &buff, 4, NULL);
                return buff;
        }
        public: double GetDangerFactor() {
                if(isInDungeon) return 99;
                double ret;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.DangerFactor), &ret, 8, NULL);
                return ret;
        }
        private: bool IsChatOpen() {
                bool buff;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.IsChatOpen), &buff, 1, NULL);
                return buff;
        }
        bool IsTargetOptionsOpen() {
                if(!client.memory.Calculate2("is_target_options_open", client.memory.IsTargetOptionsOpen)) return false;
                bool buff;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.IsTargetOptionsOpen), &buff, 1, NULL);
                return buff;
        }
        int GetRemainingAutopilotJumps() {
                int ret;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.AutopilotJumps), &ret, 4, NULL);
                return ret;
        }
        string GetAutopilotDestination() {
                wstring buff = client.memory.ReadWString(client.memory.AutopilotDestination);
                string ret = String::ws2s(buff);
                size_t startpos = ret.find("Travelling to"), endpos = ret.find(".");
                if(startpos == string::npos || endpos == string::npos) return "";
                return ret.substr(startpos+14, endpos-startpos-14);
        }
        string GetAutopilotStep() {
                wstring buff = client.memory.ReadWString(client.memory.AutopilotDestination);
                string ret = String::ws2s(buff);
                size_t startpos = ret.find_last_of('['), endpos = ret.find_last_of(']');
                if(startpos == string::npos || endpos == string::npos) return "";
                return ret.substr(startpos+1, endpos-startpos-1);
        }
        string GetPlayerNameFromBot(string &botName) {
                size_t pos1 = botName.find_first_of("(")+1, pos2 = botName.find_last_of(")");
                if(pos1 == string::npos || pos2 == string::npos) return botName;
                return botName.substr(pos1, pos2-pos1);
        }
        bool AreCombatBotsOK() {
                for(CombatBot &combatbot : combatBots) {
                        Entity* thisBot = GetEntity(combatbot.id);
                        if(thisBot == NULL) return false;
                        if(DistanceTo(thisBot) > 10000) continue;
                        if(GetEntityShieldPercent(thisBot) < 0.75) return false;
                }
                return true;
        }
        double GetEntityShieldPercent(Entity* entity) {
                if(entity->shieldMax == 0) {
                        SelectTarget(entity);
                        Sleep(1);
                        if(entity->shieldMax == 0) return 1;
                }
                return entity->shield / entity->shieldMax;
        }
        inline size_t GetCurrentPathStep() {
                for(size_t i = 0; i <currentPath.size(); i++) if(currentPath[i] == player.location) return i;
                return 9999;
        }
        int CountNormalEnemies() {
                int ret = 0;
                for(Entity* entity : client.enemies) if(!entity->IsFighter() && !entity->IsMissile()) ret++;
                return ret;
        }
        DWORD GetSelectedWeaponID() {
                DWORD ret = 0;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.SelectedWeaponID), &ret, 4, NULL);
                return ret;
        }
        inline void SelectWeapon(DWORD id) { WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.SelectedWeaponID), &id, 4, NULL); }
        double GetSelectedWeaponRange() {
                float buff = 0;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.SelectedWeaponRange), &buff, 4, NULL);
                return (double)buff;
        }
        UINT GetLockoutTime(string &from) {
                time_t ret = 0;
                size_t pos1, pos2;
                int amountBuffer = 0;
                string buffer;
                pos1 = from.find("day");
                if(pos1 != string::npos) {
                        pos2 = from.find_last_of(" ", pos1-2);
                        if(pos2 == string::npos) pos2 = 0;
                        buffer = from.substr(pos2, pos1-pos2);
                        String::trim(buffer);
                        amountBuffer = atoi(buffer.c_str());
                        ret += amountBuffer*24*60*60;
                }
                pos1 = from.find("hour");
                if(pos1 != string::npos) {
                        pos2 = from.find_last_of(" ", pos1-2);
                        if(pos2 == string::npos) pos2 = 0;
                        buffer = from.substr(pos2, pos1-pos2);
                        String::trim(buffer);
                        amountBuffer = atoi(buffer.c_str());
                        ret += amountBuffer*60*60;
                }
                pos1 = from.find("minute");
                if(pos1 != string::npos) {
                        pos2 = from.find_last_of(" ", pos1-2);
                        if(pos2 == string::npos) pos2 = 0;
                        buffer = from.substr(pos2, pos1-pos2);
                        String::trim(buffer);
                        amountBuffer = atoi(buffer.c_str());
                        ret += amountBuffer*60;
                }
                pos1 = from.find("second");
                if(pos1 != string::npos) {
                        pos2 = from.find_last_of(" ", pos1-2);
                        if(pos2 == string::npos) pos2 = 0;
                        buffer = from.substr(pos2, pos1-pos2);
                        String::trim(buffer);
                        amountBuffer = atoi(buffer.c_str());
                        ret += amountBuffer;
                }
                return ret;
        }
        long long GetCredits() {
                long long buff;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.Credits), &buff, 8, NULL);
                return buff;
        }
        bool AreFollowersHere(){
                if(followers.size() == 0) return true;
                bool ret = true;
                for(string &alt : followers){
                        bool isHere = false;
                        for(Entity &entity : client.entities) if(entity.name == alt && entity.stasisType == 0 && entity.shipname.find("Pod") == string::npos && entity.shipname != "Spirit"){ isHere = true; break; }
                        if(!isHere){
                                ret = false;
                                break;
                        }
                }
                return ret;
        }
        bool AreFollowersWithinDistance(double dist){
                if(followers.size() == 0) return true;
                bool ret = true;
                for(string &alt : followers){
                        bool ok = false;
                        for(Entity &entity : client.entities) if(entity.name == alt && DistanceTo(&entity) <= dist  && entity.stasisType == 0 && entity.shipname.find("Pod") == string::npos && entity.shipname != "Spirit"){ ok = true; break; }
                        if(!ok){
                                ret = false;
                                break;
                        }
                }
                return ret;
        }
        /// --- SETS
        void SelectTarget(DWORD id) { WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.SelectedTargetID), &id, 4, NULL); }
        void SelectTarget(Entity* entity) {
                if(entity == NULL) return;
                else WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.SelectedTargetID), &entity->id, 4, NULL);
        }
        void AutopilotOff() {
                bool buff = false;
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.AutopilotState), &buff, 1, NULL);
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.AutopilotOn), &buff, 1, NULL);
                return;
        }
        inline void ToggleAnchor(bool buff) {
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.Keyboard+OFFSETS::Keyboard_S), &buff, 1, NULL);
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.StopShip), &buff, 1, NULL);
        }
        void Unthrust() {
                bool buff = false;
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.Keyboard+OFFSETS::Keyboard_W), &buff, 1, NULL);
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.ThrustState), &buff, 1, NULL);
        }
        void ToggleTargetOptions() {
                byte buff = 1;
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.Keyboard+OFFSETS::Keyboard_X), &buff, 1, NULL);
                Sleep(50);
                buff = 0;
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.Keyboard+OFFSETS::Keyboard_X), &buff, 1, NULL);
        }
        void ToggleCloak() {
                byte buff = 1;
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.Keyboard+OFFSETS::Keyboard_K), &buff, 1, NULL);
                Sleep(50);
                buff = 0;
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.Keyboard+OFFSETS::Keyboard_K), &buff, 1, NULL);
        }
        inline void ToggleInventory(bool state) {
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.InventoryWindow+0x98), &state, 1, NULL);
        }
        bool IsInventoryOpen() {
                bool buff;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.InventoryWindow+0x98), &buff, 1, NULL);
                return buff;
        }
        void ToggleTractor() {
                bool buff = 1;
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.Keyboard+OFFSETS::Keyboard_G), &buff, 1, NULL);
                Sleep(500);
                buff = 0;
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.Keyboard+OFFSETS::Keyboard_G), &buff, 1, NULL);
        }
        void Shoot() {
                if(!ASYNC_SHOOT.valid() || IsThreadFinished(ASYNC_SHOOT)) {
                        ASYNC_SHOOT = std::async(std::launch::async, [](BOT* bot) {
                                bool buff = true;
                                WriteProcessMemory(bot->client.memory.processHandle, LongToPtr(bot->client.memory.Keyboard+OFFSETS::Keyboard_Shoot), &buff, 1, NULL);
                                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                buff = false;
                                WriteProcessMemory(bot->client.memory.processHandle, LongToPtr(bot->client.memory.Keyboard+OFFSETS::Keyboard_Shoot), &buff, 1, NULL);
                                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        }, this);
                }

        }
        inline void StopShooting() {
                bool buff = false;
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.Keyboard+OFFSETS::Keyboard_Shoot), &buff, 1, NULL);
        }
        void Dock() {
                StopShooting();
                bool buff = true;
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.Keyboard+OFFSETS::Keyboard_R), &buff, 1, NULL);
                Sleep(50);
                buff = false;
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.Keyboard+OFFSETS::Keyboard_R), &buff, 1, NULL);
        }
        bool Undock() {
                DWORD add = 0;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.StationWindow+0xC), &add, 4, NULL);
                ReadProcessMemory(client.memory.processHandle, LongToPtr(add+0x14), &add, 4, NULL);
                GUIElement undockButton(&client.memory);
                undockButton.Load(add);
                Point clickPoint = undockButton.position+15;
                clock_t start = clock();
                do {
                        Click_Left(clickPoint);
                        Sleep(500);
                        ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.IsDocked), &client.isDocked, 1, NULL);
                } while(client.isDocked && clock() < start+5*CLOCKS_PER_SEC);
                if(!client.isDocked) return true;
                return false;
        }
        inline void ToggleRadar(bool buff) {
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.RadarOn), &buff, 1, NULL);
        }
        /// --- ACTIONS
        void ClickEventTab() {
                int x,y;
                DWORD eventTab = client.chat.GetTabByName("Event");
                if(client.chat.GetSelectedTab() == eventTab) return;
                client.chat.SelectTab("Team");
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.ClientResolutionX), &x, 4, NULL);
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.ClientResolutionX+0x4), &y, 4, NULL);
                Point clickPos = Point(50, y+10);
                clock_t start = clock();
                do {
                        Click_Left(clickPos);
                        Sleep(1000);
                } while(clock() < start+10*CLOCKS_PER_SEC && client.chat.GetSelectedTab() != eventTab);
                if(IsChatOpen()) {
                        ToggleChatInput();
                        bool buff = false;
                        WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.IsChatOpen), &buff, 1, NULL);
                        WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.ChatToggle), &buff, 1, NULL);
                }
        }
        void TurnAt(double rotation) {
                /* TODO
                WriteProcessMemory(client.memory.processHandle, LongToPtr(player.ship.structurePtr+OFFSETS::EntityRotation2), &rotation, 8, NULL);
                clock_t beginTime = clock();
                double curAngle;
                ReadProcessMemory(hproc, LongToPtr(playerData+OffsetList::turnangle), &curAngle, 8, NULL);
                Point _target = GetPointAt(Point(player.posx, player.posy), angle, 100);
                while(clock() < beginTime+2 && curAngle != angle) {
                        AutopilotTo(_target.x, _target.y);
                        ReadProcessMemory(hproc, LongToPtr(playerData+OffsetList::turnangle), &curAngle, 8, NULL);
                        Sleep(10);
                }
                ToggleAnchor(false);
                Sleep(50);
                ToggleAnchor(true);
                Sleep(50);
                ToggleAnchor(false);*/
        }
        void TurnAt(Entity* entity) {
                if(entity == NULL) return;
                TurnAt(GetAngleTo(entity->pos.x, entity->pos.y));
        }
        void AutopilotTo(double px, double py) {
                /*if(shieldPercent < 0.8) {
                        Entity* star = GetEntityClosestTo(Point(px, py), OBJECT_TYPES::STAR);
                        if(star != NULL) {
                                double angleToStar = GetAngleTo(player.ship.pos, star->pos);
                                double angleToTarget = GetAngleTo(px, py);
                                double tolerance = 0.08726388888889;
                                double _min = angleToStar-tolerance, _max = angleToStar+tolerance;
                                printf("angle to star: %f\n", angleToStar);
                                printf("angle to target: %f\n", angleToTarget);
                                printf("distance to star: %.0f\n", DistanceTo(star));
                                printf("distance to target: %.0f\n", DistanceTo(px, py));
                                //
                                double m = ((star->pos.y) - (py)) / ((star->pos.x) - (px));
                                double b = star->pos.y - ((m)*(star->pos.x));
                                double t = ((py) - (player.ship.pos.y)) / ((px) - (player.ship.pos.x));
                                double c = (player.ship.pos.y) - ((t)*(player.ship.pos.x));
                                double x = (c - b) / (m - t);
                                double y = t*x + c;
                                //
                                //if(DistanceTo(star) < DistanceTo(px, py) || (DistanceTo(star) > DistanceTo(px, py) && !IsAngleBetween(_min, _max, angleToStar))){
                                if(DistanceTo(x, y) > 50){;
                                        if(star->pos.x > player.ship.pos.x && star->pos.y > player.ship.pos.y && star->pos.y > py && player.ship.pos.x < px){
                                                py = star->pos.y - 7*star->_size/10;
                                                px = star->pos.x + 20;
                			}else if(star->pos.x > player.ship.pos.x && star->pos.y > player.ship.pos.y && star->pos.y > py && player.ship.pos.x > px){
                                                py = star->pos.y + 20;
                                                px = star->pos.x - 7*star->_size/10;
                                        }else if(star->pos.x > player.ship.pos.x && star->pos.y > player.ship.pos.y && star->pos.y < py && star->pos.x > px){
                                                py = star->pos.y + 20;
                                                px = star->pos.x - 7*star->_size/10;
                			}else if(star->pos.x > player.ship.pos.x && star->pos.y > player.ship.pos.y && star->pos.y < py && star->pos.x < px){
                                                py = star->pos.y - 7*star->_size/10;
                                                px = star->pos.x + 20;
                                        }else if(star->pos.x < player.ship.pos.x && star->pos.y > player.ship.pos.y && star->pos.y < py && star->pos.x > px){
                                                py = star->pos.y - 7*star->_size/10;
                                                px = star->pos.x - 20;
                			}else if(star->pos.x < player.ship.pos.x && star->pos.y > player.ship.pos.y && star->pos.y < py && star->pos.x < px){
                                                py = star->pos.y + 20;
                                                px = star->pos.x + 7*star->_size/10;
                                        }else if(star->pos.x < player.ship.pos.x && star->pos.y > player.ship.pos.y && star->pos.y > py && player.ship.pos.x < px){
                                                py = star->pos.y + 20;
                                                px = star->pos.x + 7*star->_size/10;
                			}else if(star->pos.x < player.ship.pos.x && star->pos.y > player.ship.pos.y && star->pos.y > py && player.ship.pos.x > px){
                                                py = star->pos.y - 7*star->_size/10;
                                                px = star->pos.x - 20;
                                        }else if(star->pos.x < player.ship.pos.x && star->pos.y < player.ship.pos.y && star->pos.y < py && player.ship.pos.x < px){
                                                py = star->pos.y - 20;
                                                px = star->pos.x + 7*star->_size/10;
                			}else if(star->pos.x < player.ship.pos.x && star->pos.y < player.ship.pos.y && star->pos.y < py && player.ship.pos.x > px){
                                                py = star->pos.y + 7*star->_size/10;
                                                px = star->pos.x - 20;
                			}else if(star->pos.x < player.ship.pos.x && star->pos.y < player.ship.pos.y && star->pos.y > py && star->pos.x > px){
                                                py = star->pos.y + 7*star->_size/10;
                                                px = star->pos.x - 20;
                			}else if(star->pos.x < player.ship.pos.x && star->pos.y < player.ship.pos.y && star->pos.y > py && star->pos.x < px){
                                                py = star->pos.y - 20;
                                                px = star->pos.x + 7*star->_size/10;
                                        }else if(star->pos.x > player.ship.pos.x && star->pos.y < player.ship.pos.y && star->pos.y > py && star->pos.x > px){
                                                py = star->pos.y - 20;
                                                px = star->pos.x - 7*star->_size/10;
                			}else if(star->pos.x > player.ship.pos.x && star->pos.y < player.ship.pos.y && star->pos.y > py && star->pos.x < px){
                                                py = star->pos.y + 7*star->_size/10;
                                                px = star->pos.x + 20;
                                        }else if(star->pos.x > player.ship.pos.x && star->pos.y < player.ship.pos.y && star->pos.y < py && player.ship.pos.x > px){
                                                py = star->pos.y - 20;
                                                px = star->pos.x - 7*star->_size/10;
                			}else if(star->pos.x > player.ship.pos.x && star->pos.y < player.ship.pos.y && star->pos.y < py && player.ship.pos.x < px){
                                                py = star->pos.y + 7*star->_size/10;
                                                px = star->pos.x + 20;
                			}
                                }
                        }
                //}*/
                int x = (int)px, y = (int)py;
                autopilotTarget.x = px;
                autopilotTarget.y = py;
                bool buff = false;
                Unthrust(); /// it was bugging sometimes so i made this function
                ToggleAnchor(false);
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.AutopilotTargetPosX), &x, 4, NULL);
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.AutopilotTargetPosY), &y, 4, NULL);
                buff = true;
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.AutopilotState), &buff, 1, NULL);
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.AutopilotOn), &buff, 1, NULL);
                /*if(!ASYNC_AUTOPILOT.valid() || IsThreadFinished(ASYNC_AUTOPILOT)) {
                        ASYNC_AUTOPILOT = std::async(std::launch::async, [](BOT* bot, Point initMovement) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                ///bot->client.mtx_playerData.lock();
                                if(bot->player.ship.moveVector == initMovement){
                                        bot->ToggleAnchor(true);
                                        std::this_thread::sleep_for(std::chrono::milliseconds(25));
                                        bot->ToggleAnchor(false);
                                }
                                ///bot->client.mtx_playerData.unlock();
                        }, this, moveVector);
                }*/
        }
        inline void AutopilotTo(Entity* entity) { if(entity == NULL) return; else AutopilotTo(entity->pos.x, entity->pos.y); }

        /** \brief returns false on fail, true if still travelling */
        bool MoveTo(string &destination, short attempts = 0) {
                if(destination == player.location) return false;
                if(currentPath.size() > 0 && currentPath.back() == destination) {
                        return TraversePath();
                }
                if(client.chat.FindMessage("Unable to find", "Event") != NULL || client.chat.FindMessage("No apparent route", "Event") != NULL) {
                        if(currentPath.size() == 0 || currentPath.back() != destination) currentPath = UNIVERSE::FindPath(player.location, destination, character.warpNavigation >= 4);
                        if(currentPath.size() == 0) return false;
                        return TraversePath();
                }
                //
                if(GetAutopilotMode() >= 3 && !account.isP2P && GetAutopilotStep() == "Earthforce Perilous Outpost") {
                        currentPath = UNIVERSE::FindPath(player.location, GetAutopilotDestination(), character.warpNavigation == 4);
                        return false;
                }
                //
                Entity *targetWormhole = GetWormholeTo(destination);
                if(targetWormhole != nullptr) {
                        WarpThrough(targetWormhole);
                        return true;
                }
                string apDestination = GetAutopilotDestination();
                if((clock() > systemEnterTime+1000 && GetAutopilotMode() == 0) || (apDestination != destination && apDestination != "Unknown Galaxy")) {
                        Unthrust();
                        ToggleAnchor(false);
                        SendChatMessage("Event", "/ap \""+destination+"\"");
                }
                return true;
        }
        bool TraversePath() {
                if(CountObjects(OBJECT_TYPES::WORMHOLE) == 0) return true;
                int currentIndex = -1;
                for(size_t i = 0; i < currentPath.size(); i++) if(currentPath[i] == player.location) { currentIndex = (int)i; break; }
                if((size_t)currentIndex == currentPath.size()-1 || currentPath.size() <= 1) return false;
                if(currentIndex == -1) { // got off path, recalculate
                        currentPath = UNIVERSE::FindPath(player.location, currentPath.back(), character.warpNavigation);
                        if(currentPath.size() <= 1) return false;
                        currentIndex = 0;
                }
                targetWormhole = GetWormholeTo(currentPath[currentIndex+1]);
                if(targetWormhole == NULL) return false;
                WarpThrough(targetWormhole);
                return true;
        }
        int JumpsLeft() {
                if(GetAutopilotMode() >= 3) return GetAutopilotJumps();
                if(currentPath.size() == 0) return 0;
                int currentIndex = 0;
                for(size_t i = 0; i < currentPath.size(); i++) if(currentPath[i] == player.location) { currentIndex = (int)i; break; }
                return currentPath.size() - currentIndex;
        }
        void WarpThrough(Entity* wormhole) {
                if(DistanceTo(wormhole) > player.ship._size*0.5-5 && DistanceTo(wormhole) > wormhole->_size-5) {
                        AutopilotTo(wormhole);
                        Sleep(10);
                        if(GetSpeed() <= 5) { Thrust(500); Sleep(500); }
                } else {
                        if(GetSpeed() <= 5) {
                                ToggleAnchor(false);
                                Thrust(500);
                                Sleep(500);
                        }
                        SelectTarget(wormhole);
                        Warp();
                }
        }
        void Warp() {
                if(!ASYNC_WARP.valid() || IsThreadFinished(ASYNC_WARP)) {
                        ASYNC_WARP = std::async(std::launch::async, [](BOT* bot) {
                                bot->ToggleAnchor(true);
                                bool buff = true;
                                bot->client.mtx_playerData->lock();
                                string startAt = bot->player.location;
                                bot->client.mtx_playerData->unlock();
                                WriteProcessMemory(bot->client.memory.processHandle, LongToPtr(bot->client.memory.Keyboard+OFFSETS::Keyboard_F), &buff, 1, NULL);
                                clock_t startTime = clock();
                                do {
                                        Sleep(50);
                                        bot->client.mtx_playerData->lock();
                                        if(bot->player.location != startAt) {
                                                bot->client.mtx_playerData->unlock();
                                                break;
                                        }
                                        bot->client.mtx_playerData->unlock();
                                } while(clock() < startTime+500);
                                buff = false;
                                WriteProcessMemory(bot->client.memory.processHandle, LongToPtr(bot->client.memory.Keyboard+OFFSETS::Keyboard_F), &buff, 1, NULL);
                                bot->ToggleAnchor(false);
                                Sleep(500);
                        }, this);
                }
        }
        void Thrust(UINT time = 100) {
                if(!ASYNC_THRUST.valid() || IsThreadFinished(ASYNC_THRUST)) {
                        ASYNC_THRUST = std::async(std::launch::async, [](BOT* bot, UINT time) {
                                bool buff = true;
                                WriteProcessMemory(bot->client.memory.processHandle, LongToPtr(bot->client.memory.Keyboard+OFFSETS::Keyboard_W), &buff, 1, NULL);
                                Sleep(time);
                                buff = false;
                                WriteProcessMemory(bot->client.memory.processHandle, LongToPtr(bot->client.memory.Keyboard+OFFSETS::Keyboard_W), &buff, 1, NULL);
                        }, this, time);
                }
        }
        void Scoop() {
                if(!ASYNC_SCOOP.valid() || IsThreadFinished(ASYNC_SCOOP)) {
                        ASYNC_SCOOP = std::async(std::launch::async, [](BOT* bot) {
                                bool buff;
                                ReadProcessMemory(bot->client.memory.processHandle, LongToPtr(bot->client.memory.Keyboard+OFFSETS::Keyboard_C), &buff, 1, NULL);
                                if(buff){
                                        buff = false;
                                        WriteProcessMemory(bot->client.memory.processHandle, LongToPtr(bot->client.memory.Keyboard+OFFSETS::Keyboard_C), &buff, 1, NULL);
                                        Sleep(1000);
                                }
                                buff = true;
                                WriteProcessMemory(bot->client.memory.processHandle, LongToPtr(bot->client.memory.Keyboard+OFFSETS::Keyboard_C), &buff, 1, NULL);
                                clock_t start = clock();
                                while(clock() < start + 5*CLOCKS_PER_SEC) {
                                        bot->mtx_debris.lock();
                                        if(bot->debris.size() == 0) { bot->mtx_debris.unlock(); break; }
                                        bot->mtx_debris.unlock();
                                        Sleep(25);
                                }
                                buff = false;
                                WriteProcessMemory(bot->client.memory.processHandle, LongToPtr(bot->client.memory.Keyboard+OFFSETS::Keyboard_C), &buff, 1, NULL);
                                Sleep(1000);
                        }, this);
                }
        }
        void ScoopFast() {
                bool buff = false, wasEnabled;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.Keyboard+OFFSETS::Keyboard_C), &wasEnabled, 1, NULL);
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.Keyboard+OFFSETS::Keyboard_C), &buff, 1, NULL);
                Sleep(100);
                buff = true;
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.Keyboard+OFFSETS::Keyboard_C), &buff, 1, NULL);
                Sleep(100);
                if(!wasEnabled){
                        buff = false;
                        WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.Keyboard+OFFSETS::Keyboard_C), &buff, 1, NULL);
                }
        }
        inline void StopScooping() {
                bool buff = false;
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.Keyboard+OFFSETS::Keyboard_C), &buff, 1, NULL);
        }
        void ToggleChatInput() {
                if(!ASYNC_TOGGLECHATINPUT.valid() || IsThreadFinished(ASYNC_TOGGLECHATINPUT)) {
                        ASYNC_TOGGLECHATINPUT = std::async(std::launch::async, [](BOT* bot) {
                                SendMessage(bot->client.clientWindow, WM_KEYDOWN, 0x0D, 0);
                                Sleep(1000);
                                SendMessage(bot->client.clientWindow, WM_KEYUP, 0x0D, 0);
                        }, this);
                }
        }
public: bool SendChatMessage(const string targetTab, string msg) {
                if(!client.isCharSelected) return false;
                size_t maxMsgLength;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.ChatInput+0x4), &maxMsgLength, 4, NULL);
                clock_t start = clock();
                while(!IsChatOpen() && clock() < start + 30*CLOCKS_PER_SEC) { ToggleChatInput(); Sleep(250); }
                if(!client.chat.SelectTab(targetTab)) {
                        ToggleChatInput();
                        return false;
                }
                size_t msgLength = msg.length();
                if(msgLength > 249) {
                        msg = msg.substr(0, 249);
                        msgLength = 249;
                }
                if(msgLength > maxMsgLength) {
                        while(clock() < start+60*CLOCKS_PER_SEC && maxMsgLength < msgLength+2) {
                                SendMessage(client.clientWindow, WM_KEYDOWN, 0x20, 0);
                                Sleep(50);
                                SendMessage(client.clientWindow, WM_KEYUP, 0x20, 0);
                                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.ChatInput+0x4), &maxMsgLength, 4, NULL);
                        }
                }
                if(msgLength > maxMsgLength) return false;
                wstring wsMsg = String::s2ws(msg);
                DWORD msgTextAddress;
                ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.ChatInput), &msgTextAddress, 4, NULL);
                WriteProcessMemory(client.memory.processHandle, LongToPtr(msgTextAddress), wsMsg.c_str(), wsMsg.length()*2+2, NULL);
                wchar_t inputBuffer[1000];

                do {
                        SetActiveWindow(client.clientWindow);
                        ToggleChatInput();
                        ReadProcessMemory(client.memory.processHandle, LongToPtr(msgTextAddress), &inputBuffer, 1000, NULL);
                        Sleep(100);
                } while(IsChatOpen() && clock() < start + 60*CLOCKS_PER_SEC && wcslen(inputBuffer) > 0);
                bool buff = false;
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.IsChatOpen), &buff, 1, NULL);
                WriteProcessMemory(client.memory.processHandle, LongToPtr(client.memory.ChatToggle), &buff, 1, NULL);
                return true;
        }
private:
        void MouseMove(Point pos, int sleepTime = 1000) {
                if(!ASYNC_MOUSEMOVE.valid() || IsThreadFinished(ASYNC_MOUSEMOVE)) {
                        ASYNC_MOUSEMOVE = std::async(std::launch::async, [](BOT* bot, Point pos, int sleepTime) {
                                LPARAM lParam = MAKELPARAM((int)pos.x, (int)pos.y);
                                SendMessage(bot->client.clientWindow, WM_MOUSEMOVE, 1, lParam);
                                Sleep(sleepTime);
                                SendMessage(bot->client.clientWindow, WM_MOUSEMOVE, 0, lParam);
                        }, this, pos, sleepTime);
                }
        }
        void Click_Left(Point pos, int sleepTime = 1000) {
                if(!ASYNC_CLICKLEFT.valid() || IsThreadFinished(ASYNC_CLICKLEFT)) {
                        ASYNC_CLICKLEFT = std::async(std::launch::async, [](BOT* bot, Point pos, int sleepTime) {
                                LPARAM lParam = MAKELPARAM((int)pos.x, (int)pos.y);
                                SendMessage(bot->client.clientWindow, WM_LBUTTONDOWN, 1, lParam);
                                Sleep(sleepTime);
                                PostMessage(bot->client.clientWindow, WM_LBUTTONUP, 0, lParam);
                        }, this, pos, sleepTime);
                }
        }
        void Click_Right(Point pos, int sleepTime = 1000) {
                if(!ASYNC_CLICKRIGHT.valid() || IsThreadFinished(ASYNC_CLICKRIGHT)) {
                        ASYNC_CLICKRIGHT = std::async(std::launch::async, [](BOT* bot, Point pos, int sleepTime) {
                                LPARAM lParam = MAKELPARAM((int)pos.x, (int)pos.y);
                                SendMessage(bot->client.clientWindow, WM_RBUTTONDOWN, 1, lParam);
                                Sleep(sleepTime);
                                //SendMessage(bot->client.clientWindow, WM_RBUTTONUP, 1, lParam);
                        }, this, pos, sleepTime);
                }
        }
        void WaitForCombatBots() {
                isRetrievingCombatBot = false;
                for(CombatBot &combatbot : combatBots) {
                        if((combatbot.isLost && clock() > combatbot.lastSeenTime+5*60*CLOCKS_PER_SEC) || combatbot.isDead) {
                                if(player.location == combatbot.lastSeenAt) {
                                        UpdateCombatBots();
                                        if(GetEntity(combatbot.id) == NULL){
                                                isRetrievingCombatBot = true;
                                                AI_REGEN();
                                                if(clock() > errorReportTime1 + 60*CLOCKS_PER_SEC) {
                                                        printf(CC_RED, "Bot %s has lost it's combat slave, need assistance\n", player.name.c_str());
                                                        errorReportTime1 = clock();
                                                }
                                        }
                                } else if(player.location != combatbot.lastSeenAt && !UNIVERSE::IsDungeon(combatbot.lastSeenAt)) {
                                        isRetrievingCombatBot = true;
                                        MoveTo(combatbot.lastSeenAt);
                                }
                        }
                        if(combatbot.isDead && GetEntity(combatbot.id) != NULL) {
                                SelectTarget(combatbot.id);
                                clock_t _start = clock();
                                while(clock() < _start+5*CLOCKS_PER_SEC && !IsTargetOptionsOpen()) { ToggleTargetOptions(); Sleep(50); }
                                Sleep(500);
                                if(!client.memory.Calculate2("target_options", client.memory.TargetOptions)) return;
                                GUIElement guielement = GUIElement::FindInList(client.memory.TargetOptions, "Leave Stasis", &client.memory);
                                if(guielement.ok){
                                        Click_Left(guielement.position+5);
                                        Sleep(500);
                                        UpdateCombatBots();
                                }
                        }
                }
        }
        void ToggleGUIValue(GUIElement &target, bool val) {
                if(!target.ok) return;
                clock_t start1, start2;
                start1 = clock();
                while(target.isEnabled != val && clock() < start1+15*CLOCKS_PER_SEC) {
                        Click_Left(target.position+5);
                        Sleep(100);
                        start2 = clock();
                        while(target.isEnabled != val && clock() < start2+1*CLOCKS_PER_SEC) { target.Load(target.address); Sleep(20); }
                }
        }
        void LeaveDG() {
                Entity* targetWormhole = GetWormholeTo(UNIVERSE::GetSystemFromDG(player.location));
                if(targetWormhole != NULL) WarpThrough(targetWormhole);
                else {
                        targetWormhole = GetPreviousDGGate();
                        if(targetWormhole != NULL) WarpThrough(targetWormhole);
                        else if(clock() > systemEnterTime+1000) {
                                THIS_LOOP_ACTIONS.push_back("CANT_FIND_DG_EXIT");
                                /*if(clock() > errorReportTime1 + 30*CLOCKS_PER_SEC || errorReportTime1 == 0) {
                                        printf(CC_RED, "%s is unable to exit dg\n", player.name.c_str());
                                        errorReportTime1 = clock();
                                }*/
                        }
                }
        }
        /// --- MATHS
        double Distance(double dX0, double dY0, double dX1, double dY1) { return sqrt((dX1 - dX0)*(dX1 - dX0) + (dY1 - dY0)*(dY1 - dY0)); }
        double DistanceTo(double x, double y) { return Distance(player.ship.pos.x, player.ship.pos.y, x, y); }
        double DistanceTo(Point& point) { return Distance(player.ship.pos.x, player.ship.pos.y, point.x, point.y); }
        double DistanceTo(Entity* entity) {
                if(entity == NULL) return 0;
                return Distance(player.ship.pos.x, player.ship.pos.y, entity->pos.x, entity->pos.y);
        }
        double DistanceBetween(Entity* e1, Entity* e2) { return Distance(e1->pos.x, e1->pos.y, e2->pos.x, e2->pos.y); }
        double DistanceBetween(Point p1, Point p2) { return Distance(p1.x, p1.y, p2.x, p2.y); }
        Point MovePoint(Point start, double angle, double distance) {
                double startY = start.y;
                start.y += distance*sin(2*PI-angle),
                           start.x += sqrt(pow(distance, 2)-pow(startY  - start.y,2));
                return start;
        }
        double GetAngleTo(double x, double y) {
                if(y>player.ship.pos.y && x<player.ship.pos.x) return 2*PI-atan2(y-player.ship.pos.y,x-player.ship.pos.x);
                if(y>player.ship.pos.y && x>player.ship.pos.x) return 2*PI-atan2(y-player.ship.pos.y,x-player.ship.pos.x);
                if(y<player.ship.pos.y && x<player.ship.pos.x) return atan2(y-player.ship.pos.y,x-player.ship.pos.x)*-1;
                if(y<player.ship.pos.y && x>player.ship.pos.x) return atan2(y-player.ship.pos.y,x-player.ship.pos.x)*-1;
                return 0;
        }
        double GetAngleTo(Point from, Point to) {
                if(to.y>from.y && to.x<from.x) return 2*PI-atan2(to.y-from.y,to.x-from.x);
                if(to.y>from.y && to.x>from.x) return 2*PI-atan2(to.y-from.y,to.x-from.x);
                if(to.y<from.y && to.x<from.x) return atan2(to.y-from.y,to.x-from.x)*-1;
                if(to.y<from.y && to.x>from.x) return atan2(to.y-from.y,to.x-from.x)*-1;
                return 0;
        }
        bool IsMovingTowards(Entity *entity, Point to, double tolerance = 0) {
                double movementAngle = GetAngleTo(entity->pos, entity->pos+entity->moveVector*100);
                double target = GetAngleTo(entity->pos, to);
                if(tolerance == 0) return movementAngle == target;
                //double distanceToTarget = DistanceTo(to);
                //double tolerance = atan(maxDistanceOff/distanceToTarget) * PI /180;
                double _min = target-tolerance, _max = target+tolerance;
                return IsAngleBetween(_min, _max, movementAngle);
        }
        bool IsFacing(Point to, double tolerance = 0) {
                double rotation = player.ship.rotation;
                double target = GetAngleTo(player.ship.pos, to);
                if(tolerance == 0) return rotation == target;
                //double distanceToTarget = DistanceTo(to);
                //double tolerance = atan((distanceToTarget/maxDistanceOff)/distanceToTarget) * PI /180;
                double _min = target-tolerance, _max = target+tolerance;
                return IsAngleBetween(_min, _max, rotation);
        }
        bool IsAngleBetween(double &_min, double &_max, double &angle) {
                if(_min < 0) {
                        _min += PI*2;
                        return angle >= _min || angle <= _max;
                } else if(_max > PI*2) {
                        _max -= PI*2;
                        return angle >= _min || angle <= _max;
                }
                return angle >= _min && angle <= _max;
        }
        /// ---
        void LogIn() {
                if(account.name == "" || time(0) < client.clientBeginTime + 3) return;
                if(client.isCharSelected) return;

                if(!client.memory.Calculate2("account_login", client.memory.AccountLogin)) return;
                sleep(1);
                client.memory.Calculate2("account_login", client.memory.AccountLogin);
                if(!client.isLoggedIn) {
                        for(int i=0; i<20; i++) {
                                SendMessage(client.clientWindow, WM_KEYDOWN, VK_RIGHT, 1);
                                Sleep(100);
                                SendMessage(client.clientWindow, WM_KEYUP, VK_RIGHT, 0);
                                Sleep(50);
                                SendMessage(client.clientWindow, WM_KEYDOWN, VK_BACK, 1);
                                Sleep(100);
                                SendMessage(client.clientWindow, WM_KEYUP, VK_BACK, 0);
                                Sleep(50);
                        }
                        for(size_t i=0; i<account.name.length(); i++) {
                                SendMessage(client.clientWindow, WM_KEYDOWN, VkKeyScan(' '), 1);
                                Sleep(100);
                                SendMessage(client.clientWindow, WM_KEYUP, VkKeyScan(' '), 0);
                                Sleep(50);
                        }
                        if(!client.memory.Calculate2("account_target_login", client.memory.AccountTargetLogin)) return;
                        Sleep(500);
                        wstring wsMsg = String::s2ws(account.name);
                        DWORD targetAdd;
                        ReadProcessMemory(client.memory.processHandle, LongToPtr(client.memory.AccountTargetLogin), &targetAdd, 4, NULL);
                        WriteProcessMemory(client.memory.processHandle, LongToPtr(targetAdd), wsMsg.c_str(), wsMsg.length()*2+2, NULL);
                        client.chat.SelectTab("Event");
                        SendMessage(client.clientWindow, WM_KEYDOWN, VK_RETURN, 1);
                        Sleep(100);
                        SendMessage(client.clientWindow, WM_KEYUP, VK_RETURN, 0);
                        Sleep(1000);
                        client.Update();
                        if(!client.isCharSelected) {
                                client.chat.SelectTab("Event");
                                SendMessage(client.clientWindow, WM_KEYDOWN, VkKeyScan('1'), 1);
                                Sleep(100);
                                SendMessage(client.clientWindow, WM_KEYUP, VkKeyScan('1'), 0);
                        }
                } else if(!client.isCharSelected) {
                        client.chat.SelectTab("Event");
                        SendMessage(client.clientWindow, WM_KEYDOWN, VkKeyScan('1'), 1);
                        Sleep(100);
                        SendMessage(client.clientWindow, WM_KEYUP, VkKeyScan('1'), 0);
                }
        }
};

class CONNECTION {
public:
        thread THREAD_LISTEN, THREAD_KEEPALIVE, THREAD_RECONNECT;
        bool TERMINATE = false, CONNECTED = false, RECONNECTING = false;
        time_t CONNECTION_INIT_TIME = 0, CONNECTION_TIME;
        string machineName = "";
        string CONTROLLER_ADDRESS = "127.0.0.1", message;
        int PORT = 9001;
        int _SOCKET;
        int KEEPALIVE_INTERVAL = 5;
        struct sockaddr_in address;
        char buffer[10240];
        fd_set readfds;
        CONNECTION() {

        }

        void GetConfig() {
                vector<string> CONFIGFILE;
                CONFIGFILE = GetFileLines("config.txt");
                string buffer;
                if(GetLineByKey(buffer, "masteraddress", CONFIGFILE, "")) CONTROLLER_ADDRESS = buffer;
                if(GetLineByKey(buffer, "masterport", CONFIGFILE, "")) PORT = atoi(buffer.c_str());
                if(GetLineByKey(buffer, "machinename", CONFIGFILE, "")) machineName = buffer;
        }

        int init() {
                if((_SOCKET = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                        printf(CC_RED, "\tFailed to create socket\n");
                        PrintLastError();
                        return 1;
                }

                address.sin_family = AF_INET;
                address.sin_port = htons(PORT);
                if(InetPton(AF_INET, CONTROLLER_ADDRESS.c_str(), &address.sin_addr)<=0) {
                        printf(CC_RED, "\tInvalid controller address/Address not supported\n");
                        PrintLastError();
                        return 2;
                }

                printf("==Connecting to Master Controller\n");
                if(connect(_SOCKET, (struct sockaddr*)&address, sizeof(address)) < 0) {
                        printf(CC_YELLOW, "\tFailed to connect to master controller:\n");
                        PrintLastError();
                        printf("=Getting master address from webserver...\n");
                        if(GetMasterAddressFromWebServer()) {
                                printf("Retrying connection...\n");
                                if(connect(_SOCKET, (struct sockaddr*)&address, sizeof(address)) < 0) {
                                        printf(CC_YELLOW, "\tFailed to connect to master controller:\n");
                                        PrintLastError();
                                        printf("\tWill keep attempting to connect in the background\n");
                                        while(!TERMINATE) {
                                                if(connect(_SOCKET, (struct sockaddr*)&address, sizeof(address)) >= 0) break;
                                                sleep(5);
                                        }
                                }
                        }
                }
                printf("=Connected to Master Controller\n");
                CONNECTION_INIT_TIME = time(0);
                CONNECTION_TIME = time(0);
                CONNECTED = true;
                IS_CONNECTED = &CONNECTED;
                THREAD_LISTEN = thread(CONNECTION::_listen, this);
                THREAD_LISTEN.detach();
                THREAD_KEEPALIVE = thread(CONNECTION::_keepAlive, this);
                THREAD_KEEPALIVE.detach();
                ExchangeInfo();
                return 0;
        }

        bool GetMasterAddressFromWebServer() {
                string receiver;
                WebServer::POST("action=GetMasterAddress", &receiver);
                if(receiver == "") return false;
                auto j = json::parse(receiver.c_str());
                printf(CC_CYAN, "=Web server replied with IP: %s, port: %s\n", j["ip"].get<string>().c_str(), j["port"].get<string>().c_str());
                CONTROLLER_ADDRESS = j["ip"];
                PORT = atoi(j["port"].get<string>().c_str());

                address.sin_family = AF_INET;
                address.sin_port = htons(PORT);
                if(InetPton(AF_INET, CONTROLLER_ADDRESS.c_str(), &address.sin_addr)<=0) {
                        printf(CC_RED, "\tInvalid controller address/Address not supported\n");
                        PrintLastError();
                        return false;
                }

                EditFileLine(CONTROLLER_ADDRESS, "masteraddress", "config.txt");
                EditFileLine(to_string(PORT), "masterport", "config.txt");

                return true;
        }

        bool Send(string msg, unsigned short attempts = 0) {
                if(!CONNECTED || msg == "") return false;
                if(attempts == 0) msg += MSG_END_DELIM;
                if(attempts >= 3) {
                        Reconnect();
                        return false;
                }
                int result = send(_SOCKET, msg.c_str(), msg.length(), 0);
                if(result == SOCKET_ERROR) {
                        if(attempts == 0) {
                                printf("\t@Error sending message to master:\n");
                                PrintLastError();
                        }
                        attempts++;
                        Send(msg, attempts);
                        return false;
                }
                return true;
        }

private:
        void _keepAlive() {
                while(!TERMINATE && CONNECTED) {
                        Send("K");
                        sleep(KEEPALIVE_INTERVAL);
                }
        }

        void _listen() {
                int result;
                size_t i = 0;
                string fullMsg = "", receivedPart = "";
                while(!TERMINATE && CONNECTED) {
                        result = recv(_SOCKET, buffer, sizeof(buffer), 0);
                        if(result > 0) {
                                printf("!New data from master\n");
                                buffer[result] = '\0';
                                receivedPart = (string)buffer;
                                do {
                                        i = receivedPart.find(MSG_END_DELIM);
                                        if(i == string::npos) fullMsg += receivedPart;
                                        else {
                                                fullMsg += receivedPart.substr(0, i);
                                                ProcessMessage(fullMsg);
                                                fullMsg = "";
                                                i += strlen(MSG_END_DELIM);
                                                receivedPart = receivedPart.substr(i);
                                        }
                                } while(i != string::npos);
                        } else {
                                printf("Connection to master lost\n");
                                Reconnect();
                                break;
                        }
                        Sleep(100);
                }
        }

        void Reconnect() {
                if(RECONNECTING) return;
                RECONNECTING = true;
                CONNECTED = false;
                printf("\tAttempting to reconnect with master\n");
                close(_SOCKET);
                if((_SOCKET = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                        printf("\tFailed to create socket\n");
                        PrintLastError();
                        return;
                }
                address.sin_family = AF_INET;
                address.sin_port = htons(PORT);
                if(InetPton(AF_INET, CONTROLLER_ADDRESS.c_str(), &address.sin_addr)<=0) {
                        printf("\tInvalid controller address/Address not supported\n");
                        PrintLastError();
                        return;
                }
                if(THREAD_RECONNECT.joinable()) THREAD_RECONNECT.join();
                THREAD_RECONNECT = thread([](CONNECTION *con) {
                        while(!con->TERMINATE) {
                                if(connect(con->_SOCKET, (struct sockaddr*)&con->address, sizeof(con->address)) >= 0) {
                                        printf("=Reconnected with master\n");
                                        if(con->THREAD_LISTEN.joinable()) con->THREAD_LISTEN.join();
                                        if(con->THREAD_KEEPALIVE.joinable()) con->THREAD_KEEPALIVE.join();
                                        con->CONNECTED = true;
                                        con->THREAD_LISTEN = thread(CONNECTION::_listen, con);
                                        con->THREAD_KEEPALIVE = thread(CONNECTION::_keepAlive, con);
                                        con->RECONNECTING = false;
                                        con->CONNECTION_TIME = time(0);
                                        con->ExchangeInfo();
                                        return;
                                }
                                sleep(5);
                        }
                }, this);
        }

        void ExchangeInfo() {
                json j{
                        {"action", "ExchangeInfo"},
                        {"machineName", machineName.c_str()},
                        {"PROCESSBEGINTIME", PROCESSBEGINTIME}
                };
                for(SYSTEM& system : GALAXY) { j["UNIMAP"].push_back(system.id); }
                for(DUNGEON& dungeon : DUNGEONS) { j["DGMAP"].push_back(dungeon.name); }
                Send(j.dump());
        }

        void ProcessMessage(string& msg) {
                printf("MASTER: %s\n", msg.c_str());
                json msgData;
                try {
                        msgData = json::parse(msg.c_str());
                        if(msgData["action"] == "TaskUpdate") {
                                mtx_ReceivedMessageQueue.lock();
                                ReceivedMessageQueue.push_back(TaskMessage(msgData));
                                mtx_ReceivedMessageQueue.unlock();
                        } else if(msgData["action"] == "UpdateMachineName") {
                                machineName = msgData["machineName"];
                                printf(CC_CYAN, "!Received new name from master: %s\n", machineName.c_str());
                                EditFileLine(machineName, "machinename", "config.txt");
                        } else if(msgData["action"] == "AssignAccount") {
                                mtx_bots.lock();
                                bool alreadyExists = false;
                                for(BOT* bot : bots) if(bot->player.loggedAccount == msgData["account"]["name"]) {
                                                bot->account = ACCOUNT(msgData["account"]);
                                                bot->character = CHARACTER(msgData["character"]);
                                                bot->active = true;
                                                alreadyExists = true;
                                        }
                                if(!alreadyExists) {
                                        BOT* newBot = new BOT();
                                        newBot->account = ACCOUNT(msgData["account"]);
                                        newBot->character = CHARACTER(msgData["character"]);
                                        newBot->active = true;
                                        bots.push_back(newBot);
                                        newBot->Init();
                                }
                                mtx_bots.unlock();
                        } else if(msgData["action"] == "UpdatePointers") {
                                list<POINTERS::Pointer> _copy = POINTERS::pointers;
                                if(!POINTERS::Set(msgData["data"])) {
                                        printf(CC_YELLOW, "Failed to update pointers, resetting back to old\n");
                                        POINTERS::pointers = _copy;
                                } else {
                                        POINTERS::Save();
                                        printf(CC_GREEN, "Successfully updated pointers\n");
                                }
                                mtx_bots.lock();
                                for(BOT* bot : bots) bot->client.memory.Update();
                                mtx_bots.unlock();
                        } else if(msgData["action"] == "UpdateOffsets") {
                                list<OFFSETS::Offset> _copy = OFFSETS::offsets;
                                if(!OFFSETS::Set(msgData["data"])) {
                                        printf(CC_YELLOW, "Failed to update offsets, resetting back to old\n");
                                        OFFSETS::offsets = _copy;
                                } else {
                                        OFFSETS::Save();
                                        printf(CC_GREEN, "Successfully updated offsets\n");
                                }
                        } else if(msgData["action"] == "UpdateObjectTypes") {
                                if(!OBJECT_TYPES::Load(msgData["data"])) {
                                        printf(CC_RED, "Failed to update object types\n");
                                } else {
                                        OBJECT_TYPES::Save();
                                        printf(CC_GREEN, "Successfully updated object types\n");
                                }
                        } else if(msgData["action"] == "RequestSystemData") {
                                json response{
                                        {"action", "UpdateSystems"}
                                };
                                for(auto& el : msgData["data"]) {
                                        UINT thisId = (el.is_string())?stoi(el.get<string>(), NULL):el.get<int>();
                                        for(SYSTEM& sys : GALAXY) if(sys.id == thisId) { response["data"].push_back(sys.to_JSON()); break; }
                                }
                                if(response["data"].size() > 0) {
                                        Send(response.dump());
                                        printf(CC_BLUE, "Sent %i systems to controller\n", response["data"].size());
                                }
                        } else if(msgData["action"] == "RequestDungeonData") {
                                json response{
                                        {"action", "UpdateDungeons"}
                                };
                                for(auto& el : msgData["data"]) {
                                        for(DUNGEON& dg : DUNGEONS) {
                                                if(dg.name == el) response["data"].push_back(dg.to_JSON());
                                                break;
                                        }
                                }
                                if(response["data"].size() > 0) {
                                        Send(response.dump());
                                        printf(CC_BLUE, "Sent %i dungeons to controller\n", response["data"].size());
                                }
                        } else if(msgData["action"] == "AddSystem") {
                                printf(CC_BLUE, "Received new system data from controller (%s)\n", msgData["data"]["name"].get<string>().c_str());
                                UNIVERSE::AddSystem(msgData["data"]);
                                UNIVERSE::Save();
                        } else if(msgData["action"] == "AddDungeon") {
                                printf(CC_BLUE, "Received new dungeon data from controller (%s)\n", msgData["data"]["name"].get<string>().c_str());
                                _DUNGEONS::AddDungeon(msgData["data"]);
                                _DUNGEONS::Save();
                        } else if(msgData["action"] == "UpdateUniverseMap") {
                                try {
                                        printf(CC_BLUE, "Received universe map update from controller, %i systems\n", msgData["data"].size());
                                        UNIVERSE::AddSystems(msgData["data"]);
                                        UNIVERSE::Save();
                                } catch(const exception &e) { printf(CC_YELLOW, "Error on UpdateUniverseMap: %s\n", e.what()); }
                        } else if(msgData["action"] == "UpdateDungeonsMap") {
                                try {
                                        printf(CC_BLUE, "Received dungeons map update from controller, %i dungeons\n", msgData["data"].size());
                                        for(auto& el : msgData["data"]) _DUNGEONS::AddDungeon(el);
                                        _DUNGEONS::Save();
                                } catch(const exception &e) { printf(CC_YELLOW, "Error on UpdateDungeonsMap: %s\n", e.what()); }
                        } else if(msgData["action"] == "AddP2PSystem") {
                                UNIVERSE::AddP2PSystem(msgData["name"]);
                                UNIVERSE::SaveLists();
                        } else if(msgData["action"] == "AddIgnoredSystem") {
                                UNIVERSE::AddIgnoredSystem(msgData["name"]);
                                UNIVERSE::SaveLists();
                        } else if(msgData["action"] == "AddWarp4System") {
                                UNIVERSE::AddWarp4System(msgData["name"]);
                                UNIVERSE::SaveLists();
                        }
                } catch(const exception &e) {
                        printf("%s\n", e.what());
                };
        }
};
CONNECTION connection;

///---
clock_t NextStrayClientCheck = 0, NextReceivedMessageCheck = 0;
void GetStrayClients() {
        vector<UINT> processList = GetProcessList("Star Sonata.exe");
        mtx_bots.lock();
        bool isListed = false;
        for(auto& process : processList) {
                isListed = false;
                for(BOT* bot : bots) if(bot->client.GetPID() == process) { isListed = true; break; }
                if(!isListed) {
                        BOT* newBot = new BOT();
                        newBot->active = false;
                        bots.push_back(newBot);
                        newBot->Init(process);
                        printf(CC_BLUE, "Added new bot/client (%s)\n", newBot->player.loggedAccount.c_str());
                }
        }
        mtx_bots.unlock();
        NextStrayClientCheck = clock() + 1000 * 10;
}

void AtExit() {
        printf("==EXITING APP\n");
        mtx_bots.lock();
        for(BOT* &index : bots) { index->TERMINATE = true; }
        for(BOT* &index : bots) { if(index->THREAD.joinable()) index->THREAD.join(); }
        mtx_bots.unlock();
        connection.TERMINATE = true;
        if(connection.THREAD_KEEPALIVE.joinable()) connection.THREAD_KEEPALIVE.join();
        if(connection.THREAD_LISTEN.joinable()) connection.THREAD_LISTEN.join();
        if(connection.THREAD_RECONNECT.joinable()) connection.THREAD_RECONNECT.join();
}

void LaunchClientsFromFile() {
        vector<string> _file = GetFileLines("launch_clients.txt");

        size_t pos1/*, pos2*/;
        string accName, accPassword;
        bool _launch;

        for(string &str : _file) {
                String::trim(str);
                if(str == "") continue;
                /*pos1 = str.find_first_of(",");
                pos2 = str.find_first_of("=");
                if(pos1 == string::npos || pos2 == string::npos) continue;
                accName = str.substr(0, pos1);
                accPassword = str.substr(pos1+1, pos2-pos1-1);
                _launch = atoi(str.substr(pos2+1).c_str());*/
                pos1 = str.find_first_of("=");
                if(pos1 == string::npos) continue;
                accName = str.substr(0, pos1);
                _launch = atoi(str.substr(pos1+1).c_str());
                if(!_launch) continue;
                bool isRunning = false, isAssigned = false;
                mtx_bots.lock();
                for(BOT *bot : bots) if(bot->account.name == "" && bot->player.loggedAccount == "" && !bot->client.isCharSelected) {
                                bot->account.name = accName;
                                bot->account.password = accPassword;
                                isAssigned = true;
                                break;
                        }
                if(isAssigned) { mtx_bots.unlock(); continue; }
                for(BOT *bot : bots) if(bot->account.name == accName || bot->player.loggedAccount == accName) { isRunning = true; break; }

                if(!isRunning) {
                        BOT* newBot = new BOT();
                        newBot->active = false;
                        bots.push_back(newBot);
                        newBot->account.name = accName;
                        newBot->account.password = accPassword;
                        newBot->Init();
                        printf(CC_BLUE, "Added new bot/client (%s)\n", newBot->account.name.c_str());
                }
                mtx_bots.unlock();
        }
}

void GetConsoleInput() {
        printf(" >");
        fflush(stdin);
        string input = "";
        getline(cin, input);

        if(input == "help") {
                mtx_printf.lock();
                cout<<"==List of commands: \n";
                cout<<"=dump data\n";
                cout<<"=dump objects\n";
                cout<<"=dump friends\n";
                cout<<"=dump players\n";
                cout<<"=dump items\n";
                cout<<"=dump test (for testing purposes)\n";
                cout<<"=prospectable systems [-names] - display count\n";
                cout<<"=unexplored systems [-names] - display count\n";
                cout<<"=pause all\n";
                cout<<"=active alts\n";
                cout<<"=ap [galname]\n";
                cout<<"=squad alts - squad all currently open alts on last active client\n";
                cout<<"=launch from file\n";
                cout<<"=find window children (add)\n";
                cout<<"=return to base\n";
                cout<<"=finish dg and wait\n";
                cout<<"=resume - if bots were called to return to base/wait, use this to resume\n";
                cout<<"=forget dead wild bots\n";
                cout<<"=check galaxy - add unexplored dgs/systems to map\n";
                cout<<"=get loots - make a file with all loot dumps\n";
                cout<<"=lockouts - display lockouts\n";
                cout<<"=report locations\n";
                cout<<"=dgs completed\n";
                cout<<"=clear ignored debris\n";
                cout<<"=remove duplicate systems\n";
                cout<<"=focus (character name)\n";
                cout<<"=runtime - how long has app been running for\n";
                mtx_printf.unlock();
        } else if(input == "dump data") {
                mtx_bots.lock();
                for(BOT*& bot : bots) bot->DumpData();
                mtx_bots.unlock();
        } else if(input == "dump objects") {
                mtx_bots.lock();
                for(BOT*& bot : bots) bot->DumpObjects();
                mtx_bots.unlock();
        }  else if(input == "dump friends") {
                mtx_bots.lock();
                for(BOT*& bot : bots) bot->DumpFriends();
                mtx_bots.unlock();
        } else if(input == "dump players") {
                mtx_bots.lock();
                for(BOT*& bot : bots) bot->DumpPlayers();
                mtx_bots.unlock();
        } else if(input == "dump test") {
                mtx_bots.lock();
                for(BOT*& bot : bots) bot->DumpTest();
                mtx_bots.unlock();
        } else if(input == "dump items") {
                mtx_bots.lock();
                for(BOT*& bot : bots) bot->DumpItems();
                mtx_bots.unlock();
        } else if(input == "pause all") {
                mtx_bots.lock();
                for(BOT*& bot : bots) bot->PAUSE = true;
                mtx_bots.unlock();
        } else if(input == "active alts") {
                mtx_bots.lock();
                printf("Active alts: ");
                for(BOT*& bot : bots) if(bot->TASK != TASK_NONE) printf("%s, ", bot->player.name.c_str());
                printf("\n");
                printf("Idle alts: ");
                for(BOT*& bot : bots) if(bot->TASK == TASK_NONE) printf("%s, ", bot->player.name.c_str());
                printf("\n");
                mtx_bots.unlock();
        } else if(input == "ap") {
                string apTarget = input.substr(3);
                mtx_bots.lock();
                for(BOT*& bot : bots) { bot->SendChatMessage("Event", "/ap \""+apTarget+"\""); Sleep(1000); }
                mtx_bots.unlock();
        } else if(input == "squad alts") {
                mtx_bots.lock();
                for(BOT*& bot : bots) if(bot->client.clientWindow == LastActiveWindow) {
                                if(!bot->client.isCharSelected) break;
                                for(BOT*& otherBot : bots) {
                                        if(bot->client.clientWindow == LastActiveWindow) continue;
                                        bot->SendChatMessage("Event", "/squad "+otherBot->player.name);
                                }
                        }
                mtx_bots.unlock();
        } else if(input == "launch from file") {
                LaunchClientsFromFile();
        } else if(input.find("find window children") == 0) {
                string addstr = input.substr(21);
                if(addstr == "") return;
                DWORD add = strtoul(addstr.c_str(), NULL, 16);
                mtx_bots.lock();
                for(BOT *bot : bots) {
                        printf("Using %s\n", bot->player.name.c_str());
                        GUIElement::IterateChildren(add, "blyat", &bot->client.memory, 0);
                        break;
                }
                mtx_bots.unlock();
        } else if(input == "return to base") {
                mtx_bots.lock();
                for(BOT *bot : bots) {
                        bot->isUnloadingLoots = true;
                        bot->returnToBase = true;
                        if(bot->task_targetLocation == "" || (bot->TASK == TASK_DG && !bot->hasEnteredTargetDungeon)) bot->isReturningToBase = true;
                }
                mtx_bots.unlock();
        } else if(input == "finish dg and wait") {
                mtx_bots.lock();
                for(BOT *bot : bots) bot->finishDGAndWait = true;
                mtx_bots.unlock();
        } else if(input == "resume") {
                mtx_bots.lock();
                for(BOT *bot : bots) {
                        bot->isReturningToBase = false;
                        bot->returnToBase = false;
                        bot->finishDGAndWait = false;
                        bot->hasReportedDGWait = false;
                }
                mtx_bots.unlock();
        } else if(input.find("prospectable systems") == 0) {
                vector<pair<string, size_t>> layers;
                layers.push_back(make_pair("EarthForce", 0));
                layers.push_back(make_pair("Wild Space", 0));
                layers.push_back(make_pair("UNEXPLORED", 0));
                mtx_bots.lock();
                for(BOT *bot : bots) {
                        for(string &layer : bot->settings.prospectLayers) {
                                bool isListed = false;
                                for(auto &animeTits : layers) if(animeTits.first == layer) { isListed = true; break; }
                                if(!isListed) layers.push_back(make_pair(layer, 0));
                        }
                }
                mtx_bots.unlock();
                UNIVERSE::mtx_AddSystem.lock();
                vector<string> names;
                for(SYSTEM &system : GALAXY) {
                        if(!UNIVERSE::IsValidProspectTarget(&system)) continue;
                        string thisLayer = system.layer;
                        if(thisLayer == "") thisLayer = "UNEXPLORED";
                        for(auto &layer : layers) if(layer.first == thisLayer) { layer.second++; names.push_back(system.name); break; }
                }
                printf("Prospectable systems in:\n");
                for(auto &layer : layers) printf("-%s: %i\n", layer.first.c_str(), layer.second);
                if(input.find("-names") != string::npos) {
                        for(string &str : names) printf("%s, ", str.c_str());
                        printf("\n");
                }
                UNIVERSE::mtx_AddSystem.unlock();
        } else if(input.find("unexplored systems") == 0) {
                UNIVERSE::mtx_AddSystem.lock();
                bool printNames = input.find("-names") != string::npos;
                vector<string> unexploredSystems, unexploredShortcuts;
                for(SYSTEM &system : GALAXY) if(!system.isExplored) {
                                if(UNIVERSE::IsValidExploreTarget(&system)) {
                                        if(!UNIVERSE::IsWarp4(system.name)) unexploredSystems.push_back(system.name);
                                        else unexploredShortcuts.push_back(system.name);
                                }
                        }
                UNIVERSE::mtx_AddSystem.unlock();
                printf("Unexplored systems: %i\n", unexploredSystems.size());
                if(printNames) {
                        for(string &str : unexploredSystems) printf("%s, ", str.c_str());
                        printf("\n");
                }
                printf("Unexplored shortcuts: %i\n", unexploredShortcuts.size());
                if(printNames) {
                        for(string &str : unexploredShortcuts) printf("%s, ", str.c_str());
                        printf("\n");
                }
        } else if(input == "forget dead wild bots") {
                mtx_bots.lock();
                size_t botsRemoved = 0;
                for(BOT *bot : bots) {
                        if(bot->combatBots.size() > 0) {
                                for(size_t i = bot->combatBots.size()-1; i>= 0; i--) {
                                        if(bot->combatBots[i].isDead && bot->combatBots[i].isWild) {
                                                botsRemoved++;
                                                bot->combatBots.erase(bot->combatBots.begin()+i);
                                        }
                                        if(i==0) break;
                                }
                        }
                }
                printf("Removed %i dead wild bots\n", botsRemoved);
                mtx_bots.unlock();
        } else if(input == "check galaxy") {
                UNIVERSE::mtx_AddSystem.lock();
                for(SYSTEM &system : GALAXY) {
                        for(string &wh : system.wormholes) UNIVERSE::AddUnexploredSystem(UNIVERSE::GetSystemFromWormhole(wh));
                }
                UNIVERSE::mtx_AddSystem.unlock();
                UNIVERSE::Save();
                _DUNGEONS::Save();
        } else if(input.find( "get loots") == 0) {
                vector<string> fileLines;
                vector<pair<string, int>> augs, commods, other;
                vector<pair<string, int>> *targetType = &other;
                string path = APPLOCATION, thisFileName;
                path += "\\loot dumps\\";
                int thisQuantity;
                string thisItem;
                size_t pos1;
                DIR *dir;
                struct dirent *ent;
                if((dir = opendir(path.c_str())) != NULL) {
                        while ((ent = readdir (dir)) != NULL) {
                                if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
                                thisFileName = ent->d_name;
                                fileLines = GetFileLines(string("loot dumps/"+thisFileName));
                                for(string &line : fileLines){
                                        if(line == "=== AUGMENTERS"){ targetType = &augs; continue; }
                                        else if(line == "=== COMMODITIES"){ targetType = &commods; continue; }
                                        else if(line == "=== OTHERS"){ targetType = &other; continue; }
                                        pos1 = line.find_first_of(' ');
                                        if(pos1 == string::npos) continue;
                                        thisQuantity = atoi(line.substr(0, pos1).c_str());
                                        thisItem = line.substr(pos1+1);
                                        bool isListed = false;
                                        for(auto &item : *targetType) if(item.first == thisItem){ item.second++; isListed = true; break; }
                                        if(!isListed) targetType->push_back(make_pair(thisItem, thisQuantity));
                                }
                        }
                }
                printf("Commods: %i\nAugs: %i\nOther: %i\n", commods.size(), augs.size(), other.size());

                ofstream logFile("all_loots.txt");
                logFile<<"=== COMMODITIES"<<endl;
                thisQuantity = 0;
                for(auto &item : commods){ logFile<<item.second<<" "<<item.first.c_str()<<endl; thisQuantity += item.second; }
                printf("Total %i commods\n", thisQuantity);
                logFile<<"=== AUGMENTERS"<<endl;
                thisQuantity = 0;
                for(auto &item : augs){ logFile<<item.second<<" "<<item.first.c_str()<<endl; thisQuantity += item.second; }
                printf("Total %i augs\n", thisQuantity);
                logFile<<"=== OTHER"<<endl;
                thisQuantity = 0;
                for(auto &item : other){ logFile<<item.second<<" "<<item.first.c_str()<<endl; thisQuantity += item.second; }
                printf("Total %i other items\n", thisQuantity);
        } else if(input == "lockouts"){

        } else if(input == "report locations"){
                mtx_bots.lock();
                for(BOT *bot : bots){
                        if(bot->player.name == "") continue;
                        printf("%s - ", bot->player.name.c_str());
                        if(bot->client.isLoggedIn) printf("%s DF %.0f\n", bot->player.location.c_str(), bot->GetDangerFactor()*100);
                        else printf("offline\n");
                }
                mtx_bots.unlock();
        } else if(input == "dgs completed"){
                mtx_bots.lock();
                for(BOT *bot : bots){
                        if(bot->player.name == "") continue;
                        printf("%s (%s) - ", bot->player.name.c_str(), bot->player.ship.name);
                        if(bot->client.isLoggedIn) printf("%i\n", bot->dgsCompleted);
                }
                mtx_bots.unlock();
        } else if(input == "clear ignored debris"){
                mtx_bots.lock();
                for(BOT *bot : bots) bot->ignoredDebris.clear();
                mtx_bots.unlock();
        } else if(input == "remove duplicate systems"){
                UNIVERSE::mtx_AddSystem.lock();
                size_t erased = 0;
                auto it = GALAXY.begin();
                while(it != GALAXY.end()){
                        bool isDuplicate = false;
                        auto it2 = it;
                        ++it2;
                        while(it2 != GALAXY.end()) if((*it2).id == (*it).id){ isDuplicate = true; break; }
                        if(isDuplicate){ erased++; it = GALAXY.erase(it); }
                        else ++it;
                }
                UNIVERSE::Save();
                printf("Removed %i systems\n", erased);
                UNIVERSE::mtx_AddSystem.unlock();
        } else if(input.find("focus") != string::npos){
                string target = input.substr(5);
                String::trim(target);
                mtx_bots.lock();
                for(BOT *bot : bots) if(bot->player.name == target){ SetForegroundWindow(bot->client.clientWindow); break; }
                mtx_bots.unlock();
        }  else if(input == "runtime"){
                printf("%i seconds\n", floor(clock()/1000));
        } else if(input == "test") {
                mtx_bots.lock();
                for(BOT*& bot : bots) {
                        // Get items list
                        /*DWORD add = 0, listBeginAdd = 0, itemAdd = 0;
                        add = GUIElement::FindChild(bot->client.memory.StationWindow, "itemsDialog", &bot->client.memory, 0);
                        add = GUIElement::FindChild(add, "itemsTableBaseItems", &bot->client.memory, 0);
                        ReadProcessMemory(bot->client.memory.processHandle, LongToPtr(add+0x838), &listBeginAdd, 4, NULL);
                        add = 0;
                        ReadProcessMemory(bot->client.memory.processHandle, LongToPtr(listBeginAdd), &add, 4, NULL);
                        char memoryChunk[0x12];
                        DWORD strAdd = 0;
                        wstring strBuff;
                        int length = 0;
                        vector<pair<string, size_t>> scanresult;
                        while(add != listBeginAdd){
                                ReadProcessMemory(bot->client.memory.processHandle, LongToPtr(add+0x8), &itemAdd, 4, NULL);
                                if(itemAdd == 0){
                                        printf("error\n");
                                        break;
                                }
                                ReadProcessMemory(bot->client.memory.processHandle, LongToPtr(itemAdd+0xC0), &memoryChunk, 0x12, NULL);
                                strAdd = *reinterpret_cast<DWORD*>((char*)memoryChunk);
                                length = *reinterpret_cast<int*>((char*)memoryChunk+0x4);
                                strBuff = bot->client.memory.ReadWStringDirect(strAdd, length);
                                string name = String::ws2s(strBuff);

                                ReadProcessMemory(bot->client.memory.processHandle, LongToPtr(itemAdd+0x40), &memoryChunk, 0x12, NULL);
                                strAdd = *reinterpret_cast<DWORD*>((char*)memoryChunk);
                                length = *reinterpret_cast<int*>((char*)memoryChunk+0x4);
                                strBuff = bot->client.memory.ReadWStringDirect(strAdd, length);
                                string quantity = String::ws2s(strBuff);
                                ReadProcessMemory(bot->client.memory.processHandle, LongToPtr(add), &add, 4, NULL);
                                if(quantity == "") continue;

                                scanresult.push_back(make_pair(name, atoi(quantity.c_str())));
                        }
                        vector<pair<string, size_t>> itemList;
                        for(auto &scan : scanresult){
                                scan.first.erase(std::remove(scan.first.begin(), scan.first.end(), '*'), scan.first.end());
                                bool isListed = false;
                                for(auto &item : itemList){
                                        if(item.first == scan.first){
                                                item.second += scan.second;
                                                isListed = true;
                                                break;
                                        }
                                }
                                if(isListed) continue;
                                itemList.push_back(scan);
                        }
                        //for(auto &item : itemList) printf("-%s - %i\n", item.first.c_str(), item.second);
                        printf("=\n");*/
                        // MC
                        ofstream outputFile("mc_items_result.txt", std::ofstream::out | std::ofstream::trunc);
                        outputFile.close();
                        outputFile.open("mc_items_result_sellable.txt", std::ofstream::out | std::ofstream::trunc);
                        outputFile.close();
                        vector<string> itemList = GetFileLines("mc_items_list.txt");
                        if(itemList.size() == 0) return;
                        GUIElement mcWindow = GUIElement(&bot->client.memory);
                        size_t itemsChecked = 0;
                        for(string &item : itemList) {
                                double percentDone = (double)itemsChecked / (double)itemList.size()*100;
                                stringstream windowName;
                                windowName<<std::fixed<<std::setprecision(1)<<percentDone<<"% ("<<itemsChecked<<"/"<<itemList.size()<<")";
                                SetWindowText(bot->client.clientWindow, windowName.str().c_str());
                                itemsChecked++;
                                if(item == "") continue;
                                bot->SendChatMessage("Event", "/mc "+item);
                                clock_t start = clock();
                                while(clock() < start+15*CLOCKS_PER_SEC && !bot->client.memory.Calculate2("mc_window", bot->client.memory.MCWindow)) Sleep(15);
                                if(bot->client.memory.MCWindow == 0) {
                                        printf("error\n");
                                        break;
                                }
                                start = clock();
                                while(clock() < start+10*CLOCKS_PER_SEC && mcWindow.text1 == "") {
                                        mcWindow.Load(bot->client.memory.MCWindow);
                                        Sleep(50);
                                }
                                vector<string> lines;
                                start = clock();
                                do {
                                        mcWindow.Load(bot->client.memory.MCWindow);
                                        lines = String::split(mcWindow.text1, "\n");
                                        if(lines.size() != 0 && lines[0] == item) break;
                                } while(clock() < start+5*CLOCKS_PER_SEC);
                                if(lines.size() == 0) {
                                        printf("Failed to mc item '%s'\n", item.c_str());
                                        continue;
                                }
                                string part;
                                vector<pair<double, UINT>> buyPrices, sellPrices;
                                double thisPrice = 0, ownShopSellPrice = 0, scrapPrice = 0;
                                UINT thisQuantity = 0;
                                size_t pos1, pos2;;
                                if(lines[0] != item) {
                                        printf("Failed to mc item '%s'\n", item.c_str());
                                        continue;
                                }
                                bool getPrices = false, hasAddedScrapPrice = false;
                                outputFile.open("mc_items_result.txt", ofstream::app);
                                outputFile<<"===== "<<item.c_str()<<endl;
                                for(string &line : lines) {
                                        String::trim(line);
                                        if(line.find("Scrap Value") != string::npos) {
                                                pos1 = line.find_last_of(':');
                                                if(pos1 != string::npos) {
                                                        part = line.substr(pos1+1);
                                                        String::trim(part);
                                                        part.erase(std::remove(part.begin(), part.end(), ','), part.end());
                                                        scrapPrice = atof(part.c_str());
                                                }
                                        }
                                        if(!getPrices && line.find("Cheapest locations") != string::npos) { getPrices = true; continue; }
                                        if(!getPrices) continue;
                                        if(line == "") break;
                                        pos1 = line.find_first_of("]");
                                        if(pos1 == string::npos) continue;
                                        pos2 = line.find_first_of("(", pos1);
                                        if(pos2 == string::npos) continue;
                                        part = line.substr(pos1+1, pos2-pos1-1);
                                        String::trim(part);
                                        if(part.find(",") != string::npos) continue;
                                        pos1 = part.find(".");
                                        if(pos1 == string::npos) continue;
                                        thisPrice = stod(part.substr(0, -1));
                                        if(part.find("k") != string::npos) thisPrice *= 1e3;
                                        else if(part.find("m") != string::npos) thisPrice *= 1e6;
                                        else if(part.find("b") != string::npos) thisPrice *= 1e9;
                                        else if(part.find("t") != string::npos) thisPrice *= 1e12;
                                        pos1 = pos2;
                                        pos2 = line.find_first_of(")");
                                        if(pos2 == string::npos) continue;
                                        part = line.substr(pos1+1, pos2-pos2-1);
                                        String::trim(part);
                                        thisQuantity = atoi(part.c_str());
                                        buyPrices.push_back(make_pair(thisPrice, thisQuantity));
                                        if(thisPrice > scrapPrice && !hasAddedScrapPrice) {
                                                outputFile<<"    SCRAP: "<<String::FormatNumber(scrapPrice).c_str()<<endl;
                                                hasAddedScrapPrice = true;
                                        }
                                        if(line.find(bot->settings.unloadLoots_base) != string::npos) {
                                                outputFile<<">>> ";
                                                ownShopSellPrice = thisPrice;
                                        } else outputFile<<"    ";
                                        outputFile<<line.c_str()<<endl;
                                }
                                outputFile.close();
                                outputFile.open("mc_items_result_sellable.txt", ofstream::app);
                                getPrices = false;
                                bool addHeader = true;
                                for(string &line : lines) {
                                        String::trim(line);
                                        if(!getPrices && line.find("Most profitable") != string::npos) { getPrices = true; continue; }
                                        if(!getPrices) continue;
                                        if(line == "") break;
                                        pos1 = line.find_first_of("]");
                                        if(pos1 == string::npos) continue;
                                        pos2 = line.find_first_of("(", pos1);
                                        if(pos2 == string::npos) continue;
                                        part = line.substr(pos1+1, pos2-pos1-1);
                                        String::trim(part);
                                        if(part.find(",") != string::npos) continue;
                                        pos1 = part.find(".");
                                        if(pos1 == string::npos) continue;
                                        thisPrice = stod(part.substr(0, -1));
                                        if(part.find("k") != string::npos) thisPrice *= 1e3;
                                        else if(part.find("m") != string::npos) thisPrice *= 1e6;
                                        else if(part.find("b") != string::npos) thisPrice *= 1e9;
                                        else if(part.find("t") != string::npos) thisPrice *= 1e12;
                                        pos1 = pos2;
                                        pos2 = line.find_first_of(")");
                                        if(pos2 == string::npos) continue;
                                        part = line.substr(pos1+1, pos2-pos2-1);
                                        String::trim(part);
                                        thisQuantity = atoi(part.c_str());
                                        sellPrices.push_back(make_pair(thisPrice, thisQuantity));
                                        if(thisPrice > ownShopSellPrice) {
                                                if(addHeader) {
                                                        outputFile<<"=== "<<item.c_str()<<"("<<String::FormatNumber(ownShopSellPrice).c_str()<<")"<<endl;
                                                        addHeader = false;
                                                }
                                                outputFile<<line.c_str()<<endl;
                                        }
                                } // sell locations
                                outputFile.close();
                        } // item
                        break;
                }
                mtx_bots.unlock();
        } else { printf("=Unknown command\n"); }
}

int main() {
        printf("=====SS BOT Drone=====\n");
        printf("==Initializing...\n");
        srand((unsigned)time(NULL));
        getcwd(APPLOCATION,MAX_PATH);
        CreateDirectory(((string)APPLOCATION+"\\data").c_str(), NULL);
        CreateDirectory(((string)APPLOCATION+"\\dumps").c_str(), NULL);
        CreateDirectory(((string)APPLOCATION+"\\loot dumps").c_str(), NULL);
        atexit(AtExit);
        SetConsoleTitle("SS Bot Drone");
        {
                if(!RegisterHotKey(NULL, 1, 0x4001, 0xDB)) { /// [
                        printf("Could not register ALT+[ hotkey for [Console control], the app will close now");
                        getchar();
                        exit(0);
                }
                if(!RegisterHotKey(NULL, 2, 0x4001, VkKeyScan(']'))) {
                        printf("Could not register ALT+] hotkey for [Bot Pause], the app will close now");
                        getchar();
                        exit(0);
                }
                if(!RegisterHotKey(NULL, 3, 0x4001, VkKeyScan(';'))) {
                        printf("Could not register ALT+; hotkey for [Deploy DPS drones], the app will close now");
                        getchar();
                        exit(0);
                }
                if(!RegisterHotKey(NULL, 4, 0x4001, VkKeyScan('\''))) {
                        printf("Could not register ALT+' hotkey for [Deploy support drones], the app will close now");
                        getchar();
                        exit(0);
                }
                if(!RegisterHotKey(NULL, 5, 0x4001, VkKeyScan('/'))) {
                        printf("Could not register ALT+' hotkey for [Scoop drones], the app will close now");
                        getchar();
                        exit(0);
                }
                if(!RegisterHotKey(NULL, 6, 0x4000, VK_PAUSE)) {
                        printf("Could not register ALT+' hotkey for [Client pause/ignore], the app will close now");
                        getchar();
                        exit(0);
                }
        }
        ///Config
        GetConfig();
        POINTERS::SetFromFile();
        OFFSETS::SetFromFile();
        OBJECT_TYPES::LoadFromFile();
        UNIVERSE::LoadFromFile();
        printf("Loaded %i systems from universe map\n", GALAXY.size());
        _DUNGEONS::LoadFromFile();
        printf("Loaded %i dungeons\n", DUNGEONS.size());

        UNIVERSE::LoadIgnoreSystems();
        UNIVERSE::LoadProspectorIgnoreSystems();
        UNIVERSE::LoadInstances();
        UNIVERSE::LoadP2PSystems();
        UNIVERSE::LoadWarp4Systems();
        UNIVERSE::LoadTranswarpSystems();
        LOCKOUTS::Load();
        ///Connection
        WSADATA wsaData;
        if(WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
                printf("WSAStartup failed\n");
                PrintLastError();
                return 5;
        }

        thread CONNECTION_INIT_THREAD;
        connection = CONNECTION();
        connection.GetConfig();
        if(CONNECT_TO_MASTER) CONNECTION_INIT_THREAD = thread(CONNECTION::init, &connection);
        printf(CC_GREEN,"==Ready\n");

        MSG hotkeyMsg = { 0 } ;
        while(1) {
                /// Check for unassigned clients
                if(clock() > NextStrayClientCheck)  GetStrayClients();
                /// Set last active client
                LastActiveWindow = GetForegroundWindow();
                /// Hotkeys
                while(PeekMessage(&hotkeyMsg, NULL, 0, 0, PM_REMOVE) != 0) {
                        if(hotkeyMsg.message == WM_HOTKEY && hotkeyMsg.wParam == 1) {
                                GetConsoleInput();
                        } else if(hotkeyMsg.message == WM_HOTKEY && hotkeyMsg.wParam == 2) {
                                APP_PAUSE = !APP_PAUSE;
                                if(APP_PAUSE) { printf("==Bot paused\n"); SetConsoleTitle("[PAUSED] SS Bot Drone"); }
                                else { printf("==Bot unpaused\n"); SetConsoleTitle("SS Bot Drone"); }
                        } else if(hotkeyMsg.message == WM_HOTKEY && hotkeyMsg.wParam == 3) {
                                /// deploy dps drones
                        } else if(hotkeyMsg.message == WM_HOTKEY && hotkeyMsg.wParam == 4) {
                                /// deploy support drones
                        } else if(hotkeyMsg.message == WM_HOTKEY && hotkeyMsg.wParam == 5) {
                                /// scoop drones
                        } else if(hotkeyMsg.message == WM_HOTKEY && hotkeyMsg.wParam == 6) {
                                HWND thisWindow = GetForegroundWindow();
                                mtx_bots.lock();
                                for(BOT* &bot : bots) {
                                        if(bot->client.clientWindow == thisWindow) {
                                                bot->PAUSE = !bot->PAUSE;
                                                if(!bot->PAUSE) {
                                                        printf("==RESUMED %s\n", bot->player.name.c_str());
                                                        SetWindowText(thisWindow, bot->player.name.c_str());
                                                } else {
                                                        printf("==PAUSED %s\n", bot->player.name.c_str());
                                                        SetWindowText(thisWindow, string("[X]"+bot->player.name).c_str());
                                                }
                                                break;
                                        }
                                }
                                mtx_bots.unlock();
                        }

                        while(PeekMessage(&hotkeyMsg, NULL, 0, 0, PM_REMOVE) != 0);
                }
                /// Connection
                // - controller messages
                {
                        auto it = ConnectionMessageQueue.begin();
                        while(it != ConnectionMessageQueue.end()) {
                                connection.Send((*it));
                                it = ConnectionMessageQueue.erase(it);
                        }
                }
                // - web server messages
                if(clock() > WebServerMessageCooldown){
                        auto it = WebServerMessageQueue.begin();
                        while(it != WebServerMessageQueue.end()) {
                                string response;
                                WebServer::POST("action=PostChatMessage&key=someChatKey638&msg="+(*it), &response);
                                if(response != "") printf("%s\n", response.c_str());
                                it = WebServerMessageQueue.erase(it);
                                WebServerMessageCooldown = clock() + 5*CLOCKS_PER_SEC;
                                break;
                        }
                }
                // - task messages
                if(clock() > NextReceivedMessageCheck) {
                        mtx_ReceivedMessageQueue.lock();
                        auto it = ReceivedMessageQueue.begin();
                        while(it != ReceivedMessageQueue.end()) {
                                if(time(0) > it->receivedAt+TASK_MESSAGE_TIMEOUT) {
                                        printf("Removing timed out task message for character %s\n", it->data["receiver"].get<string>().c_str());
                                        it = ReceivedMessageQueue.erase(it);
                                }
                                ++it;
                        }
                        mtx_ReceivedMessageQueue.unlock();
                }
                /// Lockouts
                LOCKOUTS::Check();
                Sleep(100);
        }
        return 0;
}
