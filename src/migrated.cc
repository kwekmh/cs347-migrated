#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
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
#include "configuration.h"
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
  auto mappings_it = socket_struct->context->local_service_mappings.find(socket_struct->sock);
  if (mappings_it != socket_struct->context->local_service_mappings.end()) {
    std::vector<int> *mappings = mappings_it->second;
    for (auto it = mappings->begin(); it != mappings->end(); it++) {
      std::cout << "Destroying descriptor " << *it << std::endl;
      if (close(*it) < 0) {
        perror("CleanUpsocketStruct() close");
      }
    }
  }
  close(socket_struct->sock);
  delete socket_struct->addr;
  delete socket_struct;
}

void SendApplicationStateToService(Context *context, std::string server_identifier, int service_identifier) {
  MigrateServer *server = NULL;
  for (auto it = context->servers.begin(); it != context->servers.end(); it++) {
    MigrateServer *s = *it;
    if (s->GetIdentifier() == server_identifier) {
      server = s;
      break;
    }
  }
  std::cout << "SendApplicationStateToService(): Found server " << server_identifier << std::endl;
  if (server != NULL) {
    auto connections = server->GetConnections(service_identifier);

    auto services_it = context->local_services.find(service_identifier);

    if (services_it != context->local_services.end()) {
      Service *service = services_it->second;

      int sock = service->GetLocalSocket();

      for (auto conns_it = connections->begin(); conns_it != connections->end(); conns_it++) {
        Connection *connection = *conns_it;
        int client_identifier = connection->GetConnectionIdentifier();

        std::stringstream msgstream;

        std::string state_data_str = std::string(connection->GetState(), connection->GetStateSize());

        msgstream << "STATE " << service_identifier << " " << client_identifier << " " << state_data_str;

        std::string msg = msgstream.str();

        msgstream.str("");
        msgstream.clear();

        msgstream << msg.length() << " " << msg;

        msg = msgstream.str();

        if (send(sock, msg.c_str(), msg.length(), 0) < 0) {
          perror("SendApplicationStateToService() send");
        }
      }
    }
  }
}

void SendMigrationRequest(Connection *connection, Context *context) {
  std::istringstream is_state(std::string(connection->GetState(), connection->GetStateSize()));
  std::string ip_str;
  std::string port_str;
  std::string tcp_send_seq_str;
  std::string tcp_recv_seq_str;
  std::string mss_clamp_str;
  std::string snd_wscale_str;
  std::string rcv_wscale_str;
  std::string timestamp_str;
  std::string app_info_length_str;
  if (std::getline(is_state, ip_str, ' ') && std::getline(is_state, port_str, ' ') && std::getline(is_state, tcp_send_seq_str, ' ') && std::getline(is_state, tcp_recv_seq_str, ' ') && std::getline(is_state, mss_clamp_str, ' ') && std::getline(is_state, snd_wscale_str, ' ') && std::getline(is_state, rcv_wscale_str, ' ') && std::getline(is_state, timestamp_str, ' ') && std::getline(is_state, app_info_length_str, ' ')) {
    int client_port = std::stoi(context->config->Get(std::string("CLIENT_PORT")));
    std::string local_ip = context->config->Get(std::string("IP_ADDRESS"));

    sockaddr_in addr;

    int sock;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      perror("SendMigrationRequest() socket");
    }

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip_str.c_str());
    addr.sin_port = htons(client_port);

    if (connect(sock, (sockaddr *) &addr, sizeof(addr)) < 0) {
      perror("SendMigrationRequest() connect");
    }

    std::stringstream msgstream;

    msgstream << "MIGRATE " << local_ip << " " << connection->GetServiceIdentifier();

    std::string msg = msgstream.str();

    msgstream.str("");
    msgstream.clear();

    msgstream << msg.length() << " " << msg;

    msg = msgstream.str();

    if (send(sock, msg.c_str(), msg.length(), 0) < 0) {
      perror("SendMigrationRequest() send");
    }
  }
}

