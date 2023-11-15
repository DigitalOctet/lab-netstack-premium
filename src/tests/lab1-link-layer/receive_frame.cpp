/**
 * @file receive_frame.cpp
 * @brief (Checkpoint 2) Capture frames from veth2-1. In this scenario, there
 * are 20 frames sent from veth1-2.
 */

#include <ethernet/endian.h>
#include "utils.h"
#include <iostream>

int main()
{
    DeviceManager device_manager(NULL);
    int device_id = device_manager.addDevice(DST_DEVICE);
    if(device_id == -1){
        std::cerr << "Add " << DST_DEVICE << " failed!" << std::endl;
        return 0;
    }
    
    device_manager.setFrameReceiveCallback(test_callback, device_id);
    if(device_manager.capLoop(device_id, 20) != 0){
        std::cerr << "Capture failed!" << std::endl;
    }
    return 0;
}