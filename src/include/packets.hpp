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
#define MESSAGE_PACKET_SIZE (sizeof(struct pl_packet) - sizeof(struct app_packet) + MAX_PAYLOAD_SIZE)
#define ACK_PACKET_SIZE (sizeof(struct pl_packet) - sizeof(struct app_packet))

#define PL_PKT_SIZE sizeof(struct pl_packet)

struct app_packet
{
    char* data;
};

struct pl_packet
{
    uint8_t type;
    uint64_t sender;
    uint32_t seqnr;
    uint32_t length;
    struct app_packet payload;
};

/* debugging macros */
#define PRINT_PACKET(_packet) \
    printf("\t%-8s: %s\n\t%-8s: 0x%04lx\n\t%-8s: 0x%04x\n\t%-8s: 0x%04x\n\n", "Type", _packet.type == PAYLOAD ? "PAYLOAD" : "ACK", "Sender", _packet.sender, "Seqnr", _packet.seqnr, "Length", _packet.length);

#define PRINT_MSG(m)                                                                            \
    struct pl_packet _packet; uint32_t _payload;                                                \
    memcpy(&_packet, m, sizeof(struct pl_packet) - sizeof(uint32_t));                           \
    memcpy(&_packet.payload.data, &m[offsetof(struct pl_packet, payload)], sizeof(uint32_t));   \
    PRINT_PACKET(_packet);

int serialize_packet(char** dest, struct pl_packet* src);
int deserialize_packet(struct pl_packet* dest, char* src);
void free_packet(struct pl_packet* packet);