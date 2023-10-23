/**
 * @file send_frame.cpp
 * @brief (Checkpoint 2) Inject 20 frames to veth1-2.
 * 
 * The virtual network I build here is totally the same as the vnet shown in 
 * the examples in vnetUtils/README.md.
 * 
 *     ns1 -- ns2 -- ns3 -- ns4
 *                    |
 *                   ns0
 */

#include "utils.h"
#include <iostream>

int main()
{
    DeviceManager device_manager(NULL);
    int device_id = device_manager.addDevice(SRC_DEVICE);
    if(device_id == -1){
        std::cerr << "Add " << SRC_DEVICE << " failed!" << std::endl;
        return 0;
    }

    int len = strlen(payload) + 1;
    for(int i = 0; i < 20; i++){
        device_manager.sendFrame(payload, len, ETHTYPE_IPv4, 
                                 veth2_1_mac, device_id);
    }

    return 0;
}