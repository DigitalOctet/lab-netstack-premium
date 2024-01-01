/**
 * @file send_segment.cpp
 * @brief Send a segment from ns1 to ns4. Test routing a segment.
 */

#include <ip/ip.h>
#include <tcp/tcb.h>
#include <tcp/tcp.h>

int main(){
    TransportLayer transport_layer;
    TCB tcb;
    tcb.src_addr.s_addr = 0x0101640a;
    tcb.src_port = 0x0008;
    tcb.dst_addr.s_addr = 0x0203640a;
    tcb.dst_port = 0x0008;
    char msg[10] = "012345678";
    transport_layer.sendSegment(&tcb, SegmentType::ACK, msg, strlen(msg));
    // transport_layer.network_layer->sendIPPacket(tcb.src_addr, tcb.dst_addr, IPPROTO_TCP, msg, strlen(msg));
    while(true){};
}