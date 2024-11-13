#include "BlockBuffer.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

BlockBuffer::BlockBuffer(int blockNum) {
	if (blockNum < 0 || blockNum >= DISK_BLOCKS)
		this->blockNum = E_DISKFULL;
	else
		this->blockNum = blockNum;
}

BlockBuffer::BlockBuffer(char blocktypeC) {
	int blockType = blocktypeC == 'R' ? REC : 
					blocktypeC == 'I' ? IND_INTERNAL :
					blocktypeC == 'L' ? IND_LEAF : UNUSED_BLK; 

	int blockNum = getFreeBlock(blockType);
	if (blockNum < 0 || blockNum >= DISK_BLOCKS) {
		std::cout << "Error: Block is not available\n";
		this->blockNum = blockNum;
		return;
	}

	this->blockNum = blockNum;
}

RecBuffer::RecBuffer(int blockNum) : BlockBuffer(blockNum) {}

RecBuffer::RecBuffer() : BlockBuffer('R') {}

IndBuffer::IndBuffer(char blockType) : BlockBuffer(blockType) {}

IndBuffer::IndBuffer(int blockNum) : BlockBuffer(blockNum) {}

IndInternal::IndInternal() : IndBuffer('I') {}

IndInternal::IndInternal(int blockNum) : IndBuffer(blockNum) {}

IndLeaf::IndLeaf() : IndBuffer('L') {}

IndLeaf::IndLeaf(int blockNum) : IndBuffer(blockNum) {}

int BlockBuffer::getBlockNum() {
	return this->blockNum;
}

int BlockBuffer::getHeader(HeadInfo *head) {
	unsigned char *buffer;
	int ret = loadBlockAndGetBufferPtr(&buffer);
	if (ret != SUCCESS)
		return ret;

	memcpy(&head->pblock, buffer + 4, 4);
	memcpy(&head->lblock, buffer + 8, 4);
	memcpy(&head->rblock, buffer + 12, 4);
	memcpy(&head->numEntries, buffer + 16, 4);
	memcpy(&head->numAttrs, buffer + 20, 4);
	memcpy(&head->numSlots, buffer + 24, 4);

	return SUCCESS;
}

int BlockBuffer::setHeader(struct HeadInfo *head) {
	unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if (ret != SUCCESS) return ret;

	struct HeadInfo *bufferHeader = (struct HeadInfo *)bufferPtr;
	bufferHeader->blockType = head->blockType;
	bufferHeader->lblock = head->lblock;
	bufferHeader->rblock = head->rblock;
	bufferHeader->pblock = head->pblock;
	bufferHeader->numAttrs = head->numAttrs;
	bufferHeader->numEntries = head->numEntries;
	bufferHeader->numSlots = head->numSlots;

	ret = StaticBuffer::setDirtyBit(this->blockNum);
	if (ret != SUCCESS) return ret;

	return SUCCESS;
}

int RecBuffer::getRecord(union Attribute *record, int slotNum) {
	HeadInfo head;
	BlockBuffer::getHeader(&head);

	int attrCount = head.numAttrs;
	int slotCount = head.numSlots;

	unsigned char *buffer;
	int ret = loadBlockAndGetBufferPtr(&buffer);
	if (ret != SUCCESS)
		return ret;

	int recordSize = attrCount * ATTR_SIZE;
	unsigned char *slotPointer = buffer + (32 + slotCount + (recordSize * slotNum));

	memcpy(record, slotPointer, recordSize);

	return SUCCESS;
}

int RecBuffer::setRecord(union Attribute *record, int slotNum) {
	unsigned char *buffer;
	int ret = loadBlockAndGetBufferPtr(&buffer);
	if (ret != SUCCESS)
		return ret;

	HeadInfo head;
	BlockBuffer::getHeader(&head);

	int attrCount = head.numAttrs;
	int slotCount = head.numSlots;

	if (slotNum >= slotCount) return E_OUTOFBOUND;

	int recordSize = attrCount * ATTR_SIZE;
	unsigned char *slotPointer = buffer + (HEADER_SIZE + slotCount + (recordSize * slotNum));

	memcpy(slotPointer, record, recordSize);

	ret = StaticBuffer::setDirtyBit(this->blockNum);
	if (ret != SUCCESS) {
		exit(1);
	}

	return SUCCESS;
}

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr) {
	int bufferNum = StaticBuffer::getBufferNum(this->blockNum);
	if (bufferNum == E_OUTOFBOUND)
		return E_OUTOFBOUND;

	if (bufferNum != E_BLOCKNOTINBUFFER) {
		for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
			StaticBuffer::metainfo[bufferIndex].timeStamp++;
		}
		StaticBuffer::metainfo[bufferNum].timeStamp = 0;
	} else {
		bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);
		if (bufferNum == E_OUTOFBOUND || bufferNum == FAILURE)
			return bufferNum;

		Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
	}

	*buffPtr = StaticBuffer::blocks[bufferNum];

	return SUCCESS;
}

