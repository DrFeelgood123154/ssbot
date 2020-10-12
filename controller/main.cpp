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

#include "../Shared code/Helpers.h"
#include "../Shared code/WebServer.h"
#include "../Shared code/GLOBALS.h"
#include "CONNECTION.h"
#include "WEBCONNECTION.h"
#include "UNIVERSE.h"

using namespace std;

///Global variables

///Functions
void GetAppConfig() {
        vector<string> CONFIGFILE;
        CONFIGFILE = GetFileLines("config.txt");
        if(CONFIGFILE.size() == 0) {
                printf("\t@No config file. Get it and set the IP of web server.\n");
                getchar();
                exit(0);
        }
        ///webserver
        if(!GetLineByKey(WebServer::address, "webaddress", CONFIGFILE, "")) printf("\t@WARNING: WEB ADDRESS NOT FOUND IN CONFIG\n");
        string buffer;
        GetLineByKey(buffer, "webport", CONFIGFILE, "80");
        WebServer::port = atoi(buffer.c_str());
        WebServer::useragent = "SS Bot Master Controller";
        ///
        GetLineByKey(buffer, "prospect_system_cooldown", CONFIGFILE, "86400");
        UNIVERSE::PROSPECT_SYSTEM_COOLDOWN = atoi(buffer.c_str());
}

bool UpdateMasterAddress(CONNECTION* connection, WEBCONNECTION* interfaceconnection) {
        printf(CC_CYAN, "=Getting this machine's public IP...\n");
        if(WebServer::GetPublicIP()) printf(CC_CYAN, "=This machine's public IP: %s\n", WebServer::publicIP.c_str());
        else {
                printf(CC_RED, "\tFailed to get this machine's public address, will retry in 5 minutes\n");
                std::async(std::launch::async, [](CONNECTION* connection, WEBCONNECTION* interfaceconnection) {
                        std::this_thread::sleep_for(std::chrono::seconds(60*5));
                        UpdateMasterAddress(connection, interfaceconnection);
                }, connection, interfaceconnection);
                return false;
        }
        stringstream postData;
        postData<<"action=UpdateMasterAddress&masterPort="<<connection->PORT<<"&ip="<<WebServer::publicIP.c_str()<<"&key="<<WEBSERVERKEY;
        postData<<"&interfaceport="<<interfaceconnection->PORT;
        string receiver;
        if(!WebServer::POST(postData.str(), &receiver)) {
                printf(CC_RED, "\tError posting this machine's address and port to webserver at %s\n", WebServer::address.c_str());
                printf("\tWill retry in 5 minutes\n");
                PrintLastError();
                std::async(std::launch::async, [](CONNECTION* connection, WEBCONNECTION* interfaceconnection) {
                        std::this_thread::sleep_for(std::chrono::seconds(60*5));
                        UpdateMasterAddress(connection, interfaceconnection);
                }, connection, interfaceconnection);
                return false;
        }
        printf(CC_GREEN, "=Successfully uploaded IP and port to webserver\n");
        return true;
}

int main() {
        printf("=====SS BOT Master Controller=====\n");
        printf(CC_CYAN,"==Initializing...\n");
        SetConsoleTitle("SS Bot Master Controller");

        srand((unsigned)time(NULL));
        getcwd(APPLOCATION,MAX_PATH);
        CreateDirectory(((string)APPLOCATION+"\\data").c_str(), NULL);
        ///Config
        GetAppConfig();

        ACCOUNTS::Load();
        OFFSETS::SetFromFile();
        printf("Loaded %i offsets\n", OFFSETS::offsets.size());
        POINTERS::SetFromFile();
        printf("Loaded %i pointers\n", POINTERS::pointers.size());
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
                printf(CC_RED, "WSAStartup failed\n");
                PrintLastError();
                return 1;
        }
        CONNECTION connection = CONNECTION();
        connection.GetConfig();
        if(connection.init() != 0) {
                printf(CC_RED, "\tFailed to initialize connection\n");
                return 2;
        }

        WEBCONNECTION interfaceconnection = WEBCONNECTION();
        interfaceconnection.GetConfig();
        if(interfaceconnection.init() != 0) {
                printf(CC_RED, "\tFailed to initialize interface connection\n");
                return 2;
        }

        printf("==Updating MasterAddress...\n");
        UpdateMasterAddress(&connection, &interfaceconnection);

        while(true) {
                ///Timeout sockets
                connection.CheckTimeouts();
                ///Check app task queue
                {
                        auto task = APP_TASK_QUEUE.begin();
                        while(task != APP_TASK_QUEUE.end()) {
                                if(task->task == "BroadcastPointers") {
                                        json newOrder = {
                                                {"action", "UpdatePointers"},
                                                {"data", POINTERS::to_JSON()}
                                        };
                                        connection.Broadcast(newOrder.dump());
                                } else if(task->task == "BroadcastOffsets") {
                                        json newOrder = {
                                                {"action", "UpdateOffsets"},
                                                {"data", OFFSETS::to_JSON()}
                                        };
                                        connection.Broadcast(newOrder.dump());
                                } else if(task->task == "BroadcastObjectTypes") {
                                        json newOrder = {
                                                {"action", "UpdateObjectTypes"},
                                                {"data", OFFSETS::to_JSON()}
                                        };
                                        connection.Broadcast(newOrder.dump());
                                } else if(task->task == "RelayMessage") {
                                        connection.Broadcast(task->data);
                                } else { printf(CC_YELLOW, "UNKNOWN APP TASK: %s\n", task->task.c_str()); }
                                task = APP_TASK_QUEUE.erase(task);
                        }
                }
                ///Auto orders
                for(ACCOUNT& acc : accounts) {
                        if(acc.TASK == TASK_NONE && acc.canRunAuto) {
                                bool assigned = false;
                                for(CHARACTER& character : acc.characters) {
                                        if(!character.canDG || !character.canRunAuto) continue;
                                        json newOrder;
                                        newOrder["action"] = "AssignAccount";
                                        newOrder["task"] = TASK_DG;
                                        newOrder["account"] = acc.toJSON();
                                        newOrder["account"].erase("characters");
                                        newOrder["character"] = character.toJSON();
                                        string msg = newOrder.dump();
                                        for(DRONESOCKET& drone : connection.DRONE_SOCKETS) {
                                                if(!drone.ISREADY) continue;
                                                if(drone.assignedAccounts.size() < drone.clientCapacity && connection.Send(&drone, msg)) {
                                                        drone.assignedAccounts.push_back(&acc);
                                                        acc.TASK = TASK_DG;
                                                        assigned = true;
                                                        printf("Assigned account '%s'(%s) to drone %s with task DG\n", acc.name.c_str(), character.name.c_str(), drone.machineName.c_str());
                                                        break;
                                                }
                                        }
                                        if(assigned) continue;
                                }
                                if(assigned) continue;
                        }
                }
                /// Lockouts
                LOCKOUTS::Check();
                Sleep(100);
        }
        return 0;
}
