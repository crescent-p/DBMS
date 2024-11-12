#include "BlockAccess.h"

#include <cstring>

inline bool operator==(RecId lhs, RecId rhs) {
	return (lhs.block == rhs.block && lhs.slot == rhs.slot);
}

inline bool operator!=(RecId lhs, RecId rhs) {
	return (lhs.block != rhs.block || lhs.slot != rhs.slot);
}

RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op) {
	RecId prevRecId;
	RelCacheTable::getSearchIndex(relId, &prevRecId);

	int block = -1, slot = -1;

	if (prevRecId.block == -1 && prevRecId.slot == -1) {
		RelCatEntry relCatBuffer;
		RelCacheTable::getRelCatEntry(relId, &relCatBuffer);
		block = relCatBuffer.firstBlk, slot = 0;
	} else {
		block = prevRecId.block, slot = prevRecId.slot + 1;
	}

	RelCatEntry relCatBuffer;
	RelCacheTable::getRelCatEntry(relId, &relCatBuffer);
	while (block != -1) {
		RecBuffer blockBuffer(block);
		HeadInfo blockHeader;
		blockBuffer.getHeader(&blockHeader);
		unsigned char slotMap[blockHeader.numSlots];
		blockBuffer.getSlotMap(slotMap);

		if (slot >= relCatBuffer.numSlotsPerBlk) {
			block = blockHeader.rblock, slot = 0;
			continue;
		}

		if (slotMap[slot] == SLOT_UNOCCUPIED) {
			slot++;
			continue;
		}

		Attribute record[blockHeader.numAttrs];
		blockBuffer.getRecord(record, slot);

		AttrCatEntry attrCatBuffer;
		AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatBuffer);
		int attrOffset = attrCatBuffer.offset;
		int cmpVal = compareAttrs(record[attrOffset], attrVal, attrCatBuffer.attrType);

		if ((op == NE && cmpVal != 0) || (op == LT && cmpVal < 0) || (op == LE && cmpVal <= 0) ||
			(op == EQ && cmpVal == 0) || (op == GT && cmpVal > 0) || (op == GE && cmpVal >= 0)) {
			RecId newRecId = {block, slot};
			RelCacheTable::setSearchIndex(relId, &newRecId);
			return RecId{block, slot};
		}

		slot++;
	}

	RelCacheTable::resetSearchIndex(relId);
	return RecId{-1, -1};
}

int BlockAccess::renameRelation(char oldName[ATTR_SIZE], char newName[ATTR_SIZE]) {
	RelCacheTable::resetSearchIndex(RELCAT_RELID);

	Attribute newRelationName;
	strcpy(newRelationName.sVal, newName);
	RecId searchIndex = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, newRelationName, EQ);

	if (searchIndex != RecId{-1, -1})
		return E_RELEXIST;

	RelCacheTable::resetSearchIndex(RELCAT_RELID);

	Attribute oldRelationName;
	strcpy(oldRelationName.sVal, oldName);
	searchIndex = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, oldRelationName, EQ);

	if (searchIndex == RecId{-1, -1})
		return E_RELNOTEXIST;

	RecBuffer relCatBlock(RELCAT_BLOCK);
	Attribute relCatRecord[RELCAT_NO_ATTRS];
	relCatBlock.getRecord(relCatRecord, searchIndex.slot);
	strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, newName);
	relCatBlock.setRecord(relCatRecord, searchIndex.slot);

	RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

	for (int attrIndex = 0; attrIndex < relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal; attrIndex++) {
		searchIndex = BlockAccess::linearSearch(ATTRCAT_RELID, ATTRCAT_ATTR_RELNAME, oldRelationName, EQ);
		RecBuffer attrCatBlock(searchIndex.block);
		Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
		attrCatBlock.getRecord(attrCatRecord, searchIndex.slot);
		strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, newName);
		attrCatBlock.setRecord(attrCatRecord, searchIndex.slot);
	}

	return SUCCESS;
}

