#include <stdlib.h>

#include "packets.hpp"
#include "macros.hpp"

ssize_t deserialize_sequence(struct MessageSequence* dest, char* src, int type)
{
    ssize_t err;

    /* copy sequence metadata */
    memcpy(dest, src, offsetof(struct MessageSequence, payload));

    /* copy into packets */
    for (uint32_t i = 0; i < dest->numberOfPackets; i++)
    {
        err = deserialize_packet(&dest->payload[i],
            &src[offsetof(struct MessageSequence, payload)
            + i * (type == PAYLOAD ? PACKED_MESSAGE_PACKET_SIZE : PACKED_ACK_PACKET_SIZE)]);
        if (err == -1)
        {
            traceerror();
            return -1;
        }
    }
    return 0;
}

ssize_t serialize_sequence(char** dest, struct MessageSequence* src, int type)
{
    ssize_t size = offsetof(struct MessageSequence, payload)
        + src->numberOfPackets * offsetof(struct PerfectLinksPacket, payload);
    ssize_t offset = offsetof(struct MessageSequence, payload);
    for (uint32_t i = 0; i < src->numberOfPackets; i++)
        size += src->payload[i].length;

    *dest = reinterpret_cast<char*>(malloc(size));
    if (!*dest)
    {
        perror("malloc");
        return -1;
    }

    memcpy(*dest, src, offsetof(struct MessageSequence, payload));
    for (uint32_t i = 0; i < src->numberOfPackets; i++)
    {
        serialize_packet(&(*dest)[offset], &src->payload[i]);
        offset += offsetof(struct MessageSequence, payload) + src->payload[i].length;
    }

    return size;
}