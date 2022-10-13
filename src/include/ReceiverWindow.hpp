#include "SlidingWindow.hpp"

class ReceiverWindow : public SlidingWindow
{
public:
    ReceiverWindow(int socket, uint64_t sourceId, uint32_t windowSize,
        uint32_t messagesPerPacket, uint32_t lastPacketSequenceNumber)
        : SlidingWindow(socket, sourceId, windowSize, messagesPerPacket,
            lastPacketSequenceNumber)
    {
        this->sourceId = sourceId;
        this->destinationId = UNIDENTIFIED_HOST;
    }

    inline ssize_t sendAcknowledgement(char* message)
    {
        ssize_t wc;
        struct PerfectLinksPacket* packet;
        struct PerfectLinksPacket acknowledgement;

        packet = reinterpret_cast<struct PerfectLinksPacket*>(malloc(PL_PKT_SIZE));
        if (!packet)
        {
            perror("malloc");
            return -1;
        }

        deserialize_packet(packet, message); // heap allocates packet.payload->data
        acknowledgement = { ACK, sourceId, packet->seqnr, 0, NULL };

        if (senderUnidentified())
        {
            destinationId = packet->sender;
            setSource(message);
        }

        wc = sendPacket(&acknowledgement, ACK_PACKET_SIZE);
        if (wc == -1)
        {
            free_packet(packet);
            return -1;
        }

        /* packet timed out or acknowledgement got lost */
        if (duplicate(packet))
        {
            free_packet(packet);
            statistics.duplicatePackets++;
            return 0;
        }

        outOfOrderMessageBuffer->insert(packet, packet->seqnr);
        statistics.correctTransmits += 1;
        shiftWindow();

        return 0;
    }

    inline bool senderUnidentified()
    {
        return destinationId == UNIDENTIFIED_HOST;
    }

    inline void setSource(char* msg)
    {
        memcpy(&destinationAddress, &msg[MESSAGE_PACKET_SIZE], sizeof(sockaddr));
    }

    inline uint64_t getSourceId()
    {
        return sourceId;
    }

private:
    uint64_t sourceId, destinationId;
};