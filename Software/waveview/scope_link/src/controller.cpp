#include "controller.hpp"
#include "logger.hpp"

#define RAMPDEMO 1

#ifdef RAMPDEMO

#define RD_DATA_PER_CHAN 1024
#define RD_CHAN_COUNT 4
#define RD_PACKET_SIZE 4096

uint8_t RD_PACKET_ORIGINAL[RD_PACKET_SIZE]; 

#endif

controller::controller(boost::lockfree::queue<buffer*, boost::lockfree::fixed_sized<false>> *inputQ)
{
    dataQueue = inputQ;
    stopController.store(false);

    // command packet parser thread
    controllerThread = std::thread(&controller::controllerLoop, this);

    // Bridge to JS
    bridgeThread = new Bridge("testPipe", &controllerQueue_tx, &controllerQueue_rx);
    bridgeThread->TxStart();
    bridgeThread->RxStart();

    // Create pipeline threads
    triggerThread = new Trigger(dataQueue, &triggerProcessorQueue, triggerLevel);
    processorThread = new Processor(&triggerProcessorQueue, &processorPostProcessorQueue_1);
    postProcessorThread = new postProcessor(&processorPostProcessorQueue_1, &controllerQueue_tx);

    // set default values
    setCh(1);
    setTriggerCh(1);
    setLevel(50);

#ifdef RAMPDEMO
    for(int ch = 0; ch < RD_CHAN_COUNT; ch++) {
        for(int i = 0; ch == 0 && i < RD_DATA_PER_CHAN; i++) {
            RD_PACKET_ORIGINAL[i + ch*RD_DATA_PER_CHAN] = i % 24;
        }
        for(int i = 0; ch == 1 && i < RD_DATA_PER_CHAN; i++) {
            RD_PACKET_ORIGINAL[i + ch*RD_DATA_PER_CHAN] = 24 - (i % 24);
        }
        for(int i = 0; ch == 2 && i < RD_DATA_PER_CHAN; i++) {
            RD_PACKET_ORIGINAL[i + ch*RD_DATA_PER_CHAN] = (i % 24) / 12;
        }
        for(int i = 0; ch == 3 && i < RD_DATA_PER_CHAN; i++) {
            RD_PACKET_ORIGINAL[i + ch*RD_DATA_PER_CHAN] = 10;
        }
    }
#endif

    INFO << "Controller Created";
}

controller::~controller()
{
    stopController.store(true);
    controllerThread.join();

    delete triggerThread;
    delete processorThread;
    delete postProcessorThread;
    delete bridgeThread;

    DEBUG << "Controller Destroyed";
}

/*******************************************************************************
 * controllerLoop()
 *
 * Core loop for the controller.
 * Parses packets from the rx queue
 *
 * Arguments:
 *   None
 * Return:
 *   None
 ******************************************************************************/
