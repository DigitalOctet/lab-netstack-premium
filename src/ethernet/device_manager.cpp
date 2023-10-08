/**
 * @file device_manager.cpp
 */

#include "device_manager.h"
#include <iostream>

/**
 * @brief Constructor of `DeviceManager`.
 */
DeviceManager::DeviceManager(): name2id(), id2device(), 
                                next_device_ID(0), callback(NULL)
{
    int ret = 0;
    char errbuf[PCAP_ERRBUF_SIZE] = "";
    devsp = NULL;
    ret = pcap_findalldevs(&devsp, errbuf);
    if(ret != 0 || devsp == NULL){
        std::cerr << "No device available!" << std::endl;
        return;
    }
}

/**
 * @brief Destructor of `DeviceManager`. Free memory allocated by `addDevice`.
 */
DeviceManager::~DeviceManager()
{
    pcap_freealldevs(devsp);

    // TODO: try auto
    for(int2device_map::iterator id = id2device.begin(); 
        id != id2device.end(); id++)
    {
        delete id->second;
    }
}

/**
 * @brief Helper function that checks whether `device` exits.
 * 
 * @param device Name of network device to check.
 * @return 
 */
bool
DeviceManager::containDevice(const char *device)
{
    for(pcap_if_t *temp = devsp; temp != NULL; temp = temp->next){
        if(!strcmp(device, temp->name)){
            return true;
        }
    }
    return false;
}

/**
 * @brief Add a device to the library for sending/receiving packets.
 *
 * @param device Name of network device to send/receive packet on.
 * @return A non-negative _device-ID_ on success, `-1` on error.
 */
int 
DeviceManager::addDevice(const char* device)
{
    if(!containDevice(device)){
        std::cerr << device << "is not one of all devices!" << std::endl;
        return -1;
    }

    std::string device_name = device;
    string2int_map::iterator it = name2id.find(device_name);
    if(it != name2id.end()){
        std::cerr << "Device " << device << " is already added!" << std::endl;
        return -1;
    }
    else{
        name2id[device_name] = next_device_ID;
        id2device[next_device_ID] = new Device(device);
        return next_device_ID++;
    }
}

/**
 * @brief Find a device added by `addDevice`.
 *
 * @param device Name of the network device.
 * @return A non-negative _device-ID_ on success, `-1` if no such device
 * was found.
 */
int 
DeviceManager::findDevice(const char* device)
{
    std::string device_name = device;
    string2int_map::iterator it = name2id.find(device_name);
    if(it != name2id.end()){
        return it->second;
    }
    else{
        std::cerr << "Device" << device << "not found!" << std::endl;
        return -1;
    }
}

/**
* @brief Encapsulate some data into an Ethernet II frame and send it.
*
* @param buf Pointer to the payload.
* @param len Length of the payload.
* @param ethtype EtherType field value of this frame.
* @param destmac MAC address of the destination.
* @param id ID of the device(returned by `addDevice`) to send on.
* @return 0 on success, -1 on error.
* @see addDevice
*/
int 
DeviceManager::sendFrame(const void* buf, int len, int ethtype, 
                         const void* destmac, int id)
{
    int2device_map::iterator it = id2device.find(id);
    if(it != id2device.end()){
        return it->second->sendFrame(buf, len, ethtype, destmac);
    }
    else{
        std::cerr << "No device " << id << "!" << std::endl;
        return -1;
    }
}

/**
* @brief Register a callback function to be called each time an
* Ethernet II frame was received.
*
* @param callback the callback function.
* @return 0 on success, -1 on error.
* @see frameReceiveCallback
*/
int 
DeviceManager::setFrameReceiveCallback(frameReceiveCallback callback)
{
    this->callback = callback;
    return 0;
}