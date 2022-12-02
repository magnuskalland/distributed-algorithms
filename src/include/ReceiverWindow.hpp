#pragma once

#include "SlidingWindow.hpp"

class ReceiverWindow : public SlidingWindow
{
public:
    inline ReceiverWindow(int socket, uint64_t id, uint64_t destinationId,
        in_addr_t destinationIp, uint16_t destinationPort, uint32_t windowSize,
        uint32_t messagesPerPacket, uint32_t sequenceNumberOfLastPacket)
        : SlidingWindow(socket, id, destinationId, destinationIp, destinationPort,
            windowSize, messagesPerPacket, sequenceNumberOfLastPacket)
    {
        pthread_mutex_init(&deliveryLock, NULL);
        resequencingBuffer = new CircularBuffer<struct MessageSequence*>(windowSize);
        orderedMessages = new CircularBuffer<struct MessageSequence*>(windowSize);
    }

    inline ~ReceiverWindow()
    {
        pthread_mutex_destroy(&deliveryLock);
        close(socket);
        delete resequencingBuffer;
        delete orderedMessages;
    }

    inline void lockDelivery()
    {
        pthread_mutex_lock(&deliveryLock);
    }

    inline void unlockDelivery()
    {
        pthread_mutex_unlock(&deliveryLock);
    }

    inline ssize_t sendForwardedAcknowledgement(char* message)
    {
        struct MessageSequence* sequence;
        uint64_t original_sender;
        ssize_t wc;

        sequence = reinterpret_cast<struct MessageSequence*>(message);
        // original_sender = sequence->type & ((static_cast<uint64_t>(1) << 32) - 1);

        // sequence->sender = original_sender;
        sequence->type = ACK;
        wc = sendDeliveryAcknowledgement(reinterpret_cast<char*>(sequence));
        if (wc == -1)
        {
            traceerror();
            return -1;
        }

        return 0;
    }

    inline ssize_t sendDeliveryAcknowledgement(char* message)
    {
        ssize_t wc;
        struct MessageSequence* messageSequence;
        uint64_t origin;

        wc = copySequenceToHeap(&messageSequence, message);
        if (wc == -1)
        {
            traceerror();
            return -1;
        }

        deserialize_sequence(messageSequence, message, PAYLOAD);
        memcpy(&messageSequencePlaceholder, messageSequence, offsetof(struct MessageSequence, payload));

        origin = messageSequence->sender;

        messageSequencePlaceholder.sender = id;
        messageSequencePlaceholder.type = ACK;
        wc = sendSequence(&messageSequencePlaceholder);
        if (wc == -1)
        {
            traceerror();
            free(messageSequence);
            return -1;
        }


        /* packet timed out or acknowledgement got lost */
        if (duplicate(messageSequence->sequenceNumber))
        {
            free(messageSequence);
            statistics.duplicatePackets++;
            return 0;
        }

        insertSequenceOntoQueue(messageSequence);
        statistics.correctTransmits += 1;

        return 0;
    }

    inline ssize_t sendAcknowledgement(char* message)
    {
        ssize_t wc;
        struct MessageSequence* messageSequence;

        wc = copySequenceToHeap(&messageSequence, message);
        if (wc == -1)
        {
            traceerror();
            return -1;
        }

        deserialize_sequence(messageSequence, message, PAYLOAD);
        memcpy(&messageSequencePlaceholder, messageSequence, offsetof(struct MessageSequence, payload));

        if (id != getDestinationId() && sequenceRightOfWindow(messageSequencePlaceholder.sequenceNumber))
        {
            free(messageSequence);
            return 0;
        }

        messageSequencePlaceholder.type = ACK;
        messageSequencePlaceholder.sender = id;
        wc = sendSequence(&messageSequencePlaceholder);
        if (wc == -1)
        {
            traceerror();
            free(messageSequence);
            return -1;
        }

        /* packet timed out or acknowledgement got lost */
        if (duplicate(messageSequence->sequenceNumber))
        {
            free(messageSequence);
            statistics.duplicatePackets++;
            return 0;
        }

        insertSequenceOntoQueue(messageSequence);
        statistics.correctTransmits += 1;

        return 0;
    }

    inline struct MessageSequence* getMessagesToDeliver()
    {
        return orderedMessages->pop();
    }

    inline ssize_t copySequenceToHeap(MessageSequence** dest, char* src)
    {
        *dest = reinterpret_cast<struct MessageSequence*>(malloc(sizeof(MessageSequence)));
        if (!*dest)
        {
            traceerror();
            perror("malloc");
            return -1;
        }
        return 0;
    }

    inline void insertSequenceOntoQueue(MessageSequence* sequence)
    {
        if (sequence->sender == id)
        {
            return;
        }
        lockDelivery();
        resequencingBuffer->insert(sequence, sequence->sequenceNumber);
        shiftWindow();
        unlockDelivery();
    }

    inline void forceSequenceIntoOrdered(MessageSequence* sequence)
    {
        lockDelivery();
        orderedMessages->insert(sequence, sequence->sequenceNumber);
        unlockDelivery();
    }

    inline void emptyResequencingBuffer()
    {
        uint32_t i;
        MessageSequence* sequence;
        for (i = resequencingBuffer->getStart(); i < resequencingBuffer->getEnd(); i++)
        {
            sequence = resequencingBuffer->remove(i);
            if (!sequence)
            {
                continue;
            }
            free(sequence);
        }
    }

private:

    uint32_t forced = 0;

    /**
     * NOT thread safe
    */
    inline uint32_t shiftWindow() override
    {
        uint32_t shift;
        shift = resequencingBuffer->getShiftCounter();
        if (shift == 0)
        {
            return 0;
        }
        for (uint32_t i = windowStart(); i < windowStart() + shift; i++)
        {
            orderedMessages->insert(resequencingBuffer->remove(i), i);
        }
        resequencingBuffer->shift(shift);
        return shift;
    }

    inline bool duplicate(uint32_t sequenceNumber) override
    {
        return resequencingBuffer->get(sequenceNumber) || sequenceLeftOfWindow(sequenceNumber);
    }

    inline void printStatistics()
    {
        printf("Receiver window statistics:\n\t%-20s: %d\n\t%-20s: %d\n\n",
            "Correct transmits", statistics.correctTransmits,
            "Duplicate packets", statistics.duplicatePackets);
    }

    inline uint32_t windowStart() override
    {
        return resequencingBuffer->getStart();
    }

    inline uint32_t windowEnd() override
    {
        return resequencingBuffer->getEnd();
    }

    inline bool sequenceRightOfWindow(const uint32_t sequenceNumber)
    {
        return sequenceNumber >= windowEnd();
    }

    pthread_mutex_t deliveryLock;
    CircularBuffer<struct MessageSequence*>* resequencingBuffer;
    CircularBuffer<struct MessageSequence*>* orderedMessages;
};