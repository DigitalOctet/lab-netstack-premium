/**
 * @file detectNIC.cpp
 * @brief (Checkpoint 1) Show all the network interfaces on the host.
 */

#include <ethernet/device_manager.h>

int main()
{
    DeviceManager device_manager(NULL);
    device_manager.listAllDevice();
    return 0;
}