void controller::controllerLoop()
{
    EVPacket* currentPacket;

    while (stopController.load() == false) {
        while (stopController.load() == false &&
               controllerQueue_rx.pop(currentPacket)) {
            DEBUG << "Controller processing a packet";

            EVPacket* tempPacket = NULL;

            //RampDemo variables
            int rd_dataPerChan = 1024;
            int rd_chanCount = 4;

            // execute the packet command
            switch (currentPacket->command) {
                case 0x01:
                    INFO << "Packet command 0x01: GetData";
                    break;
                case 0x02:
                    ERROR << "Packet command 0x02: Reserved";
                    break;
                case 0x03:
                    ERROR << "Packet command 0x03: Reserved";
                    break;
                case 0x04:
                    ERROR << "Packet command 0x04: Reserved";
                    break;
#ifdef RAMPDEMO
                case 0x1F:
                    INFO << "Packet command 0x1F: RampDemo";
                    tempPacket = (EVPacket*) malloc(sizeof(EVPacket));
                    tempPacket->data = (int8_t*) malloc(RD_PACKET_SIZE);
                    tempPacket->dataSize = RD_PACKET_SIZE;
                    tempPacket->packetID = 0x11;
                    memcpy(tempPacket->data, (const void*)RD_PACKET_ORIGINAL, RD_PACKET_SIZE);
                    controllerQueue_tx.push(tempPacket);
                    break;
#endif
                default:
                    ERROR << "Unknown packet command";
                    break;
            }

        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

/*******************************************************************************
 * controllerPause()
 *
 * Pauses the pipeline.
 * Calls pause on each of the stages of the pipeline
 *
 * Arguments:
 *   None
 * Return:
 *   None
 ******************************************************************************/
void controller::controllerPause()
{
    DEBUG << "Pausing pipeline";
    processorThread->processorPause();
    triggerThread->triggerPause();
    postProcessorThread->postProcessorPause();
}

/*******************************************************************************
 *  controllerUnPause()
 *
 * unpauses the dsp pipeline.
 * Calls unpause on each of the stages of the pipeline
 *
 * Arguments:
 *   None
 * Return:
 *   None
 ******************************************************************************/
void controller::controllerUnPause()
{
    DEBUG << "Unpausing pipeline";
    processorThread->processorUnpause();
    triggerThread->triggerUnpause();
    postProcessorThread->postProcessorUnpause();
}

/*******************************************************************************
 * bufferFunctor()
 *
 * deallocates the buffer passed to it.
 *
 * Arguments:
 *   buffer* a - The buffer to deallocate
 * Return:
 *   None
 ******************************************************************************/
void bufferFunctor(buffer* a) {
    bufferAllocator.deallocate(a, 1);
}

/*******************************************************************************
 * controllerFlush()
 *
 * Flushes all data out of the pipeline.
 * Pauses the pipeline until its done clearing all queues.
 * Resets the persistence buffer.
 *
 * Arguments:
 *   None
 * Return:
 *   None
 ******************************************************************************/
void controller::controllerFlush()
{
    INFO << "Flushing pipeline";
    // pause while flusing
    controllerPause();

    // Clear queues
    size_t count = 0;
    count = (*dataQueue).consume_all(bufferFunctor);
    DEBUG << "Flushed inputQueue: " << count;

    count = triggerProcessorQueue.consume_all(bufferFunctor);
    DEBUG << "Flushed triggeredQueue: " << count;

    // Clear persistence buffer
    processorThread->flushPersistence();
    DEBUG << "Flushed persistence buffer";
//    count = preProcessorQueue.consume_all(bufferFunctor);
//    DEBUG << "Flushed preProcessorQueue: " << count;

    count = processorPostProcessorQueue_1.consume_all(free);
    DEBUG << "Flushed postProcessorQueue: " << count;
}

/*******************************************************************************
 * getTriggerLevel()
 *
 * returns the trigger level.
 *
 * Arguments:
 *   int8_t newLevel - New level for the trigger function
 * Return:
 *   None
 ******************************************************************************/
int8_t controller::getLevel()
{
    return triggerThread->getTriggerLevel();
}

/*******************************************************************************
 * setTriggerLevel()
 *
 * flushes the pipeline and sets a new trigger level.
 *
 * Arguments:
 *   int8_t newLevel - New level for the trigger function
 * Return:
 *   None
 ******************************************************************************/
void controller::setLevel( int8_t newLevel )
{
    triggerLevel = newLevel;

    triggerThread->setTriggerLevel(triggerLevel);

    INFO << "new trigger level: " << triggerThread->getTriggerLevel();

    controllerFlush();
}

/*******************************************************************************
 * setCh()
 *
 * sets the number of channels on each stage of the pipeline.
 *
 * Arguments:
 *   int8_t newCh - desired number of channels;
 * Return:
 *   None
 ******************************************************************************/
void controller::setCh (int8_t newCh)
{
    controllerPause();

    triggerThread->setCh(newCh);
    processorThread->setCh(newCh);
    postProcessorThread->setCh(newCh);

    controllerFlush();
}

/*******************************************************************************
 * setTriggerCh()
 *
 * set Trigger channel.
 *
 * Arguments:
 *   int8_t newTriggerCh - desired trigger channel;
 * Return:
 *   None
 ******************************************************************************/
void controller::setTriggerCh (int8_t newTriggerCh)
{
    controllerPause();

    triggerThread->setTriggerCh(newTriggerCh);

    controllerFlush();
}

void controller::setRising()
{
    controllerPause();

    triggerThread->setRising();

    controllerFlush();
}

void controller::setFalling()
{
    controllerPause();

    triggerThread->setFalling();

    controllerFlush();
}
