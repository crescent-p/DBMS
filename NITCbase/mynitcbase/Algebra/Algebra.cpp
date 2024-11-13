#include "Algebra.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstring>
#include <iostream>

int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], 
                    char attr[ATTR_SIZE], int op, char strVal[ATTR_SIZE]) 
{
    int srcRelId = OpenRelTable::getRelId(srcRel);
    if (srcRelId == E_RELNOTOPEN) return E_RELNOTOPEN;

    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);
    if (ret == E_ATTRNOTEXIST) return E_ATTRNOTEXIST;

    int type = attrCatEntry.attrType;
    Attribute attrVal;
    if (type == NUMBER) {
        if (isNumber(strVal))
            attrVal.nVal = atof(strVal);
        else
            return E_ATTRTYPEMISMATCH;
    } else if (type == STRING) {
        strcpy(attrVal.sVal, strVal);
    }

    RelCatEntry relCatEntryBuffer;
    RelCacheTable::getRelCatEntry(srcRelId, &relCatEntryBuffer);
    int srcNoAttrs = relCatEntryBuffer.numAttrs;

    char srcAttrNames[srcNoAttrs][ATTR_SIZE];
    int srcAttrTypes[srcNoAttrs];

    for (int attrIndex = 0; attrIndex < srcNoAttrs; attrIndex++) {
        AttrCatEntry attrCatEntryBuffer;
        AttrCacheTable::getAttrCatEntry(srcRelId, attrIndex, &attrCatEntryBuffer);
        strcpy(srcAttrNames[attrIndex], attrCatEntryBuffer.attrName);
        srcAttrTypes[attrIndex] = attrCatEntryBuffer.attrType;
    }

    ret = Schema::createRel(targetRel, srcNoAttrs, srcAttrNames, srcAttrTypes);
    if (ret != SUCCESS) return ret;

    int targetRelId = OpenRelTable::openRel(targetRel);
    if (targetRelId < 0 || targetRelId >= MAX_OPEN) return targetRelId;

    RelCacheTable::resetSearchIndex(srcRelId);
    AttrCacheTable::resetSearchIndex(srcRelId, attr);

    Attribute record[srcNoAttrs];
    while (BlockAccess::search(srcRelId, record, attr, attrVal, op) == SUCCESS) {
        ret = BlockAccess::insert(targetRelId, record);
        if (ret != SUCCESS) {
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    Schema::closeRel(targetRel);
    return SUCCESS;
}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], int tar_nAttrs, char tar_Attrs[][ATTR_SIZE]) 
{
    int srcRelId = OpenRelTable::getRelId(srcRel);
    if (srcRelId < 0 || srcRelId >= MAX_OPEN) return E_RELNOTOPEN;

    RelCatEntry relCatEntryBuffer;
    RelCacheTable::getRelCatEntry(srcRelId, &relCatEntryBuffer);
    int srcNoAttrs = relCatEntryBuffer.numAttrs;

    int attrOffset[tar_nAttrs];
    int attrTypes[tar_nAttrs];

    for (int attrIndex = 0; attrIndex < tar_nAttrs; attrIndex++) {
        attrOffset[attrIndex] = AttrCacheTable::getAttributeOffset(srcRelId, tar_Attrs[attrIndex]);
        if (attrOffset[attrIndex] < 0) return attrOffset[attrIndex];

        AttrCatEntry attrCatEntryBuffer;
        AttrCacheTable::getAttrCatEntry(srcRelId, tar_Attrs[attrIndex], &attrCatEntryBuffer);
        attrTypes[attrIndex] = attrCatEntryBuffer.attrType;
    }

    int ret = Schema::createRel(targetRel, tar_nAttrs, tar_Attrs, attrTypes);
    if (ret != SUCCESS) return ret;

    int targetRelId = OpenRelTable::openRel(targetRel);
    if (targetRelId < 0) {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    RelCacheTable::resetSearchIndex(srcRelId);
    Attribute record[srcNoAttrs];

    while (BlockAccess::project(srcRelId, record) == SUCCESS) {
        Attribute proj_record[tar_nAttrs];
        for (int attrIndex = 0; attrIndex < tar_nAttrs; attrIndex++)
            proj_record[attrIndex] = record[attrOffset[attrIndex]];

        ret = BlockAccess::insert(targetRelId, proj_record);
        if (ret != SUCCESS) {
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    Schema::closeRel(targetRel);
    return SUCCESS;
}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE]) 
{
    int srcRelId = OpenRelTable::getRelId(srcRel);
    if (srcRelId < 0 || srcRelId >= MAX_OPEN) return E_RELNOTOPEN;

    RelCatEntry relCatEntryBuffer;
    RelCacheTable::getRelCatEntry(srcRelId, &relCatEntryBuffer);
    int srcNoAttrs = relCatEntryBuffer.numAttrs;

    char attrNames[srcNoAttrs][ATTR_SIZE];
    int attrTypes[srcNoAttrs];

    for (int attrIndex = 0; attrIndex < srcNoAttrs; attrIndex++) {
        AttrCatEntry attrCatEntryBuffer;
        AttrCacheTable::getAttrCatEntry(srcRelId, attrIndex, &attrCatEntryBuffer);
        strcpy(attrNames[attrIndex], attrCatEntryBuffer.attrName);
        attrTypes[attrIndex] = attrCatEntryBuffer.attrType;
    }

    int ret = Schema::createRel(targetRel, srcNoAttrs, attrNames, attrTypes);
    if (ret != SUCCESS) return ret;

    int targetRelId = OpenRelTable::openRel(targetRel);
    if (targetRelId < 0 || targetRelId >= MAX_OPEN) return targetRelId;

    RelCacheTable::resetSearchIndex(srcRelId);
    Attribute record[srcNoAttrs];

    while (BlockAccess::project(srcRelId, record) == SUCCESS) {
        ret = BlockAccess::insert(targetRelId, record);
        if (ret != SUCCESS) {
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    Schema::closeRel(targetRel);
    return SUCCESS;
}

inline bool isNumber(char *str)
{
    int len;
    float ignore;
    int ret = sscanf(str, "%f %n", &ignore, &len);
    return ret == 1 && len == strlen(str);
}

int Algebra::join(char srcRelation1[ATTR_SIZE], char srcRelation2[ATTR_SIZE], char targetRelation[ATTR_SIZE], char attribute1[ATTR_SIZE], char attribute2[ATTR_SIZE]) 
{
    int srcRelId1 = OpenRelTable::getRelId(srcRelation1);
    int srcRelId2 = OpenRelTable::getRelId(srcRelation2);
    if (srcRelId1 == E_RELNOTOPEN || srcRelId2 == E_RELNOTOPEN) return E_RELNOTOPEN;

    AttrCatEntry attrCatEntry1, attrCatEntry2;
    int ret1 = AttrCacheTable::getAttrCatEntry(srcRelId1, attribute1, &attrCatEntry1);
    int ret2 = AttrCacheTable::getAttrCatEntry(srcRelId2, attribute2, &attrCatEntry2);
    if (ret1 == E_ATTRNOTEXIST || ret2 == E_ATTRNOTEXIST) return E_ATTRNOTEXIST;
    if (attrCatEntry1.attrType != attrCatEntry2.attrType) return E_ATTRTYPEMISMATCH;

    RelCatEntry relCache1, relCache2;
    RelCacheTable::getRelCatEntry(srcRelId1, &relCache1);
    RelCacheTable::getRelCatEntry(srcRelId2, &relCache2);

    for (int i = 0; i < relCache1.numAttrs; i++) {
        AttrCatEntry attrCatEntryi;
        AttrCacheTable::getAttrCatEntry(srcRelId1, i, &attrCatEntryi);
        for (int j = 0; j < relCache2.numAttrs; j++) {
            AttrCatEntry attrCatEntryj;
            AttrCacheTable::getAttrCatEntry(srcRelId2, j, &attrCatEntryj);
            if (strcmp(attrCatEntryi.attrName, attrCatEntryj.attrName) == 0) {
                if (strcmp(attrCatEntryi.attrName, attribute1) == 0 &&
                    strcmp(attrCatEntryj.attrName, attribute2) == 0) {
                    continue;
                } else {
                    return E_DUPLICATEATTR;
                }
            }
        }
    }

    int numOfAttributes1 = relCache1.numAttrs;
    int numOfAttributes2 = relCache2.numAttrs;

    if (attrCatEntry2.rootBlock == -1) {
        int ret = BPlusTree::bPlusCreate(srcRelId2, attribute2);
        if (ret != SUCCESS) return ret;
    }

    int numOfAttributesInTarget = numOfAttributes1 + numOfAttributes2 - 1;
    char targetRelAttrNames[numOfAttributesInTarget][ATTR_SIZE];
    int targetRelAttrTypes[numOfAttributesInTarget];

    int indexForTargetRel = 0;
    for (int i = 0; i < numOfAttributes1; i++) {
        AttrCatEntry entry;
        AttrCacheTable::getAttrCatEntry(srcRelId1, i, &entry);
        strcpy(targetRelAttrNames[indexForTargetRel], entry.attrName);
        targetRelAttrTypes[indexForTargetRel] = entry.attrType;
        indexForTargetRel++;
    }

    for (int i = 0; i < numOfAttributes2; i++) {
        AttrCatEntry entry;
        AttrCacheTable::getAttrCatEntry(srcRelId2, i, &entry);
        if (strcmp(entry.attrName, attribute2) == 0) continue;
        strcpy(targetRelAttrNames[indexForTargetRel], entry.attrName);
        targetRelAttrTypes[indexForTargetRel] = entry.attrType;
        indexForTargetRel++;
    }

    int ret = Schema::createRel(targetRelation, numOfAttributesInTarget, targetRelAttrNames, targetRelAttrTypes);
    if (ret != SUCCESS) return ret;

    ret = OpenRelTable::openRel(targetRelation);
    if (ret < 0) {
        Schema::deleteRel(targetRelation);
        return E_MAXRELATIONS;
    }

    Attribute record1[numOfAttributes1];
    Attribute record2[numOfAttributes2];
    Attribute targetRecord[numOfAttributesInTarget];

    AttrCacheTable::resetSearchIndex(srcRelId1, attribute1);
    RelCacheTable::resetSearchIndex(srcRelId1);

    while (BlockAccess::project(srcRelId1, record1) == SUCCESS) {
        RelCacheTable::resetSearchIndex(srcRelId2);
        AttrCacheTable::resetSearchIndex(srcRelId2, attribute2);

        while (BlockAccess::search(srcRelId2, record2, attribute2, record1[attrCatEntry1.offset], EQ) == SUCCESS) {
            int index = 0;
            for (int i = 0; i < numOfAttributes1; i++) {
                targetRecord[index] = record1[i];
                index++;
            }
            for (int i = 0; i < numOfAttributes2; i++) {
                if (strcmp(targetRelAttrNames[index], attribute2) == 0) continue;
                targetRecord[index] = record2[i];
                index++;
            }
            int targetRelId = OpenRelTable::getRelId(targetRelation);
            int ret = BlockAccess::insert(targetRelId, targetRecord);
            if (ret == E_DISKFULL) {
                OpenRelTable::closeRel(targetRelId);
                Schema::deleteRel(targetRelation);
                return E_DISKFULL;
            }
        }
    }
    int relId = OpenRelTable::getRelId(targetRelation);
    OpenRelTable::closeRel(relId);
    return SUCCESS;
}

int Algebra::insert(char relName[ATTR_SIZE], int nAttrs, char record[][ATTR_SIZE]) 
{
    if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0)
        return E_NOTPERMITTED;

    int relId = OpenRelTable::getRelId(relName);
    if (relId < 0 || relId >= MAX_OPEN) return E_RELNOTOPEN;

    RelCatEntry relCatBuffer;
    RelCacheTable::getRelCatEntry(relId, &relCatBuffer);
    if (relCatBuffer.numAttrs != nAttrs) return E_NATTRMISMATCH;

    Attribute recordValues[nAttrs];
    for (int attrIndex = 0; attrIndex < nAttrs; attrIndex++) {
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(relId, attrIndex, &attrCatEntry);

        int type = attrCatEntry.attrType;
        if (type == NUMBER) {
            if (isNumber(record[attrIndex])) {
                recordValues[attrIndex].nVal = atof(record[attrIndex]);
            } else {
                return E_ATTRTYPEMISMATCH;
            }
        } else if (type == STRING) {
            strcpy((char *)&(recordValues[attrIndex].sVal), record[attrIndex]);
        }
    }

    int ret = BlockAccess::insert(relId, recordValues);
    return ret;
}
