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

void RelCacheTable::recordToRelCatEntry(union Attribute record[RELCAT_NO_ATTRS],
									RelCatEntry* relCatEntry){
	strcpy(relCatEntry->relName, record[RELCAT_REL_NAME_INDEX].sVal);
	relCatEntry->numAttrs = record[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
	relCatEntry->numRecs = record[RELCAT_NO_RECORDS_INDEX].nVal;
	relCatEntry->firstBlk = record[RELCAT_FIRST_BLOCK_INDEX].nVal;
	relCatEntry->lastBlk = record[RELCAT_LAST_BLOCK_INDEX].nVal;
	relCatEntry->numSlotsPerBlk = record[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal;
}
