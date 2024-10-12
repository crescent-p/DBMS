#include "BlockAccess.h"

#include <cstring>
#include <stdio.h>

RecId BlockAccess::linearSearch(int relId, char *attrName, Attribute attrVal, int op){
	RecId* prevRecId = new RecId;
	RelCacheTable::getSearchIndex(relId, prevRecId);

	RecId currRecId;
	
	RelCatEntry* firstRel = new RelCatEntry;
	int ret = RelCacheTable::getRelCatEntry(relId, firstRel);
	
	if(prevRecId->block == -1 && prevRecId->slot == -1){
		
		//get corresponding relation from cache and set block to first block.	
		currRecId.block = firstRel->firstBlk;
		currRecId.slot = 0;
	}else{
		currRecId.block = prevRecId->block;
		currRecId.slot = prevRecId->slot + 1;
	}
	
	while(currRecId.block != -1){
		RecBuffer recBuffer(currRecId.block);
		
		HeadInfo headInfo = *(new HeadInfo);
		
		int res = recBuffer.getHeader(&headInfo);
		if(res != SUCCESS){
			currRecId.slot = -1;
			currRecId.block = -1;
			RelCacheTable::setSearchIndex(relId, &currRecId);
			return {-1, -1};
		}

		if(currRecId.slot >= headInfo.numSlots){
			currRecId.slot = 0;
			currRecId.block = headInfo.rblock;
			continue;
		}

		union Attribute record[headInfo.numAttrs];
		recBuffer.getRecord(record, currRecId.slot);
		
		unsigned char* slotMap = new unsigned char[headInfo.numSlots];
		recBuffer.getSlotMap(slotMap);
		

		if(slotMap[currRecId.slot] == SLOT_UNOCCUPIED){
			currRecId.slot++;
			continue;
		}


		AttrCatEntry* currAttrEntry = new AttrCatEntry;
		int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, currAttrEntry);

		if(ret != SUCCESS){
			return {-1, -1};
		}

		Attribute currAttrVal = record[currAttrEntry->offset];

		int cmpVal = compareAttrs(currAttrVal, attrVal, currAttrEntry->attrType);

		if ((
		            (op == NE && cmpVal != 0) ||    
		            (op == LT && cmpVal < 0)  ||    
		            (op == LE && cmpVal <= 0) ||    
		            (op == EQ && cmpVal == 0) ||    
		            (op == GT && cmpVal > 0)  ||    
		            (op == GE && cmpVal >= 0) )      
        ){

			RelCacheTable::setSearchIndex(relId, &currRecId);

			return currRecId;
		}
		currRecId.slot++;
	}

	currRecId.slot = -1;
	currRecId.block = -1;
	RelCacheTable::setSearchIndex(relId, &currRecId);
	return RecId{-1,-1};
}