int BlockAccess::renameAttribute(char relName[ATTR_SIZE], char oldName[ATTR_SIZE], char newName[ATTR_SIZE]) {
	RelCacheTable::resetSearchIndex(RELCAT_RELID);

	Attribute relNameAttr;
	strcpy(relNameAttr.sVal, relName);
	RecId searchIndex = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, relNameAttr, EQ);

	if (searchIndex == RecId{-1, -1})
		return E_RELNOTEXIST;

	RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

	RecId attrToRenameRecId{-1, -1};

	while (true) {
		searchIndex = BlockAccess::linearSearch(ATTRCAT_RELID, ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);

		if (searchIndex == RecId{-1, -1})
			break;

		RecBuffer attrCatBlock(searchIndex.block);
		Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
		attrCatBlock.getRecord(attrCatRecord, searchIndex.slot);

		if (strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldName) == 0) {
			attrToRenameRecId = searchIndex;
			break;
		}

		if (strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName) == 0)
			return E_ATTREXIST;
	}

	if (attrToRenameRecId == RecId{-1, -1})
		return E_ATTRNOTEXIST;

	RecBuffer attrCatBlock(attrToRenameRecId.block);
	Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
	attrCatBlock.getRecord(attrCatRecord, attrToRenameRecId.slot);
	strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName);
	attrCatBlock.setRecord(attrCatRecord, attrToRenameRecId.slot);

	return SUCCESS;
}

int BlockAccess::insert(int relId, Attribute *record) {
	RelCatEntry relCatEntry;
	RelCacheTable::getRelCatEntry(relId, &relCatEntry);

	int blockNum = relCatEntry.firstBlk;
	RecId rec_id = {-1, -1};
	int numOfSlots = relCatEntry.numSlotsPerBlk;
	int numOfAttributes = relCatEntry.numAttrs;
	int prevBlockNum = -1;

	while (blockNum != -1) {
		RecBuffer blockBuffer(blockNum);
		HeadInfo blockHeader;
		blockBuffer.getHeader(&blockHeader);
		unsigned char slotMap[blockHeader.numSlots];
		blockBuffer.getSlotMap(slotMap);

		for (int slotIndex = 0; slotIndex < blockHeader.numSlots; slotIndex++) {
			if (slotMap[slotIndex] == SLOT_UNOCCUPIED) {
				rec_id = RecId{blockNum, slotIndex};
				break;
			}
		}

		if (rec_id != RecId{-1, -1})
			break;

		prevBlockNum = blockNum;
		blockNum = blockHeader.rblock;
	}

	if (rec_id == RecId{-1, -1}) {
		if (relId == RELCAT_RELID)
			return E_MAXRELATIONS;

		RecBuffer blockBuffer;
		blockNum = blockBuffer.getBlockNum();

		if (blockNum == E_DISKFULL)
			return E_DISKFULL;

		rec_id = RecId{blockNum, 0};

		HeadInfo blockHeader;
		blockHeader.blockType = REC;
		blockHeader.lblock = prevBlockNum, blockHeader.rblock = blockHeader.pblock = -1;
		blockHeader.numAttrs = numOfAttributes, blockHeader.numSlots = numOfSlots, blockHeader.numEntries = 0;
		blockBuffer.setHeader(&blockHeader);

		unsigned char slotMap[numOfSlots];
		for (int slotIndex = 0; slotIndex < numOfSlots; slotIndex++)
			slotMap[slotIndex] = SLOT_UNOCCUPIED;
		blockBuffer.setSlotMap(slotMap);

		if (prevBlockNum != -1) {
			RecBuffer prevBlockBuffer(prevBlockNum);
			HeadInfo prevBlockHeader;
			prevBlockBuffer.getHeader(&prevBlockHeader);
			prevBlockHeader.rblock = blockNum;
			prevBlockBuffer.setHeader(&prevBlockHeader);
		} else {
			relCatEntry.firstBlk = blockNum;
			RelCacheTable::setRelCatEntry(relId, &relCatEntry);
		}

		relCatEntry.lastBlk = blockNum;
		RelCacheTable::setRelCatEntry(relId, &relCatEntry);
	}

	RecBuffer blockBuffer(rec_id.block);
	blockBuffer.setRecord(record, rec_id.slot);

	unsigned char slotmap[numOfSlots];
	blockBuffer.getSlotMap(slotmap);
	slotmap[rec_id.slot] = SLOT_OCCUPIED;
	blockBuffer.setSlotMap(slotmap);

	HeadInfo blockHeader;
	blockBuffer.getHeader(&blockHeader);
	blockHeader.numEntries++;
	blockBuffer.setHeader(&blockHeader);

	relCatEntry.numRecs++;
	RelCacheTable::setRelCatEntry(relId, &relCatEntry);

	int flag = SUCCESS;
	for (int attrindex = 0; attrindex < numOfAttributes; attrindex++) {
		AttrCatEntry attrCatEntryBuffer;
		AttrCacheTable::getAttrCatEntry(relId, attrindex, &attrCatEntryBuffer);
		int rootBlock = attrCatEntryBuffer.rootBlock;

		if (rootBlock != -1) {
			int ret = BPlusTree::bPlusInsert(relId, attrCatEntryBuffer.attrName, record[attrindex], rec_id);
			if (ret == E_DISKFULL) {
				flag = E_INDEX_BLOCKS_RELEASED;
			}
		}
	}

	return flag;
}

