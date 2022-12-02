#pragma once

#include "SlidingWindow.hpp"
#include "ReceiverWindow.hpp"
#include "DeliveryWindow.hpp"

class SenderWindow : public SlidingWindow
{
public:
    inline SenderWindow(int socket, uint64_t id, uint64_t destinationId,
        in_addr_t destinationIp, uint16_t destinationPort, uint32_t windowSize,
        uint32_t messagesPerPacket, uint32_t sequenceNumberOfLastPacket,
        void (*broadcastLogFunction)(uint32_t), ReceiverWindow* receiver,
        DeliveryWindow* deliverer)
        : SlidingWindow(socket, id, destinationId, destinationIp, destinationPort,
            windowSize, messagesPerPacket, sequenceNumberOfLastPacket)
    {
        received_windows = new CircularBuffer<bool*>(windowSize);
        this->broadcastLogFunction = broadcastLogFunction;
        this->receiver = receiver;
        this->deliverer = deliverer;
    }

    inline ~SenderWindow()
    {
        delete received_windows;
        close(socket);
    }

    inline ssize_t initiateTransmission(Timer* timer)
    {
        ssize_t wc;
        wc = sendWindow(timer);
        return wc;
    }


    inline ssize_t sendPayloadSequence(uint32_t windowNumber)
    {
        uint32_t i;
        ssize_t wc;
        messageSequencePlaceholder.sequenceNumber = windowNumber;
        for (i = 0; i < messagesPerPacket &&
            messagesPerPacket * (windowNumber - 1) + i + 1 <= sequenceNumberOfLastPacket;
            i++)
        {
            setPayload(&messageSequencePlaceholder.payload[i],
                messagesPerPacket * (windowNumber - 1) + i + 1);
        }
        messageSequencePlaceholder.numberOfPackets = i;
        messageSequencePlaceholder.type = PAYLOAD;
        /* log broadcast before we send */
        broadcastLogFunction(windowNumber);

        /* If we send to ourself, put in packet buffer. We do this because we might get majority    */
        /* before we get payload from ourself. This is because of the implementation that says      */
        /* that this host will always be in the majority set.                                       */
        if (id == getDestinationId())
        {
            MessageSequence* seq;
            char* serialized;
            receiver->lock();
            receiver->copySequenceToHeap(&seq, reinterpret_cast<char*>(&messageSequencePlaceholder));
            memcpy(seq, &messageSequencePlaceholder, sizeof(struct MessageSequence));
            for (uint32_t i = 0; i < messageSequencePlaceholder.numberOfPackets; i++)
                memcpy(&seq->payload[i], &messageSequencePlaceholder.payload[i], sizeof(struct PerfectLinksPacket));
            receiver->forceSequenceIntoOrdered(seq);
            receiver->unlock();
        }
        wc = sendSequence(&messageSequencePlaceholder);
        if (wc == -1)
        {
            traceerror();
            return -1;
        }
        return wc;
    }

    inline void incrementTimeouts()
    {
        if (windowStart() > 1)
            statistics.timeouts++;
    }

    int parseAcknowledgementSequence(Timer* timer, char* buffer)
    {
        ssize_t rc, wc;
        uint32_t shift;
        uint32_t i;
        struct MessageSequence sequence;
        deserialize_sequence(&sequence, buffer, ACK);

        /* if ack to an already acked message (timeout on packet,           */
        /* retransmitted packet arrive before the previously transmitted)   */
        if (sequenceLeftOfWindow(sequence.sequenceNumber))
        {
            statistics.duplicatePackets++;
            return 0;
        }
        if (duplicate(sequence.sequenceNumber))
        {
            statistics.duplicatePackets++;
            return 0;
        }

        wc = timer->armTimer(sequence.sequenceNumber, DISARM);
        if (wc == -1)
        {
            return -1;
        }

        received_windows->insert(&t, sequence.sequenceNumber);
        statistics.correctTransmits += 1;

        /* if packet is not the first in window */
        if (sequence.sequenceNumber > windowStart())
        {
            return 0;
        }

        /* start shift of window */
        shift = shiftWindow();
        timer->shift(shift);
        /* send packets that got into the window after the shift */
        for (i = windowEnd() - shift; i < windowEnd(); i++)
        {
            /* if we have sent the last packet but still waiting for ACK, don't resend */
            if ((i - 1) * messagesPerPacket >= sequenceNumberOfLastPacket)
            {
                break;
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

    inline uint32_t sendNewMessages(Timer* timer, uint32_t shift)
    {
        uint32_t i;
        ssize_t wc;

        if (windowStart() + shift >= sequenceNumberOfLastPacket)
        {
            shift = sequenceNumberOfLastPacket - windowStart();
        }

        if (shift == 0)
        {
            return 0;
        }

        for (uint32_t i = windowStart(); i < windowStart() + shift; i++)
        {
            received_windows->insert(&f, i);
        }

        received_windows->shift(shift);
        timer->shift(shift);
        /* send packets that got into the window after the shift */
        for (i = windowEnd() - shift; i < windowEnd(); i++)
        {
            /* if we have sent the last packet but still waiting for ACK, don't resend */
            if ((i - 1) * messagesPerPacket >= sequenceNumberOfLastPacket)
            {
                break;
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
        return shift;
    }

    inline bool firstInWindow(uint32_t sequenceNumber)
    {
        return sequenceNumber == windowStart();
    }

private:
    inline uint32_t shiftWindow() override
    {
        uint32_t shift;
        // shift = received_windows->getShiftCounterWithPred(&t);

        if (windowStart() < deliverer->pWindowStart())
        {
            shift = 1;
        }
        else
        {
            shift = deliverer->getMajorityNumber();
            if (shift == 0)
            {
                return 0;
            }
        }

        if (windowStart() + shift >= sequenceNumberOfLastPacket)
        {
            shift = sequenceNumberOfLastPacket - windowStart();
        }

        for (uint32_t i = windowStart(); i < windowStart() + shift; i++)
        {
            received_windows->insert(&f, i);
        }
        received_windows->shift(shift);
        return shift;
    }

    inline uint32_t windowStart() override
    {
        return received_windows->getStart();
    }

    inline uint32_t windowEnd() override
    {
        return received_windows->getEnd();
    }

    inline bool duplicate(uint32_t sequenceNumber) override
    {
        return received_windows->get(sequenceNumber) == &t ||
            sequenceLeftOfWindow(sequenceNumber);
    }

    inline ssize_t sendWindow(Timer* timer)
    {
        ssize_t wc;
        for (uint32_t i = windowStart(); i < windowEnd(); i++)
        {
            if ((i - 1) * messagesPerPacket >= sequenceNumberOfLastPacket)
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

    inline int setPayload(struct PerfectLinksPacket* packet, uint32_t sequenceNumber)
    {
        packet->sequenceNumber = sequenceNumber;
        packet->type = PAYLOAD;
        packet->length = MAX_PAYLOAD_SIZE;
        memcpy(packet->payload.data, &packet->sequenceNumber, sizeof(packet->sequenceNumber));
        return 0;
    }

    inline void printStatistics()
    {
        printf("Sender window statistics:\n\t%-20s: %d\n\t%-20s: %d\n\t%-20s: %d\n\n",
            "Correct transmits", statistics.correctTransmits,
            "Duplicate packets", statistics.duplicatePackets,
            "(Regular) timeouts", statistics.timeouts);
    }

    void(*broadcastLogFunction)(uint32_t);
    CircularBuffer<bool*>* received_windows;
    ReceiverWindow* receiver;
    DeliveryWindow* deliverer;
};