#include "packets.hpp"
#include "macros.hpp"

#include <stdlib.h>

int deserialize_packet(struct pl_packet* dest, char* src)
{
    memcpy(dest, src, sizeof(struct pl_packet));
    dest->payload.data = static_cast<char*>(calloc(1, dest->length));
    if (!dest->payload.data)
    {
        perror("calloc");
        return -1;
    }
    memcpy(dest->payload.data, &src[offsetof(struct pl_packet, payload.data)], dest->length);
    return 0;
}

int serialize_packet(char** dest, struct pl_packet* src)
{
    size_t payload_len = src->length;
    size_t metadata_len = sizeof(struct pl_packet) - sizeof(src->payload.data) + payload_len;
    *dest = static_cast<char*>(calloc(1, sizeof(struct pl_packet) - sizeof(src->payload.data) + src->length));
    if (!*dest)
    {
        perror("calloc");
        return -1;
    }

    memcpy(*dest, src, metadata_len);
    memcpy(&((*dest)[offsetof(struct pl_packet, payload.data)]), src->payload.data, payload_len);
    return 0;
}

void free_packet(struct pl_packet* packet)
{
    free(packet->payload.data);
    free(packet);
}