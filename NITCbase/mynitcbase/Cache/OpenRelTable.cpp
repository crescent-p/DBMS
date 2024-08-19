#include "OpenRelTable.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

/*
Step 1: get RelationCatalog and store in relCacheEntry, then store it in relCache which is declared in RelCacheTable.cpp
	currently there are only two realtionCatalog caches RelationCatalog and AttrCatalog

Step 2: get RelCatalog and store in attrCacheEntry. Create a linked list of all the attributes and store in RELATTR_RELID of attrCache
do the same for AttrCatalog
*/

OpenRelTable::OpenRelTable(){
	for(int i = 0; i < MAX_OPEN; i++){
		AttrCacheTable::attrCache[i] = nullptr;
		RelCacheTable::relCache[i] = nullptr;
	}

	RecBuffer relCatBlock(RELCAT_BLOCK);

	HeadInfo relCatHeadInfo;
	relCatBlock.getHeader(&relCatHeadInfo);


	for(int relId = 0; relId < relCatHeadInfo.numEntries; relId++){
		Attribute relCatRecord[RELCAT_NO_ATTRS];
		relCatBlock.getRecord(relCatRecord, relId);

		struct RelCacheEntry relCacheEntry;
		RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
		relCacheEntry.recId.block = RELCAT_BLOCK;
		relCacheEntry.recId.slot = relId;

		RelCacheTable::relCache[relId] = new RelCacheEntry;
		*(RelCacheTable::relCache[relId]) = relCacheEntry;	
	}
	


	Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

	RecBuffer attrCatBlock(ATTRCAT_BLOCK);

	HeadInfo attrCatHeadInfo;
	attrCatBlock.getHeader(&attrCatHeadInfo);


	//setting relCatAtributes to attrCache.
	AttrCacheEntry* head = new AttrCacheEntry;
	AttrCacheEntry* attrHeadcpy = head;
	int j = 0;
	for(int i = 0; i < relCatHeadInfo.numEntries; i++){
		head = new AttrCacheEntry;
		AttrCacheEntry* attrHeadcpy = head;
		Attribute relCatRecord[RELCAT_NO_ATTRS];
		relCatBlock.getRecord(relCatRecord, i);


		RelCatEntry* relCatBuf = new RelCatEntry;
		RelCacheTable::getRelCatEntry(i, relCatBuf);
		int doFor = relCatBuf->numAttrs;
		

		for(; doFor-- ; j++){
			attrCatBlock.getRecord(attrCatRecord, j);
			AttrCatEntry* temp = new AttrCatEntry;
			AttrCacheTable::recordToAttrCatEntry(attrCatRecord, temp);

			if(strcmp(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal )== 0 ){		
					if(temp == nullptr) break;
					head->attrCatEntry = *temp;
					head->recId.slot = j;
					head->recId.block = ATTRCAT_BLOCK;
					head->next = new AttrCacheEntry;
					head = head->next;	
				}
		}  
		head = NULL;

		AttrCacheTable::attrCache[i] = attrHeadcpy;
	}


}

OpenRelTable::~OpenRelTable(){

	//free Relation cache tables.
	free(RelCacheTable::relCache[RELCAT_RELID]);
	free(RelCacheTable::relCache[ATTRCAT_RELID]);

	//free all the linked list elements from attrCacheTabel.
	AttrCacheEntry* head =  AttrCacheTable::attrCache[RELCAT_RELID];
	// while(head != nullptr){
	// 	AttrCacheEntry* temp = head->next;
	// 	free(head);
	// 	head = temp;
	// }
 	head =  AttrCacheTable::attrCache[ATTRCAT_RELID];
	while(head != nullptr){
		AttrCacheEntry* temp = head->next;
		free(head);
		head = temp;
	}
}

int OpenRelTable::getRelId(char relName[ATTR_SIZE]){
	
	if(strcmp(relName, ATTRCAT_RELNAME) == 0) return ATTRCAT_RELID;
	if(strcmp(relName, RELCAT_RELNAME) == 0) return RELCAT_RELID;
	if(strcmp(relName, "Students") == 0) return 3;
	return E_RELNOTOPEN;
}