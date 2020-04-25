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
#include <set>
#include <iostream>
#include <sstream>
#include <cmath>
#include <iomanip>
#include "helpers.h"

using namespace std;

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

void decode_int(struct messageFormat *msg, char* decodified) {
	int number;
	number = (msg->content[1] - 0) * (1 << 24) + (msg->content[2] - 0) * (1 << 16) + (msg->content[3] - 0) * (1 << 8) + (msg->content[4] - 0);
	if ((msg->content[0] - 0) == 1) {
		number *= -1;
	}

	strcat(decodified, "INT - ");
	string numberString = to_string(number);
	char char_number[numberString.length() + 1];
	strcpy(char_number, numberString.c_str());
	strcat(decodified, char_number);
}

void decode_short_real(struct messageFormat *msg, char* decodified) {
	double number;
	number = (msg->content[0] - 0) * 256 + (msg->content[1] - 0);
	strcat(decodified, "SHORT_REAL - ");

	std::ostringstream streamObj3;
	streamObj3 << fixed;
	streamObj3 << setprecision(2);
	streamObj3 << number / 100;
	string numberString = streamObj3.str();

	char char_number[numberString.length() + 1];
	strcpy(char_number, numberString.c_str());

	strcat(decodified, char_number);
}

void decode_float(struct messageFormat *msg, char* decodified) {
	double number = 0;
	number = (msg->content[1] - 0) * 256 * 256 * 256 +
				(msg->content[2] - 0) * 256 * 256 +
				(msg->content[3] - 0) * 256 +
				(msg->content[4] - 0);
	int ordin_putere = msg->content[5] - 0;
	number = number / pow(10, ordin_putere);
	if ((msg->content[0] - 0) == 1) {
		number *= -1;
	}
	strcat(decodified, "FLOAT - ");
	string numberString = to_string(number);
	char char_number[numberString.length() + 1];
	strcpy(char_number, numberString.c_str());
	strcat(decodified, char_number);
}

void decode_string(struct messageFormat *msg, char* decodified) {
	strcat(decodified, "STRING - ");
	char tmpContent[BUFLEN];
	memcpy(tmpContent, msg->content, sizeof(msg->content));
	strcat(decodified, tmpContent);
}
int main(int argc, char *argv[])
{
	// Used to store data about the clients and their connections
	unordered_map<string, int> socketMapping;
	unordered_map<string, int>::iterator socketIt;
	unordered_map<string, set<string>> topicMapping;
	unordered_map<string, set<string>> topicSFMapping;

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

		// Check if an exit command was issued
		if (FD_ISSET(0, &tmp_fds)) {
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN - 1, stdin);

			if (strncmp(buffer, "exit", 4) == 0) {
				break;
			}
		}

		// Check if any socket is set
		for (i = 0; i <= fdmax; ++i) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == udpSocket) {
					// UDP message received
					struct sockaddr_in serv_addr_udp;
					socklen_t received_size;

					struct messageFormat *msg = (struct messageFormat *) malloc(sizeof(struct messageFormat));
					result = recvfrom(i, msg, sizeof(struct messageFormat), 0,
						(struct sockaddr *)&serv_addr_udp, &received_size);

					DIE(result < 0, "Error when receiving data from UDP socket");

					// Message header
					char decodified[BUFLEN];
					memset(decodified, 0, BUFLEN);

					char *ip = inet_ntoa(serv_addr_udp.sin_addr);
					int portNumber = serv_addr_udp.sin_port;

					strcat(decodified, ip);
					strcat(decodified, ":");
					string portString = to_string(portNumber);
					char portToString[portString.size() + 1];
					strcpy(portToString, portString.c_str());
					strcat(decodified, portToString);

					strcat(decodified, " - ");
					strcat(decodified, msg->topic);
					strcat(decodified, " - ");
					
					// Decodify message content, based on type
					if (msg->type - 0 == 0) {
						decode_int(msg, decodified);
					} else if (msg->type - 0 == 1) {
						decode_short_real(msg, decodified);
					} else if (msg->type - 0 == 2) {
						decode_float(msg, decodified);
					} else if (msg->type - 0 == 3) {
						decode_string(msg, decodified);
					}

					if (topicMapping.find(msg->topic) != topicMapping.end()) {
						for (auto client : topicMapping[msg->topic]) {
							result = send(socketMapping[client], decodified, sizeof(decodified), 0);
							DIE(result < 0, "Could not send topic message");
						}
					}
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

					// Add the new client
					if (!found) {
						FD_SET(new_client, &read_fds);
						fdmax = max(fdmax - 1, new_client) + 1;
						socketMapping.insert(make_pair(buffer, new_client));

						// Notify the server that a new client has connected
						printf("New client %s connected from %s:%d.\n",
						buffer, inet_ntoa(cli_addr.sin_addr),
						ntohs(cli_addr.sin_port));
					}
				} else {
					memset(buffer, 0, BUFLEN);
					result = recv(i, buffer, BUFLEN, 0);
					// Find the message sender
					string findName;
					for (auto it : socketMapping) {
						if (it.second == i) {
							findName = it.first;
							break;
						}
					}
					if (strncmp(buffer, "exit", 4) == 0) {
						// Client issued an exit command, so we disconnect him and remove his socket
						cout << "Client " << findName << " disconnected.\n";
						close(i);
						socketMapping.erase(findName);
						FD_CLR(i, &read_fds);
					} else {
						// The server received a subscribe/unsubscribe message from a client
						cout << "Message received from user " << findName << " is " << buffer;
						string temp(buffer), word;
						stringstream ss(temp);
						ss >> word;
						if (word == "subscribe") {
							string topic;
							string SF;
							ss >> topic;
							ss >> SF;

							if (topicMapping.find(topic) == topicMapping.end()) {
								set<string> tempSet;
								tempSet.insert(findName);
								topicMapping.insert(make_pair(topic, tempSet));
							} else {
								topicMapping[topic].insert(findName);
							}
						} else if (word == "unsubscribe") {
							string topic;
							ss >> topic;
							if (topicMapping.find(topic) != topicMapping.end()) {
								topicMapping[topic].erase(findName);
							}
						}
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