int BlockAccess::search(int relId, Attribute *record, char attrName[ATTR_SIZE], Attribute attrVal, int op) {
	RecId recId;
	AttrCatEntry attrCatEntry;
	int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

	if (ret != SUCCESS)
		return ret;

	int rootBlock = attrCatEntry.rootBlock;

	if (rootBlock == -1) {
		recId = linearSearch(relId, attrName, attrVal, op);
	} else {
		recId = BPlusTree::bPlusSearch(relId, attrName, attrVal, op);
	}

	if (recId == RecId{-1, -1})
		return E_NOTFOUND;

	RecBuffer recordBuffer(recId.block);
	recordBuffer.getRecord(record, recId.slot);

	return SUCCESS;
}

int BlockAccess::deleteRelation(char relName[ATTR_SIZE]) {
	if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0)
		return E_NOTPERMITTED;

	RelCacheTable::resetSearchIndex(RELCAT_RELID);

	Attribute relNameAttr;
	strcpy((char*)relNameAttr.sVal, (const char*)relName);
	RecId relCatRecId = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, relNameAttr, EQ);

	if (relCatRecId == RecId{-1, -1})
		return E_RELNOTEXIST;

	RecBuffer relCatBlockBuffer(relCatRecId.block);
	Attribute relCatEntryRecord[RELCAT_NO_ATTRS];
	relCatBlockBuffer.getRecord(relCatEntryRecord, relCatRecId.slot);

	int firstBlock = relCatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;
	int numAttributes = relCatEntryRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

	int currentBlockNum = firstBlock;
	while (currentBlockNum != -1) {
		RecBuffer currentBlockBuffer(currentBlockNum);
		HeadInfo currentBlockHeader;
		currentBlockBuffer.getHeader(&currentBlockHeader);
		currentBlockNum = currentBlockHeader.rblock;
		currentBlockBuffer.releaseBlock();
	}

	RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
	int numberOfAttributesDeleted = 0;

	while (true) {
		RecId attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, RELCAT_ATTR_RELNAME, relNameAttr, EQ);

		if (attrCatRecId == RecId{-1, -1})
			break;

		numberOfAttributesDeleted++;

		RecBuffer attrCatBlockBuffer(attrCatRecId.block);
		HeadInfo attrCatHeader;
		attrCatBlockBuffer.getHeader(&attrCatHeader);
		Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
		attrCatBlockBuffer.getRecord(attrCatRecord, attrCatRecId.slot);

		int rootBlock = attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
		unsigned char slotmap[attrCatHeader.numSlots];
		attrCatBlockBuffer.getSlotMap(slotmap);
		slotmap[attrCatRecId.slot] = SLOT_UNOCCUPIED;
		attrCatBlockBuffer.setSlotMap(slotmap);
		attrCatHeader.numEntries--;
		attrCatBlockBuffer.setHeader(&attrCatHeader);

		if (attrCatHeader.numEntries == 0) {
			RecBuffer prevBlock(attrCatHeader.lblock);
			HeadInfo leftHeader;
			prevBlock.getHeader(&leftHeader);
			leftHeader.rblock = attrCatHeader.rblock;
			prevBlock.setHeader(&leftHeader);

			if (attrCatHeader.rblock != INVALID_BLOCKNUM) {
				RecBuffer nextBlock(attrCatHeader.rblock);
				HeadInfo rightHeader;
				nextBlock.getHeader(&rightHeader);
				rightHeader.lblock = attrCatHeader.lblock;
				nextBlock.setHeader(&rightHeader);
			} else {
				RelCatEntry relCatEntryBuffer;
				RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatEntryBuffer);
				relCatEntryBuffer.lastBlk = attrCatHeader.lblock;
				RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &relCatEntryBuffer);
			}

			attrCatBlockBuffer.releaseBlock();
			RecId nextSearchIndex = {attrCatHeader.rblock, 0};
			RelCacheTable::setSearchIndex(ATTRCAT_RELID, &nextSearchIndex);
		}

		if (rootBlock != -1) {
			BPlusTree::bPlusDestroy(rootBlock);
		}

		if (numberOfAttributesDeleted == numAttributes)
			break;
	}

	HeadInfo relCatHeader;
	relCatBlockBuffer.getHeader(&relCatHeader);
	relCatHeader.numEntries--;
	relCatBlockBuffer.setHeader(&relCatHeader);

	unsigned char slotmap[relCatHeader.numSlots];
	relCatBlockBuffer.getSlotMap(slotmap);
	slotmap[relCatRecId.slot] = SLOT_UNOCCUPIED;
	relCatBlockBuffer.setSlotMap(slotmap);

	RelCatEntry relCatEntryBuffer;
	RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatEntryBuffer);
	relCatEntryBuffer.numRecs--;
	RelCacheTable::setRelCatEntry(RELCAT_RELID, &relCatEntryBuffer);

	RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatEntryBuffer);
	relCatEntryBuffer.numRecs -= numberOfAttributesDeleted;
	RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &relCatEntryBuffer);

	return SUCCESS;
}

