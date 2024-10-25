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

	
	for(int i = 2; i < MAX_OPEN; i++){
		if(tableMetaInfo[i].free == false){
			OpenRelTable::closeRel(i);
		}
	}
	// * Closing the catalog relations in the relation cache
    // TODO: release the relation cache entry of the attribute catalog
    // if RelCatEntry of the ATTRCAT_RELID-th RelCacheEntry has been modified 
	if (RelCacheTable::relCache[ATTRCAT_RELID]->dirty)
	{
        /* Get the Relation Catalog entry from RelCacheTable::relCache
        Then convert it to a record using RelCacheTable::relCatEntryToRecord(). */
		RelCatEntry relCatBuffer;
		RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatBuffer);

		Attribute relCatRecord [RELCAT_NO_ATTRS];
		RelCacheTable::relCatEntryToRecord(&relCatBuffer, relCatRecord);

		RecId recId = RelCacheTable::relCache[ATTRCAT_RELID]->recId;

        // declaring an object of RecBuffer class to write back to the buffer
        RecBuffer relCatBlock(recId.block);

        // Write back to the buffer using relCatBlock.setRecord() with recId.slot
		relCatBlock.setRecord(relCatRecord, recId.slot);
    }
    // free the memory dynamically allocated to this RelCacheEntry
	free(RelCacheTable::relCache[ATTRCAT_RELID]);

    // TODO: release the relation cache entry of the relation catalog
    // if RelCatEntry of the RELCAT_RELID-th RelCacheEntry has been modified
	if (RelCacheTable::relCache[RELCAT_RELID]->dirty)
	{
        /* Get the Relation Catalog entry from RelCacheTable::relCache
        Then convert it to a record using RelCacheTable::relCatEntryToRecord(). */
		RelCatEntry relCatBuffer;
		RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatBuffer);

		Attribute relCatRecord [RELCAT_NO_ATTRS];
		RelCacheTable::relCatEntryToRecord(&relCatBuffer, relCatRecord);

		RecId recId = RelCacheTable::relCache[RELCAT_RELID]->recId;

        // declaring an object of RecBuffer class to write back to the buffer
        RecBuffer relCatBlock(recId.block);

        // Write back to the buffer using relCatBlock.setRecord() with recId.slot
		relCatBlock.setRecord(relCatRecord, recId.slot);
    }
    // free the memory dynamically allocated for this RelCacheEntry
	free(RelCacheTable::relCache[RELCAT_RELID]);

    // free the memory allocated for the attribute cache entries of the
    // relation catalog and the attribute catalog
	// for (int relId = ATTRCAT_RELID; relId >= RELCAT_RELID; relId--)
	// {
	// 	AttrCacheEntry *curr = AttrCacheTable::attrCache[relId], *next = nullptr;
	// 	for (int attrIndex = 0; attrIndex < 6; attrIndex++)
	// 	{
	// 		next = curr->next;

	// 		// check if the AttrCatEntry was written back
	// 		if (curr->dirty)
	// 		{
	// 			AttrCatEntry attrCatBuffer;
	// 			AttrCacheTable::getAttrCatEntry(relId, attrIndex, &attrCatBuffer);

	// 			Attribute attrCatRecord [ATTRCAT_NO_ATTRS];
	// 			AttrCacheTable::attrCatEntryToRecord(&attrCatBuffer, attrCatRecord);

	// 			RecId recId = curr->recId;

	// 			// declaring an object if RecBuffer class to write back to the buffer
	// 			RecBuffer attrCatBlock (recId.block);

	// 			// write back to the buffer using RecBufer.setRecord()
	// 			attrCatBlock.setRecord(attrCatRecord, recId.slot);
	// 		}

	// 		free (curr);
	// 		curr = next;
	// 	}
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
	
	int relId = OpenRelTable::getRelId(relName);
	if(relId > 0){
		return relId;
	}

	int freeSlot = OpenRelTable::getFreeOpenRelTableEntry();

	if(freeSlot == E_CACHEFULL){
		return E_CACHEFULL;
	}


	RelCacheTable::resetSearchIndex(RELCAT_RELID);
	Attribute relAttribute ;
	strcpy(relAttribute.sVal, relName);
	// char* relCatRelName = RELCAT_RELNAME;
	// char* attrCatRelName = ATTRCAT_RELNAME;

	RecId recId = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, relAttribute, EQ);

	if(recId.slot == -1 || recId.block == -1){
		return E_RELNOTEXIST;
	}

	RecBuffer relBuffer(recId.block);
	Attribute relationRecord[RELCAT_NO_ATTRS];
	RelCacheEntry *relCacheBuffer = new RelCacheEntry;

	relBuffer.getRecord(relationRecord, recId.slot);

	RelCacheTable::recordToRelCatEntry(relationRecord, &(relCacheBuffer->relCatEntry));
	relCacheBuffer->recId.block = recId.block;
	relCacheBuffer->recId.slot = recId.slot;
	
	RelCacheTable::relCache[relId] = relCacheBuffer;

	Attribute record[ATTRCAT_NO_ATTRS];
	AttrCacheEntry* attrCacheEntry = (new AttrCacheEntry); 
	
	AttrCacheEntry* head = attrCacheEntry;
	RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
	RecId attrCatRecId = {-1, -1};
	attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, RELCAT_ATTR_RELNAME, relAttribute, EQ);
		
	while(attrCatRecId.block != -1 && attrCatRecId.slot != -1){
		RecBuffer attrBuffer(attrCatRecId.block);
		attrBuffer.getRecord(record, attrCatRecId.slot);
		AttrCatEntry* attrCatEntry = new AttrCatEntry;
		if(strcmp(record[ATTRCAT_REL_NAME_INDEX].sVal, relName) == 0){
			AttrCacheTable::recordToAttrCatEntry(record, attrCatEntry);
			head->attrCatEntry = *attrCatEntry;
			head->dirty = false;
			head->recId = {recId.block, recId.slot};
			head->searchIndex = {-1, -1};
			head->next = new AttrCacheEntry;
			head = head->next;
		}
		attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, RELCAT_ATTR_RELNAME, relAttribute, EQ);
		
	}

	if(attrCacheEntry != head){
		head = NULL;
		AttrCacheTable::attrCache[freeSlot] = attrCacheEntry;
	}else{
		return E_RELNOTEXIST;
	}

	OpenRelTable::tableMetaInfo[freeSlot].free = false;
	strcpy(tableMetaInfo[freeSlot].relName, relName);

	return SUCCESS;
}

int OpenRelTable::closeRel(int relId){

	if(relId <= 1){ //attr and cat entry
		return E_NOTPERMITTED;
	}

	if(relId < 0 || relId >= MAX_OPEN){
		return E_OUTOFBOUND;
	}

	if(RelCacheTable::relCache[relId]->dirty == true){
		RelCatEntry relCatEntry = *(new RelCatEntry);

		RelCacheTable::getRelCatEntry(relId, &relCatEntry);

		Attribute* record = new Attribute[RELCAT_NO_ATTRS];
		RelCacheTable::relCatEntryToRecord(&relCatEntry, record);

		RecId recId = RelCacheTable::relCache[relId]->recId;

		RecBuffer relCatBuf(recId.block);
		int ret = relCatBuf.setRecord(record, recId.slot);
		
		if(ret != SUCCESS){
			return ret;
		}	 
	}

	tableMetaInfo[relId].free = true;

	free(AttrCacheTable::attrCache[relId]);
	free(RelCacheTable::relCache[relId]);

	AttrCacheTable::attrCache[relId] = nullptr;
	RelCacheTable::relCache[relId] = nullptr;

	return SUCCESS;
}