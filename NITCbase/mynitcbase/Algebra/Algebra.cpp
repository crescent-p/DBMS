#include "Algebra.h"

#include <cstring>
#include <stdio.h>
#include <string>
// #include <iostream>


bool isNumber(char* str);

int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], char attr[ATTR_SIZE], int op, char strVal[ATTR_SIZE]){
	int srcRelId = OpenRelTable::getRelId(srcRel);
	if(srcRelId == E_RELNOTOPEN){
		return E_RELNOTOPEN;
	}

	AttrCatEntry attrCatEntry = *(new AttrCatEntry);
	int res = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);
	if(res != SUCCESS){
		return E_ATTRNOTEXIST;
	}

	int type = attrCatEntry.attrType;
	Attribute attrVal;
	if(type == NUMBER){
		if(isNumber(strVal)){
			std::string temp = strVal;
			attrVal.nVal = stof(temp);
		}else{
			return E_ATTRTYPEMISMATCH;
		}
	}else if(type == STRING){
		strcpy(attrVal.sVal, strVal);
	}

	RelCacheTable::resetSearchIndex(srcRelId);
	
	RelCatEntry relCatEntry = *(new RelCatEntry);
	RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

	int src_nAttrs = relCatEntry.numAttrs;
	//(will store the attribute names of rel)
	char attrNames[src_nAttrs][ATTR_SIZE];
	int attr_types[src_nAttrs];

	for(int offset = 0; offset < src_nAttrs; offset++){
		AttrCatEntry* attrCatEntry = new AttrCatEntry;
		AttrCacheTable::getAttrCatEntry(srcRelId, offset, attrCatEntry);
		strcpy(attrNames[offset], attrCatEntry->attrName);
		attr_types[offset] = attrCatEntry->attrType;
	}
	
	res = Schema::createRel(targetRel, src_nAttrs, attrNames, attr_types);
	
	if(res != SUCCESS){
		return res;
	}

	res = OpenRelTable::openRel(targetRel);
	if(res != SUCCESS){
		Schema::deleteRel(targetRel);
		return res;
	}

	int targetRelId = OpenRelTable::getRelId(targetRel);

	// inserting into target relation
	RelCacheTable::resetSearchIndex(srcRelId);

	Attribute* record = new Attribute[src_nAttrs];

	RelCacheTable::resetSearchIndex(srcRelId);
	//TODO:: after implementing B-Tree
	//AttrCacheTable::resetSearchIndex(srcRelId, attr);

	while(BlockAccess::search(srcRelId, record, attr, attrVal, op) == SUCCESS){
		int ret = BlockAccess::insert(targetRelId, record);
		if(ret != SUCCESS){
			Schema::closeRel(targetRel);
			Schema::deleteRel(targetRel);
			return ret;
		}
	}

	Schema::closeRel(targetRel);
	return SUCCESS;	
}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE]){
	int srcRelId = OpenRelTable::getRelId(srcRel); 	
	if(srcRelId == E_RELNOTOPEN){
		return srcRelId;
	}
	RelCatEntry* relCatEntry = new RelCatEntry;
	RelCacheTable::getRelCatEntry(srcRelId, relCatEntry);

	int srcNumberOfAttrs = relCatEntry->numAttrs;

	char attrNames[srcNumberOfAttrs][ATTR_SIZE];
	char attrType[srcNumberOfAttrs];

	for(int offset = 0; offset < srcNumberOfAttrs; offset++){
		AttrCatEntry* attrCatEntry = new AttrCatEntry;
		AttrCacheTable::getAttrCatEntry( srcRelId, offset, attrCatEntry);
		strcpy(attrNames[offset], attrCatEntry->attrName);
		attrType[offset] = attrCatEntry->offset;
	}

	 /*** Creating and opening the target relation ***/
	Schema::openRel(targetRel);
	int res = OpenRelTable::openRel(targetRel);
	if(res != SUCCESS){
		Schema::deleteRel(targetRel);
		return res;
	} 
	int targetRelId = OpenRelTable::getRelId(targetRel);

	RelCacheTable::resetSearchIndex(srcRelId);
	Attribute* record = new Attribute[srcNumberOfAttrs];
	while(BlockAccess::project(srcRelId, record) == SUCCESS){
		
		int ret = BlockAccess::insert(targetRelId, record);
		
		if(ret != SUCCESS){
			Schema::deleteRel(targetRel);
			return ret;
		}
	}
	Schema::closeRel(targetRel);
	
	return SUCCESS;
}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], int tar_nAttrs, char tar_Attrs[][ATTR_SIZE]){
	int srcRelId = OpenRelTable::getRelId(srcRel);
	if(srcRelId == E_RELNOTOPEN){
		return srcRelId;
	}
	RelCatEntry* relCatEntry = new RelCatEntry;
	RelCacheTable::getRelCatEntry(srcRelId, relCatEntry);

	int src_nAtts = relCatEntry->numAttrs;
	
	int targetOffset[tar_nAttrs];
	int attr_types[tar_nAttrs];

	for(int i = 0; i < tar_nAttrs; i++){
		AttrCatEntry* attrCatEntry = new AttrCatEntry;
		int ret = AttrCacheTable::getAttrCatEntry(srcRelId, tar_Attrs[i], attrCatEntry);
		if(ret == E_ATTRNOTEXIST){
			return E_ATTRNOTEXIST;
		}
		targetOffset[i] = attrCatEntry->offset;
		attr_types[i] = attrCatEntry->attrType;
	}
	int ret = Schema::createRel(targetRel, tar_nAttrs, tar_Attrs, attr_types);
	if(ret != SUCCESS){
		return ret;
	}
	ret = OpenRelTable::openRel(targetRel);
	if(ret != SUCCESS){
		Schema::deleteRel(targetRel);
		return ret;
	}
	int targetRelId = OpenRelTable::getRelId(targetRel);
	 
	RelCacheTable::resetSearchIndex(srcRelId);

	Attribute record[src_nAtts];

	while(BlockAccess::project(srcRelId, record) == SUCCESS) {
		Attribute proj_record[tar_nAttrs];

		for(int i = 0; i < tar_nAttrs; i++){
			proj_record[i] = record[targetOffset[i]];
		}
		ret = BlockAccess::insert(targetRelId, proj_record);
		if(ret != SUCCESS){
			Schema::deleteRel(targetRel);
			return ret;
		}
	}
	OpenRelTable::closeRel(targetRelId);
	return SUCCESS;

}



