#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

BlockBuffer::BlockBuffer(int blockNum){
	this->blockNum = blockNum;
}

RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum){}

BlockBuffer::BlockBuffer(char blockType){
	int ret = getFreeBlock(blockType);
	
	if(ret == E_DISKFULL){
		this->blockNum = E_DISKFULL;
		return;
	}
	this->blockNum = ret;

	unsigned char* blockPtr = new unsigned char[BLOCK_SIZE];
	ret = loadBlockAndGetBufferPtr(&blockPtr);
	if(ret != SUCCESS){
		this->blockNum = ret;
		return;
	}

}
RecBuffer::RecBuffer() : BlockBuffer('R'){}

int BlockBuffer::getHeader(struct HeadInfo *head){
	unsigned char* bufferptr;

	int res = loadBlockAndGetBufferPtr(&bufferptr);

	if(res != SUCCESS){
		return res;
	}

	// Disk::readBlock(buffer, this->blockNum);

	memcpy(&head->numSlots, bufferptr + 24, 4);
	memcpy(&head->numEntries, bufferptr + 16, 4);
	memcpy(&head->numAttrs, bufferptr + 20, 4);
	memcpy(&head->rblock, bufferptr + 12, 4);
	memcpy(&head->lblock, bufferptr + 8, 4);
	memcpy(&head->pblock, bufferptr + 4, 4);

	return SUCCESS;
}
using namespace std;
int RecBuffer::getRecord(union Attribute *rec, int slotNum){
		
	int startBlock = blockNum;

	struct HeadInfo head;
	RecBuffer::getHeader(&head);

	//std::cout<<slotNum<<std::endl;
	while(slotNum >= head.numSlots){
		slotNum -= head.numSlots;
		if(head.rblock == -1){
			return E_NOTFOUND;
		}
		blockNum = head.rblock;

		RecBuffer::getHeader(&head);
	}

	RecBuffer:getHeader(&head);

	int attrCount = head.numAttrs;
	int slotCount = head.numSlots;


	unsigned char* buffer = new unsigned char[BLOCK_SIZE];

	// Disk::readBlock(buffer, this->blockNum);
	int res = loadBlockAndGetBufferPtr(&buffer);
	if(res != SUCCESS){
		return res;
	}

	int recordSize = attrCount * ATTR_SIZE;
	//each slot require one byte for slotCount
	//recordSize = attrCount* ATTR_size
	int slotMapSize = slotCount;
	unsigned char *slotPointer = (buffer + HEADER_SIZE + slotMapSize + recordSize * slotNum);
	
	memcpy(rec, slotPointer, recordSize);

	blockNum = startBlock;

	return SUCCESS;
}

int RecBuffer::getSlotMap(unsigned char *slotMap){
	unsigned char* blockBuffPtr = new unsigned char[BLOCK_SIZE];

	int res = BlockBuffer::loadBlockAndGetBufferPtr(&blockBuffPtr);

	if(res != SUCCESS){
		return res;
	}

	HeadInfo headInfo;
	RecBuffer::getHeader(&headInfo);

	int slotCount = headInfo.numSlots;

	unsigned char* slotMapPtr = blockBuffPtr + HEADER_SIZE;

	memcpy(slotMap, slotMapPtr, slotCount);

	return SUCCESS;
}

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **bufferptr){
	if(blockNum < 0) return E_OUTOFBOUND;
	int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

	if(bufferNum != E_BLOCKNOTINBUFFER){
		for(int i = 0; i < BUFFER_CAPACITY; i++){
			if(StaticBuffer::metainfo[i].free == false)
				StaticBuffer::metainfo[i].timeStamp++;	
		}
		StaticBuffer::metainfo[bufferNum].timeStamp = 0;
	}else{
		bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

		if(bufferNum == E_OUTOFBOUND){
			return E_OUTOFBOUND;
		}

		Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
		*bufferptr = StaticBuffer::blocks[bufferNum];
		StaticBuffer::metainfo[bufferNum].blockNum = this->blockNum;
		StaticBuffer::metainfo[bufferNum].free = false;
		//StaticBuffer::metainfo[bufferNum].dirty = true;
		return SUCCESS;
	}

	*bufferptr = StaticBuffer::blocks[bufferNum];
	StaticBuffer::metainfo[bufferNum].blockNum = this->blockNum;
	StaticBuffer::metainfo[bufferNum].free = false;
	//StaticBuffer::metainfo[bufferNum].dirty = true;

	return SUCCESS;
}

int compareAttrs(Attribute attr1, Attribute attr2, int attrType){
	double diff = 0;
	if(attrType == STRING){
		diff = strcmp(attr1.sVal, attr2.sVal);
	}else{
		diff = attr1.nVal - attr2.nVal;
	}
	if(diff > 0) return 1;
	if(diff < 0) return -1;
	return 0;
}