int BlockAccess::project(int relId, Attribute *record) {
	RecId prevSearchIndex;
	RelCacheTable::getSearchIndex(relId, &prevSearchIndex);

	int block, slot;

	if (prevSearchIndex.block == -1 && prevSearchIndex.slot == -1) {
		RelCatEntry relCatEntryBuffer;
		RelCacheTable::getRelCatEntry(relId, &relCatEntryBuffer);
		block = relCatEntryBuffer.firstBlk, slot = 0;
	} else {
		block = prevSearchIndex.block, slot = prevSearchIndex.slot + 1;
	}

	while (block != -1) {
		RecBuffer currentBlockBuffer(block);
		HeadInfo currentBlockHeader;
		currentBlockBuffer.getHeader(&currentBlockHeader);
		unsigned char slotmap[currentBlockHeader.numSlots];
		currentBlockBuffer.getSlotMap(slotmap);

		if (slot >= currentBlockHeader.numSlots) {
			block = currentBlockHeader.rblock, slot = 0;
		} else if (slotmap[slot] == SLOT_UNOCCUPIED) {
			slot++;
		} else {
			break;
		}
	}

	if (block == -1) {
		return E_NOTFOUND;
	}

	RecId nextSearchIndex{block, slot};
	RelCacheTable::setSearchIndex(relId, &nextSearchIndex);

	RecBuffer recordBlockBuffer(block);
	recordBlockBuffer.getRecord(record, slot);

	return SUCCESS;
}
