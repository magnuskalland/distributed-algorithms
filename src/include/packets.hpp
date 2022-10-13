#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

/* message types */
#define PAYLOAD 0x00
#define ACK 0x01

/* packet size constants */
#define MAX_PAYLOAD_SIZE 4
#define APP_PKT_SIZE(pkt) (MAX_PAYLOAD_SIZE + pkt.length)
#define MESSAGE_PACKET_SIZE (sizeof(struct PerfectLinksPacket) - sizeof(struct ApplicationPacket) + MAX_PAYLOAD_SIZE)
#define ACK_PACKET_SIZE (sizeof(struct PerfectLinksPacket) - sizeof(struct ApplicationPacket))

#define PL_PKT_SIZE sizeof(struct PerfectLinksPacket)

struct ApplicationPacket
{
    char* data;
};

struct PerfectLinksPacket
{
    uint8_t type;
    uint64_t sender;
    uint32_t seqnr;
    uint32_t length;
    struct ApplicationPacket payload;
};

struct WrappedPerfectLinksPacket
{
    uint8_t n_packets;
    struct PerfectLinksPacket** payload;
};

/* debugging macros */
#define PRINT_PACKET(_packet) \
    printf("\t%-8s: %s\n\t%-8s: 0x%04lx\n\t%-8s: 0x%04x\n\t%-8s: 0x%04x\n\n", "Type", _packet.type == PAYLOAD ? "PAYLOAD" : "ACK", "Sender", _packet.sender, "Seqnr", _packet.seqnr, "Length", _packet.length);

#define PRINT_MSG(m)                                                                            \
    struct PerfectLinksPacket _packet; uint32_t _payload;                                                \
    memcpy(&_packet, m, sizeof(struct PerfectLinksPacket) - sizeof(uint32_t));                           \
    memcpy(&_packet.payload.data, &m[offsetof(struct PerfectLinksPacket, payload)], sizeof(uint32_t));   \
    PRINT_PACKET(_packet);

int serialize_packet(char** dest, struct PerfectLinksPacket* src);
int deserialize_packet(struct PerfectLinksPacket* dest, char* src);
void free_packet(struct PerfectLinksPacket* packet);