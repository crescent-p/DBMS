#include "Schema.h"

#include <cmath>
#include <cstring>

inline bool operator==(RecId lhs, RecId rhs) {
    return (lhs.block == rhs.block && lhs.slot == rhs.slot);
}

inline bool operator!=(RecId lhs, RecId rhs) {
    return !(lhs == rhs);
}

int Schema::openRel(char relName[ATTR_SIZE]) {
    int ret = OpenRelTable::openRel(relName);

    if (ret >= 0)
        return SUCCESS;

    return ret;
}

int Schema::closeRel(char relName[ATTR_SIZE]) {
    if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0)
        return E_NOTPERMITTED;

    int relId = OpenRelTable::getRelId(relName);

    if (relId == E_RELNOTOPEN)
        return E_RELNOTOPEN;

    return OpenRelTable::closeRel(relId);
}

int Schema::renameRel(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE]) {
    if (strcmp(oldRelName, RELCAT_RELNAME) == 0 || strcmp(oldRelName, ATTRCAT_RELNAME) == 0)
        return E_NOTPERMITTED;

    if (strcmp(newRelName, RELCAT_RELNAME) == 0 || strcmp(newRelName, ATTRCAT_RELNAME) == 0)
        return E_NOTPERMITTED;

    int relId = OpenRelTable::getRelId(oldRelName);
    if (relId != E_RELNOTOPEN)
        return E_RELOPEN;

    return BlockAccess::renameRelation(oldRelName, newRelName);
}

int Schema::renameAttr(char *relName, char *oldAttrName, char *newAttrName) {
    if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0)
        return E_NOTPERMITTED;

    int relId = OpenRelTable::getRelId(relName);
    if (relId != E_RELNOTOPEN)
        return E_RELOPEN;

    return BlockAccess::renameAttribute(relName, oldAttrName, newAttrName);
}

int Schema::createRel(char relName[], int nAttrs, char attrs[][ATTR_SIZE], int attrtype[]) {
    Attribute relNameAsAttribute;
    strcpy((char *)relNameAsAttribute.sVal, (const char *)relName);

    RecId targetRelId;

    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    targetRelId = BlockAccess::linearSearch(RELCAT_RELID, "RelName", relNameAsAttribute, EQ);

    if (targetRelId != RecId{-1, -1})
        return E_RELEXIST;

    for (int i = 0; i < nAttrs - 1; i++)
        for (int j = i + 1; j < nAttrs; j++)
            if (strcmp(attrs[i], attrs[j]) == 0)
                return E_DUPLICATEATTR;
 
    
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, relName);
    relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal = nAttrs;
    relCatRecord[RELCAT_NO_RECORDS_INDEX].nVal = 0;
    relCatRecord[RELCAT_FIRST_BLOCK_INDEX].nVal = -1;
    relCatRecord[RELCAT_LAST_BLOCK_INDEX].nVal = -1;
    relCatRecord[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal = floor((2016 * 1.00) / (16 * nAttrs + 1));

    int retVal = BlockAccess::insert(RELCAT_RELID, relCatRecord);

    if (retVal != SUCCESS)
        return retVal;

    for (int attrIndex = 0; attrIndex < nAttrs; attrIndex++) {
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relName);
        strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrs[attrIndex]);
        attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal = attrtype[attrIndex];
        attrCatRecord[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = -1;
        attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal = -1;
        attrCatRecord[ATTRCAT_OFFSET_INDEX].nVal = attrIndex;

        retVal = BlockAccess::insert(ATTRCAT_RELID, attrCatRecord);

        if (retVal != SUCCESS) {
            Schema::deleteRel(relName);
            return E_DISKFULL;
        }
    }

    return SUCCESS;
}

int Schema::deleteRel(char *relName) {
    if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0)
        return E_NOTPERMITTED;

    int relId = OpenRelTable::getRelId(relName);

    if (relId >= 0 && relId < MAX_OPEN)
        return E_RELOPEN;

    return BlockAccess::deleteRelation(relName);
}

int Schema::createIndex(char relName[ATTR_SIZE], char attrName[ATTR_SIZE]) {
    if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0)
        return E_NOTPERMITTED;

    int relId = OpenRelTable::getRelId(relName);

    if (relId == E_RELNOTOPEN)
        return E_RELNOTOPEN;

    return BPlusTree::bPlusCreate(relId, attrName);
}

int Schema::dropIndex(char *relName, char *attrName) {
    if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0)
        return E_NOTPERMITTED;

    int relId = OpenRelTable::getRelId(relName);

    if (relId == E_RELNOTOPEN)
        return E_RELNOTOPEN;

    AttrCatEntry attrCatEntryBuffer;

    if (AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntryBuffer) != SUCCESS)
        return E_ATTRNOTEXIST;

    int rootBlock = attrCatEntryBuffer.rootBlock;

    if (rootBlock == -1)
        return E_NOINDEX;

    BPlusTree::bPlusDestroy(rootBlock);

    attrCatEntryBuffer.rootBlock = -1;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntryBuffer);

    return SUCCESS;
}
