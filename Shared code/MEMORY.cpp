#include "MEMORY.h"

std::mutex OFFSETS::mtx;
list<OFFSETS::Offset> OFFSETS::offsets;
std::mutex POINTERS::mtx;
list<POINTERS::Pointer> POINTERS::pointers;

bool OFFSETS::ok = false;
DWORD OFFSETS::EntityID,OFFSETS::EntityName, OFFSETS::EntityPosX, OFFSETS::EntityPosY, OFFSETS::EntityPosX2, OFFSETS::EntityPosY2;
DWORD OFFSETS::EntityLevel, OFFSETS::HullUsed, OFFSETS::HullMax, OFFSETS::EntityShield, OFFSETS::EntityShieldRegen, OFFSETS::EntityEnergy;
DWORD OFFSETS::EntityEnergyRegen, OFFSETS::EntityEnergyMax, OFFSETS::EntityShieldMax, OFFSETS::EntityShipName, OFFSETS::EntityShipType;
DWORD OFFSETS::EntityTeamName, OFFSETS::EntityRotation, OFFSETS::EntityRotation2, OFFSETS::EntityDroneLevel, OFFSETS::EntityOwner;
DWORD OFFSETS::EntityIsTractored, OFFSETS::EntityStasisType, OFFSETS::Inventory, OFFSETS::IsPlayer, OFFSETS::IsFriend, OFFSETS::EntitySize;
DWORD OFFSETS::Keyboard_S, OFFSETS::Keyboard_W, OFFSETS::Keyboard_Shoot, OFFSETS::Keyboard_F, OFFSETS::IsSquadMember;
DWORD OFFSETS::Keyboard_1, OFFSETS::Keyboard_X, OFFSETS::Keyboard_G, OFFSETS::Keyboard_C, OFFSETS::EntitySpeed, OFFSETS::EntitySpeedMax;
DWORD OFFSETS::EntityMoveVectorX, OFFSETS::Keyboard_R, OFFSETS::EntityIsVisible, OFFSETS::Keyboard_K;

DWORD OFFSETS::Item_Name, OFFSETS::Item_Slot, OFFSETS::Item_Type, OFFSETS::Item_Tech, OFFSETS::Item_Weight;
DWORD OFFSETS::Item_Size, OFFSETS::Item_Rarity, OFFSETS::Item_ID, OFFSETS::Item_ID2, OFFSETS::Item_Mods, OFFSETS::Item_Equipped, OFFSETS::Item_Quantity;
DWORD OFFSETS::Item_MaxCharge, OFFSETS::Item_CurrentCharge, OFFSETS::Item_Durability;

