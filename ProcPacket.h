#pragma once

class User;
class CPacket;
class ChatServer;


class PacketProcessor
{
public:
	bool ProcLogin(User* user, CPacket* packet);
	bool ProcSectorMove(User* user, CPacket* packet);
	bool ProcMessage(User* user, CPacket* packet);

	void SetServer(ChatServer* server)
	{
		m_server = server;
	}

private:

	ChatServer* m_server;

};

