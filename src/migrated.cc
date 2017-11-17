#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <thread>
#include <chrono>
#include <string>
#include <sstream>
#include <sys/stat.h>
#include <sys/un.h>
#include <vector>

#include "migrated.h"
#include "neighbourhood.h"

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

std::string GetIdentifier() {
  char hostname[HOST_NAME_MAX + 1];

  gethostname(hostname, HOST_NAME_MAX);

  return std::string(hostname);
}

void CleanUpSocketStruct(LocalDaemonSocket *socket_struct) {
  delete socket_struct->addr;
  delete socket_struct;
}

int AwaitSocketMessage(int sock) {
  std::cout << "Awaiting descriptor on " << sock << std::endl;
  //char buf[SOCK_BUF_MAX_SIZE];
  struct {
    struct cmsghdr h;
    int fd[1];
  } buf;

  struct msghdr msghdr;
  char nothing;
  struct iovec nothing_ptr;
  struct cmsghdr *cmsghdr;

  int fd;

  nothing_ptr.iov_base = &nothing;
  nothing_ptr.iov_len = 1;

  msghdr.msg_name = NULL;
  msghdr.msg_namelen = 0;
  msghdr.msg_iov = &nothing_ptr;
  msghdr.msg_iovlen = 1;
  msghdr.msg_flags = 0;
  msghdr.msg_control = &buf;
  msghdr.msg_controllen = sizeof(struct cmsghdr) + sizeof(int);
  cmsghdr = CMSG_FIRSTHDR(&msghdr);
  cmsghdr->cmsg_len = msghdr.msg_controllen;
  cmsghdr->cmsg_level = SOL_SOCKET;
  cmsghdr->cmsg_type = SCM_RIGHTS;

  ((int *) CMSG_DATA(cmsghdr))[0] = -1;

  if (recvmsg(sock, &msghdr, 0) < 0) {
    perror("AwaitSocketMessage() recvmsg");
    return -1;
  }

  std::cout << "Received socket descriptor" << std::endl;

  fd = ((int *) CMSG_DATA(cmsghdr))[0];

  return fd;
}

int AwaitSocketMessages(int sock, int *fds, int fd_count) {
  std::cout << "Awaiting " << fd_count << " descriptor(s) on " << sock << std::endl;
  int i;
  //char buf[SOCK_BUF_MAX_SIZE];
  struct {
    struct cmsghdr h;
    int fd[SOCK_BUF_MAX_SIZE];
  } buf;

  struct msghdr msghdr;
  char nothing;
  struct iovec nothing_ptr;
  struct cmsghdr *cmsghdr;

  nothing_ptr.iov_base = &nothing;
  nothing_ptr.iov_len = 1;

  msghdr.msg_name = NULL;
  msghdr.msg_namelen = 0;
  msghdr.msg_iov = &nothing_ptr;
  msghdr.msg_iovlen = 1;
  msghdr.msg_flags = 0;
  msghdr.msg_control = &buf;
  msghdr.msg_controllen = sizeof(struct cmsghdr) + sizeof(int) * fd_count;
  cmsghdr = CMSG_FIRSTHDR(&msghdr);
  cmsghdr->cmsg_len = msghdr.msg_controllen;
  cmsghdr->cmsg_level = SOL_SOCKET;
  cmsghdr->cmsg_type = SCM_RIGHTS;

  for (i = 0; i < fd_count; i++) {
    ((int *) CMSG_DATA(cmsghdr))[i] = -1;
  }

  if (recvmsg(sock, &msghdr, 0) < 0) {
    perror("AwaitSocketMessage() recvmsg");
    return -1;
  }

  std::cout << "Received socket descriptors" << std::endl;

  for (i = 0; i < fd_count; i++) {
    *(fds + i) = ((int *) CMSG_DATA(cmsghdr))[i];
  }

  int rcvd_fd_count = (cmsghdr->cmsg_len - sizeof(struct cmsghdr)) / sizeof(int);

  return rcvd_fd_count;
}

void * HandleLocalDaemonConnection(void * s) {
  LocalDaemonSocket *socket_struct = (LocalDaemonSocket *) s;

  char buf[MSG_BUFFER_SIZE];

  int in_bytes;

  while (1) {
    in_bytes = recv(socket_struct->sock, buf, MSG_BUFFER_SIZE - 1, 0);
    if (in_bytes < 0) {
      perror("HandleLocalDaemonConnection() recv");
      CleanUpSocketStruct(socket_struct);
      pthread_exit(NULL);
    } else if (in_bytes == 0) {
      std::cout << "Application connection closed" << std::endl;
      CleanUpSocketStruct(socket_struct);
      pthread_exit(NULL);
    }
    buf[in_bytes] = '\0';
    std::cout << "LOCALMSG: " << buf << std::endl;
    if (in_bytes > 7 && strncmp(buf, "SOCKETS", 7) == 0) {
      int count = 0;

      int i = 0;
      for (i = 8; i < in_bytes; i++) {
        if (buf[i] != '\0') {
          count++;
        }
      }

      char fd_count_str[count + 1];

      strncpy(fd_count_str, buf + 8, count);

      fd_count_str[count] = '\0';

      int fd_count = atoi(fd_count_str);

      int *fds = new int[fd_count];

      int rcvd_fd_count = AwaitSocketMessages(socket_struct->sock, fds, fd_count);

      std::cout << "Received " << rcvd_fd_count << " descriptors" << std::endl;
      for (i = 0; i < rcvd_fd_count; i++) {
        std::cout << "Desciptor " << i << ": " << fds[i] << std::endl;
      }
    } else if (strncmp(buf, "SOCKET", in_bytes) == 0) {
      int fd;
      if ((fd = AwaitSocketMessage(socket_struct->sock)) < 0) {
        perror("HandleLocalDaemonConnection() AwaitsocketMessage");
      } else {
        std::cout << "Received descriptor " << fd << std::endl;
        socket_struct->context->local_sockets.push_back(fd);
      }
    }
  }
}

