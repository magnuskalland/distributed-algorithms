#include "SlidingWindow.hpp"

class SenderWindow : public SlidingWindow
{
public:
    SenderWindow(int socket, uint64_t sourceId, in_addr_t destinationIp,
        uint16_t destinationPort, uint32_t windowSize, uint32_t messagesPerPacket,
        uint32_t sequenceNumberOfLastPacket)
        : SlidingWindow(socket, sourceId, windowSize, messagesPerPacket,
            sequenceNumberOfLastPacket)
    {
        setuphost(destinationAddress, destinationIp, destinationPort);
    }
    inline ssize_t initiateTransmission(Timer* timer)
    {
        return sendWindow(timer);
    }

    inline ssize_t sendPayloadSequence(uint32_t windowNumber)
    {
        uint32_t i;
        messageSequencePlaceholder.sequenceNumber = windowNumber;
        for (i = 0; i < messagesPerPacket &&
            messagesPerPacket * (windowNumber - 1) + i + 1 <= sequenceNumberOfLastPacket;
            i++)
        {
            setPayload(&messageSequencePlaceholder.payload[i], messagesPerPacket * (windowNumber - 1) + i + 1); // heap allocates .payload[i].data
        }
        messageSequencePlaceholder.numberOfPackets = i;
        return sendSequence(&messageSequencePlaceholder);
    }

    inline void incrementTimeouts()
    {
        statistics.timeouts++;
    }

    ssize_t receiveAcknowledgementSequence(Timer* timer)
    {
        ssize_t rc, wc;
        uint32_t shift;
        struct MessageSequence* sequence;

        sequence = reinterpret_cast<struct MessageSequence*>(malloc(sizeof(struct MessageSequence)));
        if (!sequence)
        {
            perror("malloc");
            return -1;
        }

        rc = receiveAcknowledgementSequence(sequence);
        if (rc == -1)
        {
            return -1;
        }

        /* if ack to an already acked message (timeout on packet,           */
        /* retransmitted packet arrive before the previously transmitted)   */
        if (sequenceLeftOfWindow(sequence))
        {
            statistics.duplicatePackets++;
            free_sequence(sequence);
            return 0;
        }

        if (duplicate(sequence))
        {
            statistics.duplicatePackets++;
            free_sequence(sequence);
            return 0;
        }

        wc = timer->armTimer(sequence->sequenceNumber, DISARM);
        if (wc == -1)
        {
            free_sequence(sequence);
            return -1;
        }

        /* put packet in buffer */
        messageSequencesOutOfOrder->insert(sequence, sequence->sequenceNumber);
        statistics.correctTransmits += 1;

        /* if packet is not the first in window */
        if (sequence->sequenceNumber > windowStart())
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
            if (i * messagesPerPacket > sequenceNumberOfLastPacket)
            {
                return 0;
            }

            wc = sendPayloadSequence(i);
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
    inline ssize_t sendWindow(Timer* timer)
    {
        ssize_t wc;
        for (uint32_t i = windowStart(); i < windowEnd(); i++)
        {
            if (i * windowSize > sequenceNumberOfLastPacket)
            {
                return 0;
            }

            wc = sendPayloadSequence(i);
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

    inline ssize_t receiveAcknowledgementSequence(struct MessageSequence* sequence)
    {
        ssize_t rc = recv(socket, buffer, PACKED_ACK_SEQUENCE_SIZE, 0);
        if (rc == -1)
        {
            perror("recv");
            return -1;
        }
        deserialize_sequence(sequence, buffer, ACK);
        return rc;
    }

    inline int setPayload(struct PerfectLinksPacket* packet, uint32_t sequenceNumber)
    {
        packet->sequenceNumber = sequenceNumber;
        packet->type = PAYLOAD;
        packet->length = MAX_PAYLOAD_SIZE;
        packet->payload.data = reinterpret_cast<char*>(calloc(1, MAX_PAYLOAD_SIZE));
        if (!packet->payload.data)
        {
            perror("malloc");
            return -1;
        }
        memcpy(packet->payload.data, &packet->sequenceNumber, sizeof(packet->sequenceNumber));
        return 0;
    }
};