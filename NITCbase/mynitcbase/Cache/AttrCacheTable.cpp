#include "AttrCacheTable.h"

#include <cstring>

AttrCacheEntry* AttrCacheTable::attrCache[MAX_OPEN];

int AttrCacheTable::getAttrCatEntry(int relId, int attrOffset, AttrCatEntry* attrCatBuf){
	if(relId < 0 || relId >= MAX_OPEN){
		return E_OUTOFBOUND;
	}

	if(AttrCacheTable::attrCache[relId] == nullptr){
		return E_RELNOTOPEN;
	}

	for(AttrCacheEntry* entry = AttrCacheTable::attrCache[relId]; entry != nullptr; entry = entry->next){
		if(entry != nullptr && entry->attrCatEntry.offset == attrOffset){

			strcpy(attrCatBuf->relName, entry->attrCatEntry.relName); 
			strcpy(attrCatBuf->attrName, entry->attrCatEntry.attrName);
			attrCatBuf->attrType = entry->attrCatEntry.attrType;
			attrCatBuf->primaryFlag = entry->attrCatEntry.primaryFlag;
			attrCatBuf->rootBlock = entry->attrCatEntry.rootBlock;
			attrCatBuf->offset = entry->attrCatEntry.offset;

			return SUCCESS;
		}
	}

	return E_ATTRNOTEXIST;
}

void AttrCacheTable::recordToAttrCatEntry(union Attribute record[ATTRCAT_NO_ATTRS], AttrCatEntry *attrCatEntry){

	strcpy(attrCatEntry->relName, record[ATTRCAT_REL_NAME_INDEX].sVal);
	strcpy(attrCatEntry->attrName, record[ATTRCAT_ATTR_NAME_INDEX].sVal);
	attrCatEntry->attrType = record[ATTRCAT_ATTR_TYPE_INDEX].nVal;
	attrCatEntry->primaryFlag = record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal;
	attrCatEntry->rootBlock = record[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
	attrCatEntry->offset = record[ATTRCAT_OFFSET_INDEX].nVal;

}
