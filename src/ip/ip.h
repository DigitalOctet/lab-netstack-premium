/**
 * @file ip.h
 * @brief Library supporting sending/receiving IPv4 packets encapsulated
 * in an Ethernet II frame.
 */

#ifndef IP_IP_H
#define IP_IP_H

#include "routing_table.h"
#include <netinet/ip.h>
#include <chrono>
#include <mutex>
#include <thread>

/**
 * @brief Process an IP packet upon receiving it.
 *
 * @param buf Pointer to the packet.
 * @param len Length of the packet.
 * @return 0 on success, -1 on error.
 * @see addDevice
 */
typedef int (* IPPacketReceiveCallback)(const void* buf , int len);

class DeviceManager;

/**
 * @brief Class supporting sending/receiving IPv4 packets encapsulated
 * in an Ethernet II frame.
 */
class NetworkLayer
{
private:
    DeviceManager device_manager;
    IPPacketReceiveCallback callback;
    RoutingTable routing_table;
    std::thread timer_thread;
    std::mutex timer_mutex;
    bool timer_running;
    void timerCallback(int interval_milliseconds);
    void startTimer(int interval_milliseconds);
    void stopTimer();
    bool handleHello(const u_char *buf, int len, int device_id);
    bool handleLinkState(const u_char *buf, int len, int device_id);
public:
    NetworkLayer();
    ~NetworkLayer();
    int sendIPPacket(const struct in_addr src, const struct in_addr dest,
                     int proto, const void* buf, int len);
    int setIPPacketReceiveCallback(IPPacketReceiveCallback callback);
    int setRoutingTable(const struct in_addr dest, const struct in_addr mask,
                        const void* nextHopMAC, const char* device);
    int callBack(const u_char *buf, int len, int device_id);
    bool sendHelloPacket();
    bool sendLinkStatePacket();
};

#endif /**< IP_IP_H */