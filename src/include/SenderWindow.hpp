#include "SlidingWindow.hpp"

class SenderWindow : public SlidingWindow
{
public:
    SenderWindow(int socket, uint64_t sourceId, in_addr_t destinationIp, uint16_t destinationPort, uint32_t* windowSize, uint32_t lastPacketSequenceNumber)
        : SlidingWindow(socket, sourceId, windowSize, lastPacketSequenceNumber)
    {
        setuphost(destinationAddress, destinationIp, destinationPort);
    }
    ~SenderWindow()
    {

    }
    inline ssize_t initiateTransmission(Timer* timer)
    {
        return sendWindow(timer);
    }

    inline ssize_t sendMessage(uint32_t sequenceNumber)
    {
        setPayload(&packetPlaceholder, sequenceNumber);
        return sendPayload(&packetPlaceholder);
    }

    inline void incrementTimeouts()
    {
        statistics.timeouts++;
    }

    inline ssize_t receiveAcknowledgement(Timer* timer)
    {
        ssize_t rc, wc;
        uint32_t shift;
        struct pl_packet* packet;

        packet = reinterpret_cast<struct pl_packet*>(malloc(PL_PKT_SIZE));
        if (!packet)
        {
            perror("malloc");
            return -1;
        }

        rc = receivePacket(packet);
        if (rc == -1)
        {
            return -1;
        }

        /* if ack to an already acked message (timeout on packet,           */
        /* retransmitted packet arrive before the previously transmitted)   */
        if (packetLeftOfWindow(packet))
        {
            statistics.duplicatePackets++;
            free_packet(packet);
            return 0;
        }

        if (duplicate(packet))
        {
            statistics.duplicatePackets++;
            free_packet(packet);
            return 0;
        }

        wc = timer->armTimer(packet->seqnr, DISARM);
        if (wc == -1)
        {
            free_packet(packet);
            return -1;
        }

        /* put packet in buffer */
        outOfOrderMessageBuffer->insert(packet, packet->seqnr);
        statistics.correctTransmits += 1;

        /* if packet is not the first in window */
        if (packet->seqnr > windowStart())
        {
            return 0;
        }

        /* start shift of window */
        shift = shiftWindow();
        timer->shift(shift);

        /* send packets that got into the window after the shift */
        for (uint32_t i = windowEnd() - shift; i < windowEnd(); i++)
        {
            /* if we have sent the last packet but still waiting for ACK, don't resend */
            if (i > lastPacketSequenceNumber)
            {
                return 0;
            }

            wc = sendMessage(i);
            if (wc == -1)
            {
                return -1;
            }
            wc = timer->armTimer(i, ARM);
            if (wc == -1)
            {
                return -1;
            }
        }
        return 0;
    }

private:
    ssize_t sendWindow(Timer* timer)
    {
        ssize_t wc;
        packetPlaceholder.type = PAYLOAD;

        for (uint32_t i = windowStart(); i < windowEnd(); i++)
        {
            wc = sendMessage(i);
            if (wc == -1)
            {
                return -1;
            }
            wc = timer->armTimer(i, ARM);
            if (wc == -1)
            {
                return -1;
            }
        }
        return 0;
    }
    inline ssize_t sendPayload(struct pl_packet* packet)
    {
        return sendPacket(packet, MESSAGE_PACKET_SIZE);
    }

    inline void setPayload(struct pl_packet* packet, uint32_t sequenceNumber)
    {
        packet->seqnr = sequenceNumber;
        packet->length = MAX_PAYLOAD_SIZE;
        bzero(packet->payload.data, MAX_PAYLOAD_SIZE);
        memcpy(packet->payload.data, &packet->seqnr, sizeof(packet->seqnr));
    }
};