void SendSocketRequest(int sock, int service_identifier, int count) {
  std::stringstream msgstream;

  msgstream << "REQ " << service_identifier << " " << count;

  std::string msg = msgstream.str();

  msgstream.str("");
  msgstream.clear();

  msgstream << msg.length() << " " << msg;

  msg = msgstream.str();

  if (send(sock, msg.c_str(), msg.length(), 0) < 0) {
    perror("SendSocketRequest() send");
  }
}

void SendMessage(int sock, int service_identifier, std::string in_msg) {
  std::stringstream msgstream;

  msgstream << in_msg << " " << service_identifier;

  std::string msg = msgstream.str();

  msgstream.str("");
  msgstream.clear();

  msgstream << msg.length() << " " << msg;

  msg = msgstream.str();

  if (send(sock, msg.c_str(), msg.length(), 0) < 0) {
    perror("SendMessage() send");
  }
}

void SendClientMapping(int sock, int service_identifier, int client_identifier, int fd) {
  std::stringstream msgstream;

  msgstream << "MAP " << service_identifier << " " << client_identifier << " " << fd;

  std::string msg = msgstream.str();

  msgstream.str("");
  msgstream.clear();

  msgstream << msg.length() << " " << msg;

  msg = msgstream.str();

  if (send(sock, msg.c_str(), msg.length(), 0) < 0) {
    perror("SendClientMapping() send");
  }
}

std::unordered_map<int, int> RepairSockets(int *fds, int fd_count, int service_identifier, std::vector<Connection *> *clients) {
  int aux_sendq = TCP_SEND_QUEUE;
  int aux_recvq = TCP_RECV_QUEUE;
  int ret;
  std::unordered_map<int, int> mappings;
  auto clients_it = clients->begin();
  for (int i = 0; i < fd_count && clients_it != clients->end(); i++, clients_it++) {
    int fd = fds[i];
    std::cout << "Repairing socket " << fd << std::endl;
    if (TcpRepairOn(fd)) {
      Connection *connection = *clients_it;
      int client_identifier = connection->GetConnectionIdentifier();
      std::cout << "Repairing to: " << std::string(connection->GetState(), connection->GetStateSize()) << std::endl;
      std::istringstream is_state(std::string(connection->GetState(), connection->GetStateSize()));
      std::string ip_str;
      std::string port_str;
      std::string tcp_send_seq_str;
      std::string tcp_recv_seq_str;
      std::string mss_clamp_str;
      std::string snd_wscale_str;
      std::string rcv_wscale_str;
      std::string timestamp_str;
      std::string app_info_length_str;
      if (std::getline(is_state, ip_str, ' ') && std::getline(is_state, port_str, ' ') && std::getline(is_state, tcp_send_seq_str, ' ') && std::getline(is_state, tcp_recv_seq_str, ' ') && std::getline(is_state, mss_clamp_str, ' ') && std::getline(is_state, snd_wscale_str, ' ') && std::getline(is_state, rcv_wscale_str, ' ') && std::getline(is_state, timestamp_str, ' ') && std::getline(is_state, app_info_length_str, ' ')) {
        int remote_port = std::stoi(port_str);
        unsigned int tcp_send_seq = (unsigned int) std::stoul(tcp_send_seq_str);
        unsigned int tcp_recv_seq = (unsigned int) std::stoul(tcp_recv_seq_str);
        sockaddr_in addr;

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(remote_port);
        addr.sin_addr.s_addr = inet_addr(ip_str.c_str());

        connect(fd, (sockaddr *) &addr, sizeof(addr));

        setsockopt(fd, SOL_TCP, TCP_REPAIR_QUEUE, &aux_sendq, sizeof(aux_sendq));
        setsockopt(fd, SOL_TCP, TCP_QUEUE_SEQ, &tcp_send_seq, sizeof(tcp_send_seq));
        setsockopt(fd, SOL_TCP, TCP_REPAIR_QUEUE, &aux_recvq, sizeof(aux_recvq));
        setsockopt(fd, SOL_TCP, TCP_QUEUE_SEQ, &tcp_recv_seq, sizeof(tcp_recv_seq));

        uint32_t mss_clamp = (uint32_t) std::stoul(mss_clamp_str);
        uint32_t snd_wscale = (uint32_t) std::stoul(snd_wscale_str);
        uint32_t rcv_wscale = (uint32_t) std::stoul(rcv_wscale_str);
        uint32_t timestamp = (uint32_t) std::stoul(timestamp_str);

        struct tcp_repair_opt opts[4];

        // SACK
        opts[0].opt_code = TCPOPT_SACK_PERMITTED;
        opts[0].opt_val = 0;

        // Window scales
        opts[1].opt_code = TCPOPT_WINDOW;
        opts[1].opt_val = snd_wscale + (rcv_wscale << 16);

        // Timestamps
        opts[2].opt_code = TCPOPT_TIMESTAMP;
        opts[2].opt_val = 0;

        // MSS clamp
        opts[3].opt_code = TCPOPT_MAXSEG;
        opts[3].opt_val = mss_clamp;

        setsockopt(fd, SOL_TCP, TCP_REPAIR_OPTIONS, opts, 4 * sizeof(struct tcp_repair_opt));

        setsockopt(fd, SOL_TCP, TCP_TIMESTAMP, &timestamp, sizeof(timestamp));

        ret = TcpRepairOff(fd);

        // START OF DEBUG CODE
        sockaddr_in peer_addr;
        socklen_t peer_addr_len = sizeof(peer_addr);

        getpeername(fd, (sockaddr *) &peer_addr, &peer_addr_len);

        char new_ip_str[INET_ADDRSTRLEN];

        inet_ntop(AF_INET, &(peer_addr.sin_addr), new_ip_str, INET_ADDRSTRLEN);

        std::cout << "New peer address after reparation: " << new_ip_str << std::endl;
        // END OF DEBUG CODE

        if (ret < 0) {
          std::cout << "Failed to repair socket " << ip_str << " " << port_str << std::endl;
        } else {
          mappings[client_identifier] = fd;
        }
      }
    } else {
      std::cout << "Error putting socket " << fd << " into repair mode" << std::endl;
    }
  }

  return mappings;
}