int BlockAccess::renameRelation(char oldName[ATTR_SIZE], char newName[ATTR_SIZE]){
	
	Attribute attr;
	char* relName = new char[ATTR_SIZE];
	RecId recId; 


	strcpy(relName, RELCAT_ATTR_RELNAME);
	strcpy(attr.sVal, newName);
	RelCacheTable::resetSearchIndex(RELCAT_RELID);
	recId = BlockAccess::linearSearch(RELCAT_RELID, relName, attr, EQ);

	if(recId.slot != -1 && recId.block != -1){
		return E_RELEXIST;
	}



	strcpy(attr.sVal, oldName);
	strcpy(relName, ATTRCAT_ATTR_RELNAME);
	RelCacheTable::resetSearchIndex(RELCAT_RELID);
	recId = BlockAccess::linearSearch(RELCAT_RELID, relName, attr, EQ);

	if(recId.slot == -1 && recId.block == -1){
		return E_RELNOTEXIST;
	}


	RecBuffer recBuffer(RELCAT_BLOCK);

	HeadInfo headInfo = *(new HeadInfo);

	recBuffer.getHeader(&headInfo);

	Attribute *relRecord = new Attribute[headInfo.numAttrs];
	recBuffer.getRecord(relRecord, recId.slot);

	strcpy(relRecord[RELCAT_REL_NAME_INDEX].sVal, newName);
	recBuffer.setRecord(relRecord, recId.slot);

// //tesing if setRecord is working fine
//  	RecBuffer recBufferr(RELCAT_BLOCK);

//  	recBufferr.getHeader(&headInfo);
//  	recBufferr.getRecord(relRecord, recId.slot);
////

	
	RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

	RecBuffer attrBuffer(ATTRCAT_BLOCK);

	strcpy(attr.sVal, oldName);

	recId = BlockAccess::linearSearch(ATTRCAT_RELID, relName, attr, EQ);

	while(recId.block != -1){
		Attribute *relRecord = new Attribute[RELCAT_NO_ATTRS];
		attrBuffer.getRecord(relRecord, recId.slot);

		strcpy(relRecord[ATTRCAT_REL_NAME_INDEX].sVal, newName);
		
		attrBuffer.setRecord(relRecord, recId.slot);

		recId = BlockAccess::linearSearch(ATTRCAT_RELID, relName, attr, EQ);
	}
	
	return SUCCESS;
}


int BlockAccess::renameAttribute(char *relName, char *oldName, char *newName){

	if(strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) ==0){
		return E_NOTPERMITTED;
	}

	RelCacheTable::resetSearchIndex(RELCAT_RELID);

	Attribute attribute = *(new Attribute);
	strcpy(attribute.sVal, relName);

	char* relCatName = new char[ATTR_SIZE];
	strcpy(relCatName, RELCAT_ATTR_RELNAME);

	RecId recId = BlockAccess::linearSearch( RELCAT_RELID, relCatName, attribute, EQ);

	if(recId.slot == -1 || recId.block == -1){
		return E_RELNOTEXIST;
	}

	RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

	recId = {-1, -1};
	RecId attrToRenameRecId ={-1, -1};
	bool atleastOneFound = false;

	RecBuffer attrBlock(ATTRCAT_BLOCK);
	Attribute* record = new Attribute[ATTRCAT_NO_ATTRS];

	while(true){

		strcpy(attribute.sVal, oldName);

		char* attrCatName = new char[ATTR_SIZE];
		strcpy(attrCatName, ATTRCAT_ATTR_ATTRIBUTE_NAME);

		recId = BlockAccess::linearSearch( ATTRCAT_RELID, attrCatName, attribute, EQ);

		if(recId.slot == -1  || recId.block == -1){
			break;
		}

		
	
		attrBlock.getRecord(record, recId.slot);

		if(strcmp(record[ATTRCAT_ATTR_NAME_INDEX].sVal, oldName) == 0){
			attrToRenameRecId.slot = recId.slot;
			attrToRenameRecId.block = recId.block;
			break;
		}
	
	}
	
	if(attrToRenameRecId.slot == -1 || attrToRenameRecId.block == -1){
		return E_ATTRNOTEXIST;
	}

	attrBlock.getRecord(record, recId.slot);
	strcpy(record[ATTRCAT_ATTR_NAME_INDEX].sVal, newName);
	attrBlock.setRecord(record, recId.slot);


	return SUCCESS;
}


