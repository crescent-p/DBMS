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
		OpenRelTable::tableMetaInfo[i].free = true;
	}

	RecBuffer relCatBlock(RELCAT_BLOCK);

	HeadInfo relCatHeadInfo;
	relCatBlock.getHeader(&relCatHeadInfo);


	for(int relId = 0; relId < 2 /*for relCat and attrCat*/; relId++){
		Attribute relCatRecord[RELCAT_NO_ATTRS];
		int ret = relCatBlock.getRecord(relCatRecord, relId);
		if(ret != SUCCESS){
			continue;
		}

		struct RelCacheEntry relCacheEntry;
		RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
		relCacheEntry.recId.block = relId + RELCAT_BLOCK;
		relCacheEntry.recId.slot = relId;

		RelCacheTable::relCache[relId] = new RelCacheEntry;
		*(RelCacheTable::relCache[relId]) = relCacheEntry;	
		OpenRelTable::tableMetaInfo[relId].free = false;
		strcpy(OpenRelTable::tableMetaInfo[relId].relName, relCatRecord[RELCAT_REL_NAME_INDEX].sVal);
	}
	


	Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

	RecBuffer attrCatBlock(ATTRCAT_BLOCK);

	HeadInfo attrCatHeadInfo;
	attrCatBlock.getHeader(&attrCatHeadInfo);

	// for(int i = 0; i < relCatHeadInfo.numEntries; i++){
	// 	RelCatEntry relCatEntry = *(new RelCatEntry);
		
	// 	int res = RelCacheTable::getRelCatEntry(i, &relCatEntry);
	// 	if(res != SUCCESS){
	// 		return;
	// 	}
	// 	RecBuffer mybuffer(relCatEntry.firstBlk);

	// 	HeadInfo headInfo;
	// 	mybuffer.getHeader(&headInfo);

	// 	int numOfEntries = headInfo.numEntries;

	// 	AttrCacheEntry* head = new AttrCacheEntry;
	// 	AttrCacheEntry* headCpy = head;

	// 	for(int j = 0; j < numOfEntries; j++){
	// 		Attribute* nextAttribute= new Attribute[headInfo.numAttrs]; 
	// 		int res = mybuffer.getRecord(nextAttribute, j);
	// 		if(res != SUCCESS){
	// 			return;
	// 		}
	// 		AttrCatEntry* attrCatEntry = new AttrCatEntry;
	// 		AttrCacheTable::recordToAttrCatEntry(nextAttribute, attrCatEntry);

	// 		head->recId.slot = j;
	// 		head->recId.block = i;
	// 		head->next = new AttrCacheEntry;
	// 		head = head->next;

	// 	}
	// 	head = NULL;

	// 	AttrCacheTable::attrCache[i] = headCpy;
	// }


	//setting relCatAtributes to attrCache.
	AttrCacheEntry* head = new AttrCacheEntry;
	AttrCacheEntry* attrHeadcpy = head;
	int j = 0;
	for(int i = 0; i < 2/*for relcat and attrcat*/; i++){
		head = new AttrCacheEntry;
		AttrCacheEntry* attrHeadcpy = head;
		Attribute relCatRecord[RELCAT_NO_ATTRS];
		int ret = relCatBlock.getRecord(relCatRecord, i);	
		
		if(ret != SUCCESS){
			continue;
		}


		RelCatEntry* relCatBuf = new RelCatEntry;
		RelCacheTable::getRelCatEntry(i, relCatBuf);
		int doFor = relCatBuf->numAttrs;
		

		for(; doFor-- ; j++){
			attrCatBlock.getRecord(attrCatRecord, j);
			AttrCatEntry* temp = new AttrCatEntry;
			AttrCacheTable::recordToAttrCatEntry(attrCatRecord, temp);

			if(strcmp(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal )== 0 || 1){		
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
	// AttrCacheEntry* head =  AttrCacheTable::attrCache[RELCAT_RELID];
	// while(head != nullptr){
	// 	AttrCacheEntry* temp = head->next;
	// 	free(head);
	// 	head = temp;
	// }
 	// head =  AttrCacheTable::attrCache[ATTRCAT_RELID];
	// while(head != nullptr){
	// 	AttrCacheEntry* temp = head->next;
	// 	free(head);
	// 	head = temp;
	// }
}
int OpenRelTable::getFreeOpenRelTableEntry(){
	for(int i = 0; i < MAX_OPEN; i++){
		if(tableMetaInfo[i].free == true){
			return i;
		}
	}
	return E_CACHEFULL;
}

int OpenRelTable::getRelId(char relName[ATTR_SIZE]){

	for(int i = 0; i < MAX_OPEN; i++){
		if(OpenRelTable::tableMetaInfo[i].free == false){
			if(strcmp(tableMetaInfo[i].relName, relName) == 0){
				return i;
			}
		}
	}

	return E_RELNOTOPEN;
}

int OpenRelTable::openRel(char relName[ATTR_SIZE]){
	
	int ret = OpenRelTable::getRelId(relName);
	if(ret != E_RELNOTOPEN){
		return E_RELOPEN;
	}

	int freeSlot = OpenRelTable::getFreeOpenRelTableEntry();

	if(freeSlot == E_CACHEFULL){
		return E_CACHEFULL;
	}


	OpenRelTable::tableMetaInfo[freeSlot].free = false;
	strcpy(tableMetaInfo[freeSlot].relName, relName);


	RelCacheTable::resetSearchIndex(RELCAT_RELID);
	Attribute relAttribute = *(new Attribute);
	strcpy((char*)relAttribute.sVal, (char*)relName);
	// char* relCatRelName = RELCAT_RELNAME;
	// char* attrCatRelName = ATTRCAT_RELNAME;

	// RecId recId = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_RELNAME, relAttribute, EQ);

	RecBuffer relBlock(RELCAT_BLOCK);

	HeadInfo headInfo;
	relBlock.getHeader(&headInfo);

	RelCacheEntry* relCacheEntry = new RelCacheEntry;
	RelCatEntry* relCatEntry = new RelCatEntry;

	int slotNum = 0;
	Attribute *record= new Attribute[ATTR_SIZE];

	//shoudl use slotMap and blockNum != -1 with nextBlock = rBlock for handling large number of relations.
	for(; slotNum < headInfo.numEntries; slotNum++){
		relBlock.getRecord(record, slotNum);

		if(strcmp(record[RELCAT_REL_NAME_INDEX].sVal, relName) == 0){
			break;
		}

	}
	if(slotNum == headInfo.numEntries){
		return E_RELNOTEXIST;
	}

	RelCacheTable::recordToRelCatEntry(record, relCatEntry);
	relCacheEntry->relCatEntry = *relCatEntry;
	relCacheEntry->dirty = false;
	relCacheEntry->recId = {RELCAT_BLOCK, slotNum};
	relCacheEntry->searchIndex = {-1, -1};

	RelCacheTable::relCache[freeSlot] = relCacheEntry;


	AttrCatEntry* attrCatEntry = new AttrCatEntry;
	AttrCacheEntry* attrCacheEntry = (new AttrCacheEntry); 
	AttrCacheEntry* head = attrCacheEntry;

	int recordNum = 0;

	RecBuffer attrBlock(ATTRCAT_BLOCK);

	record = new Attribute[ATTR_SIZE];
	ret = attrBlock.getRecord(record, recordNum);

	while(ret == SUCCESS){
		if(strcmp(record[ATTRCAT_ATTR_NAME_INDEX].sVal, relName) == 0){
			AttrCacheTable::recordToAttrCatEntry(record, attrCatEntry);
			head->attrCatEntry = *attrCatEntry;
			head->dirty = false;
			head->recId = {ATTRCAT_BLOCK, recordNum};
			head->searchIndex = {-1, -1};
			head->next = new AttrCacheEntry;
			head = head->next;
		}
		recordNum++;
		ret = attrBlock.getRecord(record, recordNum);
	}
	head = NULL;

	if(attrCacheEntry != head){
		AttrCacheTable::attrCache[freeSlot] = attrCacheEntry;
	}


	return SUCCESS;

	// if(recId.slot == -1 || recId.block == -1){
	// 	return E_ATTRNOTEXIST;
	// }

	// Attribute* record = new Attribute[ATTR_SIZE];

	// /*
	// 	get slot number of relation using linear search
	// 	get the corresponding record
	// 	convert to relCacheEntry and add it to cache.

	// */

	// RecBuffer relBufferBlock(recId.block);
	// relBufferBlock.getRecord(record, recId.slot);

	// RelCatEntry* relCatEntry = new RelCatEntry;
	// RelCacheTable::recordToRelCatEntry(record, relCatEntry);

	// RelCacheEntry* relCacheEntry = new RelCacheEntry;
	// relCacheEntry->relCatEntry = *relCatEntry;
	// relCacheEntry->recId = recId;
	// relCacheEntry->searchIndex = {-1, -1};

	// RelCacheTable::relCache[freeSlot] = relCacheEntry;

	// RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
	// AttrCacheEntry* attrCacheEntry = new AttrCacheEntry;
	// AttrCacheEntry* head = attrCacheEntry;

	// recId = BlockAccess::linearSearch(ATTRCAT_RELID, relCatRelName, relAttribute, EQ);

	// RecBuffer attrBufferBlock(recId.block);

	// while(recId.block != -1 && recId.slot != -1){
	// 	AttrCatEntry attrCatEntry = *(new AttrCatEntry);
	// 	Attribute* record = new Attribute[ATTRCAT_NO_ATTRS];
		
	// 	AttrCacheTable::recordToAttrCatEntry(record, &attrCatEntry);

	// 	head->attrCatEntry = attrCatEntry;
	// 	head->recId = recId;
	// 	head->searchIndex = {-1, -1};
	// 	head->next = new AttrCacheEntry;

	// 	head = head->next;

	// 	recId = BlockAccess::linearSearch(ATTRCAT_RELID, relCatRelName, relAttribute, EQ);
	// }
	// head->next = NULL;

	// AttrCacheTable::attrCache[freeSlot] = attrCacheEntry;
	// strcpy(OpenRelTable::tableMetaInfo[freeSlot].relName, relName);
	// OpenRelTable::tableMetaInfo[freeSlot].free = false;

	// return SUCCESS;

}

int OpenRelTable::closeRel(int relId){

	if(relId <= 1){ //attr and cat entry
		return E_NOTPERMITTED;
	}

	if(relId < 0 || relId >= MAX_OPEN){
		return E_OUTOFBOUND;
	}

	tableMetaInfo[relId].free = true;

	free(AttrCacheTable::attrCache[relId]);
	free(RelCacheTable::relCache[relId]);

	AttrCacheTable::attrCache[relId] = nullptr;
	RelCacheTable::relCache[relId] = nullptr;

	return SUCCESS;
}