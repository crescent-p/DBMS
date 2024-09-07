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

	RelCatEntry relCatEntry = *(new RelCatEntry);
	RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

	// printf("|");
	for(int i = 0; i < relCatEntry.numAttrs; i++){
		AttrCatEntry attrCatEntry = *(new AttrCatEntry);
		AttrCacheTable::getAttrCatEntry(srcRelId ,i, &attrCatEntry);
		printf("%s |", attrCatEntry.attrName);
	}
	printf("\n");
	RelCacheTable::resetSearchIndex(srcRelId);
	while(true){
		
		RecId searchRes = BlockAccess::linearSearch(srcRelId, attr, attrVal, op);
		// std::cout<<searchRes.block<<' '<<searchRes.slot<<std::endl;
		if(searchRes.block != -1 && searchRes.slot != -1){
			RecBuffer blockBuffer(searchRes.block);
			
			HeadInfo headInfo;
			blockBuffer.getHeader(&headInfo);

			Attribute selectAttr[headInfo.numAttrs];
			blockBuffer.getRecord(selectAttr,searchRes.slot);

			for(int i = 0; i < headInfo.numAttrs; i++){
				int outed = printf("%s", selectAttr[i].sVal);
				// std::cout<<outed<<std::endl;
				if(outed){
					printf(" |");
				}
				if(outed == 0){
					printf(" %d |", (int)selectAttr[i].nVal);
				}
			}
			printf("\n");
		
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