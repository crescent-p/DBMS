#include "StaticBuffer.h"
#include <stdio.h>

// the declarations for this class can be found at "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];
unsigned char StaticBuffer::blockAllocMap[DISK_BLOCKS];

void printBuffer(int bufferIndex, unsigned char buffer[]) {
	for (int i = 0; i < BLOCK_SIZE; i++) {
		if (i % 32 == 0) printf("\n");
		printf("%u ", buffer[i]);
	}
	printf("\n\n");
	for (int i = 0; i < BLOCK_SIZE; i++) {
		if (i % 32 == 0) printf("\n");
		printf("%c ", buffer[i]);
	}
	printf("\n");
}

void printBlockAllocMap(unsigned char blockAllocMap[]) {
	for (int i = 0; i < DISK_BLOCKS; i++) {
		if (i % 32 == 0) printf("\n");
		printf("%u ", blockAllocMap[i]);
	}
	printf("\n");
}

StaticBuffer::StaticBuffer() {
	for (int blockIndex = 0, blockAllocMapSlot = 0; blockIndex < 4; blockIndex++) {
		unsigned char buffer[BLOCK_SIZE];
		Disk::readBlock(buffer, blockIndex);

		for (int slot = 0; slot < BLOCK_SIZE; slot++, blockAllocMapSlot++)
			StaticBuffer::blockAllocMap[blockAllocMapSlot] = buffer[slot];
	}

	// initialize all blocks as free
	for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
		metainfo[bufferIndex].free = true;
		metainfo[bufferIndex].dirty = false;
		metainfo[bufferIndex].timeStamp = -1;
		metainfo[bufferIndex].blockNum = -1;
	}
}

// write back all modified blocks on system exit
StaticBuffer::~StaticBuffer() {
	for (int blockIndex = 0, blockAllocMapSlot = 0; blockIndex < 4; blockIndex++) {
		unsigned char buffer[BLOCK_SIZE];

		for (int slot = 0; slot < BLOCK_SIZE; slot++, blockAllocMapSlot++)
			buffer[slot] = blockAllocMap[blockAllocMapSlot];

		Disk::writeBlock(buffer, blockIndex);
	}

	// iterate through all the buffer blocks, write back blocks 
	// with metainfo as free=false,dirty=true using Disk::writeBlock()
	for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
		if (!metainfo[bufferIndex].free && metainfo[bufferIndex].dirty) {
			Disk::writeBlock(blocks[bufferIndex], metainfo[bufferIndex].blockNum);
		}
	}
}

int StaticBuffer::getFreeBuffer(int blockNum) {
	if (blockNum < 0 || blockNum >= DISK_BLOCKS) return E_OUTOFBOUND;
	for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
		metainfo[bufferIndex].timeStamp++;

	int allocatedBuffer = 0;

	// find the first free block in the buffer
	for (; allocatedBuffer < BUFFER_CAPACITY; allocatedBuffer++)
		if (metainfo[allocatedBuffer].free) break;

	if (allocatedBuffer == BUFFER_CAPACITY) {
		int lastTimestamp = -1, bufferNum = -1;
		for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
			if (metainfo[bufferIndex].timeStamp > lastTimestamp) {
				lastTimestamp = metainfo[bufferIndex].timeStamp;
				bufferNum = bufferIndex;
			}
		}

		allocatedBuffer = bufferNum;
		if (metainfo[allocatedBuffer].dirty) {
			Disk::writeBlock(StaticBuffer::blocks[allocatedBuffer], metainfo[allocatedBuffer].blockNum);
		}
	}

	metainfo[allocatedBuffer].free = false;
	metainfo[allocatedBuffer].dirty = false;
	metainfo[allocatedBuffer].timeStamp = 0;
	metainfo[allocatedBuffer].blockNum = blockNum;

	return allocatedBuffer;
}

int StaticBuffer::getBufferNum(int blockNum) {
	if (blockNum < 0 || blockNum >= DISK_BLOCKS) return E_OUTOFBOUND;

	for (int bufferBlock = 0; bufferBlock < BUFFER_CAPACITY; bufferBlock++) {
		if (!metainfo[bufferBlock].free && metainfo[bufferBlock].blockNum == blockNum)
			return bufferBlock;
	}

	return E_BLOCKNOTINBUFFER;
}

int StaticBuffer::setDirtyBit(int blockNum) {
	int bufferIndex = getBufferNum(blockNum);

	if (bufferIndex == E_BLOCKNOTINBUFFER || bufferIndex == E_OUTOFBOUND)
		return bufferIndex;

	metainfo[bufferIndex].dirty = true;
	return SUCCESS;
}

int StaticBuffer::getStaticBlockType(int blockNum) {
	if (blockNum < 0 || blockNum >= DISK_BLOCKS) return E_OUTOFBOUND;

	return static_cast<int>(blockAllocMap[blockNum]);
}
