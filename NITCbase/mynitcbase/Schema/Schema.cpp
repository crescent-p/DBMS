#include "Schema.h"

#include <cmath>
#include <cstring>


int Schema::openRel(char relName[ATTR_SIZE]){
	return OpenRelTable::openRel(relName);
}

int Schema::closeRel(char relName[ATTR_SIZE]){
	if(relName == RELCAT_RELNAME || relName == ATTRCAT_RELNAME){
		return E_NOTPERMITTED;
	}
	int relId = OpenRelTable::getRelId(relName);

	if(relId == E_RELNOTOPEN){
		return E_RELNOTOPEN;
	}

	return OpenRelTable::closeRel(relId);
}

int Schema::renameRel(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE]){


	if(oldRelName == RELCAT_RELNAME || oldRelName == ATTRCAT_RELNAME){
		return E_NOTPERMITTED;
	}

    int ret = OpenRelTable::getRelId(oldRelName);
    if(ret != E_RELNOTOPEN){
    	return E_RELOPEN;
    }

    // ret = RelCacheTable::findSlotOfRelation(newRelName);
    // if(ret != E_RELNOTEXIST){
    // 	return E_RELEXIST;
    // }

    int retVal = BlockAccess::renameRelation(oldRelName, newRelName);

	return retVal;
}

int Schema::renameAttr(char relName[ATTR_SIZE], char oldAttrName[ATTR_SIZE], char newAttrName[ATTR_SIZE]){


	if(relName == RELCAT_RELNAME || relName == ATTRCAT_RELNAME){
		return E_NOTPERMITTED;
	}

    int ret = OpenRelTable::getRelId(relName);
    if(ret != E_RELNOTOPEN){
    	return E_RELOPEN;
    }

    int retVal = BlockAccess::renameAttribute(relName, oldAttrName, newAttrName);


	return retVal;
}