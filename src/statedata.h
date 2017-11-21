#ifndef MIGRATED_STATEDATA_H_
#define MIGRATED_STATEDATA_H_

#include <cstdlib>

class StateData {
  char *m_data;
  std::size_t m_size;

public:
  StateData(char *data, std::size_t size);
  ~StateData();
  char * GetData();
  void SetData(char *data);
  size_t GetSize();
  void SetSize(std::size_t size);
};

#endif
