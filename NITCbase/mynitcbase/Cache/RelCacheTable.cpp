#include "RelCacheTable.h"

#include <cstring>

RelCacheEntry* RelCacheTable::relCache[MAX_OPEN];

int RelCacheTable::getRelCatEntry(int relId, RelCatEntry* relCatBuf){
	if(relId < 0 || relId >= MAX_OPEN){
		return E_OUTOFBOUND;
	}

	if(relCache[relId] == nullptr){
		return E_RELNOTOPEN;
	}

	*relCatBuf = relCache[relId]->relCatEntry;

	return SUCCESS;
}

int RelCacheTable::findSlotOfRelation(char relName[ATTR_SIZE]){

	RecBuffer relBlock(RELCAT_BLOCK);

	HeadInfo headInfo;
	relBlock.getHeader(&headInfo);

	for(int i = 0; i < headInfo.numEntries; i++){
		Attribute* rels = new Attribute[ATTR_SIZE];
		relBlock.getRecord(rels, i);
		if(strcmp(relName, rels[RELCAT_REL_NAME_INDEX].sVal) == 0) {
			return i;
		}
	}

	return E_RELNOTEXIST;
}


void RelCacheTable::recordToRelCatEntry(union Attribute record[RELCAT_NO_ATTRS],
									RelCatEntry* relCatEntry){
	strcpy(relCatEntry->relName, record[RELCAT_REL_NAME_INDEX].sVal);
	relCatEntry->numAttrs = record[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
	relCatEntry->numRecs = record[RELCAT_NO_RECORDS_INDEX].nVal;
	relCatEntry->firstBlk = record[RELCAT_FIRST_BLOCK_INDEX].nVal;
	relCatEntry->lastBlk = record[RELCAT_LAST_BLOCK_INDEX].nVal;
	relCatEntry->numSlotsPerBlk = record[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal;
}
int RelCacheTable::getSearchIndex(int relId, RecId *searchIndex){
	if(relId < 0 || relId > MAX_OPEN){
		return E_OUTOFBOUND;
	}
	if(RelCacheTable::relCache[relId] == nullptr){
		return E_RELNOTOPEN;
	}

	*searchIndex = RelCacheTable::relCache[relId]->searchIndex;
	return SUCCESS;
}

int RelCacheTable::setSearchIndex(int relId, RecId *searchIndex){
	if(relId < 0 || relId >= MAX_OPEN){
		return E_OUTOFBOUND;
	}
	if(relCache[relId] == nullptr){
		return E_RELNOTOPEN;
	}

	relCache[relId]->searchIndex.block = searchIndex->block;
	relCache[relId]->searchIndex.slot = searchIndex->slot;
	return SUCCESS;
}

int RelCacheTable::resetSearchIndex(int relId){
	if(relId < 0 || relId >= MAX_OPEN){
		return E_OUTOFBOUND;
	}
	if(relCache[relId] == nullptr){
		return E_RELNOTOPEN;
	}
	relCache[relId]->searchIndex.block = -1;
	relCache[relId]->searchIndex.slot = -1;

	return SUCCESS;
}