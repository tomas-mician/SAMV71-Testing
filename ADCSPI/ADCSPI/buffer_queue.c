#include "buffer_queue.h"

BinBufferQueue bin_buffer_queue = {.head = 0, .tail = 0, .count = 0};
EventBufferQueue event_buffer_queue = {.head = 0, .tail = 0, .count = 0};

void bin_buffer_enqueue(BinBufferItem item) {
	if (bin_buffer_queue.count >= BIN_BUFFER_QUEUE_SIZE) {
		// Bin buffer is full, can't enqueue
		return;
	}

	bin_buffer_queue.items[bin_buffer_queue.tail] = item;
	bin_buffer_queue.tail = (bin_buffer_queue.tail + 1) % BIN_BUFFER_QUEUE_SIZE;
	bin_buffer_queue.count++;
}

BinBufferItem bin_buffer_dequeue() {
	if (bin_buffer_queue.count > 0) {
		BinBufferItem item = bin_buffer_queue.items[bin_buffer_queue.head];
		bin_buffer_queue.head = (bin_buffer_queue.head + 1) % BIN_BUFFER_QUEUE_SIZE;
		bin_buffer_queue.count--;
		return item;
	}

	// If the queue is empty, return an item with zeros
	BinBufferItem emptyItem = {.mode = 0, .secondCounter = 0, .milliCounter = 0, .data = {0}};
	return emptyItem;
}

void event_buffer_enqueue(EventBufferItem item) {
	if (event_buffer_queue.count >= EVENT_BUFFER_QUEUE_SIZE) {
		// Event buffer is full, can't enqueue
		return;
	}

	event_buffer_queue.items[event_buffer_queue.tail] = item;
	event_buffer_queue.tail = (event_buffer_queue.tail + 1) % EVENT_BUFFER_QUEUE_SIZE;
	event_buffer_queue.count++;
}

EventBufferItem event_buffer_dequeue() {
	if (event_buffer_queue.count > 0) {
		EventBufferItem item = event_buffer_queue.items[event_buffer_queue.head];
		event_buffer_queue.head = (event_buffer_queue.head + 1) % EVENT_BUFFER_QUEUE_SIZE;
		event_buffer_queue.count--;
		return item;
	}

	// If the queue is empty, return an item with zeros
	EventBufferItem emptyItem = {.mode = 0, .secondCounter = 0, .milliCounter = 0, .microCounter = 0, .data = {0}};
	return emptyItem;
}
