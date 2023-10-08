/**
 * @file device_manager.h
 * @brief Class supporting network device management.
 */

#ifndef ETHERNET_DEVICE_MANAGER_H
#define ETHERNET_DEVICE_MANAGER_H

#include "device.h"
#include <cstring>
#include <map>
#include <string>
#include <pcap.h>

/**
 * Some typedef's for map.
 * TODO: Try using `auto` and change this method.
 */
typedef std::map<std::string, int> string2int_map;
typedef std::map<int, Device*> int2device_map;

/**
* @brief Process a frame upon receiving it.
*
* @param buf Pointer to the frame.
* @param len Length of the frame.
* @param id ID of the device (returned by `addDevice`) receiving current frame.
* @return 0 on success, -1 on error.
* @see addDevice
*/
typedef int (* frameReceiveCallback)(const void*, int, int);

/**
 * @brief Class supporting network device management.
 */
class DeviceManager
{
private:
    pcap_if_t *devsp;
    int next_device_ID;
    string2int_map name2id;
    int2device_map id2device;
    frameReceiveCallback callback;
    bool containDevice(const char *device);
public:
    DeviceManager();
    ~DeviceManager();
    int addDevice(const char* device);
    int findDevice(const char* device);
    int sendFrame(const void* buf, int len, int ethtype, 
                  const void* destmac, int id);
    int setFrameReceiveCallback(frameReceiveCallback callback);
};

#endif /**< ETHERNET_DEVICE_MANAGER_H */