int BlockAccess::insert(int relId, Attribute* record){
	RelCatEntry relCatBuf= *(new RelCatEntry);
	RelCacheTable::getRelCatEntry(relId, &relCatBuf);

	int blockNum = relCatBuf.firstBlk;

	RecId recId = {-1, -1};

	int numOfSlots = relCatBuf.numSlotsPerBlk;
	int numOfAttributes = relCatBuf.numAttrs;

	int prevBlockNum = -1; //linked list prev 

	//traverse until free slot is found;
	while(blockNum != -1){
		RecBuffer recBuffer(blockNum);
		HeadInfo headInfo = *(new HeadInfo);
		recBuffer.getHeader(&headInfo);
		unsigned char* slotMap = new unsigned char[headInfo.numSlots]; 
		recBuffer.getSlotMap(slotMap);

		for(int i = 0; i < headInfo.numSlots; i++){
			if(slotMap[i] == SLOT_UNOCCUPIED){
				recId = {blockNum, i};
				break;
			}
		}
		prevBlockNum = blockNum;
		blockNum = headInfo.rblock;
	}




	if(recId.block == -1 && recId.slot == -1){
		if(relId == 0) return E_MAXRELATIONS;
		RecBuffer recBuffer;

		blockNum = recBuffer.getBlockNum();
		if(blockNum == E_DISKFULL)
			return E_DISKFULL;

		recId.slot = 0;
		recId.block = blockNum;

		//setting Header 
		HeadInfo headInfo = *(new HeadInfo);
		headInfo.blockType = REC;
		headInfo.lblock = prevBlockNum;
		headInfo.rblock = -1;
		headInfo.numAttrs = numOfAttributes;
		headInfo.numSlots = numOfSlots;
		headInfo.pblock = -1;
		headInfo.numEntries = 0;

		recBuffer.setHeader(&headInfo);

		unsigned char* slotMap = new unsigned char[numOfSlots];
		for(int i = 0; i < numOfSlots; i++){
			slotMap[i] = SLOT_UNOCCUPIED;
		}
		recBuffer.setSlotMap(slotMap);

		if(prevBlockNum != -1){
			HeadInfo headInfo = *(new HeadInfo);
			RecBuffer recBuffer(prevBlockNum);
			recBuffer.getHeader(&headInfo);
			headInfo.rblock = blockNum;
			recBuffer.setHeader(&headInfo);
		}else{
			relCatBuf.firstBlk = blockNum;
			RelCacheTable::setRelCatEntry(relId, &relCatBuf);
		}
		relCatBuf.lastBlk = blockNum;
		RelCacheTable::setRelCatEntry(relId, &relCatBuf);
	}

	RecBuffer insertBuff(recId.block);
	insertBuff.setRecord(record, recId.slot);
	unsigned char* slotMap = new unsigned char[numOfSlots];
	insertBuff.getSlotMap(slotMap);
	slotMap[recId.slot] = SLOT_OCCUPIED;
	insertBuff.setSlotMap(slotMap);

	HeadInfo headInfo = *(new HeadInfo);
	insertBuff.getHeader(&headInfo);
	headInfo.numEntries++;
	insertBuff.setHeader(&headInfo);

	relCatBuf.numRecs++;
	RelCacheTable::setRelCatEntry(relId, &relCatBuf);

	return SUCCESS;

}

int BlockAccess::search(int relId, Attribute *record, char *attrName, Attribute attrVal, int op){
	RecId recId;
	recId = BlockAccess::linearSearch(relId, attrName, attrVal, op);
	if(recId.block == -1 && recId.slot == -1){
		return E_NOTFOUND;
	}else{
		RecBuffer recBuffer = RecBuffer(recId.block);
		recBuffer.getRecord(record, recId.slot);
		return SUCCESS;
	}

}

