#include "WEBCONNECTION.h"

WEBCONNECTION::WEBCONNECTION() {
        //ctor
}

void WEBCONNECTION::GetConfig() {
        vector<string> CONFIGFILE;
        CONFIGFILE = GetFileLines("config.txt");
        string buffer;
        if(GetLineByKey(buffer, "interfaceport", CONFIGFILE, "")) PORT = atoi(buffer.c_str());
}

int WEBCONNECTION::init() {
        printf("==Setting up interface listener...\n");
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
        printf("=Interface listener set on port %i\n", PORT);
        if(listen(MASTER_SOCKET, 5) < 0) {
                printf("\tListener error, exiting app\n");
                PrintLastError();
                return 4;
        }
        addrlen = sizeof(address);

        THREAD = thread(_listen, this);
        return 0;
}

void WEBCONNECTION::_listen() {
        srand((unsigned)time(NULL)+std::random_device()());
        while(!TERMINATE) {
                FD_ZERO(&readfds);
                FD_SET(MASTER_SOCKET, &readfds);
                max_sd = MASTER_SOCKET;
                for(WEBSOCKET &sock : WEB_SOCKETS) {
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
                        printf("New interface connection, socket fd: %d, ip: %s, port: %d\n", NEW_SOCKET, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                        unsigned long mode = false;
                        ioctlsocket(NEW_SOCKET, FIONBIO, &mode);
                        WEB_SOCKETS.push_back(WEBSOCKET(NEW_SOCKET));
                        WEB_SOCKETS.back().ip = inet_ntoa(address.sin_addr);
                }
                ///IO from existing connection
                auto it = WEB_SOCKETS.begin();
                while(it != WEB_SOCKETS.end()) {
                        sd = it->_socket;
                        if(FD_ISSET(sd, &readfds)) {
                                int bread = recv(sd, buffer, 10240, 0);
                                ///disconnected
                                if(bread == 0) {
                                        getpeername(sd, (struct sockaddr*)&address, (int*)&addrlen);
                                        printf("Interface disconnected, ip %s, port %d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                                        close(sd);
                                        it = WEB_SOCKETS.erase(it);
                                        continue;
                                }
                                ///message
                                else if(bread != -1) {
                                        buffer[bread] = '\0';
                                        ProcessMessage(&(*it), buffer, bread);
                                } else {
                                        printf("Connection lost\n");
                                        close(sd);
                                        it = WEB_SOCKETS.erase(it);
                                        continue;
                                }
                        }
                        ++it;
                }
                Sleep(1);
        }
}

void WEBCONNECTION::ProcessMessage(WEBSOCKET* websocket, string msg, int bytes) {
        printf("%s: %s\n", websocket->ip.c_str(), msg.c_str());
        try {
                json msgData = json::parse(msg.c_str());
                if(msgData["action"] == "AddAccount") {
                        printf("Adding new account: %s\n", msgData["name"]);
                        bool isListed = false;
                        for(ACCOUNT& acc : accounts) if(acc.name == msgData["name"]) { isListed = true; break; }
                        if(isListed) {
                                Send(websocket, "Account is already on the list");
                                return;
                        }
                        ACCOUNTS::Add(msgData["name"], msgData["password"]);
                        Send(websocket, "Account added");
                        return;
                } if(msgData["action"] == "AddCharacter") {
                        printf("Adding new character %s to %s\n", msgData["characterName"], msgData["accountName"]);
                        bool isListed = false;
                        for(ACCOUNT& acc : accounts) for(CHARACTER& character : acc.characters) if(character.name == msgData["characterName"]) { isListed = true; break; }
                        if(isListed) {
                                Send(websocket, "Character is already on the list");
                                return;
                        }
                        if(!ACCOUNTS::AddCharacter(msgData["accountName"], msgData["characterName"])) {
                                Send(websocket, "Error adding new character");
                                return;
                        }
                        Send(websocket, "Character added");
                        return;
                } else if(msgData["action"] == "RequestAccountsList") {
                        std::ifstream file("data/accounts.txt");
                        std::string strdata((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                        Send(websocket, strdata);
                        return;
                } else if(msgData["action"] == "RequestPointers") {
                        std::ifstream file("data/pointers.txt");
                        std::string strdata((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                        Send(websocket, strdata);
                        return;
                } else if(msgData["action"] == "RequestOffsets") {
                        std::ifstream file("data/offsets.txt");
                        std::string strdata((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                        Send(websocket, strdata);
                        return;
                } else if(msgData["action"] == "RequestObjectTypes") {
                        std::ifstream file("data/objecttypes.txt");
                        std::string strdata((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                        Send(websocket, strdata);
                        return;
                } else if(msgData["action"] == "UpdatePointers") {
                        printf("Updating pointers list\n");
                        if(!POINTERS::Set(msgData["data"])) {
                                Send(websocket, "Failed to update pointers\n");
                                POINTERS::SetFromFile();
                        } else {
                                Send(websocket, "Successfully updated pointers\n");
                                POINTERS::Save();
                                APP_TASK newTask("BroadcastPointers");
                                APP_TASK_QUEUE.push_back(newTask);
                        }
                        return;
                } else if(msgData["action"] == "UpdateOffsets") {
                        printf("Updating offsets list\n");
                        if(!OFFSETS::Set(msgData["data"])) {
                                Send(websocket, "Failed to update offsets\n");
                                OFFSETS::SetFromFile();
                        } else {
                                Send(websocket, "Successfully updated offsets\n");
                                OFFSETS::Save();
                                APP_TASK newTask("BroadcastOffsets");
                                APP_TASK_QUEUE.push_back(newTask);
                        }
                        return;
                } else if(msgData["action"] == "UpdateObjectTypes") {
                        printf("Updating object types\n");
                        if(!OBJECT_TYPES::Load(msgData["data"])) {
                                Send(websocket, "Failed to update object types\n");
                                OBJECT_TYPES::LoadFromFile();
                        } else {
                                Send(websocket, "Successfully updated object types\n");
                                OBJECT_TYPES::Save();
                                APP_TASK newTask("BroadcastObjectTypes");
                                APP_TASK_QUEUE.push_back(newTask);
                        }
                        return;
                }
        } catch(const exception &e) {
                printf("%s\n", e.what());
        };
        Send(websocket, "K");
}

bool WEBCONNECTION::Send(WEBSOCKET* to, string msg) {
        int result = send(to->_socket, msg.c_str(), msg.length(), 0);
        if(result == SOCKET_ERROR) {
                printf(CC_RED, "\t@Error sending message:\n");
                PrintLastError();
                return false;
        }
        return true;
}
