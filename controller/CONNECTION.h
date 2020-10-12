#ifndef CONNECTION_H
#define CONNECTION_H
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
#include "../json/single_include/nlohmann/json.hpp"
#include "ACCOUNTS.h"
#include "../Shared code/GLOBALS.h"
#include "../Shared code/MEMORY.h"
#include "UNIVERSE.h"
using namespace std;
using json = nlohmann::json;

extern std::mutex mtx_dronesocket;
struct DRONESOCKET {
        int _socket;
        clock_t lastActive, lastOrderTime = 0;
        time_t BEGINTIME, MACHINEBEGINTIME; /// machinebegintime is received from drone, refers to process' begin time
        bool ISREADY = false; /// set to true after initial info exchange
        string machineName, ip;
        size_t clientCapacity = 5;
        list<ACCOUNT*> assignedAccounts;
        string message;

        DRONESOCKET(int p);
};

class CONNECTION {
public:
        thread THREAD;
        bool TERMINATE = false;
        clock_t DRONE_CONNECTION_TIMEOUT = 10*1000;
        int PORT = 9001;
        int MASTER_SOCKET, NEW_SOCKET, addrlen, maxClients = 30, activity, sd, max_sd;
        vector<DRONESOCKET> DRONE_SOCKETS;
        struct sockaddr_in address;
        char buffer[1024];
        fd_set readfds;
        CONNECTION();

        void GetConfig();
        int init();
        void _listen();
        void ProcessMessage(DRONESOCKET *dronesocket, string& msg);
        void UnsetTask(DRONESOCKET *dronesocket, string characterName);
        void ProcessTaskRequest(DRONESOCKET *dronesocket, json &data);
        void ProcessFail(DRONESOCKET *dronesocket, json &data);
        void CheckTimeouts();
        bool Send(DRONESOCKET* to, string msg);
        void Broadcast(string msg, DRONESOCKET* exclude = NULL);
        void OnDisconnect(DRONESOCKET *dronesocket);
};

#endif // CONNECTION_H