int BlockAccess::deleteRelation(char *relName){
	if((strcmp(relName, RELCAT_RELNAME) == 0 )|| (strcmp(relName, ATTRCAT_RELNAME) == 0)){
		return E_NOTPERMITTED;
	}
	RecId recId;
	Attribute* attribute = new Attribute;
	strcpy(attribute->sVal, relName);
	char* relCatAttrRelName;
	strcpy(relCatAttrRelName, RELCAT_ATTR_RELNAME);
	recId = BlockAccess::linearSearch(RELCAT_RELID, relCatAttrRelName, *attribute, EQ);

	if(recId.slot == -1 && recId.block == -1){
		return E_RELNOTEXIST;
	}

	RecBuffer recBuffer = RecBuffer(recId.block);
	Attribute* relCatEntryRecord = new Attribute[RELCAT_NO_ATTRS];
	recBuffer.getRecord(relCatEntryRecord, recId.slot);
	int firstBlock = relCatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;
	int numOfAttrs = relCatEntryRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
	
	int nextBlock = firstBlock;

	while(nextBlock != -1){
		recBuffer = RecBuffer(nextBlock);
		HeadInfo* header = new HeadInfo;
		recBuffer.getHeader(header);

		recBuffer.releaseBlock();
		nextBlock = header->rblock;
	}
	RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

	int numberOfAttributesDeleted = 0;

	while(true){
		RecId attrCatRecId;
		char* attrCatRelName;
		strcpy(attrCatRelName, ATTRCAT_ATTR_RELNAME);
		attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, attrCatRelName, *attribute, EQ);
		if(attrCatRecId.block == -1 && attrCatRecId.slot == -1){
			break;
		}

		numberOfAttributesDeleted++;

		recBuffer = RecBuffer(attrCatRecId.block);
		HeadInfo* headInfo = new HeadInfo;
		Attribute* attribute = new Attribute[ATTRCAT_NO_ATTRS];
		recBuffer.getRecord( attribute, attrCatRecId.slot);
		recBuffer.getHeader(headInfo);

		int rootBlock = attribute[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
		unsigned char* slotMap = new unsigned char[headInfo->numSlots];
		recBuffer.getSlotMap(slotMap);
		slotMap[recId.slot] = SLOT_UNOCCUPIED;
		recBuffer.setSlotMap(slotMap);
		headInfo->numEntries--;

		if(headInfo->numEntries == 0){
			if(headInfo->lblock != -1){
				RecBuffer leftBuffer = RecBuffer(headInfo->lblock);
				HeadInfo* leftHeadInfo = new HeadInfo;
				leftBuffer.getHeader(leftHeadInfo);
				leftHeadInfo->rblock = headInfo->rblock;
				leftBuffer.setHeader(leftHeadInfo);
			}
			if(headInfo->rblock != -1){
				RecBuffer rightBuffer = RecBuffer(headInfo->rblock);
				HeadInfo* rightHeadInfo = new HeadInfo;
				rightBuffer.getHeader(rightHeadInfo);
				rightHeadInfo->lblock = headInfo->lblock;
				rightBuffer.setHeader(rightHeadInfo);
			}
			recBuffer.releaseBlock();
		}
		if(rootBlock != -1){
			//BPlusTree::bPlusDestroy(rootBlock);
		}
	}
	 /*** Delete the entry corresponding to the relation from relation catalog ***/
	HeadInfo* relCatHeadInfo = new HeadInfo;
	RecBuffer relCatBuffer = RecBuffer(RELCAT_BLOCK);
	relCatBuffer.getHeader(relCatHeadInfo);
	relCatHeadInfo->numEntries--;
	relCatBuffer.setHeader(relCatHeadInfo);
	unsigned char* slotMap = new unsigned char[relCatHeadInfo->numSlots];

	relCatBuffer.getSlotMap(slotMap);
	slotMap[recId.slot] = SLOT_UNOCCUPIED;
	relCatBuffer.setSlotMap(slotMap);

	/*** Updating the Relation Cache Table ***/
	RelCatEntry* relCatEntry = new RelCatEntry;
	RelCacheTable::getRelCatEntry(RELCAT_RELID, relCatEntry);
	relCatEntry->numRecs--;
	RelCacheTable::setRelCatEntry(RELCAT_RELID, relCatEntry);
	
	RelCatEntry* attrCatEntry = new RelCatEntry;
	RelCacheTable::getRelCatEntry(ATTRCAT_RELID, attrCatEntry);
	attrCatEntry->numAttrs -= numberOfAttributesDeleted;
	RelCacheTable::setRelCatEntry(ATTRCAT_RELID, attrCatEntry);

	return SUCCESS; 
}