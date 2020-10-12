#include "CONNECTION.h"

std::mutex mtx_dronesocket;

DRONESOCKET::DRONESOCKET(int p) {
        _socket = p;
        lastActive = clock();
        BEGINTIME = time(0);
}

CONNECTION::CONNECTION() {

}

void CONNECTION::GetConfig() {
        vector<string> CONFIGFILE;
        CONFIGFILE = GetFileLines("config.txt");
        string buffer;
        if(GetLineByKey(buffer, "masterport", CONFIGFILE, "")) PORT = atoi(buffer.c_str());
        if(GetLineByKey(buffer, "drone_connection_timeout", CONFIGFILE, "")) DRONE_CONNECTION_TIMEOUT = atoi(buffer.c_str())*1000;
}

int CONNECTION::init() {
        printf("==Setting up listener...\n");
        if((MASTER_SOCKET = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
                printf("\tFailed to create master socket, exiting app\n");
                PrintLastError();
                return 1;
        }
        int opt = true;
        if(setsockopt(MASTER_SOCKET, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0) {
                printf("\tFailed to set master socket options, exiting app\n");
                PrintLastError();
                return 2;
        }
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(PORT);
        if(bind(MASTER_SOCKET, (struct sockaddr*)&address, sizeof(address))<0) {
                printf("\tFailed to bind socket to port %i, exiting app\n", PORT);
                PrintLastError();
                return 3;
        }
        printf("=Listener set on port %i\n", PORT);
        if(listen(MASTER_SOCKET, 5) < 0) {
                printf("\tListener error, exiting app\n");
                PrintLastError();
                return 4;
        }
        addrlen = sizeof(address);

        THREAD = thread(CONNECTION::_listen, this);
        return 0;
}

void CONNECTION::_listen() {
        srand((unsigned)time(NULL)+std::random_device()());
        while(!TERMINATE) {
                FD_ZERO(&readfds);
                FD_SET(MASTER_SOCKET, &readfds);
                max_sd = MASTER_SOCKET;
                for(DRONESOCKET &sock : DRONE_SOCKETS) {
                        sd = sock._socket;
                        if(sd>0) FD_SET(sd, &readfds);
                        if(sd>max_sd) max_sd = sd;
                }
                activity = select(max_sd+1, &readfds, NULL, NULL, NULL);
                if((activity < 0) && (errno!=EINTR)) {
                        printf("\tSelect error\n");
                        PrintLastError();
                        return;
                }

                ///New connection
                if(FD_ISSET(MASTER_SOCKET, &readfds)) {
                        if((NEW_SOCKET = accept(MASTER_SOCKET, (struct sockaddr *)&address, (int*)&addrlen))<0) {
                                printf("\tError accepting new socket\n");
                                PrintLastError();
                                return;
                        }
                        printf("New connection, socket fd: %d, ip: %s, port: %d\n", NEW_SOCKET, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                        unsigned long mode = false;
                        ioctlsocket(NEW_SOCKET, FIONBIO, &mode);
                        mtx_dronesocket.lock();
                        DRONE_SOCKETS.push_back(DRONESOCKET(NEW_SOCKET));
                        DRONE_SOCKETS.back().ip = inet_ntoa(address.sin_addr);
                        mtx_dronesocket.unlock();
                }
                ///IO from existing connection
                auto it = DRONE_SOCKETS.begin();
                while(it != DRONE_SOCKETS.end()) {
                        sd = it->_socket;
                        if(FD_ISSET(sd, &readfds)) {
                                int bread = recv(sd, buffer, 1024, 0);
                                ///disconnected
                                if(bread == 0) {
                                        getpeername(sd, (struct sockaddr*)&address, (int*)&addrlen);
                                        printf("Disconnected, ip %s, port %d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                                        OnDisconnect(&(*it));
                                        close(sd);
                                        it = DRONE_SOCKETS.erase(it);
                                        continue;
                                }
                                ///message
                                else if(bread != -1) {
                                        it->lastActive = clock();
                                        if(bread != 1+strlen(MSG_END_DELIM)) {
                                                printf("!New data from %s(%s)\n", it->ip.c_str(), it->machineName.c_str());
                                                buffer[bread] = '\0';
                                                string receivedPart = (string)buffer;
                                                size_t i = 0;
                                                do {
                                                        i = receivedPart.find(MSG_END_DELIM);
                                                        if(i == string::npos) it->message += receivedPart;
                                                        else {
                                                                it->message += receivedPart.substr(0, i);
                                                                ProcessMessage(&(*it), it->message);
                                                                it->message = "";
                                                                i += strlen(MSG_END_DELIM);
                                                                receivedPart = receivedPart.substr(i);
                                                        }
                                                } while(i != string::npos);
                                        }
                                } else {
                                        printf("Connection lost\n");
                                        OnDisconnect(&(*it));
                                        close(sd);
                                        it = DRONE_SOCKETS.erase(it);
                                        continue;
                                }
                        }
                        ++it;
                }
                Sleep(1);
        }
}

void CONNECTION::ProcessMessage(DRONESOCKET *dronesocket, string& msg) {
        printf("%s: %s\n", dronesocket->ip.c_str(), msg.c_str());
        json msgData;
        try {
                msgData = json::parse(msg.c_str());
                if(msgData["action"] == "RequestTaskUpdate") ProcessTaskRequest(dronesocket, msgData);
                else if(msgData["action"] == "Fail") ProcessFail(dronesocket, msgData);
                else if(msgData["action"] == "ExchangeInfo") {
                        dronesocket->MACHINEBEGINTIME = msgData["PROCESSBEGINTIME"];
                        dronesocket->machineName = msgData["machineName"];
                        if(dronesocket->machineName == "") {
                                dronesocket->machineName = random_string(15);
                                printf("Generated new name for machine %s: %s\n", dronesocket->ip.c_str(), dronesocket->machineName.c_str());
                                json response = {
                                        {"action", "UpdateMachineName"},
                                        {"machineName", dronesocket->machineName.c_str()},
                                };
                                Send(dronesocket, response.dump());
                        }
                        printf(CC_BLUE, "Syncing data with %s...\n", dronesocket->machineName.c_str());
                        json newOrder = {
                                {"action", "UpdatePointers"},
                                {"data", POINTERS::to_JSON()}
                        };
                        Send(dronesocket, newOrder.dump());
                        newOrder = {
                                {"action", "UpdateOffsets"},
                                {"data", OFFSETS::to_JSON()}
                        };
                        Send(dronesocket, newOrder.dump());
                        newOrder = {
                                {"action", "UpdateObjectTypes"},
                                {"data", OBJECT_TYPES::to_JSON()}
                        };
                        Send(dronesocket, newOrder.dump());
                        newOrder = {
                                {"action", "RequestSystemData"}
                        };
                        /// -- see which aren't on controller's data
                        for(auto& el : msgData["UNIMAP"]) {
                                UINT thisID = (el.is_string())?stoi(el.get<string>(), NULL):el.get<int>();
                                bool inList = false;
                                for(SYSTEM& sys : GALAXY) if(sys.id == thisID) { inList = true; break; }
                                if(inList) continue;
                                newOrder["data"].push_back(thisID);
                        }
                        if(newOrder["data"].size() > 0){
                                Send(dronesocket, newOrder.dump());
                                printf("Requested %i new systems data from drone %s\n", newOrder["data"].size(), dronesocket->machineName.c_str());
                        }
                        newOrder = {
                                {"action", "RequestDungeonData"}
                        };
                        for(auto& el : msgData["DGMAP"]) {
                                bool inList = false;
                                for(DUNGEON& dg : DUNGEONS) if(dg.name == el) { inList = true; break; }
                                if(inList) continue;
                                newOrder["data"].push_back(el);
                        }
                        if(newOrder["data"].size() > 0){
                                Send(dronesocket, newOrder.dump());
                                printf("Requested %i new dungeons data from drone %s\n", newOrder["data"].size(), dronesocket->machineName.c_str());
                        }
                        /// -- see which aren't on drone's data
                        newOrder = {
                                {"action", "UpdateUniverseMap"}
                        };
                        for(SYSTEM &sys : GALAXY) {
                                bool isListed = false;
                                for(auto& el : msgData["UNIMAP"]) if((el.is_string())?(UINT)stoi(el.get<string>(), NULL):el.get<UINT>() == sys.id) { isListed = true; break; }
                                if(isListed) continue;
                                newOrder["data"].push_back(sys.to_JSON());
                        }
                        if(newOrder["data"].size() > 0) Send(dronesocket, newOrder.dump());
                        newOrder = {
                                {"action", "UpdateDungeonsMap"}
                        };
                        for(DUNGEON &dg : DUNGEONS) {
                                bool isListed = false;
                                for(auto& el : msgData["DGMAP"]) if(el == dg.name) { isListed = true; break; }
                                if(isListed) continue;
                                newOrder["data"].push_back(dg.to_JSON());
                        }
                        if(newOrder["data"].size() > 0) Send(dronesocket, newOrder.dump());
                        printf(CC_GREEN, "Finished syncing data with %s\n", dronesocket->machineName.c_str());
                        dronesocket->ISREADY = true;
                }else if(msgData["action"] == "UpdateSystems"){
                        printf("Received %i new systems data from %s\n", msgData["data"].size(), dronesocket->machineName.c_str());
                        UNIVERSE::AddSystems(msgData["data"]);
                        UNIVERSE::Save();
                }else if(msgData["action"] == "UpdateDungeons"){
                        printf("Received %i new dungeons data from %s\n", msgData["data"].size(), dronesocket->machineName.c_str());
                        for(auto& el : msgData["data"]) _DUNGEONS::AddDungeon(el);
                        _DUNGEONS::Save();
                }else if(msgData["action"] == "AddSystem"){
                        printf("Received new system data %s from %s\n", msgData["data"]["name"].get<string>().c_str(), dronesocket->machineName.c_str());
                        bool success = UNIVERSE::AddSystem(msgData["data"]);
                        UNIVERSE::Save();
                        if(success){
                                json newMsg{
                                        {"action", "AddSystem"},
                                        {"data", msgData["data"]}
                                };
                                Broadcast(newMsg.dump(), dronesocket);
                        }
                }else if(msgData["action"] == "AddDungeon"){
                        printf("Received new dungeon data %s from %s\n", msgData["name"].get<string>().c_str(), dronesocket->machineName.c_str());
                        bool success = _DUNGEONS::AddDungeon(msgData["data"]);
                        _DUNGEONS::Save();
                        if(success){
                                json newMsg{
                                        {"action", "AddDungeon"},
                                        {"data", msgData["data"]}
                                };
                                Broadcast(newMsg.dump(), dronesocket);
                        }
                }else if(msgData["action"] == "AddP2PSystem"){
                        UNIVERSE::AddP2PSystem(msgData["name"]);
                        UNIVERSE::SaveLists();
                        Broadcast(msgData.dump(), dronesocket);
                }else if(msgData["action"] == "AddWarp4System"){
                        UNIVERSE::AddWarp4System(msgData["name"]);
                        UNIVERSE::SaveLists();
                        Broadcast(msgData.dump(), dronesocket);
                }else if(msgData["action"] == "DGCompleted"){
                        for(DUNGEON &dungeon : DUNGEONS){
                                if(dungeon.name == msgData["target"]){
                                        dungeon.lastCleared = time(0);
                                        _DUNGEONS::Save();
                                        printf("Drone %s has completed %s\n", dronesocket->machineName.c_str(), msgData["target"].get<string>().c_str());
                                        LOCKOUTS::Add(msgData["character"], msgData["target"], msgData["lockoutTime"]);
                                        break;
                                }
                        }
                }
        } catch(const exception &e) {
                printf("%s\n", e.what());
        };
}
void CONNECTION::UnsetTask(DRONESOCKET *dronesocket, string characterName){
        json response{
                {"action", "TaskUpdate"},
                {"receiver", characterName},
                {"setTask", "NONE"}
        };
        Send(dronesocket, response.dump());
}
void CONNECTION::ProcessTaskRequest(DRONESOCKET *dronesocket, json &data){
        try{
                if(data["task"] == AI_TASK::TASK_EXPLORE){
                        string location = data["location"];
                        pair<SYSTEM*, SYSTEM*> newTarget = UNIVERSE::FindNearestUnexplored(location, data["maxWarp"]);
                        if(newTarget.first == NULL){
                                UnsetTask(dronesocket, data["character"]);
                                printf(CC_PINK, "No available unexplored systems found, unsetting task for %s\n", data["character"].get<string>().c_str());
                        }else{
                                string adjacentSystem = "";
                                if(newTarget.second != NULL) adjacentSystem = newTarget.second->name;
                                json response{
                                        {"action", "TaskUpdate"},
                                        {"receiver", data["character"]},
                                        {"type", "NewExploreTarget"},
                                        {"target", newTarget.first->name}
                                };
                                newTarget.first->timeTakenByExplorer = time(0);
                                Send(dronesocket, response.dump());
                        }
                }else if(data["task"] == AI_TASK::TASK_DG){
                        vector<string> layers;
                        for(auto &el : data["layers"]) layers.push_back(el.get<string>());
                        DUNGEON *newTarget = UNIVERSE::FindNearestAvailableDG(data["location"].get<string>(), data["minDF"].get<double>(),
                                                                        data["maxDF"].get<double>(), layers, NULL, data["character"].get<string>());
                        if(newTarget == nullptr){
                                printf(CC_PINK, "No available dungeons left for %s\n", data["character"].get<string>().c_str());
                        }else{
                                json response{
                                        {"action", "TaskUpdate"},
                                        {"receiver", data["character"]},
                                        {"type", "NewDGTarget"},
                                        {"target", newTarget->name}
                                };
                                newTarget->lastTaken = time(0);
                                Send(dronesocket, response.dump());
                        }
                }
        }catch(const exception &e){ printf(CC_YELLOW, "Error at ProcessTaskRequest from drone %s\n", dronesocket->machineName.c_str()) ; }
}
void CONNECTION::ProcessFail(DRONESOCKET *dronesocket, json &data){
        try{
                if(data["task"] == "EXPLORE"){
                        printf(CC_YELLOW, "%s from drone %s failed to explore %s, adding it to ignored systems\n", data["character"], dronesocket->machineName.c_str(), data["target"]);
                        string failedName = data["target"];
                        UNIVERSE::ignoreSystems.push_back(failedName);
                }
        }catch(const exception &e){ printf(CC_YELLOW, "Error at ProcessTaskFail from drone %s\n", dronesocket->machineName.c_str()) ; }
}


void CONNECTION::CheckTimeouts() {
        auto it = DRONE_SOCKETS.begin();
        while(it != DRONE_SOCKETS.end()) {
                if(clock() > it->lastActive + DRONE_CONNECTION_TIMEOUT) {
                        getpeername(it->_socket, (struct sockaddr*)&address, (int*)&addrlen);
                        printf("Connection timed out, fd %d, ip %s, port %d\n", it->_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                        OnDisconnect(&(*it));
                        close(it->_socket);
                        it = DRONE_SOCKETS.erase(it);
                        continue;
                }
                ++it;
        }
}

bool CONNECTION::Send(DRONESOCKET* to, string msg) {
        msg += MSG_END_DELIM; /// indicated message end
        int result = send(to->_socket, msg.c_str(), msg.length(), 0);
        if(result == SOCKET_ERROR) {
                printf(CC_RED, "\t@Error sending message:\n");
                PrintLastError();
                return false;
        }
        return true;
}

void CONNECTION::Broadcast(string msg, DRONESOCKET* exclude) {
        for(DRONESOCKET &i : DRONE_SOCKETS) if(&i != exclude) Send(&i, msg);
}

void CONNECTION::OnDisconnect(DRONESOCKET* dronesocket) {
        for(ACCOUNT* acc : dronesocket->assignedAccounts) if(acc != NULL) acc->TASK = TASK_NONE;
}
