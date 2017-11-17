#ifndef MIGRATED_NEIGHBOURHOOD_H_
#define MIGRATED_NEIGHBOURHOOD_H_

#include <string>

#define NEIGHBOURHOOD_MULTICAST_GROUP "239.255.1.1"
#define NEIGHBOURHOOD_MULTICAST_PORT 12500

void SendMessageToNeighbourhood(std::string message);

#endif
