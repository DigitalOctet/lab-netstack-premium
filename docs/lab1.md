# Lab 1: Link-layer

## Writing Task 1 (WT1)

1. How many frames are there in the filtered results? (Hint: see the status bar)
    827 in total.

2. What is the destination address of this Ethernet frame and what makes this address special?
    `ff:ff:ff:ff:ff:ff`. This is a special address reserved for broadcasting.
    
3. What is the 71th byte (count from 0) of this frame?
    0x15.

## Programming Task 1 & 2 (PT1 & PT2)

```
-src/
--ethernet/
---device_manager.h/device_manager.cpp
-----DeviceManager::addDevice
-----DeviceManager::findDevice
-----DeviceManager::sendFrame
-----DeviceManager::setFrameReceiveCallback
---device.h/device.cpp
-----Device::sendFrame
```

##