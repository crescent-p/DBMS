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
		
		HeadInfo headInfo;
		
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
// 	RecBuffer recBufferr(RELCAT_BLOCK);

// 	recBufferr.getHeader(&headInfo);
// 	recBufferr.getRecord(relRecord, recId.slot);
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

	RelCacheTable::resetSearchIndex(RELCAT_BLOCK);

	Attribute attribute = *(new Attribute);
	strcpy(attribute.sVal, relName);

	char* relCatName = new char[ATTR_SIZE];
	strcpy(relCatName, RELCAT_ATTR_RELNAME);

	RecId recId = BlockAccess::linearSearch( RELCAT_RELID, relCatName, attribute, EQ);

	if(recId.slot == -1 || recId.block == -1){
		return E_RELNOTEXIST;
	}

	RelCacheTable::resetSearchIndex(ATTRCAT_BLOCK);

	recId = {-1, -1};
	RecId attrToRenameRecId ={-1, -1};
	bool atleastOneFound = false;

	RecBuffer attrBlock(ATTRCAT_BLOCK);
	Attribute* record = new Attribute[ATTRCAT_NO_ATTRS];

	while(true){

		strcpy(attribute.sVal, oldName);

		char* attrCatName = new char[ATTR_SIZE];
		strcpy(attrCatName, ATTRCAT_ATTR_RELNAME);

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








// RecId BlockAccess::linearSearch(int relId, char *attrName, Attribute attrVal, int op){

// 	RecId* prevRecId = new RecId;
// 	RelCacheTable::getSearchIndex(relId, prevRecId);

// 	RecId currRecId;
// 	if(prevRecId->block == -1 && prevRecId->slot == -1){
		
// 		//get corresponding relation from cache and set block to first block.
// 		RelCatEntry* firstRel = new RelCatEntry;
// 		int ret = RelCacheTable::getRelCatEntry(relId,firstRel);
// 		if(ret != SUCCESS){
// 			return {-1, -1};
// 		}
		
// 		currRecId.block = firstRel->firstBlk;
// 		currRecId.slot = 0;
// 	}else{
// 		currRecId.block = prevRecId->block;
// 		currRecId.slot = prevRecId->slot + 1;
// 	}

// 	int blockNum = currRecId.block;
// 	int slotNum = currRecId.slot;

// 	RecBuffer blockBuffer(blockNum);

// 	HeadInfo headInfo;
// 	blockBuffer.getHeader(&headInfo);

// 	AttrCatEntry* attrCatEntry = new AttrCatEntry;

// 	int res =  AttrCacheTable::getAttrCatEntry(relId, attrName, attrCatEntry);
// 	if(res != SUCCESS){
// 		return {-1, -1};
// 	}

// 	int offset = attrCatEntry->offset;

// 	while(blockNum != -1 && slotNum != -1){
// 		Attribute attributes[headInfo.numAttrs];
// 		int ret = blockBuffer.getRecord(attributes, slotNum);
// 		if(ret != SUCCESS){
// 			return {-1, -1};
// 		}

// 		Attribute currAttrVal = attributes[offset];

// 		int cmpVal = compareAttrs(currAttrVal, attrVal, currAttrEntry->attrType);

// 		if ((
// 		            (op == NE && cmpVal != 0) ||    
// 		            (op == LT && cmpVal < 0)  ||    
// 		            (op == LE && cmpVal <= 0) ||    
// 		            (op == EQ && cmpVal == 0) ||    
// 		            (op == GT && cmpVal > 0)  ||    
// 		            (op == GE && cmpVal >= 0) )      
//         ){

//         	if(slotMap[currRecId.slot] == SLOT_UNOCCUPIED){
// 				currRecId.slot++;
// 				continue;
// 			}


// 			RelCacheTable::setSearchIndex(relId, &currRecId);

// 			return currRecId;
// 		}
// 		currRecId.slot++;
// 	}

// 	currRecId.slot = -1;
// 	currRecId.block = -1;
// 	RelCacheTable::setSearchIndex(relId, &currRecId);
// 	return RecId{-1,-1};
// 	}


// 	return {-1, -1};
// }