int RecBuffer::getSlotMap(unsigned char *slotMap) {
	unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if (ret != SUCCESS)
		return ret;

	RecBuffer recordBlock(this->blockNum);
	struct HeadInfo head;
	recordBlock.getHeader(&head);

	int slotCount = head.numSlots;
	unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;

	for (int slot = 0; slot < slotCount; slot++) 
		*(slotMap + slot) = *(slotMapInBuffer + slot);

	return SUCCESS;
}

int RecBuffer::setSlotMap(unsigned char *slotMap) {
	unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if (ret != SUCCESS) return ret;

	HeadInfo blockHeader;
	getHeader(&blockHeader);

	int numSlots = blockHeader.numSlots;
	unsigned char *slotPointer = bufferPtr + HEADER_SIZE;
	memcpy(slotPointer, slotMap, numSlots);

	ret = StaticBuffer::setDirtyBit(this->blockNum);
	if (ret != SUCCESS) return ret;

	return SUCCESS;
}

int BlockBuffer::setBlockType(int blockType) {
	unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if (ret != SUCCESS) return ret;

	(*(int32_t*) bufferPtr) = blockType;
	StaticBuffer::blockAllocMap[this->blockNum] = blockType;

	ret = StaticBuffer::setDirtyBit(this->blockNum);
	if (ret != SUCCESS) return ret;

	return SUCCESS;
}

int BlockBuffer::getFreeBlock(int blockType) {
	int blockNum = 0;
	for (; blockNum < DISK_BLOCKS; blockNum++) {
		if (StaticBuffer::blockAllocMap[blockNum] == UNUSED_BLK)
			break;
	}

	if (blockNum == DISK_BLOCKS) return E_DISKFULL;

	this->blockNum = blockNum;
	int bufferIndex = StaticBuffer::getFreeBuffer(blockNum);

	if (bufferIndex < 0 && bufferIndex >= BUFFER_CAPACITY) {
		return bufferIndex;
	}

	HeadInfo blockHeader;
	blockHeader.pblock = blockHeader.rblock = blockHeader.lblock = -1;
	blockHeader.numEntries = blockHeader.numAttrs = blockHeader.numSlots = 0;

	setHeader(&blockHeader);
	setBlockType(blockType);

	return blockNum;
}

void BlockBuffer::releaseBlock() {
	if (blockNum == INVALID_BLOCKNUM || 
		StaticBuffer::blockAllocMap[blockNum] == UNUSED_BLK)
		return;

	int bufferIndex = StaticBuffer::getBufferNum(blockNum);
	if (bufferIndex >= 0 && bufferIndex < BUFFER_CAPACITY)
		StaticBuffer::metainfo[bufferIndex].free = true;

	StaticBuffer::blockAllocMap[blockNum] = UNUSED_BLK;
	this->blockNum = INVALID_BLOCKNUM;
}

int compareAttrs(Attribute attr1, Attribute attr2, int attrType) {
	return attrType == NUMBER ? 
		(attr1.nVal < attr2.nVal ? -1 : (attr1.nVal > attr2.nVal ? 1 : 0)) : 
		strcmp(attr1.sVal, attr2.sVal);
}

int IndInternal::getEntry(void *ptr, int indexNum) {
	if (indexNum < 0 || indexNum >= MAX_KEYS_INTERNAL) return E_OUTOFBOUND;

	unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if (ret != SUCCESS) return ret;

	struct InternalEntry *internalEntry = (struct InternalEntry *)ptr;
	unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * 20);

	memcpy(&(internalEntry->lChild), entryPtr, sizeof(int32_t));
	memcpy(&(internalEntry->attrVal), entryPtr + 4, sizeof(Attribute));
	memcpy(&(internalEntry->rChild), entryPtr + 20, 4);

	return SUCCESS;
}

int IndLeaf::getEntry(void *ptr, int indexNum) {
	if (indexNum < 0 || indexNum >= MAX_KEYS_LEAF) return E_OUTOFBOUND;

	unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if (ret != SUCCESS) return ret;

	unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE);
	memcpy((struct Index *)ptr, entryPtr, LEAF_ENTRY_SIZE);

	return SUCCESS;
}

int IndInternal::setEntry(void *ptr, int indexNum) {
	if (indexNum < 0 || indexNum >= MAX_KEYS_INTERNAL) return E_OUTOFBOUND;

	unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if (ret != SUCCESS) return ret;

	struct InternalEntry *internalEntry = (struct InternalEntry *)ptr;
	unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * 20);

	memcpy(entryPtr, &(internalEntry->lChild), sizeof(int32_t));
	memcpy(entryPtr + 4, &(internalEntry->attrVal), sizeof(Attribute));
	memcpy(entryPtr + 20, &(internalEntry->rChild), 4);

	return SUCCESS;
}

int IndLeaf::setEntry(void *ptr, int indexNum) {
	if (indexNum < 0 || indexNum >= MAX_KEYS_LEAF) return E_OUTOFBOUND;

	unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if (ret != SUCCESS) return ret;

	unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE);
	memcpy(entryPtr, (struct Index *)ptr, LEAF_ENTRY_SIZE);

	return SUCCESS;
}
