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


	if(strcmp(newRelName, RELCAT_RELNAME) == 0 || strcmp(newRelName, ATTRCAT_RELNAME) ==0){
		return E_NOTPERMITTED;
	}
	if(strcmp(oldRelName, RELCAT_RELNAME) == 0 || strcmp(oldRelName, ATTRCAT_RELNAME) ==0){
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

int Schema::createRel(char relName[], int numOfAttributes, char attrNames[][ATTR_SIZE], int attrType[]){
	Attribute attrVal;
	strcpy(attrVal.sVal, relName);
	
	RecId targetId = {-1, -1};

	RelCacheTable::resetSearchIndex(RELCAT_RELID);
	char* name = new char[ATTR_SIZE];
	strcpy(name, RELCAT_ATTR_RELNAME);
	targetId = BlockAccess::linearSearch(RELCAT_RELID, name, attrVal, EQ);

	if(targetId.slot != -1 && targetId.block != -1) return E_RELEXIST;

	for(int i = 0; i < numOfAttributes; i++){
		for(int j = i + 1; j < numOfAttributes; j++){
			if(strcmp(attrNames[i], attrNames[j]) == 0) return E_DUPLICATEATTR;
		}
	}

	Attribute* relRecord = new Attribute[RELCAT_NO_ATTRS];


	strcpy(relRecord[RELCAT_REL_NAME_INDEX].sVal , relName);
	relRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal = numOfAttributes;
	relRecord[RELCAT_NO_RECORDS_INDEX].nVal = 0;
	relRecord[RELCAT_FIRST_BLOCK_INDEX].nVal = -1;
	relRecord[RELCAT_LAST_BLOCK_INDEX].nVal = -1;
	relRecord[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal = (BLOCK_SIZE - HEADER_SIZE) / (16*numOfAttributes + 1);
	
	int retVal = BlockAccess::insert(RELCAT_RELID, relRecord);

	if(retVal != SUCCESS) return retVal;

	for(int i = 0; i < numOfAttributes; i++){
		Attribute* attrRecord = new Attribute[ATTRCAT_NO_ATTRS];

		strcpy(attrRecord[ATTRCAT_REL_NAME_INDEX].sVal, relName);
		strcpy(attrRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrNames[i]);
		attrRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal = attrType[i];
		attrRecord[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = -1;
		attrRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal = -1;
		attrRecord[ATTRCAT_OFFSET_INDEX].nVal = i;

		int ret = BlockAccess::insert(ATTRCAT_RELID, attrRecord);

		if(ret != SUCCESS){
			Schema::deleteRel(relName);
			return ret;
		}
	} 
	return SUCCESS;

}

int Schema::deleteRel(char relName[ATTR_SIZE]){
	if(strcmp(relName, RELCAT_RELNAME) || strcmp(relName, ATTRCAT_RELNAME)){
		return E_NOTPERMITTED;
	}
	
	int relId = OpenRelTable::getRelId(relName);

	if(relId != E_RELNOTOPEN) return E_RELOPEN;

	int retVal = BlockAccess::deleteRelation(relName);

	return retVal;

}
