#include "BlockAccess.h"

#include <cstring>

RecId BlockAccess::linearSearch(int relId, char *attrName, Attribute attrVal, int op){
	RecId* prevRecId = new RecId;
	RelCacheTable::getSearchIndex(relId, prevRecId);

	RecId currRecId;
	if(prevRecId->block == -1 && prevRecId->slot == -1){
		currRecId.block = ATTRCAT_BLOCK;
		currRecId.slot = 0;
	}else{
		currRecId.block = prevRecId->block;
		currRecId.slot = prevRecId->slot + 1;
	}
	
	while(currRecId.block != -1){
		RecBuffer recBuffer(currRecId.block);
		union Attribute record[ATTRCAT_NO_ATTRS];
		recBuffer.getRecord(record, currRecId.slot);
		HeadInfo headInfo;
		recBuffer.getHeader(&headInfo);
		unsigned char* slotMap;
		recBuffer.getSlotMap(slotMap);

		if(currRecId.slot >= headInfo.numSlots){
			currRecId.slot = 0;
			currRecId.block = headInfo.rblock;
			continue;
		}
		//might end in infintite loop if attrname is not found.
		if(slotMap[currRecId.slot] == SLOT_UNOCCUPIED){
			currRecId.slot++;
			if(currRecId.slot >= headInfo.numSlots){
				currRecId.slot = -1;
				currRecId.block = -1;
				RelCacheTable::setSearchIndex(relId, &currRecId);
				return RecId{-1,-1};
			}
			continue;
		}
		AttrCatEntry* currAttrEntry = new AttrCatEntry;
		AttrCacheTable::getAttrCatEntry(relId, attrName, currAttrEntry);

		Attribute currAttrVal = record[currAttrEntry->offset];

		int cmpVal = compareAttrs(currAttrVal, attrVal, currAttrEntry->attrType);

		if (
            (op == NE && cmpVal != 0) ||    // if op is "not equal to"
            (op == LT && cmpVal < 0) ||     // if op is "less than"
            (op == LE && cmpVal <= 0) ||    // if op is "less than or equal to"
            (op == EQ && cmpVal == 0) ||    // if op is "equal to"
            (op == GT && cmpVal > 0) ||     // if op is "greater than"
            (op == GE && cmpVal >= 0)       // if op is "greater than or equal to"
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