bool TcpRepairOn(int fd) {
  int aux = 1;
  if (setsockopt(fd, SOL_TCP, TCP_REPAIR, &aux, sizeof(aux)) < 0) {
    perror("TcpRepairOn");
    return false;
  } else {
    return true;
  }
}

bool TcpRepairOff(int fd) {
  int aux = 0;
  if (setsockopt(fd, SOL_TCP, TCP_REPAIR, &aux, sizeof(aux)) < 0) {
    perror("TcpRepairOff");
    return false;
  } else {
    return true;
  }
}

pthread_mutex_t * GetMutex(Context *context, int service_identifier) {
  pthread_mutex_t *mutex_ptr;
  pthread_mutex_lock(&context->mutex_map_mutex);
  auto it = context->local_service_mappings_mutexes.find(service_identifier);

  if (it != context->local_service_mappings_mutexes.end()) {
    mutex_ptr = it->second;
  } else {
    mutex_ptr = new pthread_mutex_t;
    pthread_mutex_init(mutex_ptr, NULL);
    context->local_service_mappings_mutexes[service_identifier] = mutex_ptr;
  }
  pthread_mutex_unlock(&context->mutex_map_mutex);
  return mutex_ptr;
}

pthread_cond_t * GetCond(Context *context, int service_identifier) {
  pthread_cond_t *cond_ptr;
  pthread_mutex_lock(&context->cond_map_mutex);
  auto it = context->fds_ready_conds.find(service_identifier);

  if (it != context->fds_ready_conds.end()) {
    cond_ptr = it->second;
  } else {
    cond_ptr = new pthread_cond_t;
    pthread_cond_init(cond_ptr, NULL);
    context->fds_ready_conds[service_identifier] = cond_ptr;
  }
  pthread_mutex_unlock(&context->cond_map_mutex);

  return cond_ptr;
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
    in_bytes = recv(socket_struct->sock, buf, MSG_BUFFER_SIZE, 0);
    if (in_bytes < 0) {
      perror("HandleLocalDaemonConnection() recv");
      CleanUpSocketStruct(socket_struct);
      pthread_exit(NULL);
    } else if (in_bytes == 0) {
      std::cout << "Application connection closed" << std::endl;
      CleanUpSocketStruct(socket_struct);
      pthread_exit(NULL);
    }
    std::cout << "LOCALMSG: " << std::string(buf, in_bytes) << std::endl;

    int i = 0;

    while (i < in_bytes) {
      std::stringstream msg_size_ss;

      for (; i < in_bytes; i++) {
        if (buf[i] != ' ') {
          msg_size_ss << buf[i];
        } else {
          break;
        }
      }

      i++;

      std::string msg_size_str = msg_size_ss.str();

      int msg_size = std::stoi(msg_size_ss.str());

      if (msg_size > 3 && strncmp(buf + i, "REG", 3) == 0) {
        std::stringstream service_ident_ss;
        int max_bytes = i + msg_size;

        for (i += 4; i < max_bytes; i++) {
          service_ident_ss << buf[i];
        }
        int service_identifier = std::stoi(service_ident_ss.str());

        socket_struct->service_identifier = service_identifier;

        pthread_mutex_lock(&context->local_services_mutex);
        Service *service;
        auto service_it = context->local_services.find(service_identifier);
        if (service_it == context->local_services.end()) {
          service = new Service(service_identifier);
          context->local_services[service_identifier] = service;
        } else {
          service = service_it->second;
        }
        service->SetLocalSocket(socket_struct->sock);
        pthread_mutex_unlock(&context->local_services_mutex);
      } else if (msg_size > 7 && strncmp(buf + i, "SOCKETS", 7) == 0) {
        std::stringstream fd_count_ss;
        int max_bytes = i + msg_size;

        for (i += 8; i < max_bytes; i++) {
          if (buf[i] != ' ') {
            fd_count_ss << buf[i];
          } else {
            break;
          }
        }

        int fd_count = std::stoi(fd_count_ss.str());

        int *fds = new int[fd_count];

        int rcvd_fd_count = AwaitSocketMessages(socket_struct->sock, fds, fd_count);

        std::cout << "Received " << rcvd_fd_count << " descriptors" << std::endl;
        for (int i = 0; i < rcvd_fd_count; i++) {
          std::cout << "Descriptor " << i << ": " << fds[i] << std::endl;
        }

        pthread_mutex_t *mutex_ptr = GetMutex(context, socket_struct->service_identifier);
        pthread_cond_t *cond_ptr = GetCond(context, socket_struct->service_identifier);

        std::cout << "HandleLocalDaemonConnection: " << mutex_ptr << " " << cond_ptr << std::endl;

        pthread_mutex_lock(&context->local_service_mappings_mutex);
        std::vector<int> *mappings;
        auto local_service_mappings_it = context->local_service_mappings.find(socket_struct->sock);
        if (local_service_mappings_it != context->local_service_mappings.end()) {
          mappings = local_service_mappings_it->second;
        } else {
          mappings = new std::vector<int>();
          context->local_service_mappings[socket_struct->sock] = mappings;
        }
        for (int i = 0; i < rcvd_fd_count; i++) {
          mappings->push_back(fds[i]);
        }
        pthread_mutex_unlock(&context->local_service_mappings_mutex);

        pthread_mutex_lock(mutex_ptr);
        auto service_fds_it = context->service_fds.find(socket_struct->service_identifier);
        int *fds_arr;
        if (service_fds_it != context->service_fds.end()) {
          fds_arr = service_fds_it->second;
          if (fds_arr != NULL) {
            delete [] fds_arr;
          }
        }
        fds_arr = new int[rcvd_fd_count];
        context->service_fds[socket_struct->service_identifier] = fds_arr;
        for (int i = 0; i < rcvd_fd_count; i++) {
          fds_arr[i] = fds[i];
        }
        context->service_fds_ready[socket_struct->service_identifier] = true;
        pthread_cond_signal(cond_ptr);
        pthread_mutex_unlock(mutex_ptr);
        std::cout << "Processed sockets for " << socket_struct->service_identifier << std::endl;
      } else if (msg_size > 6 && strncmp(buf + i, "SOCKET", 6) == 0) {
        i += 7;
        int fd;
        if ((fd = AwaitSocketMessage(socket_struct->sock)) < 0) {
          perror("HandleLocalDaemonConnection() AwaitSocketMessage");
        } else {
          std::cout << "Received descriptor " << fd << std::endl;
          socket_struct->context->local_sockets.push_back(fd);
        }
      } else if (msg_size > 5 && strncmp(buf + i, "STATE", 5) == 0) {
        // State information received from application

        std::stringstream service_ss;
        std::stringstream conn_ident_ss;
        StateData *state_data;
        char *state_data_buf;
        int sz;

        int max_bytes = i + msg_size;

        for (i += 6; i < max_bytes; i++) {
          if (buf[i] != ' ') {
            service_ss << buf[i];
          } else {
            break;
          }
        }

        i++;

        for (; i < max_bytes; i++) {
          if (buf[i] != ' ') {
            conn_ident_ss << buf[i];
          } else {
            break;
          }
        }

        i++;

        if (max_bytes > i + 1) {
          sz = max_bytes - i;
          state_data_buf = new char[sz];
          int j;
          for (j = 0; i < max_bytes; i++, j++) {
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

        service->SetLocalSocket(socket_struct->sock);

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
          std::cout << debug_service_id << std::endl << "----------------------" << std::endl;
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

        msgstream.str("");
        msgstream.clear();

        int msg_size = msg.length();

        msgstream << msg_size << " " << msg;

        msg = msgstream.str();

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
    if ((in_bytes = recvfrom(sock, buf, MSG_BUFFER_SIZE, 0, (struct sockaddr *) &addr, &addrlen)) < 0) {
      perror("StartHeartbeatListener() recvfrom");
    }
    int i = 0;
    std::cout << "MSG: " << std::string(buf, in_bytes) << std::endl;
    while (i < in_bytes) {
      std::stringstream msg_size_ss;
      for (; i < in_bytes; i++) {
        if (buf[i] != ' ') {
          msg_size_ss << buf[i];
        } else {
          break;
        }
      }

      i++;

      int msg_size = std::stoi(msg_size_ss.str());
      if (msg_size > 5 && strncmp(buf + i, "STATE", 5) == 0) {
        std::stringstream ident_ss;
        std::stringstream service_ss;
        std::stringstream conn_ident_ss;
        int sz;
        char *state_data;
        int max_bytes = i + msg_size;

        for (i += 6; i < max_bytes; i++) {
          if (buf[i] != ' ') {
            ident_ss << buf[i];
          } else {
            break;
          }
        }

        i++;

        for (; i < max_bytes; i++) {
          if (buf[i] != ' ') {
            service_ss << buf[i];
          } else {
            break;
          }
        }

        i++;

        for (; i < max_bytes; i++) {
          if (buf[i] != ' ') {
            conn_ident_ss << buf[i];
          } else {
            break;
          }
        }

        i++;

        if (max_bytes > i + 1) {
          sz = max_bytes - i;
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

        std::cout << "Servers (on " << GetIdentifier() << "):" << std::endl;

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

        server->SetCounter(context->counter);

        std::cout << "Adding new connection " << service_identifier << " " << connection_identifier << " " << state_data << std::endl;
        server->AddOrUpdateConnection(service_identifier, connection_identifier, state_data, sz);
        pthread_mutex_unlock(&context->servers_mutex);
      }
    }
  }

  return NULL;
}

void * StartFailureDetector(void *c) {
  std::cout << "Starting Failure Detector" << std::endl;
  Context *context = (Context*) c;

  MigrateServer *server;

  while (1) {
    for (auto it = context->servers.begin(); it != context->servers.end(); it++) {
      server = *it;

      if (server->GetStatus() == 1 && context->counter - server->GetCounter() > 10) {
        server->SetStatus(0);
        std::cout << server->GetIdentifier() << " has failed!" << std::endl;
        std::vector<int> services = server->GetServices();
        for (auto services_it = services.begin(); services_it != services.end(); services_it++) {
          int service_identifier = *services_it;
          auto local_service = context->local_services[service_identifier];

          int sock = local_service->GetLocalSocket();

          auto clients = server->GetConnections(service_identifier);

          // TODO: Request for sockets from local service
          context->service_fds_ready[service_identifier] = false;
          SendSocketRequest(sock, service_identifier, clients->size());
          std::cout << "Socket request sent" << std::endl;
          pthread_mutex_t *mutex_ptr = GetMutex(context, service_identifier);
          pthread_cond_t *cond_ptr = GetCond(context, service_identifier);
          std::cout << "Failure Detector: " << mutex_ptr << " " << cond_ptr << std::endl;
          pthread_mutex_lock(mutex_ptr);
          std::cout << "Failure Detector: obtained mutex" << std::endl;
          while (!context->service_fds_ready[service_identifier]) {
            std::cout << "Waiting for sockets to be processed" << std::endl;
            pthread_cond_wait(cond_ptr, mutex_ptr);
          }
          context->service_fds_ready[service_identifier] = false;
          pthread_mutex_unlock(mutex_ptr);
          std::cout << "Sockets processed, back to failure detector" << std::endl;
          int *fds = NULL;
          if (context->service_fds.find(service_identifier) != context->service_fds.end()) {
            fds = context->service_fds[service_identifier];
          }
          if (fds != NULL) {
            // TODO: Send application state to local service
            // TODO: Repair the sockets
            std::cout << "Repairing sockets" << std::endl;
            auto mappings = RepairSockets(fds, clients->size(), service_identifier, clients);
            std::cout << "Repaired sockets" << std::endl;
            // TODO: Send mappings of sockets to client identifiers
            for (auto mappings_it = mappings.begin(); mappings_it != mappings.end(); mappings_it++) {
              int client_identifier = mappings_it->first;
              int fd = mappings_it->second;
              std::cout << "Sending mapping " << client_identifier << " " << fd << std::endl;
              SendClientMapping(sock, service_identifier, client_identifier, fd);
            }
            SendApplicationStateToService(context, server->GetIdentifier(), service_identifier);
            SendMessage(sock, service_identifier, std::string("DONE"));
            for (auto clients_it = clients->begin(); clients_it != clients->end(); clients_it++) {
              Connection *connection = *clients_it;
              SendMigrationRequest(connection, context);
            }
          }
        }
      } else if (server->GetStatus() == 1) {
        std::cout << server->GetIdentifier() << " is alive" << std::endl;
      }
      context->counter++;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return NULL;
}

void InitServer(Context *context) {
  pthread_t hb_sender_pthread;
  pthread_t hb_listener_pthread;
  pthread_t local_daemon_pthread;
  pthread_t failure_detector_pthread;

  pthread_create(&hb_sender_pthread, NULL, StartHeartbeatSender, (void *) context);
  pthread_create(&hb_listener_pthread, NULL, StartHeartbeatListener, (void *) context);
  pthread_create(&local_daemon_pthread, NULL, StartLocalDaemon, (void *) context);
  pthread_create(&failure_detector_pthread, NULL, StartFailureDetector, (void *) context);

  pthread_join(hb_sender_pthread, NULL);
  pthread_join(hb_listener_pthread, NULL);
  pthread_join(local_daemon_pthread, NULL);
  pthread_join(failure_detector_pthread, NULL);
}

int main() {
  Context *context = new Context;
  std::vector<MigrateServer *> servers;
  pthread_mutex_init(&context->local_services_mutex, NULL);
  pthread_mutex_init(&context->local_service_mappings_mutex, NULL);
  pthread_mutex_init(&context->servers_mutex, NULL);
  pthread_mutex_init(&context->mutex_map_mutex, NULL);
  pthread_mutex_init(&context->cond_map_mutex, NULL);
  context->servers = servers;
  Configuration *config = new Configuration(std::string(DEFAULT_CONFIG_FILE));
  config->PrintMappings();
  context->config = config;
  InitServer(context);
  return 0;
}
