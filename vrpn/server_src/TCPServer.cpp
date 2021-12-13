/*
	Steps to set up a Winsock server (from https://docs.microsoft.com/en-us/windows/desktop/winsock/winsock-server-application):
		- Initializing Winsock. 
		- Creating a listening socket for the server. 
		- Binding that listening socket to the server IP address & listening port. 
			-> we use a sockaddr_in structure for this matter.
		- Accept a connection from a client. 
			-> once a connection is established, the recv() function is used to receive data from that specific client. 
		- Receive & send data. 
		- Disconnect server once everything is done. 

	To handle multiple clients on the server without using multithreading, I used the select() function.
	(from https://www.geeksforgeeks.org/socket-programming-in-cc-handling-multiple-clients-on-server-without-multi-threading/)
		- Select allows to store multiple active sockets inside a file descriptor. 
		- Select will detect if a socket gets activated. 
			-> when a socket is "activated", two things can happen: 
				1. If the socket is the listening socket, it means that a new client is trying to connect to the server. 
				2. If not, it means a connected client is sending data. 



*/

#include "TCPServer.h"
#include <iostream>
#include <string>
#include <sstream>
#include "vrpn_Generic_server_object.h"
#include "vrpn_Tracker.h"
#include "vrpn_Types.h"
const int MAX_BUFFER_SIZE = 4096;			//Constant value for the buffer size = where we will store the data received.
using namespace std;
#include <vector>
#include <string>


TCPServer::TCPServer() { }


TCPServer::TCPServer(std::string ipAddress, int port)
	: listenerIPAddress(ipAddress), listenerPort(port) {
}

TCPServer::~TCPServer() {
	cleanupWinsock();			//Cleanup Winsock when the server shuts down. 
}


//Function to check whether we were able to initialize Winsock & start the server. 
bool TCPServer::initWinsock() {

	WSADATA data;
	WORD ver = MAKEWORD(2, 2);

	int wsInit = WSAStartup(ver, &data);

	if (wsInit != 0) {
		printf("Error: can't initialize Winsock. \n"); 
		return false;
	}

	return true;

}


//Function that creates a listening socket of the server. 
SOCKET TCPServer::createSocket() {

	SOCKET listeningSocket = socket(AF_INET, SOCK_STREAM, 0);	//AF_INET = IPv4. 

	if (listeningSocket != INVALID_SOCKET) {

		sockaddr_in hint;		//Structure used to bind IP address & port to specific socket. 
		hint.sin_family = AF_INET;		//Tell hint that we are IPv4 addresses. 
		hint.sin_port = htons(listenerPort);	//Tell hint what port we are using. 
		//inet_pton(AF_INET, listenerIPAddress.c_str(), &hint.sin_addr); 	//Converts IP string to bytes & pass it to our hint. hint.sin_addr is the buffer. PPBB
		hint.sin_addr.s_addr = INADDR_ANY;
		int bindCheck = bind(listeningSocket, (sockaddr*)&hint, sizeof(hint));	//Bind listeningSocket to the hint structure. We're telling it what IP address family & port to use. 

		if (bindCheck != SOCKET_ERROR) {			//If bind OK:

			int listenCheck = listen(listeningSocket, SOMAXCONN);	//Tell the socket is for listening. 
			if (listenCheck == SOCKET_ERROR) {
				return -1;
			}
		}

		else {
			return -1;
		}

		return listeningSocket;

	}

}

