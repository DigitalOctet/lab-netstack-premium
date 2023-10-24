/**
 * @file routing_table.h
 * @brief Defines routing table class.
 */

#ifndef IP_ROUTING_TABLE_H
#define IP_ROUTING_TABLE_H

#include "../ethernet/device_manager.h"
#include "../ethernet/frame.h"
#include <netinet/ip.h>
#include <vector>

/**
 * @brief My routing table class.
 */
class RoutingTable
{
private:
    typedef struct{
        struct in_addr IP_addr;
        struct in_addr mask;
        int device_id;
    }Entry;

    typedef struct{
        struct in_addr router_id; // IP addr of device 0
        int distance;
    }LinkState;

    std::vector<Entry> routing_table;
    std::vector<LinkState> link_state_table;
    std::vector<struct in_addr> my_IP_addr;
    DeviceManager *device_manager;
public:
    RoutingTable(DeviceManager *device_manager);
    ~RoutingTable() = default;
    int findEntry(struct in_addr addr);
    int setMyIP();
    bool findMyIP(struct in_addr addr);
};

#endif /**< IP_ROUTING_TABLE_H */