#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <iterator>
#include <string>
#include <iostream>
#include "helpers.h"

using namespace std;

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	// Used to store data about the clients and their connections
	unordered_map<string, int> socketMapping;
	unordered_map<string, int>::iterator socketIt;

	int tcpSocket, udpSocket, newsockfd, portno;
	char buffer[BUFLEN];

	struct sockaddr_in serv_addr, cli_addr;
	int n, i, ret;
	socklen_t clilen;

	fd_set read_fds;	// read descriptor stream
	fd_set tmp_fds;		// temporary descriptor stream
	int fdmax;			// maximum descriptor value

	if (argc < 2) {
		usage(argv[0]);
	}

	// Clear the read and tmp descriptor streams
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);;

	// Initialize the port
	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	// Assign and bind TCP and UDP sockets and deactivate Nagle algorithm
	tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
	DIE(tcpSocket < 0, "Couldn't open TCP socket");

	int flag = 1;
	int result = setsockopt(tcpSocket, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
	DIE(result < 0, "Could not deactivate Nagle on TCP socket");

	result = bind(tcpSocket, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(result < 0, "Could not bind TCP socket");

	udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(udpSocket < 0, "Couldn't open UDP socket");

	result = bind(udpSocket, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(result < 0, "Could not bind UDP socket");

	// Add the sockets (and the stdin fd) to the read stream
	FD_SET(0, &read_fds);
	FD_SET(udpSocket, &read_fds);
	FD_SET(tcpSocket, &read_fds);
	fdmax = max(udpSocket, tcpSocket) + 1;

	// Listen for any updates
	result = listen(tcpSocket, MAX_CLIENTS);

	// Wait for inputs and parse them
	while (1) {
		tmp_fds = read_fds; 
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "No socket found for select");

		// Check if any socket is set
		for (i = 0; i <= fdmax; ++i) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == udpSocket) {

				} else if (i == tcpSocket) {
					// New connection request
					socklen_t len = sizeof(cli_addr);
					int new_client = accept(tcpSocket, (struct sockaddr*) &cli_addr, &len);
					DIE(new_client < 0, "Could not connect to server");
					// Disable Nagle's algorithm on the new socket
					result = setsockopt(new_client, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
					DIE(result < 0, "Could not deactivate Nagle on new TCP socket");
					// Receive the connection message
					memset(buffer, 0, BUFLEN);
					result = recv(new_client, buffer, sizeof(buffer), 0);
					// Check if the user is already subscribed
					bool found = false;
					string toFind(buffer);
					if (socketMapping.find(toFind) != socketMapping.end()) {
						strcpy(buffer, "User already connected with the current ID. Try using another id.");
						send(new_client, buffer, sizeof(buffer), 0);
						// Close the newly opened socket
						close(new_client);
						FD_CLR(new_client, &read_fds);
						found = true;
					}
					// for (clientIt = clientList.begin(); clientIt != clientList.end(); ++clientIt) {
					// 	if (clientIt->second.name == buffer) {
					// 		strcpy(buffer, "User already connected with the current ID. Try using another id.");
					// 		send(new_client, buffer, sizeof(buffer), 0);
					// 		// Close the newly opened socket
					// 		close(new_client);
					// 		FD_CLR(new_client, &read_fds);
					// 		found = true;
					// 	}
					// }

					// Add the new client
					if (!found) {
						FD_SET(new_client, &read_fds);
						fdmax = max(fdmax - 1, new_client) + 1;

						// if (clientList.find(new_client) != clientList.end()) {
						// 	struct subscriber temp;
						// 	temp.connected = true;
						// 	temp.name = buffer;
						// 	clientList.insert(make_pair(new_client, temp));
						// } else {
						// 	clientList[new_client].connected = true;
						// 	clientList[new_client].name = buffer;
						// }
						socketMapping.insert(make_pair(buffer, new_client));
						// Notify the server that a new client has connected
						printf("New client %s connected from %s:%d.\n",
						buffer, inet_ntoa(cli_addr.sin_addr),
						ntohs(cli_addr.sin_port));
					}
				} else {
					memset(buffer, 0, BUFLEN);
					result = recv(i, buffer, BUFLEN, 0);
					if (strncmp(buffer, "exit", 4) == 0) {
						// Client issued an exit command, so we disconnect him and remove his socket
						string deleteName;
						for (auto it : socketMapping) {
							if (it.second == i) {
								deleteName = it.first;
								break;
							}
						}
						cout << "Client " << deleteName << " disconnected.\n";
						close(i);
						socketMapping.erase(deleteName);
						FD_CLR(i, &read_fds);
					}
				}
			}
		}
	}

	// Close the used sockets
	close(tcpSocket);
	close(udpSocket);
	return 0;
}