vector<string> global_split(string data, string token)
{
    vector<string> output;
    size_t pos = string::npos; // size_t to avoid improbable overflow
    do
    {
        pos = data.find(token);
        output.push_back(data.substr(0, pos));
        if (string::npos != pos)
            data = data.substr(pos + token.size());
    } while (string::npos != pos);
    return output;
}
//Function doing the main work of the server -> evaluates sockets & either accepts connections or receives data. 
void TCPServer::run(void *generic_server) {

	char buf[MAX_BUFFER_SIZE];		//Create the buffer to receive the data from the clients. 
	SOCKET listeningSocket = createSocket();		//Create the listening socket for the server. 
	std::vector<std::string> packet_vector;
	int packetType = 0;
	while (true) {

		if (listeningSocket == INVALID_SOCKET) {
			break;
		}

		fd_set master;				//File descriptor storing all the sockets.
		FD_ZERO(&master);			//Empty file file descriptor. 

		FD_SET(listeningSocket, &master);		//Add listening socket to file descriptor. 

		while (true) {

			fd_set copy = master;	//Create new file descriptor bc the file descriptor gets destroyed every time. 
			int socketCount = select(0, &copy, nullptr, nullptr, nullptr);				//Select() determines status of sockets & returns the sockets doing "work". 

			for (int i = 0; i < socketCount; i++) {				//Server can only accept connection & receive msg from client. 

				SOCKET sock = copy.fd_array[i];					//Loop through all the sockets in the file descriptor, identified as "active". 

				if (sock == listeningSocket) {				//Case 1: accept new connection.

					SOCKET client = accept(listeningSocket, nullptr, nullptr);		//Accept incoming connection & identify it as a new client. 
					FD_SET(client, &master);		//Add new connection to list of sockets.  
					std::string welcomeMsg = "Welcome to VRPN Server.\n";			//Notify client that he entered the chat. 
					send(client, welcomeMsg.c_str(), welcomeMsg.size() + 1, 0);
					printf("New sender device joined the network.\n");			//Log connection on server side. 

				}
				else {										//Case 2: receive a msg.	

					ZeroMemory(buf, MAX_BUFFER_SIZE);		//Clear the buffer before receiving data. 
					int bytesReceived = recv(sock, buf, MAX_BUFFER_SIZE, 0);	//Receive data into buf & put it into bytesReceived. 

					if (bytesReceived <= 0) {	//No msg = drop client. 
						closesocket(sock);
						FD_CLR(sock, &master);	//Remove connection from file director.
					}
					else {						//Send msg to other clients & not listening socket. 

						for (int i = 0; i < master.fd_count; i++) {			//Loop through the sockets. 
							SOCKET outSock = master.fd_array[i];	

							if (outSock != listeningSocket) {

								if (outSock == sock) {		//If the current socket is the one that sent the message:
									std::string msgSent = "\n\nChataigne : Message delivered to vrpn Server.\n";
									send(outSock, msgSent.c_str(), msgSent.size() + 1, 0);	//Notify the client that the msg was delivered. 

									std::cout << std::string(buf, 0, bytesReceived) << std::endl;			//Log the message on the server side. 
									

									vrpn_Generic_Server_Object* serverObject = (vrpn_Generic_Server_Object*)generic_server;
									packetType = 0;
									packet_vector.empty();
									packet_vector = global_split(buf, "|");
									string strPktType = packet_vector.at(0);
									packetType = atoi(strPktType.c_str());

									vector <string>::size_type vector_length; 
									vector_length = packet_vector.size();
									switch (packetType)
									{
									case 100: // vrpn_UDP_Tracker_Pkt
										{
								
											if(serverObject->num_trackers > 0 && (int)vector_length >=7)
											{
												vrpn_UDP_Tracker* tracker = (vrpn_UDP_Tracker*)(serverObject->trackers[0]);
												tracker->pos[0] = atof(packet_vector.at(1).c_str());
												tracker->pos[1] = atof(packet_vector.at(2).c_str());
												tracker->pos[2] = atof(packet_vector.at(3).c_str());
												tracker->d_quat[0] = atof(packet_vector.at(4).c_str());
												tracker->d_quat[1] = atof(packet_vector.at(5).c_str());
												tracker->d_quat[2] = atof(packet_vector.at(6).c_str());
												tracker->d_quat[3] = 0;						 			
												std::cout <<"--broadcast tracker msg to vrpn clients--\n\n" << std::endl;
											}		
										}
										break;
									case 101:// vrpn_UDP_Analog_Pkt
										{
											if(serverObject->num_mouses > 0 && (int)vector_length >=4)
											{
												
												
												vrpn_UDP_Analog* analog = (vrpn_UDP_Analog*)(serverObject->mouses[0]);

												if(analog->num_channel != (int)vector_length -1)
												{
													std::cout <<"--Bad Message Found. Ignored!--\n\n" << std::endl;
													break;
												}
												analog->readFlag = false;
												/*if((int)vector_length -1 > 128)
												{
													std::cout <<"--max channel count must be less 128--" << std::endl;

													analog->num_channel = 128;
												}
												else
													analog->num_channel = (int)vector_length -1;*/
												for(int i = 0; i<analog->num_channel; i++)
												{
													analog->channel[i] = atof(packet_vector.at(i+1).c_str());													
												}

												analog->readFlag = true;
												
												std::cout <<"--broadcast analog msg to vrpn clients--\n\n" << std::endl;

											}						 
										}							 
										break;
									case 102:// vrpn_UDP_Button_Pkt
										{
											if(serverObject->num_Keyboards > 0 && (int)vector_length >=2)
											{
												vrpn_UDP_Button* button = (vrpn_UDP_Button*)(serverObject->Keyboards[0]);

												if(button->num_buttons != (int)vector_length -1)
												{
													std::cout <<"--Bad Message Found. Ignored!--\n\n" << std::endl;
													break;
												}
												button->readFlag = false;
												/*if((int)vector_length -1 > 256)
												{
													std::cout <<"--max button count must be less 256--" << std::endl;
													button->num_buttons = 256;

												}
												else
													button->num_buttons = (int)vector_length -1;*/


												for (i = 0; i < button->num_buttons; i++) {
													button->buttons[i] = atoi(packet_vector.at(i+1).c_str());														  
												}
												button->readFlag = true;

												std::cout <<"--broadcast button msg to vrpn clients--\n\n" << std::endl;
											}
										}
										break;
									default:
										break;
						
									}
								}
								else {						//If the current sock is not the sender -> it should receive the msg. 
									//std::ostringstream ss;
									//ss << "SOCKET " << sock << ": " << buf << "\n";
									//std::string strOut = ss.str();
									send(outSock, buf, bytesReceived, 0);		//Send the msg to the current socket. 
								}

							}
						}

						

						

						

					}

				}
			}
		}


	}

}


//Function to send the message to a specific client. 
void TCPServer::sendMsg(int clientSocket, std::string msg) {

	send(clientSocket, msg.c_str(), msg.size() + 1, 0);

}


void TCPServer::cleanupWinsock() {

	WSACleanup();

}


