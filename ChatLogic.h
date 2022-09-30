#pragma once
#include <unordered_map>
using std::unordered_map;

#include "CPacket.h"
#include "User.h"
#include "Tracer.h"

extern unordered_map<SS_ID, User*> g_UserMap;
extern SRWLOCK g_UserMapSRW;

extern unsigned int g_connect_cnt;
extern unsigned int g_login_cnt;

extern Tracer g_Tracer;

bool FindUser(SS_ID s_id, User** user);
void CreateUser(SS_ID s_id);
void DeleteUser(SS_ID s_id);

void SendMessageUni(CPacket* packet, User* user);
void SendMessageSector(CPacket* packet, int sector_x, int sector_y);
void SendMessageAround(CPacket* packet, User* user);