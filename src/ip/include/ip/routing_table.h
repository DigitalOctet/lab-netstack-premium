/**
 * @file routing_table.h
 * @brief Defines routing table class.
 */

#pragma once

#include <ethernet/device_manager.h>
#include <ethernet/frame.h>
#include "packet.h"
#include <netinet/ip.h>
#include <mutex>
#include <vector>

/**
 * @brief Routing table entry.
 */
typedef struct{
    struct in_addr IP_addr;
    struct in_addr mask;
    int device_id;
}Entry;

class DeviceManager;

/**
 * @brief My routing table class.
 */
class RoutingTable
{
private:
    std::mutex table_mutex;
    std::vector<Entry> routing_table;

    // For link state
    unsigned int seq;
    std::mutex neighbor_mutex;
    std::mutex link_state_mutex;
    std::vector<std::pair<struct in_addr, unsigned int>> neighbors;
    std::vector<LinkStatePacket *> link_state_list;

    // For IP
    std::vector<struct in_addr> my_IP_addrs;
    std::vector<struct in_addr> masks;
    std::vector<int> device_ids;
    DeviceManager *device_manager;

    friend class NetworkLayer;

    void shortest_path();
public:
    RoutingTable(DeviceManager *dm);
    ~RoutingTable();
    int findEntry(struct in_addr addr);
    int setMyIP();
    bool findMyIP(struct in_addr addr);
    void updateStates();
};