DWORD OFFSETS::Get(const string name) { for(auto& offset : offsets) if(offset.name == name) return offset.offset; return 0; }
bool OFFSETS::Set(json data) {
        mtx.lock();
        offsets.clear();
        try {
                for(auto& el : data) {
                        if(el["offset"].is_string()) offsets.push_back(Offset(el["name"], strtoul(el["offset"].get<string>().c_str(), NULL, 16)));
                        else offsets.push_back(Offset(el["name"], el["offset"]));
                }
        } catch(const exception &e) { printf("%s\n", e.what()); mtx.unlock(); return false; }

        EntityID = Get("entity_id");
        EntityName = Get("entity_name");
        EntityPosX = Get("entity_posx");
        EntityPosX2 = Get("entity_posx2");
        EntityPosY = Get("entity_posy");
        EntityPosY2 = Get("entity_posy2");
        EntityLevel = Get("entity_level");
        HullUsed = Get("hull_used");
        HullMax = Get("hull_max");
        EntityShield = Get("entity_shield");
        EntityShieldRegen = Get("entity_shieldregen");
        EntityShieldMax = Get("entity_shieldmax");
        EntityEnergy = Get("entity_energy");
        EntityEnergyRegen = Get("entity_energyregen");
        EntityEnergyMax = Get("entity_energymax");
        EntityShipName = Get("entity_shipname");
        EntityShipType = Get("entity_shiptype");
        EntityTeamName = Get("entity_teamname");
        EntityRotation = Get("entity_rotation");
        EntityRotation2 = Get("entity_rotation2");
        EntityDroneLevel = Get("entity_dronelevel");
        EntityOwner = Get("entity_owner");
        EntityIsTractored = Get("entity_istractored");
        EntityStasisType = Get("entity_stasistype");
        EntitySize = Get("entity_size");
        EntitySpeed = Get("entity_speed");
        EntitySpeedMax = Get("entity_speedmax");
        EntityMoveVectorX = Get("entity_move_vector_x");
        EntityIsVisible = Get("entity_isvisible");
        Inventory = Get("inventory");
        IsPlayer = Get("isplayer");
        IsFriend = Get("isfriend");
        IsSquadMember = Get("is_squad_member");
        Keyboard_S = Get("keyboard_s");
        Keyboard_W = Get("keyboard_w");
        Keyboard_Shoot = Get("keyboard_shoot");
        Keyboard_F = Get("keyboard_f");
        Keyboard_1 = Get("keyboard_1");
        Keyboard_X = Get("keyboard_x");
        Keyboard_G = Get("keyboard_g");
        Keyboard_C = Get("keyboard_c");
        Keyboard_R = Get("keyboard_r");
        Keyboard_K = Get("keyboard_k");
        Item_Name = Get("item_name");
        Item_Slot = Get("item_slot");
        Item_Type = Get("item_type");
        Item_Tech = Get("item_tech");
        Item_Weight = Get("item_weight");
        Item_Size = Get("item_size");
        Item_Rarity = Get("item_rarity");
        Item_ID = Get("item_id");
        Item_ID2 = Get("item_id2");
        Item_Mods = Get("item_mods");
        Item_Equipped = Get("item_equipped");
        Item_Quantity = Get("item_quantity");
        Item_MaxCharge = Get("item_maxcharge");
        Item_CurrentCharge = Get("item_currentcharge");
        Item_Durability = Get("item_durability");
        mtx.unlock();
        ok = true;
        return true;
}
void OFFSETS::SetFromFile() {
        std::ifstream file("data/offsets.txt");
        std::string strdata((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if(strdata.length() == 0) { printf(CC_RED, "Offsets file not found\n"); return; }
        json data;
        try {
                data = json::parse(strdata.c_str());
                if(!Set(data)) printf(CC_RED, "Error loading offsets from text file\n");
        } catch(const exception &e) { printf(CC_RED, "Error loading offsets from text file: %s\n", e.what()); }
}
json OFFSETS::to_JSON() {
        json data;
        size_t i = 0;
        for(Offset& offset : offsets) { data[i]["name"] = offset.name; data[i]["offset"] = offset.offset; i++; }
        return data;
}
void OFFSETS::Save() {
        ofstream file("data/offsets.txt");
        file<<to_JSON().dump(2);
}

POINTERS::Pointer* POINTERS::Get(const string name) { for(auto& pointer : pointers) if(pointer.name == name) return &pointer; return NULL; }
bool POINTERS::Set(json data) {
        mtx.lock();
        pointers.clear();
        try {
                for(auto& el : data) {
                        Pointer newPtr;
                        newPtr.name = el["name"];
                        for(size_t i = 0; i<el["path"].size(); i++) {
                                for(auto& el2 : el["path"].items()) {
                                        if(el2.key() != to_string(i)) continue;
                                        if(el2.value().is_string()) newPtr.path.push_back(strtoul(el2.value().get<string>().c_str(), NULL, 16));
                                        else newPtr.path.push_back(el2.value());
                                        break;
                                }
                        }
                        pointers.push_back(newPtr);
                }
        } catch(const exception &e) { printf("%s\n", e.what()); return false; }
        mtx.unlock();
        return true;
}
void POINTERS::SetFromFile() {
        std::ifstream file("data/pointers.txt");
        std::string strdata((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if(strdata.length() == 0) { printf(CC_RED, "Pointers file not found\n"); return; }
        json data;
        try {
                data = json::parse(strdata.c_str());
                if(!Set(data)) printf(CC_RED, "Error loading pointers from text file\n");
        } catch(const exception &e) { printf(CC_RED, "Error loading pointers from text file: %s\n", e.what()); }
}
json POINTERS::to_JSON() {
        json data;
        size_t i = 0;
        for(Pointer& pointer : pointers) {
                data[i]["name"] = pointer.name;
                for(DWORD& step : pointer.path) data[i]["path"].push_back(step);
                i++;
        }
        return data;
}
void POINTERS::Save() {
        ofstream file("data/pointers.txt");
        file<<to_JSON().dump(2);
}

MEMORY::MEMORY() {
        //ctor
}

void MEMORY::SetHandle(UINT PID) {
        if(processHandle != NULL) { printf("Closing handle %X\n", processHandle); CloseHandle(processHandle); processHandle = NULL; }
        processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, PID);
        if(processHandle == NULL){
                printf("Failed to open handle for PID %i\n", PID);
                ok = false;
        }
        GetModuleBaseAddress(PID);
}

void MEMORY::GetModuleBaseAddress(UINT PID) {
        const char* moduleName = "Star Sonata.exe";
        MODULEENTRY32 module32;
        module32.dwSize = sizeof(MODULEENTRY32);
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, PID);
        Module32First(hSnap, &module32);
        do {
                if(!strcmp(module32.szModule, moduleName)) {
                        moduleBase = (DWORD64)module32.modBaseAddr;
                        break;
                }
        } while(Module32Next(hSnap, &module32));
        CloseHandle(hSnap);
}

void MEMORY::Update() {
        ok = true;
        POINTERS::mtx.lock();
        Calculate("client_resolution_x", ClientResolutionX);
        Calculate("player_name", PlayerName);
        Calculate("player_data", PlayerData);
        Calculate("credits", Credits);
        Calculate("logged_in_account", LoggedInAccount);
        Calculate("player_speed", PlayerSpeed);
        Calculate("player_location", PlayerLocation);
        Calculate("universe_layer", UniverseLayer);
        Calculate("is_logged_in", IsLoggedIn);
        Calculate("is_char_selected", IsCharSelected);
        Calculate("is_docked", IsDocked);
        Calculate("visibility_modifier", VisibilityModifier);
        Calculate("obj_list_begin", ObjListBegin);
        Calculate("selected_target_id", SelectedTargetID);
        Calculate("autopilot_state", AutopilotState);
        Calculate("autopilot_on", AutopilotOn);
        Calculate("radar_on", RadarOn);
        Calculate("selected_weapon_id", SelectedWeaponID);
        Calculate("selected_weapon_range", SelectedWeaponRange);
        Calculate("is_shooting", IsShooting);
        Calculate("keyboard", Keyboard);
        Calculate("stop_ship", StopShip);
        Calculate("autopilot_target_posx", AutopilotTargetPosX);
        Calculate("autopilot_target_posy", AutopilotTargetPosY);
        Calculate("autopilot_jumps", AutopilotJumps);
        Calculate("autopilot_destination", AutopilotDestination);
        Calculate("thrust_state", ThrustState);
        Calculate("danger_factor", DangerFactor);
        Calculate("chat_messages_begin", ChatMessagesBegin);
        Calculate("chat_input", ChatInput);
        Calculate("is_chat_open", IsChatOpen);
        Calculate("chat_toggle", ChatToggle);
        Calculate("chat_tab_list", ChatTabList);
        Calculate("chat_target_tab_id", ChatTargetTabID);
        Calculate("chat_input_structure", ChatInputStructure);
        Calculate("hotkey_1_item", Hotkey1Item);
        Calculate("inventory_window", InventoryWindow);
        Calculate("inventory_item_options", InventoryItemOptions);
        Calculate("station_window", StationWindow);
        POINTERS::mtx.unlock();
}

/// This code is mostly copied from tool4
void MEMORY::Calculate(const string ptrName, DWORD& receiver, bool addLast) {
        POINTERS::Pointer* pointerPath = POINTERS::Get(ptrName);
        if(pointerPath == NULL) {
                printf(CC_RED, "Failed to find pointer %s in the pointers list\n", ptrName.c_str());
                ok = false;
                return;
        }
        if(pointerPath->path.size() == 0) {
                printf(CC_RED, "Error reading pointer %s: empty path\n", pointerPath->name.c_str());
                ok = false;
                return;
        }
        receiver = moduleBase + pointerPath->path[0];
        if(pointerPath->path.size() == 1) return;
        ReadProcessMemory(processHandle, LongToPtr(receiver), &receiver, 4, NULL);

        for(size_t i=1; i<pointerPath->path.size(); i++) {
                receiver = receiver+pointerPath->path[i];
                if(addLast && pointerPath->path.size() == 2) break;
                ReadProcessMemory(processHandle, LongToPtr(receiver), &receiver, 4, NULL);
                if(receiver <= 0x10000) {
                        printf(CC_RED, "Error reading pointer %s: path returned zero at step %i\n", pointerPath->name.c_str(), i);
                        ok = false;
                        return;
                }
                if(addLast && i==pointerPath->path.size()-2) {
                        receiver += pointerPath->path.back();
                        break;
                }
        }
}
bool MEMORY::Calculate2(const string ptrName, DWORD& receiver, bool addLast) {
        if(processHandle == NULL) return false;
        POINTERS::Pointer* pointerPath = POINTERS::Get(ptrName);
        if(pointerPath == NULL) return false;
        if(pointerPath->path.size() == 0)  return false;
        receiver = moduleBase + pointerPath->path[0];
        if(pointerPath->path.size() == 1) return false;
        ReadProcessMemory(processHandle, LongToPtr(receiver), &receiver, 4, NULL);

        for(size_t i=1; i<pointerPath->path.size(); i++) {
                receiver = receiver+pointerPath->path[i];
                if(addLast && pointerPath->path.size() == 2) break;
                ReadProcessMemory(processHandle, LongToPtr(receiver), &receiver, 4, NULL);
                if(receiver <= 0x10000) return false;
                if(addLast && i==pointerPath->path.size()-2) {
                        receiver += pointerPath->path.back();
                        break;
                }
        }
        return true;
}

string MEMORY::ReadString(DWORD address) {
        if(address == 0 || processHandle == NULL) return "";
        size_t length;
        ReadProcessMemory(processHandle, LongToPtr(address+0x14), &length, 4, NULL);
        if(length <= 1) return "";
        if(length > 500) length = 500;
        char ret[500];
        if(length >= 16) ReadProcessMemory(processHandle, LongToPtr(address), &address, 4, NULL);
        ReadProcessMemory(processHandle, LongToPtr(address), &ret, length+1, NULL);
        return string(ret);
}
string MEMORY::ReadStringDirect(DWORD address, size_t length) {
        if(address == 0 || processHandle == NULL) return "";
        char ret[500];
        ReadProcessMemory(processHandle, LongToPtr(address), &ret, length+1, NULL);
        return ret;
}

wstring MEMORY::ReadWString(DWORD address) {
        if(address == 0 || processHandle == NULL) return L"";
        size_t length;
        ReadProcessMemory(processHandle, LongToPtr(address+0x4), &length, 4, NULL);
        if(length <= 1) return L"";
        else if(length > 5000) length = 5000;
        ReadProcessMemory(processHandle, LongToPtr(address), &address, 4, NULL);
        wchar_t ret[5000];
        ReadProcessMemory(processHandle, LongToPtr(address), &ret, length*2+2, NULL);
        return ret;
}
wstring MEMORY::ReadWStringDirect(DWORD address, size_t length) {
        if(address == 0 || processHandle == NULL) return L"";
        wchar_t ret[5000];
        ReadProcessMemory(processHandle, LongToPtr(address), &ret, length*2+2, NULL);
        return ret;
}
