#ifndef BUFFER_QUEUE_H
#define BUFFER_QUEUE_H

#include <stdint.h>

#define NUM_OF_DETECTOR 4
#define NUM_OF_ENERGY_LEVELS 16
#define EVENT_BUFFER_SIZE (1 + sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint16_t) + NUM_OF_DETECTOR)
#define BIN_BUFFER_SIZE (1 + sizeof(uint32_t) + sizeof(uint16_t) + NUM_OF_DETECTOR * NUM_OF_ENERGY_LEVELS)

typedef struct {
	uint8_t mode;
	uint32_t secondCounter;
	uint16_t milliCounter;
	uint8_t data[NUM_OF_DETECTOR * NUM_OF_ENERGY_LEVELS]; // 16 bins per detector
} BinBufferItem;

typedef struct {
	uint8_t mode;
	uint32_t secondCounter;
	uint16_t milliCounter;
	uint16_t microCounter;
	uint8_t data[NUM_OF_DETECTOR];
} EventBufferItem;

#define BIN_BUFFER_QUEUE_SIZE 10
#define EVENT_BUFFER_QUEUE_SIZE 10

typedef struct {
	BinBufferItem items[BIN_BUFFER_QUEUE_SIZE];
	uint8_t head;
	uint8_t tail;
	uint8_t count;
} BinBufferQueue;

typedef struct {
	EventBufferItem items[EVENT_BUFFER_QUEUE_SIZE];
	uint8_t head;
	uint8_t tail;
	uint8_t count;
} EventBufferQueue;

void bin_buffer_enqueue(BinBufferItem item);
BinBufferItem bin_buffer_dequeue();

void event_buffer_enqueue(EventBufferItem item);
EventBufferItem event_buffer_dequeue();

#endif // BUFFER_QUEUE_H
