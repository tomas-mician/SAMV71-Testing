#ifndef BUFFER_QUEUE_H
#define BUFFER_QUEUE_H

#define NUM_OF_DETECTOR 4

#include <stdint.h>

typedef struct {
	uint8_t buffer[1 + sizeof(uint32_t) + sizeof(uint16_t) + NUM_OF_DETECTOR];
} BufferItem;

#define BUFFER_QUEUE_SIZE 10

void enqueue(BufferItem item);
BufferItem dequeue();

#endif // BUFFER_QUEUE_H
