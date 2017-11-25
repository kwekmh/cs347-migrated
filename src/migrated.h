#ifndef MIGRATED_MIGRATED_H_
#define MIGRATED_MIGRATED_H_

#include <memory>
#include <vector>
#include <cstdint>
#include <pthread.h>

#include "configuration.h"
#include "migrateserver.h"
#include "connection.h"

#define STR_VALUE(arg) #arg

#define MSG_BUFFER_SIZE 256
#define DAEMON_DATA_DIRECTORY STR_VALUE(/var/migrated)
#define DAEMON_SOCKET_FILENAME STR_VALUE(local-socket)
#define DAEMON_MAX_CONNECTIONS 500
#define SOCK_BUF_MAX_SIZE 960

#define DEFAULT_CONFIG_FILE STR_VALUE(/etc/migrated/config)

#include <vector>
#include <unordered_map>

#include "service.h"

typedef struct Context {
  std::vector<int> local_sockets;
  std::unordered_map<int, Service *> local_services;
  std::vector<MigrateServer *> servers;
  Configuration *config;
  pthread_mutex_t local_services_mutex;
  pthread_mutex_t servers_mutex;
  int counter;
} Context;

typedef struct LocalDaemonSocket {
  int sock;
  sockaddr_un *addr;
  Context *context;
} LocalDaemonSocket;

void InitServer();
void CleanUpSocketStruct(LocalDaemonSocket *socket_struct);
int AwaitSocketMessage(int sock);
int AwaitSocketMessages(int sock, int *fds, int fd_count);
void SendApplicationStateToService(Context *context, int service_identifier);
void * HandleLocalDaemonConnection(void *s);
void * StartLocalDaemon(void *c);
void * StartHeartbeatSender(void *c);
void * StartHeartbeatListener(void *c);
void * StartFailureDetector(void *c);

#endif
