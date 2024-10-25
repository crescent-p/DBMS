#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

BlockBuffer::BlockBuffer(int blockNum){
	this->blockNum = blockNum;
}

RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum){}

BlockBuffer::BlockBuffer(char blocktype){
	int blocVal = (blocktype == 'R') ? REC : UNUSED_BLK; 
	int blockNum = getFreeBlock(blocVal);
	
	if(blockNum < 0 || blockNum >= DISK_BLOCKS){
		this->blockNum = blockNum;
		return;
	}
	this->blockNum = blockNum;

	// unsigned char* blockPtr = new unsigned char[BLOCK_SIZE];
	// ret = loadBlockAndGetBufferPtr(&blockPtr);
	// if(ret != SUCCESS){
	// 	this->blockNum = ret;
	// 	return;
	// }

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

int RecBuffer::getRecord(union Attribute *rec, int slotNum){
		
	int startBlock = blockNum;

	struct HeadInfo head;
	RecBuffer::getHeader(&head);

	// //std::cout<<slotNum<<std::endl;
	// while(slotNum >= head.numSlots){
	// 	slotNum -= head.numSlots;
	// 	if(head.rblock == -1){
	// 		return E_NOTFOUND;
	// 	}
	// 	blockNum = head.rblock;

	// 	RecBuffer::getHeader(&head);
	// }

	// RecBuffer:getHeader(&head);

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

	//blockNum = startBlock;

	return SUCCESS;
}

int RecBuffer::getSlotMap(unsigned char *slotMap){
	unsigned char* blockBuffPtr = new unsigned char[BLOCK_SIZE];

	int res = BlockBuffer::loadBlockAndGetBufferPtr(&blockBuffPtr);

	if(res != SUCCESS){
		return res;
	}

	HeadInfo headInfo;
	RecBuffer recBuffer = RecBuffer(this->blockNum);
	recBuffer.getHeader(&headInfo);

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
	}else if(bufferNum == E_BLOCKNOTINBUFFER){
		bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

		if(bufferNum == E_OUTOFBOUND || bufferNum == FAILURE){
			return bufferNum;
		}

		Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
	}

	*bufferptr = StaticBuffer::blocks[bufferNum];

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


	// while(slotNum > headInfo.numSlots){
	// 	slotNum -= headInfo.numSlots;
	// 	if(headInfo.rblock == -1){
	// 		return E_NOTFOUND;
	// 	}
	// 	blockNum = headInfo.rblock;

	// 	RecBuffer::getHeader(&headInfo);
	// }

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

	ret = StaticBuffer::setDirtyBit(this->blockNum);
	
	if(ret != SUCCESS){
		return ret;
	}

	return SUCCESS;	
}

int BlockBuffer::setHeader(struct HeadInfo *head){
	unsigned char* bufferPtr;


	int ret = loadBlockAndGetBufferPtr(&bufferPtr);

	if(ret != SUCCESS){
		return ret;
	}
	 struct HeadInfo *bufferHeader = (struct HeadInfo *)bufferPtr;

	// copy the fields of the HeadInfo pointed to by head (except reserved) to
	// the header of the block (pointed to by bufferHeader)
	//(hint: bufferHeader->numSlots = head->numSlots )
	bufferHeader->blockType = head->blockType;
	bufferHeader->lblock = head->lblock;
	bufferHeader->rblock = head->rblock;
	bufferHeader->pblock = head->pblock;
	bufferHeader->numAttrs = head->numAttrs;
	bufferHeader->numEntries = head->numEntries;
	bufferHeader->numSlots = head->numSlots;

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
	int32_t *blockTypePtr = (int32_t*) bufferPtr;
	*blockTypePtr = blockType;

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

			HeadInfo headInfo;
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
	unsigned char* bufferPtr;
 
	int retVal = loadBlockAndGetBufferPtr(&bufferPtr);
	if(retVal != SUCCESS) return retVal;

	HeadInfo headInfo;
	getHeader(&headInfo);

	int numSlots = headInfo.numSlots;

	unsigned char* slotPtr = bufferPtr + HEADER_SIZE;

	memcpy(slotPtr, slotMap, numSlots);
	int ret = StaticBuffer::setDirtyBit(this->blockNum);

	// if setDirtyBit failed, return the value returned by the call
  	if (ret != SUCCESS)
   		return ret;
	return SUCCESS;

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