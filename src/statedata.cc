#include <cstdlib>

#include "statedata.h"

StateData::StateData(char *data, std::size_t size) {
  this->m_data = data;
  this->m_size = size;
}

StateData::~StateData() {
  delete [] this->m_data;
}

char * StateData::GetData() {
  return this->m_data;
}

void StateData::SetData(char *data) {
  char *tmp = this->m_data;
  this->m_data = data;
  delete [] tmp;
}

std::size_t StateData::GetSize() {
  return this->m_size;
}

void StateData::SetSize(size_t size) {
  this->m_size = size;
}
