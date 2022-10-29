#pragma once
#include <unordered_map>
using std::unordered_map;

#define dfUSER_MAP_HASH 5

#include "CPacket.h"
#include "User.h"
#include "Tracer.h"



extern unsigned int g_connect_cnt;
extern unsigned int g_login_cnt;
extern unsigned int g_duplicate_login;
extern unsigned int g_message_tps;

extern Tracer g_Tracer;

bool AcquireUser(SS_ID s_id, User** user);
void ReleaseUser(User* user);

void CreateUser(SS_ID s_id);
void DeleteUser(SS_ID s_id);

void SendMessageUni(CPacket* packet, User* user);
void SendMessageSector(CPacket* packet, int sector_x, int sector_y);
void SendMessageAround(CPacket* packet, User* user);