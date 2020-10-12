#include "ACCOUNTS.h"

std::mutex mtx_accounts;
list<ACCOUNT> accounts;

CHARACTER::CHARACTER() {}
CHARACTER::CHARACTER(json &data) {
        Load(data);
}

ACCOUNT::ACCOUNT() {}
ACCOUNT::ACCOUNT(json &data) {
        Load(data);
}

bool CHARACTER::Load(json data) {
        try {
                name = data["name"];
                char_class = toCharacterClass(data["class"].get<int>());
                canRunAuto = data["canRunAuto"].get<bool>();
                canDG = data["canDG"].get<bool>();
                warpNavigation = data["warpNavigation"].get<byte>();
        } catch(exception &e) { printf("%s\n", e.what()); return false; }
        return true;
}

json CHARACTER::toJSON() {
        json data;
        data["name"] = name;
        data["class"] = char_class;
        data["canRunAuto"] = canRunAuto;
        data["canDG"] = canDG;
        return data;
}

bool ACCOUNT::Load(json data) {
        try {
                name = data["name"];
                password = data["password"];
                isP2P = data["isP2P"].get<bool>();
                canRunAuto = data["canRunAuto"].get<bool>();
        } catch(exception &e) { printf("%s\n", e.what()); return false; }
        return true;
}

json ACCOUNT::toJSON() {
        json data;
        data["name"] = name;
        data["password"] = password;
        data["isP2P"] = isP2P;
        data["canRunAuto"] = canRunAuto;
        return data;
}

void ACCOUNTS::Load() {
        printf(CC_CYAN, "==Loading accounts... ");
        std::ifstream file("data/accounts.txt");
        std::string strdata((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if(strdata.length() > 2) {
                json data;
                try {
                        data = json::parse(strdata);
                        for(auto& el : data["accounts"]) {
                                ACCOUNT newAcc;
                                if(!newAcc.Load(el)) { printf("Error loading account\n"); continue; }
                                for(auto& el2 : el["characters"]) {
                                        CHARACTER newChar;
                                        if(!newChar.Load(el2)) { printf("Error loading character for account %s\n", newAcc.name.c_str()); continue; }
                                        newAcc.characters.push_back(newChar);
                                }
                                accounts.push_back(newAcc);
                        }
                } catch(exception &e) { printf("%s\n", e.what()); };
        }
        printf("Loaded %i accounts\n", accounts.size());
}

void ACCOUNTS::Save() {
        json data;
        size_t i = 0;
        size_t i2 = 0;
        for(ACCOUNT& acc : accounts) {
                data["accounts"][i] = acc.toJSON();

                for(CHARACTER& character : acc.characters) {
                        data["accounts"][i]["characters"][i2] = character.toJSON();
                        i2++;
                }
                i++;
                i2=0;
        }
        ofstream file("data/accounts.txt");
        file<<data.dump(2);
}

void ACCOUNTS::Add(string name, string password) {
        ACCOUNT newAcc;
        newAcc.name = name;
        newAcc.password = password;
        accounts.push_back(newAcc);
        Save();
}

bool ACCOUNTS::AddCharacter(string accName, string charName) {
        for(ACCOUNT& acc : accounts) if(acc.name == accName) {
                        if(acc.characters.size() >= 6) return false;
                        CHARACTER newChar;
                        newChar.name = charName;
                        acc.characters.push_back(newChar);
                        Save();
                        return true;
                }
        return false;
}
