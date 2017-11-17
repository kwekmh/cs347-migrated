#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <iostream>

#include "neighbourhood.h"

void SendMessageToNeighbourhood(std::string message) {
  struct sockaddr_in addr;

  int sock;

  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("SendMessageToNeighbourhood() sock");
    exit(1);
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(NEIGHBOURHOOD_MULTICAST_GROUP);
  addr.sin_port = htons(NEIGHBOURHOOD_MULTICAST_PORT);

  if (sendto(sock, message.c_str(), message.length(), 0, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    perror("SendMessageToNeighbourhood() sendto");
  }
}
