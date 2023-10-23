/**
 * @file routing_table.h
 * @brief Defines routing table class.
 */

#ifndef IP_ROUTING_TABLE_H
#define IP_ROUTING_TABLE_H

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
    }entry;
    std::vector<entry> table;
public:
    RoutingTable();
    ~RoutingTable() = default;
};

#endif /**< IP_ROUTING_TABLE_H */