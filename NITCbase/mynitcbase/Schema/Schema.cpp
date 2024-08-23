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