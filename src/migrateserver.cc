#include <iostream>
#include <algorithm>

#include "migrateserver.h"
#include "connection.h"

MigrateServer::MigrateServer(std::string identifier) {
  this->m_identifier = identifier;
  this->m_status = 1;
}

MigrateServer::~MigrateServer() {

}

std::string MigrateServer::GetIdentifier() {
  return this->m_identifier;
}

void MigrateServer::SetIdentifier(std::string identifier) {
  this->m_identifier = identifier;
}

void MigrateServer::AddService(int service) {
  this->m_services.push_back(service);
}

bool MigrateServer::HasService(int service) {
  return std::find(this->m_services.begin(), this->m_services.end(), service) != this->m_services.end();
}

std::vector<int> MigrateServer::GetServices() {
  return this->m_services;
}

void MigrateServer::AddConnection(int service, Connection *conn) {
  auto conns = this->m_connections.find(service);
  if (conns != this->m_connections.end()) {
    conns->second->push_back(conn);
  }
}

void MigrateServer::AddOrUpdateConnection(int service_identifier, int connection_identifier, char *state, int state_size) {
  if (!this->HasService(service_identifier)) {
    this->AddService(service_identifier);
  }
  auto conns = this->GetConnections(service_identifier);
  if (conns == NULL) {
    conns = new std::vector<Connection *>();
    this->m_connections[service_identifier] = conns;
  }

  std::vector<Connection *>::iterator it;
  Connection *conn = NULL;

  for (it = conns->begin(); it != conns->end(); it++) {
    if ((*it)->GetServiceIdentifier() == service_identifier && (*it)->GetConnectionIdentifier() == connection_identifier) {
      conn = *it;
      break;
    }
  }

  if (conn != NULL) {
    std::cout << "AddOrUpdateConnection(): Updating state" << std::endl;
    conn->SetState(state);
    conn->SetStateSize(state_size);
  } else {
    std::cout << "AddOrUpdateConnection(): New connection" << std::endl;
    Connection *conn = new Connection(service_identifier, connection_identifier, state, state_size);
    conns->push_back(conn);
  }
}

std::vector<Connection *> * MigrateServer::GetConnections(int service) {
  auto conns = this->m_connections.find(service);
  if (conns != this->m_connections.end()) {
    return conns->second;
  } else {
    return NULL;
  }
}

int MigrateServer::GetCounter() {
  return this->m_counter;
}

void MigrateServer::SetCounter(int counter) {
  this->m_counter = counter;
}

void MigrateServer::IncrementCounter() {
  this->m_counter++;
}

int MigrateServer::GetStatus() {
  return this->m_status;
}

void MigrateServer::SetStatus(int status) {
  this->m_status = status;
}
