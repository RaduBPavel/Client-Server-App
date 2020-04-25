#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include "helpers.h"

using namespace std;

void usage(char *file)
{
	fprintf(stderr, "Usage: %s id_client server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];
	string name;

	if (argc < 4) {
		usage(argv[0]);
	}

	// Open the socket, initialize port data and connect the socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "Could not open socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "Could not translate IPv4 address");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "Could not connect the socket");

	// Disable Nagle's algorithm
	int flag = 1;
	int result = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
	DIE(result < 0, "Could not disable Nagle's algorithm");

	// Initialize the read socket stream
	fd_set read_fds;
	fd_set tmp_fds;
	int fdmax;

	FD_SET(sockfd, &read_fds);
	FD_SET(0, &read_fds);
	fdmax = sockfd + 1;

	// Send an initial message with the name
	memset(buffer, 0, BUFLEN);
	strcpy(buffer, argv[1]);
	result = send(sockfd, buffer, strlen(buffer), 0);
	DIE(result < 0, "Could not send initial message");

	// Search for any set sockets in the read_fds stream
	while (1) {
		tmp_fds = read_fds;
		result = select(fdmax, &tmp_fds, NULL, NULL, NULL);
		DIE(result < 0, "Select error");

		// Input came from stdin
		if (FD_ISSET(0, &tmp_fds)) {
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN - 1, stdin);

			// Send the message to the server
			result = send(sockfd, buffer, strlen(buffer), 0);
			DIE(result < 0, "Could not send message");
		}

		// Input came from the socket connected to the server
		if (FD_ISSET(sockfd, &tmp_fds)) {
			memset(buffer, 0, BUFLEN);
			result = recv(sockfd, buffer, BUFLEN, 0);
			DIE(result < 0, "Error when receiving message from server");
			if (strcmp(buffer, "User already connected with the current ID. Try using another id.") == 0) {
				printf("%s\n", buffer);
				break;
			}
			if (result == 0) {
				printf("Server connection terminated.\n");
				break;
			}
			printf("%s\n", buffer);
		}
	}

	close(sockfd);

	return 0;
}
