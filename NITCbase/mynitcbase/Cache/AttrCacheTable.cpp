#include "AttrCacheTable.h"

#include <cstring>

AttrCacheEntry *attrCache[MAX_OPEN];

int AttrCacheTable::getAttrCatEntry(int relId, int attrOffset, AttrCatEntry* attrCatBuf){
	if(relId < 0 || relId >= MAX_OPEN){
		return E_OUTOFBOUND;
	}

	if(attrCache[relId] == nullptr){
		return E_RELNOTOPEN;
	}

	for(AttrCacheEntry* entry = attrCache[relId]; entry != nullptr; entry = entry->next){
		if(entry->attrCatEntry.offset == attrOffset){
			/*
				typedef struct AttrCatEntry {
				  char relName[ATTR_SIZE];
				  char attrName[ATTR_SIZE];
				  int attrType;
				  bool primaryFlag;
				  int rootBlock;
				  int offset;

				} AttrCatEntry;
			*/
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