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
