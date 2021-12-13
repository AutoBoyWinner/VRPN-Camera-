#pragma once
#define WIN32_LEAN_AND_MEAN
#include <string>
#include <WinSock2.h>
#include <Windows.h>
#include <WS2tcpip.h>

#pragma comment (lib, "ws2_32.lib")

class TCPServer; 

//Callback fct = fct with fct as parameter.
typedef void(*MessageReceivedHandler)(TCPServer* listener, int socketID, std::string msg);

class TCPServer {
public:
	TCPServer(); 
	TCPServer(std::string ipAddress, int port);
	~TCPServer(); 

	void sendMsg(int clientSocket, std::string msg);
	bool initWinsock(); 
	void run(void* generic_server);
	void cleanupWinsock(); 
	

private: 
	SOCKET createSocket(); 
	std::string listenerIPAddress;	
	int listenerPort; 
	//MessageReceivedHandler messageReceived; 
};