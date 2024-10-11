#include "StaticBuffer.h"
#include <string.h>
#include <stdio.h>

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];
unsigned char StaticBuffer::blockAllocMap[DISK_BLOCKS];


StaticBuffer::StaticBuffer(){

	for(int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++){
			metainfo[bufferIndex].free = true;
			metainfo[bufferIndex].dirty = false;
			metainfo[bufferIndex].blockNum = -1;
			metainfo[bufferIndex].timeStamp = -1;

		}
	
	//load BlockallocationMap into Buffer.
	for(int blockNum = 0; blockNum <= 3; blockNum++){
		Disk::readBlock(blockAllocMap + BLOCK_SIZE*blockNum, blockNum);
	}


}

StaticBuffer::~StaticBuffer(){
	for(int blockNum = 0; blockNum <= 3; blockNum++){
		Disk::writeBlock(blockAllocMap + BLOCK_SIZE*blockNum, blockNum);
	}

	for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
		if (metainfo[bufferIndex].free == false and
			metainfo[bufferIndex].dirty == true) {
			Disk::writeBlock(blocks[bufferIndex], metainfo[bufferIndex].blockNum);
		}
  }
}

int StaticBuffer::getFreeBuffer(int blockNum){
	if(blockNum < 0 || blockNum > DISK_BLOCKS){
		return E_OUTOFBOUND;
	}
	int allocatedBuffer = -1;
	for(int i = 0; i < BUFFER_CAPACITY; i++){
		if(metainfo[i].free == false){
			metainfo[i].timeStamp++;
		}
	}


	for(int i = 0; i < BUFFER_CAPACITY; i++){
		if(metainfo[i].free == true){
			allocatedBuffer = i;
			break;
		}
	}

	if(allocatedBuffer == -1){
		int highestIndex = 0;
		for(int i = 0; i < BUFFER_CAPACITY; i++){
			if(metainfo[i].timeStamp > highestIndex){
				highestIndex = i;
			}
		}
		if(metainfo[highestIndex].dirty == true){
			Disk::writeBlock(blocks[highestIndex], metainfo[highestIndex].blockNum);
		}
		allocatedBuffer =  highestIndex;
	}
	metainfo[allocatedBuffer].free = false;
  	metainfo[allocatedBuffer].blockNum = blockNum;
  	metainfo[allocatedBuffer].dirty = false;
  	metainfo[allocatedBuffer].timeStamp = 0;

	return allocatedBuffer;

}

int StaticBuffer::getBufferNum(int blockNum){
	if(blockNum < 0 || blockNum > DISK_BLOCKS){
		return E_OUTOFBOUND;
	}
	for(int i = 0; i < BUFFER_CAPACITY; i++){
		if(metainfo[i].blockNum == blockNum){
			return i;
		}
	}
	return E_BLOCKNOTINBUFFER;
}

int StaticBuffer::setDirtyBit(int blockNum){
	int index = StaticBuffer::getBufferNum(blockNum);

	if(index == E_OUTOFBOUND) { 
		return E_OUTOFBOUND;
	}
	if(index == E_BLOCKNOTINBUFFER) {
		return E_BLOCKNOTINBUFFER;
	}

	metainfo[index].dirty = true;

	return SUCCESS;
}