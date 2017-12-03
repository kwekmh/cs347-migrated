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
#define DEFAULT_STATE_UPDATE_INTERVAL 1
#define DEFAULT_FAILURE_INTERVAL 10

#include <vector>
#include <unordered_map>

#include "service.h"

typedef struct Context {
  std::vector<int> local_sockets;
  std::unordered_map<int, std::vector<int> *>local_service_mappings;
  std::unordered_map<int, Service *> local_services;
  std::vector<MigrateServer *> servers;
  std::unordered_map<int, pthread_mutex_t *> local_service_mappings_mutexes;
  std::unordered_map<int, pthread_cond_t *> fds_ready_conds;
  std::unordered_map<int, int *> service_fds;
  std::unordered_map<int, bool> service_fds_ready;
  Configuration *config;
  pthread_mutex_t local_services_mutex;
  pthread_mutex_t local_service_mappings_mutex;
  pthread_mutex_t servers_mutex;
  pthread_mutex_t mutex_map_mutex;
  pthread_mutex_t cond_map_mutex;
  int counter;
  int state_update_interval;
  int failure_interval;
} Context;

typedef struct LocalDaemonSocket {
  int sock;
  sockaddr_un *addr;
  Context *context;
  int service_identifier;
} LocalDaemonSocket;

void InitServer();
void CleanUpSocketStruct(LocalDaemonSocket *socket_struct);
int AwaitSocketMessage(int sock);
int AwaitSocketMessages(int sock, int *fds, int fd_count);
void SendApplicationStateToService(Context *context, std::string server_identifier, int service_identifier);
void SendMigrationRequest(Connection *connection, Context *context);
void SendSocketRequest(int sock, int count);
void SendMessage(int sock, int service_identifier, std::string in_msg);
void SendClientMapping(int sock, int service_identifier, int client_identifier, int fd);
std::unordered_map<int, int> RepairSockets(int fds, int fd_count, std::unordered_map<int, StateData *> *clients);
bool TcpRepairOn(int fd);
bool TcpRepairOff(int fd);
pthread_mutex_t * GetMutex(Context *context, int service_identifier);
pthread_cond_t * GetCond(Context *context, int service_identifier);
void * HandleLocalDaemonConnection(void *s);
void * StartLocalDaemon(void *c);
void * StartHeartbeatSender(void *c);
void * StartHeartbeatListener(void *c);
void * StartFailureDetector(void *c);

#endif
