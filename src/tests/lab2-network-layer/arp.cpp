/**
 * @file arp.cpp
 */

#include "../lab1-link-layer/utils.h"
#include <iostream>

int main()
{
    DeviceManager device_manager(NULL);
    device_manager.addAllDevice();

    for(int i = 0; i < 20; i++){
        device_manager.requestARP();
    }

    return 0;
}