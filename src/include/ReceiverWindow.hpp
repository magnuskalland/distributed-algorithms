#include "SlidingWindow.hpp"

class ReceiverWindow : public SlidingWindow
{
public:
    ReceiverWindow(int socket, uint64_t sourceId, uint32_t* windowSize, uint32_t lastPacketSequenceNumber)
        : SlidingWindow(socket, sourceId, windowSize, lastPacketSequenceNumber)
    {
        this->sourceId = sourceId;
        this->destinationId = UNIDENTIFIED_HOST;
    }
    ~ReceiverWindow()
    {

    }

    inline ssize_t sendAcknowledgement(char* message)
    {
        ssize_t wc;
        struct pl_packet* packet;
        struct pl_packet acknowledgement;

        packet = reinterpret_cast<struct pl_packet*>(malloc(PL_PKT_SIZE));
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