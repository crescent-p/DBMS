#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

BlockBuffer::BlockBuffer(int blockNum){
	this->blockNum = blockNum;
}

RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum){}

int BlockBuffer::getHeader(struct HeadInfo *head){
	unsigned char buffer[BLOCK_SIZE];

	Disk::readBlock(buffer, this->blockNum);

	memcpy(&head->numSlots, buffer + 24, 4);
	memcpy(&head->numEntries, buffer + 16, 4);
	memcpy(&head->numAttrs, buffer + 20, 4);
	memcpy(&head->rblock, buffer + 12, 4);
	memcpy(&head->lblock, buffer + 8, 4);
	memcpy(&head->pblock, buffer + 4, 4);

	return SUCCESS;
}
using namespace std;
int RecBuffer::getRecord(union Attribute *rec, int slotNum){
	struct HeadInfo head;

	RecBuffer:getHeader(&head);

	int attrCount = head.numAttrs;
	int slotCount = head.numSlots;


	unsigned char buffer[BLOCK_SIZE];

	Disk::readBlock(buffer, this->blockNum);

	int recordSize = attrCount * ATTR_SIZE;
	//each slot require one byte for slotCount
	//recordSize = attrCount* ATTR_size
	int slotMapSize = slotCount;
	unsigned char *slotPointer = (buffer + HEADER_SIZE + slotMapSize + recordSize * slotNum);
	
	memcpy(rec, slotPointer, recordSize);

	return SUCCESS;
}