int RecBuffer::setRecord(union Attribute *rec, int slotNum){
	unsigned char* bufferPtr = new unsigned char[BLOCK_SIZE];
	int ret = BlockBuffer::loadBlockAndGetBufferPtr(&bufferPtr);	

	if(ret != SUCCESS){
		return ret;
	}

	HeadInfo headInfo = *(new HeadInfo);
	getHeader(&headInfo);


	while(slotNum > headInfo.numSlots){
		slotNum -= headInfo.numSlots;
		if(headInfo.rblock == -1){
			return E_NOTFOUND;
		}
		blockNum = headInfo.rblock;

		RecBuffer::getHeader(&headInfo);
	}

	int attrCount = headInfo.numAttrs;
	int slotCount = headInfo.numSlots;

	if(slotNum < 0 || slotNum >= slotCount){
		return E_OUTOFBOUND;
	}
	if(blockNum < 0 || blockNum > DISK_BLOCKS){
		return E_OUTOFBOUND;
	}
	

	int recordSize = attrCount * ATTR_SIZE;
	//each slot require one byte for slotCount
	//recordSize = attrCount* ATTR_size
	int slotMapSize = slotCount;
	bufferPtr = (bufferPtr + HEADER_SIZE + slotMapSize + recordSize * slotNum);


	memcpy(bufferPtr, rec, recordSize);

	StaticBuffer::setDirtyBit(this->blockNum);

	return SUCCESS;	
}

int BlockBuffer::setHeader(struct HeadInfo *head){
	unsigned char* bufferPtr = new unsigned char[BLOCK_SIZE];


	int ret = loadBlockAndGetBufferPtr(&bufferPtr);

	if(ret != SUCCESS){
		return SUCCESS;
	}
	memcpy(bufferPtr + 24, &head->numSlots, 4);
	memcpy(bufferPtr + 16, &head->numEntries, 4);
	memcpy(bufferPtr + 20, &head->numAttrs, 4);
	memcpy(bufferPtr + 12, &head->rblock, 4);
	memcpy(bufferPtr + 8, &head->lblock, 4);
	memcpy(bufferPtr + 4, &head->pblock, 4);

	ret = StaticBuffer::setDirtyBit(this->blockNum);

	if(ret != SUCCESS){
		return ret;
	}

	return SUCCESS;
}

int BlockBuffer::setBlockType(int blockType){
	unsigned char* bufferPtr = new unsigned char[BLOCK_SIZE];

	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if(ret != SUCCESS){
		return ret;
	}
	//setting first 4 bytes to blockType.
	*((int32_t *)bufferPtr) = blockType;

	StaticBuffer::blockAllocMap[this->blockNum] = blockType;
	ret = StaticBuffer::setDirtyBit(this->blockNum);
	if(ret != SUCCESS){
		return ret;
	}
	return SUCCESS;
}

int BlockBuffer::getFreeBlock(int blockType){

	for(int blockNum = 0; blockNum < DISK_BLOCKS; blockNum++){
		if(StaticBuffer::blockAllocMap[blockNum] == UNUSED_BLK){
			
			this->blockNum = blockNum;

			HeadInfo headInfo = *(new HeadInfo);
			headInfo.lblock = -1;
			headInfo.rblock = -1;
			headInfo.pblock = -1;
			headInfo.numAttrs = 0;
			headInfo.numEntries = 0;
			headInfo.numSlots = 0;
			setHeader(&headInfo);
			setBlockType(blockType);

			return blockNum;
		}
	}

	return E_DISKFULL;
}

int RecBuffer::setSlotMap(unsigned char *slotMap){
	unsigned char* bufferPtr = new unsigned char[BLOCK_SIZE];

	int retVal = loadBlockAndGetBufferPtr(&bufferPtr);
	if(retVal != SUCCESS) return retVal;

	HeadInfo headInfo = *(new HeadInfo);
	getHeader(&headInfo);

	int numSlots = headInfo.numSlots;

	unsigned char* slotPtr = bufferPtr + HEADER_SIZE;

	memcpy(slotPtr, slotMap, numSlots);

	retVal = StaticBuffer::setDirtyBit(this->blockNum);

	return retVal;

}

int BlockBuffer::getBlockNum(){
	return this->blockNum;
}

void BlockBuffer::releaseBlock(){
	if(this->blockNum == INVALID_BLOCKNUM) return;
	else{
		int bufferNum = StaticBuffer::getBufferNum(this->blockNum);
		if(bufferNum != E_BLOCKNOTINBUFFER){
			StaticBuffer::metainfo[bufferNum].free = true;
		}
		StaticBuffer::blockAllocMap[this->blockNum] = UNUSED_BLK;

		this->blockNum = INVALID_BLOCKNUM;
	}
}