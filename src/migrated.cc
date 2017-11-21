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
#include <pthread.h>

#include "migrated.h"
#include "neighbourhood.h"
#include "service.h"
#include "statedata.h"

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
  Context *context = socket_struct->context;

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
    } else if (in_bytes > 6 && strncmp(buf, "SOCKET", 6) == 0) {
      int fd;
      if ((fd = AwaitSocketMessage(socket_struct->sock)) < 0) {
        perror("HandleLocalDaemonConnection() AwaitsocketMessage");
      } else {
        std::cout << "Received descriptor " << fd << std::endl;
        socket_struct->context->local_sockets.push_back(fd);
      }
    } else if (in_bytes > 5 && strncmp(buf, "STATE", 5) == 0) {
      // State information received from application

      std::stringstream service_ss;
      std::stringstream conn_ident_ss;
      StateData *state_data;
      char *state_data_buf;
      int sz;

      int i;

      for (i = 6; i < in_bytes; i++) {
        if (buf[i] != ' ') {
          service_ss << buf[i];
        } else {
          break;
        }
      }

      i++;

      for (; i < in_bytes; i++) {
        if (buf[i] != ' ') {
          conn_ident_ss << buf[i];
        } else {
          break;
        }
      }

      i++;

      if (in_bytes > i + 1) {
        sz = in_bytes - i;
        state_data_buf = new char[sz];
        int j;
        for (j = 0; i < in_bytes; i++, j++) {
          state_data_buf[j] = buf[i];
        }
      }

      state_data = new StateData(state_data_buf, sz);

      int service_identifier = std::stoi(service_ss.str());
      int connection_identifier = std::stoi(conn_ident_ss.str());

      pthread_mutex_lock(&context->local_services_mutex);
      Service *service;
      auto service_it = context->local_services.find(service_identifier);
      if (service_it == context->local_services.end()) {
        service = new Service(service_identifier);
        context->local_services[service_identifier] = service;
      } else {
        service = service_it->second;
      }

      service->AddClient(connection_identifier, state_data);
      std::cout << "Added client to service " << service_identifier << ": " << connection_identifier << ", " << state_data->GetData() << std::endl;
      pthread_mutex_unlock(&context->local_services_mutex);

      // START OF DEBUG CODE
      pthread_mutex_lock(&context->local_services_mutex);
      std::unordered_map<int, Service *>::iterator debug_it;
      std::unordered_map<int, StateData *>::iterator debug_service_it;
      std::unordered_map<int, StateData *> debug_clients;
      Service *debug_s;
      int debug_service_id;
      StateData* debug_sd;
      int debug_connection_id;
      for (debug_it = context->local_services.begin(); debug_it != context->local_services.end(); debug_it++) {
        debug_service_id = debug_it->first;
        debug_s = debug_it->second;
        debug_clients = debug_s->GetClients();
        for (debug_service_it = debug_clients.begin(); debug_service_it != debug_clients.end(); debug_service_it++) {
          debug_connection_id = debug_service_it->first;
          debug_sd = debug_service_it->second;

          std::string debug_data = std::string(debug_sd->GetData(), debug_sd->GetSize());

          std::cout << debug_service_id << " " << debug_connection_id << " " << debug_sd->GetSize() << " " << debug_sd->GetData() << std::endl;
          pthread_mutex_unlock(&context->local_services_mutex);
          //END OF DEBUG CODE
        }
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
  Context *context = (Context *) c;
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

  std::string message;

  while (1) {
    std::unordered_map<int, Service*> services = context->local_services;

    std::unordered_map<int, Service *>::iterator it;
    std::unordered_map<int, StateData *>::iterator clients_it;
    std::unordered_map<int, StateData *> clients;
    Service *service;
    int service_id;
    StateData *state_data;
    int connection_id;

    for (it = services.begin(); it != services.end(); it++) {
      service_id = it->first;
      service = it->second;
      clients = service->GetClients();
      for (clients_it = clients.begin(); clients_it != clients.end(); clients_it++) {
        connection_id = clients_it->first;
        state_data = clients_it->second;

        std::stringstream msgstream;

        msgstream << "STATE " << GetIdentifier() << " " << service_id << " " << connection_id << " " << std::string(state_data->GetData(), state_data->GetSize());

        std::string msg = msgstream.str();

        if (sendto(sock, msg.c_str(), msg.length(), 0, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
          perror("StartHeartbeatSender() sendto");
          exit(1);
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }

  return NULL;
}

void * StartHeartbeatListener(void *c) {
  std::cout << "Starting Heartbeat Listener" << std::endl;
  Context *context = (Context *) c;

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
    if (in_bytes > 5 && strncmp(buf, "STATE", 5) == 0) {
      std::stringstream ident_ss;
      std::stringstream service_ss;
      std::stringstream conn_ident_ss;
      int i;
      int sz;
      char *state_data;

      for (i = 6; i < in_bytes; i++) {
        if (buf[i] != ' ') {
          ident_ss << buf[i];
        } else {
          break;
        }
      }

      i++;

      for (; i < in_bytes; i++) {
        if (buf[i] != ' ') {
          service_ss << buf[i];
        } else {
          break;
        }
      }

      i++;

      for (; i < in_bytes; i++) {
        if (buf[i] != ' ') {
          conn_ident_ss << buf[i];
        } else {
          break;
        }
      }

      i++;

      if (in_bytes > i + 1) {
        sz = in_bytes - i;
        state_data = new char[sz];
        int j;
        for (j = 0; i < in_bytes; i++, j++) {
          state_data[j] = buf[i];
        }
      }

      std::string ident = ident_ss.str();
      int service_identifier = std::stoi(service_ss.str());
      int connection_identifier = std::stoi(conn_ident_ss.str());

      // DEBUG CODE FOR CHECKING STATE OF SERVERS
      pthread_mutex_lock(&context->servers_mutex);
      std::vector<MigrateServer *>::iterator debug_it;
      std::vector<int>::iterator debug_serv_it;
      std::vector<Connection *>::iterator debug_conn_it;
      MigrateServer *debug_s;
      std::vector<int> debug_services;
      std::vector<Connection *> * debug_conns;
      Connection *debug_c;

      std::cout << "Servers:" << std::endl;

      for (debug_it = context->servers.begin(); debug_it != context->servers.end(); debug_it++) {
        debug_s = *debug_it;
        std::cout << debug_s->GetIdentifier() << std::endl;
        std::cout << "------------------------" << std::endl;
        debug_services = debug_s->GetServices();
        for (debug_serv_it = debug_services.begin(); debug_serv_it != debug_services.end(); debug_serv_it++) {
          std::cout << *debug_serv_it << std::endl << "---------" << std::endl;
          debug_conns = debug_s->GetConnections(*debug_serv_it);
          for (debug_conn_it = debug_conns->begin(); debug_conn_it != debug_conns->end(); debug_conn_it++) {
            debug_c = *debug_conn_it;
            std::cout << debug_c->GetServiceIdentifier() << ", " << debug_c->GetConnectionIdentifier() << ", " << std::string(debug_c->GetState(), debug_c->GetStateSize()) << std::endl;
          }
        }
      }
      pthread_mutex_unlock(&context->servers_mutex);
      // END OF DEBUG CODE

      pthread_mutex_lock(&context->servers_mutex);
      MigrateServer *server = NULL;

      std::vector<MigrateServer *>::iterator it;

      for (it = context->servers.begin(); it != context->servers.end(); it++) {
        if ((*it)->GetIdentifier() == ident) {
          server = *it;
          break;
        }
      }

      if (server == NULL) {
        server = new MigrateServer(ident);
        context->servers.push_back(server);
      }

      std::cout << "Adding new connection " << service_identifier << " " << connection_identifier << " " << state_data << std::endl;
      server->AddOrUpdateConnection(service_identifier, connection_identifier, state_data, sz);
      pthread_mutex_unlock(&context->servers_mutex);
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

  pthread_join(hb_sender_pthread, NULL);
  pthread_join(hb_listener_pthread, NULL);
  pthread_join(local_daemon_pthread, NULL);
}

int main() {
  Context *context = new Context;
  std::vector<MigrateServer *> servers;
  context->servers = servers;
  InitServer(context);
  return 0;
}
