#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include <limits.h>

#include "config.hpp"
#include "macros.hpp"

/* message types */
#define PAYLOAD         (static_cast<uint64_t>(1) << 60)
#define ACK             (static_cast<uint64_t>(1) << 61)
#define FORWARD         (static_cast<uint64_t>(1) << 62)
#define DELIVERY        (static_cast<uint64_t>(1) << 63)

#define payload(type) \
    (type == PAYLOAD)
#define ack(type) \
    (type == ACK)
#define forward(type) \
    ((type & FORWARD) == FORWARD)
#define delivery(type) \
    ((type & DELIVERY) == DELIVERY)

/* packet size constants */
#define MAX_PAYLOAD_SIZE                4
#define MAX_MESSAGES_PER_SEQUENCE       MESSAGES_PER_PACKET

#define PACKED_ACK_PACKET_SIZE          (offsetof(struct PerfectLinksPacket, payload))
#define PACKED_ACK_SEQUENCE_SIZE        (offsetof(struct MessageSequence, payload) + PACKED_ACK_PACKET_SIZE * MAX_MESSAGES_PER_SEQUENCE)

#define PACKED_MESSAGE_PACKET_SIZE      (offsetof(struct PerfectLinksPacket, payload) + MAX_PAYLOAD_SIZE)
#define PACKED_MESSAGE_SEQUENCE_SIZE    (offsetof(struct MessageSequence, payload) + PACKED_MESSAGE_PACKET_SIZE * MAX_MESSAGES_PER_SEQUENCE)

#define LATE_MAJORITY   (static_cast<uint32_t>(1 << 31))

struct ApplicationPacket
{
    char data[MAX_PAYLOAD_SIZE];
};

struct PerfectLinksPacket
{
    uint64_t type;
    uint32_t sequenceNumber;
    uint32_t length;
    struct ApplicationPacket payload;
};

struct MessageSequence
{

    /**
     * If set to FORWARD or DELIVERY, this field's least significant bits
     * contains the ID of the host that's forwarding this packet.
     *
     * Assumes the network size doesn't exceed the unsigned 32 bit range.
    */
    uint64_t type;

    /**
     * ID of the original sender, as given by input file.
    */
    uint64_t sender;

    /**
     * Sequence number for the window this sequence represents.
     * Should always be equal to sequence number of first packet divided
     * by WINDOW_SIZE.
    */
    uint32_t sequenceNumber;

    /**
     * The number of messages this sequence contains.
    */
    uint32_t numberOfPackets;
    struct PerfectLinksPacket payload[MAX_MESSAGES_PER_SEQUENCE];
};


/* debugging macros */
#define printpacket(_packet) \
    printf("\t%-8s: %s\n\t%-8s: 0x%04x\n\t%-8s: 0x%04x\n\n", "Type", _packet.type == PAYLOAD ? "PAYLOAD" : "ACK", "Seqnr", _packet.sequenceNumber, "Length", _packet.length);

#define PRINT_MSG(m)                                                                            \
    struct PerfectLinksPacket _packet; uint32_t _payload;                                                \
    memcpy(&_packet, m, sizeof(struct PerfectLinksPacket) - sizeof(uint32_t));                           \
    memcpy(&_packet.payload.data, &m[offsetof(struct PerfectLinksPacket, payload)], sizeof(uint32_t));   \
    printpacket(_packet);

#define printsequence(s) \
    printf("\t%-20s: %s\n\t%-20s: %ld\n\t%-20s: %d\n\t%-20s: %d\n\n", "Type:", s->type == PAYLOAD ? "Payload" : "Acknowledgement", "Sender", s->sender, "Window number", s->sequenceNumber, "Number of packets", s->numberOfPackets); \
    for (uint32_t _i = 0; _i < s->numberOfPackets; _i++) { PRINT_MSG(reinterpret_cast<char*>(&s->payload[_i])); }

#define printsequenceoffset()   \
    printf("struct MessageSequence {\n\t%ld: type\n\t%ld: sender\n\t%ld: sequenceNumber\n\t%ld: numberOfPackets\n\t%ld: payload\n}\n", offsetof(struct MessageSequence, type), offsetof(struct MessageSequence, sender), offsetof(struct MessageSequence, sequenceNumber), offsetof(struct MessageSequence, numberOfPackets), offsetof(struct MessageSequence, payload))

#define printperfectlinkspacketoffset()   \
    printf("struct PerfectLinksPacket {\n\t%ld: type\n\t%ld: sequenceNumber\n\t%ld: length\n\t%ld: payload\n}\n", offsetof(struct PerfectLinksPacket, type), offsetof(struct PerfectLinksPacket, sequenceNumber), offsetof(struct PerfectLinksPacket, length), offsetof(struct PerfectLinksPacket, payload))

inline void deserialize_sequence(struct MessageSequence* dest, char* src, uint64_t type)
{
    struct PerfectLinksPacket* pkt_dst;
    char* pkt_src;

    /* copy sequence metadata */
    memcpy(dest, src, offsetof(struct MessageSequence, payload));

    if (ack(type) || delivery(type))
    {
        return;
    }
    /* copy into packets */
    for (uint32_t i = 0; i < dest->numberOfPackets; i++)
    {
        pkt_dst = &dest->payload[i];
        pkt_src = &src[offsetof(struct MessageSequence, payload) +
            i * (type == PAYLOAD ? PACKED_MESSAGE_PACKET_SIZE : PACKED_ACK_PACKET_SIZE)];
        memcpy(pkt_dst, pkt_src, offsetof(struct PerfectLinksPacket, payload));
        memcpy(pkt_dst->payload.data, &pkt_src[offsetof(struct PerfectLinksPacket, payload.data)],
            pkt_dst->length);
    }
}

inline ssize_t serialize_sequence(char** dest, struct MessageSequence* src)
{
    ssize_t size, offset;
    char* pkt_dst;
    struct PerfectLinksPacket* pkt_src;

    size = offsetof(struct MessageSequence, payload);
    offset = offsetof(struct MessageSequence, payload);

    if (payload(src->type) || forward(src->type))
    {
        size += src->numberOfPackets * offsetof(struct PerfectLinksPacket, payload);
        for (uint32_t i = 0; i < src->numberOfPackets; i++)
        {
            size += src->payload[i].length;
        }
    }

    *dest = reinterpret_cast<char*>(malloc(size));
    if (!*dest)
    {
        perror("malloc");
        fprintf(stderr, "malloc(%ld)\n", size);
        traceerror();
        return -1;
    }

    memcpy(*dest, src, offsetof(struct MessageSequence, payload));

    if (ack(src->type) || delivery(src->type))
    {
        return size;
    }

    for (uint32_t i = 0; i < src->numberOfPackets; i++)
    {
        pkt_dst = &(*dest)[offset];
        pkt_src = &src->payload[i];
        memcpy(pkt_dst, pkt_src, offsetof(struct PerfectLinksPacket, payload));
        memcpy(&pkt_dst[offsetof(struct PerfectLinksPacket, payload)],
            &pkt_src->payload, pkt_src->length);
        offset += offsetof(struct PerfectLinksPacket, payload) + src->payload[i].length;
    }

    return size;
}