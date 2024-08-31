#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

BlockBuffer::BlockBuffer(int blockNum){
	this->blockNum = blockNum;
}

RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum){}

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
	}

	*bufferptr = StaticBuffer::blocks[bufferNum];
	StaticBuffer::metainfo[bufferNum].blockNum = this->blockNum;
	StaticBuffer::metainfo[bufferNum].free = false;

	StaticBuffer::setDirtyBit(bufferNum);

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


	while(slotNum >= headInfo.numSlots){
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

	int recordSize = attrCount * ATTR_SIZE;
	//each slot require one byte for slotCount
	//recordSize = attrCount* ATTR_size
	int slotMapSize = slotCount;
	bufferPtr = (bufferPtr + HEADER_SIZE + slotMapSize + recordSize * slotNum);


	memcpy(bufferPtr, rec, recordSize);

	return SUCCESS;	
}
