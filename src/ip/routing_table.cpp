/**
 * @file routing_table.cpp
 */

#include "routing_table.h"
#include <ifaddrs.h>

/**
 * @brief Default constructor of `RoutingTable`.
 */
RoutingTable::RoutingTable(DeviceManager *dm): 
    routing_table(), my_IP_addrs(), link_state_list(), neighbors(), 
    device_manager(dm), seq(0), device_ids()
{
    setMyIP();
}

/**
 * @brief Destructor of `RoutingTable`. Delete all pointers of 
 * `link_state_list` to free memory allocated on receiving link state packets
 */
RoutingTable::~RoutingTable()
{
    while(!link_state_list.empty()){
        LinkStatePacket *link_state = link_state_list.back();
        delete link_state;
        link_state_list.pop_back();
    }
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
    table_mutex.lock();
    for(auto &entry: routing_table){
        if(addr.s_addr & entry.mask.s_addr == entry.IP_addr.s_addr){
            if(((~entry.mask.s_addr) & (longest_prefix.s_addr)) == 0){
                longest_prefix = entry.IP_addr;
                device_id = entry.device_id;
            }
        }
    }
    table_mutex.unlock();
    return device_id;
}

/**
 * @brief Find all IP addresses on the host machine. And set IP addresses of 
 * every device.
 * @return 0 on success, -1 on error.
 * @note In my implementation, IP addresses are considered to be configured 
 * previously by default. Here, I use "ifaddrs.h" to get IP addresses instead.
 */
int 
RoutingTable::setMyIP()
{
    struct ifaddrs *ifap, *ifa;
    struct sockaddr_in *addr;

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
            my_IP_addrs.push_back(addr->sin_addr);
            addr = (struct sockaddr_in *)ifa->ifa_netmask;
            masks.push_back(addr->sin_addr);
            device_manager->setIP(addr->sin_addr, ifa->ifa_name);
            device_ids.push_back(device_manager->findDevice(ifa->ifa_name));
        }
    }

    freeifaddrs(ifap);
    return 0;
}

/**
 * @brief Find whether the destination of the packet is on the host machine.
 * @param addr IPv4 address of the packet.
 * @return true on success, false on failure.
 */
bool 
RoutingTable::findMyIP(struct in_addr addr)
{
    for(auto &i: my_IP_addrs){
        if(i.s_addr == addr.s_addr){
            return true;
        }
    }
    return false;
}

/**
 * @brief
 */
void 
RoutingTable::updateStates()
{
    // Minus age field in `neighbors` and `link_state_list`
    neighbor_mutex.lock();
    for(auto it = neighbors.begin(); it != neighbors.end(); ){
        if(it->second == 0){
            it = neighbors.erase(it);
        }
        else{
            it->second -= 10;
            it++;
        }
    }
    neighbor_mutex.unlock();
    link_state_mutex.lock();
    for(auto it = link_state_list.begin(); it != link_state_list.end(); ){
        if((*it)->age == 0){
            it = link_state_list.erase(it);
        }
        else{
            (*it)->age -= 10;
            it++;
        }
    }

    // Update routing table
    neighbor_mutex.lock();
    shortest_path();
    link_state_mutex.unlock();
}

/**
 * @brief Dijkstra
 */
void 
RoutingTable::shortest_path()
{
    #define MAX_NODES 1024
    int dist[MAX_NODES][MAX_NODES];
    memset(dist, 0, MAX_NODES * MAX_NODES);
    enum l{
        permanent,
        tentative
    };
    struct state{
        int predecessor;
        int length;
        l label;
    }state[MAX_NODES];
    int i, k, min;
    int n = link_state_list.size() + 1;
    struct state *p;
    for(p = &state[0]; p < &state[n]; p++){
        p->predecessor = -1;
        p->length = MAX_NODES;
        p->label = tentative;
    }
    state[0].length = 0;
    state[0].label = permanent;
    k = 0;
    for(int j = 0; j < n - 1; j++){
        for(i = 0; i < n; i++){
            if(dist[k][i] != 0 && state[i].label == tentative){
                if(state[k].length + dist[k][i] < state[i].length){
                    state->predecessor = k;
                    k = i;
                }
            }
        }
        state[k].label = permanent;
    }
    routing_table.clear();
    for(int j = 1; j < n; j++){
        k = j;
        if(state[k].predecessor == -1) 
            continue;
        if(state[k].predecessor == 0){
            for(auto &ip: link_state_list[j - 1]->router_id){
                Entry e;
                e.device_id = k;
                e.IP_addr = ip;
                e.mask.s_addr = IPv4_ADDR_BROADCAST;
                routing_table.push_back(e);
            }
            continue;
        }
        while(state[k].predecessor != 0){
            k = state[k].predecessor;
        }
        for(int i = 0; i < link_state_list[j - 1]->router_id.size(); i++){
            Entry e;
            e.device_id = k;
            e.IP_addr = link_state_list[j - 1]->router_id[i];
            e.mask = link_state_list[j - 1]->mask[i];
            bool exist =false;
            for(auto &entry: routing_table){
                if(entry.IP_addr.s_addr == e.IP_addr.s_addr &&
                   entry.mask.s_addr == e.mask.s_addr)
                {
                    exist = true;
                    break;
                }
            }
            if(!exist){
                routing_table.push_back(e);
            }
        }
    }
}