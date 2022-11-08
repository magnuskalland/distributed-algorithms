#pragma once

#include "SlidingWindow.hpp"

class ReceiverWindow : public SlidingWindow
{
public:
    ReceiverWindow(int socket, uint64_t sourceId, uint32_t windowSize,
        uint32_t messagesPerPacket, uint32_t sequenceNumberOfLastPacket)
        : SlidingWindow(socket, sourceId, windowSize, messagesPerPacket,
            sequenceNumberOfLastPacket)
    {
        this->sourceId = sourceId;
        this->destinationId = UNIDENTIFIED_HOST;
    }

    inline ssize_t sendAcknowledgement(char* message)
    {
        ssize_t wc;
        struct MessageSequence* messageSequence;
        messageSequence = reinterpret_cast<struct MessageSequence*>(malloc(sizeof(struct MessageSequence)));
        if (!messageSequence)
        {
            perror("malloc");
            return -1;
        }

        deserialize_sequence(messageSequence, message, PAYLOAD); // heap allocates packet.payload->data

        if (senderUnidentified())
        {
            destinationId = messageSequence->sender;
            setSource(message);
        }

        memcpy(&messageSequencePlaceholder, messageSequence, offsetof(struct MessageSequence, payload));
        messageSequencePlaceholder.sender = sourceId;
        wc = sendAcknowledgementSequence(&messageSequencePlaceholder);
        if (wc == -1)
        {
            freesequencedata(messageSequence);
            free(messageSequence);
            return -1;
        }

        /* packet timed out or acknowledgement got lost */
        if (duplicate(messageSequence))
        {
            freesequencedata(messageSequence);
            free(messageSequence);
            statistics.duplicatePackets++;
            return 0;
        }

        resequencingBuffer->insert(messageSequence, messageSequence->sequenceNumber);
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
        memcpy(&destinationAddress, &msg[PACKED_MESSAGE_SEQUENCE_SIZE], sizeof(sockaddr));
    }

    inline uint64_t getSourceId()
    {
        return sourceId;
    }

private:
    inline ssize_t sendAcknowledgementSequence(struct MessageSequence* sequence)
    {
        for (uint32_t i = 0; i < sequence->numberOfPackets; i++)
        {
            sequence->payload[i].type = ACK;
            sequence->payload[i].sequenceNumber = messagesPerPacket * (sequence->sequenceNumber - 1) + i + 1;
            sequence->payload[i].length = 0;

        }
        return sendSequence(sequence, ACK);
    }

    uint64_t sourceId, destinationId;
};