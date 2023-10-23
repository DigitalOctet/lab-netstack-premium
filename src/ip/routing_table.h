/**
 * @file routing_table.h
 * @brief Defines routing table class.
 */

#ifndef IP_ROUTING_TABLE_H
#define IP_ROUTING_TABLE_H

#include "../ethernet/frame.h"
#include <netinet/ip.h>

class RoutingTable
{
private:
    struct in_addr IP_addr;
    struct in_addr mask;
    int device_id;
public:
    RoutingTable();
    ~RoutingTable();
};

#endif /**< IP_ROUTING_TABLE_H */