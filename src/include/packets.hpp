#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "config.hpp"
#include "macros.hpp"

/* message types */
#define PAYLOAD 0x00
#define ACK     0x01

/* packet size constants */
#define MAX_PAYLOAD_SIZE                4
#define MAX_MESSAGES_PER_SEQUENCE       MESSAGES_PER_PACKET

#define PACKED_ACK_PACKET_SIZE          (offsetof(struct PerfectLinksPacket, payload))
#define PACKED_ACK_SEQUENCE_SIZE        (offsetof(struct MessageSequence, payload) + PACKED_ACK_PACKET_SIZE * MAX_MESSAGES_PER_SEQUENCE)

#define PACKED_MESSAGE_PACKET_SIZE      (offsetof(struct PerfectLinksPacket, payload) + MAX_PAYLOAD_SIZE)
#define PACKED_MESSAGE_SEQUENCE_SIZE    (offsetof(struct MessageSequence, payload) + PACKED_MESSAGE_PACKET_SIZE * MAX_MESSAGES_PER_SEQUENCE)

struct ApplicationPacket
{
    char* data;
};

struct PerfectLinksPacket
{
    uint8_t type;
    uint32_t sequenceNumber;
    uint32_t length;
    struct ApplicationPacket payload;
};

struct MessageSequence
{
    uint64_t sender;
    uint32_t sequenceNumber;
    uint32_t numberOfPackets;
    struct PerfectLinksPacket payload[MAX_MESSAGES_PER_SEQUENCE];
};

// struct TransportLayerPacket
// {
//     struct sockaddr src;
//     struct MessageSequence payload;
// };

/* debugging macros */
#define PRINT_PACKET(_packet) \
    printf("\t%-8s: %s\n\t%-8s: 0x%04x\n\t%-8s: 0x%04x\n\n", "Type", _packet.type == PAYLOAD ? "PAYLOAD" : "ACK", "Seqnr", _packet.sequenceNumber, "Length", _packet.length);

#define PRINT_MSG(m)                                                                            \
    struct PerfectLinksPacket _packet; uint32_t _payload;                                                \
    memcpy(&_packet, m, sizeof(struct PerfectLinksPacket) - sizeof(uint32_t));                           \
    memcpy(&_packet.payload.data, &m[offsetof(struct PerfectLinksPacket, payload)], sizeof(uint32_t));   \
    PRINT_PACKET(_packet);

#define PRINT_SEQUENCE(s) \
    printf("\t%-20s: %ld\n\t%-20s: %d\n\t%-20s: %d\n\n", "Sender", s->sender, "Window number", s->sequenceNumber, "Number of packets", s->numberOfPackets);     \
    for (uint32_t _i = 0; _i < s->numberOfPackets; _i++) { PRINT_MSG(reinterpret_cast<char*>(&s->payload[_i])); }

ssize_t serialize_sequence(char** dest, struct MessageSequence* src);
ssize_t deserialize_sequence(struct MessageSequence* dest, char* src, int type);

/* Deserializing */

inline ssize_t deserialize_message(struct ApplicationPacket* dest, char* src, uint32_t length)
{
    dest->data = reinterpret_cast<char*>(calloc(1, length));
    if (!dest->data)
    {
        perror("malloc");
        return -1;
    }
    memcpy(dest->data, src, length);
    return 0;
}

inline ssize_t deserialize_packet(struct PerfectLinksPacket* dest, char* src)
{
    ssize_t err;
    memcpy(dest, src, offsetof(struct PerfectLinksPacket, payload));
    err = deserialize_message(&dest->payload,
        &src[offsetof(struct PerfectLinksPacket, payload.data)], dest->length);
    if (err == -1)
    {
        traceerror();
        return -1;
    }
    return 0;
}

/* Serializing */

inline void serialize_message(char* dest, struct ApplicationPacket* src,
    uint32_t length)
{
    memcpy(dest, src, length);
}

inline void serialize_packet(char* dest, struct PerfectLinksPacket* src)
{
    memcpy(dest, src, offsetof(struct PerfectLinksPacket, payload));
    serialize_message(&dest[offsetof(struct PerfectLinksPacket, payload)],
        &src->payload, src->length);
}

inline void free_sequence(struct MessageSequence* sequence)
{
    for (uint32_t i = 0; i < sequence->numberOfPackets; i++)
    {
        free(sequence->payload[i].payload.data);
    }
}
