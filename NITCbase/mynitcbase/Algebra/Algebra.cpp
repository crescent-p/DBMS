#include "Algebra.h"

#include <cstring>
#include <stdio.h>
#include <string>



bool isNumber(char* str);
int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], char attr[ATTR_SIZE], int op, char strVal[ATTR_SIZE]){
	int srcRelId = OpenRelTable::getRelId(srcRel);
	if(srcRelId == E_RELNOTOPEN){
		return E_RELNOTOPEN;
	}

	AttrCatEntry attrCatEntry;
	int res = AttrCacheTable::getAttrCatEntry( srcRelId, attr, &attrCatEntry);
	if(res == E_ATTRNOTEXIST){
		return res;
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

	RelCatEntry relCatEntry;
	RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatEntry);

	printf("|");
	for(int i = 0; i < relCatEntry.numAttrs; i++){
		AttrCatEntry attrCatEntry;
		AttrCacheTable::getAttrCatEntry(srcRelId ,i, &attrCatEntry);
		printf("%s |", attrCatEntry.attrName);
	}
	printf("\n");
	while(true){
		RecId searchRes = BlockAccess::linearSearch(srcRelId, attr, attrVal, op);
		if(searchRes.block != -1 && searchRes.slot != -1){
			RecBuffer blockBuffer(searchRes.block);
			Attribute selectAttr[relCatEntry.numAttrs];
			blockBuffer.getRecord(selectAttr,searchRes.slot);

			for(int i = 0; i < relCatEntry.numAttrs; i++){
				AttrCatEntry attrCatEntry;
				AttrCacheTable::getAttrCatEntry(srcRelId ,i, &attrCatEntry);
				if(attrCatEntry.attrType == NUMBER){
					printf(" %d | ", (int)selectAttr[i].nVal);
				}else{
					printf(" %s | ", selectAttr[i].sVal);
			}
		}printf("\n");
		
		}else{
			break;
		}
	}

	return SUCCESS;
}


bool isNumber(char* str){
	int len;
	float ignore;

	int res = sscanf(str, "%f %n", &ignore, &len);

	return res == 1 && len == strlen(str);
}