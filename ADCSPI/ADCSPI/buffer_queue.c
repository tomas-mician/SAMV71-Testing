#include "buffer_queue.h"

volatile BufferItem bufferQueue[BUFFER_QUEUE_SIZE];
volatile uint16_t queueStart = 0;
volatile uint16_t queueEnd = 0;
volatile uint16_t queueSize = 0;

void enqueue(BufferItem item) {
	if (queueSize >= BUFFER_QUEUE_SIZE) {
		// If the queue is full, remove the oldest item
		dequeue();
	}
	// Then add the new item
	bufferQueue[queueEnd] = item;
	queueEnd = (queueEnd + 1) % BUFFER_QUEUE_SIZE;
	queueSize++;

}


BufferItem dequeue() {
	if (queueSize > 0) {
		BufferItem item = bufferQueue[queueStart];
		queueStart = (queueStart + 1) % BUFFER_QUEUE_SIZE;
		queueSize--;
		return item;
	}
	// else, return an empty item
	BufferItem emptyItem = {{0x00}};
	return emptyItem;
}