void * StartLocalDaemon(void *c) {
  Context *context = (Context *) c;
  struct stat stat_info;
  if (stat(DAEMON_DATA_DIRECTORY, &stat_info) != 0) {
    mkdir(DAEMON_DATA_DIRECTORY, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  }

  std::stringstream daemon_data_dir_ss;

  daemon_data_dir_ss << DAEMON_DATA_DIRECTORY;

  daemon_data_dir_ss.seekg(-1, std::ios::end);

  char last_char;

  daemon_data_dir_ss >> last_char;

  if (last_char != '/') {
    daemon_data_dir_ss << '/';
  }

  std::string daemon_data_dir = daemon_data_dir_ss.str();

  std::stringstream ss;

  ss << daemon_data_dir << DAEMON_SOCKET_FILENAME;

  std::string socket_file = ss.str();

  int sock = socket(AF_UNIX, SOCK_STREAM, 0);

  struct sockaddr_un addr, remote;

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_file.c_str(), sizeof(addr.sun_path));

  unlink(socket_file.c_str());

  if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    perror("StartLocalDaemon() bind");
    exit(1);
  }

  if (listen(sock, DAEMON_MAX_CONNECTIONS) < 0) {
    perror("StartLocalDaemon() listen");
    exit(1);
  }

  while (1) {
    int new_sock;
    sockaddr_un *remote_ptr = new sockaddr_un;
    socklen_t addrlen = sizeof(remote);

    if ((new_sock = accept(sock, (struct sockaddr *) remote_ptr, &addrlen)) < 0) {
      perror("StartLocalDaemon() accept");
    }

    pthread_t client_pthread;

    LocalDaemonSocket *socket_struct = new LocalDaemonSocket;

    socket_struct->context = context;

    socket_struct->addr = remote_ptr;
    socket_struct->sock = new_sock;

    pthread_create(&client_pthread, NULL, HandleLocalDaemonConnection, (void *) socket_struct);
  }

  return NULL;
}

void * StartHeartbeatSender(void *c) {
  std::cout << "Starting Heartbeat Sender" << std::endl;
  struct sockaddr_in addr;

  int sock;

  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("StartHeartbeatSender() sock");
    exit(1);
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(NEIGHBOURHOOD_MULTICAST_GROUP);
  addr.sin_port = htons(NEIGHBOURHOOD_MULTICAST_PORT);

  std::stringstream ss;

  ss << "ALIVE " << GetIdentifier();

  std::string message = ss.str();

  while (1) {
    if (sendto(sock, message.c_str(), message.length(), 0, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
      perror("StartHeartbeatSender() sendto");
      exit(1);
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }

  return NULL;
}

void * StartHeartbeatListener(void *c) {
  std::cout << "Starting Heartbeat Listener" << std::endl;

  struct sockaddr_in addr;

  int sock;

  int in_bytes;

  socklen_t addrlen;

  struct ip_mreq mreq;

  char buf[MSG_BUFFER_SIZE];

  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("StartHeartbeatListener() sock");
    exit(1);
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(NEIGHBOURHOOD_MULTICAST_PORT);

  if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    perror("StartHeartbeatListener() bind");
    exit(1);
  }

  mreq.imr_multiaddr.s_addr = inet_addr(NEIGHBOURHOOD_MULTICAST_GROUP);
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);

  if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
    perror("StartHeartbeatListener() setsockopt");
    exit(1);
  }

  while (1) {
    addrlen = sizeof(addr);
    if ((in_bytes = recvfrom(sock, buf, MSG_BUFFER_SIZE - 1, 0, (struct sockaddr *) &addr, &addrlen)) < 0) {
      perror("StartHeartbeatListener() recvfrom");
    }
    buf[in_bytes] = '\0';
    std::cout << "MSG: " << buf << std::endl;
    if (in_bytes > 5 && strncmp(buf, "ALIVE", 5) == 0) {
      std::stringstream ident_ss;
      int i;
      for (i = 6; i < in_bytes; i++) {
        if (buf[i] != ' ') {
          ident_ss << buf[i];
        } else {
          break;
        }
      }

      std::string ident = ident_ss.str();

      std::cout << "Received heartbeat message from " << ident << std::endl;
    }
  }

  return NULL;
}

void InitServer(Context *context) {
  pthread_t hb_sender_pthread;
  pthread_t hb_listener_pthread;
  pthread_t local_daemon_pthread;

  pthread_create(&hb_sender_pthread, NULL, StartHeartbeatSender, (void *) context);
  pthread_create(&hb_listener_pthread, NULL, StartHeartbeatListener, (void *) context);
  pthread_create(&local_daemon_pthread, NULL, StartLocalDaemon, (void *) context);
}

int main() {
  Context *context = new Context;
  InitServer(context);
  return 0;
}
