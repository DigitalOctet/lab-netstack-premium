/**
 * @file routing_table.cpp
 */

#include "routing_table.h"
#include <ifaddrs.h>

/**
 * @brief Default constructor of `RoutingTable`.
 */
RoutingTable::RoutingTable(DeviceManager *device_manager): 
    routing_table(), link_state_table(), my_IP_addr()
{
    this->device_manager = device_manager;
    setMyIP();
}

/**
 * @brief Find the device ID in the routing table to send the packet whose 
 * destination IPv4 address is `addr`.
 * 
 * @param addr Destination IPv4 address.
 * @return Device ID on success, -1 if not found.
 */
int 
RoutingTable::findEntry(struct in_addr addr)
{
    int device_id = -1;
    struct in_addr longest_prefix = {0};
    for(auto &entry: routing_table){
        if(addr.s_addr & entry.mask.s_addr == entry.IP_addr.s_addr){
            if(((~entry.mask.s_addr) & (longest_prefix.s_addr)) == 0){
                longest_prefix = entry.IP_addr;
                device_id = entry.device_id;
            }
        }
    }
    return device_id;
}

/**
 * @brief Find all IP addresses on the host machine. And set IP addresses of 
 * every device.
 * @return 0 on success, -1 on error.
 */
int 
RoutingTable::setMyIP()
{
    struct ifaddrs *ifap, *ifa;
    struct sockaddr_in *addr;
    char ip[IPv4_ADDR_LEN];

    if (getifaddrs(&ifap) == -1) {
        perror("getifaddrs");
        return -1;
    }

    bool lo_out = false;
    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_INET){
            if(!lo_out && !strcmp(ifa->ifa_name, "lo")){
                lo_out = true;
                continue;
            }
            addr = (struct sockaddr_in *)ifa->ifa_addr;
            my_IP_addr.push_back(addr->sin_addr);
            device_manager->setIP(addr->sin_addr, ifa->ifa_name);
        }
    }

    freeifaddrs(ifap);
}

/**
 * @brief Find whether the destination of the packet is on the host machine.
 * @param addr IPv4 address of the packet.
 * @return true on success, false on failure.
 */
bool 
RoutingTable::findMyIP(struct in_addr addr)
{
    for(auto &i: my_IP_addr){
        if(i.s_addr == addr.s_addr){
            return true;
        }
    }
    return false;
}