bool isNumber(char* str){
	int len;
	float ignore;

	int res = sscanf(str, "%f %n", &ignore, &len);

	return res == 1 && len == strlen(str);
}

 int Algebra::insert(char relName[ATTR_SIZE], int numberOfAttributes, char record[][ATTR_SIZE]){
	if(strcmp(relName, ATTRCAT_RELNAME) == 0 || strcmp(relName, RELCAT_RELNAME) == 0){
		return E_NOTPERMITTED;
	}
	int relId = OpenRelTable::getRelId(relName);

	if(relId == E_RELNOTOPEN) return E_RELNOTOPEN;

	RelCatEntry relCatEntry= *(new RelCatEntry);

	int ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);

	if(ret != SUCCESS){
		return ret;
 	}

	if(relCatEntry.numAttrs != numberOfAttributes){
		return E_NATTRMISMATCH;
	}

	Attribute* attrRecord = new Attribute[numberOfAttributes];
	
	for(int attrCount = 0; attrCount < numberOfAttributes; attrCount++){

		AttrCatEntry attrCatEntry = *(new AttrCatEntry);
		AttrCacheTable::getAttrCatEntry(relId, attrCount, &attrCatEntry);

		int type = attrCatEntry.attrType;
		if(type == NUMBER){
			if(isNumber(record[attrCount])){
				attrRecord[attrCount].nVal = atof(record[attrCount]);
			}else{
				return E_ATTRTYPEMISMATCH;
			}
		}else{
			if(!isNumber(record[attrCount])){
				strcpy(attrRecord[attrCount].sVal , record[attrCount]);
			}else{
				return E_ATTRTYPEMISMATCH;
			}
		}

	}

	int retVal = BlockAccess::insert(relId, attrRecord);
	return retVal;
 }