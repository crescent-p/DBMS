#include "OpenRelTable.h"

#include <cstring>
#include <stdlib.h>
#include <stdio.h>

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

inline bool operator==(const RecId& lhs, const RecId& rhs) {
	return (lhs.block == rhs.block && lhs.slot == rhs.slot);
}

AttrCacheEntry* createAttrCacheEntryList(int size) {
	AttrCacheEntry* head = nullptr, * curr = nullptr;
	head = curr = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
	size--;
	while (size--) {
		curr->next = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
		curr = curr->next;
	}
	curr->next = nullptr;
	return head;
}

OpenRelTable::OpenRelTable() {
	for (int i = 0; i < MAX_OPEN; ++i) {
		RelCacheTable::relCache[i] = nullptr;
		AttrCacheTable::attrCache[i] = nullptr;
		tableMetaInfo[i].free = true;
	}

	RecBuffer relCatBlock(RELCAT_BLOCK);
	Attribute relCatRecord[RELCAT_NO_ATTRS];
	RelCacheEntry* relCacheEntry = nullptr;

	for (int relId = RELCAT_RELID; relId <= ATTRCAT_RELID; relId++) {
		relCatBlock.getRecord(relCatRecord, relId);
		relCacheEntry = (RelCacheEntry*)malloc(sizeof(RelCacheEntry));
		RelCacheTable::recordToRelCatEntry(relCatRecord, &(relCacheEntry->relCatEntry));
		relCacheEntry->recId.block = RELCAT_BLOCK;
		relCacheEntry->recId.slot = relId;
		relCacheEntry->searchIndex = { -1, -1 };
		RelCacheTable::relCache[relId] = relCacheEntry;
	}

	RecBuffer attrCatBlock(ATTRCAT_BLOCK);
	Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
	AttrCacheEntry* attrCacheEntry = nullptr, * head = nullptr;

	for (int relId = RELCAT_RELID, recordId = 0; relId <= ATTRCAT_RELID; relId++) {
		int numberOfAttributes = RelCacheTable::relCache[relId]->relCatEntry.numAttrs;
		head = createAttrCacheEntryList(numberOfAttributes);
		attrCacheEntry = head;

		while (numberOfAttributes--) {
			attrCatBlock.getRecord(attrCatRecord, recordId);
			AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &(attrCacheEntry->attrCatEntry));
			attrCacheEntry->recId.slot = recordId++;
			attrCacheEntry->recId.block = ATTRCAT_BLOCK;
			attrCacheEntry = attrCacheEntry->next;
		}
		AttrCacheTable::attrCache[relId] = head;
	}

	tableMetaInfo[RELCAT_RELID].free = false;
	strcpy(tableMetaInfo[RELCAT_RELID].relName, RELCAT_RELNAME);

	tableMetaInfo[ATTRCAT_RELID].free = false;
	strcpy(tableMetaInfo[ATTRCAT_RELID].relName, ATTRCAT_RELNAME);
}

OpenRelTable::~OpenRelTable() {
	for (int relId = 2; relId < MAX_OPEN; relId++) {
		if (!OpenRelTable::tableMetaInfo[relId].free) {
			OpenRelTable::closeRel(relId);
		}
	}

	if (RelCacheTable::relCache[ATTRCAT_RELID]->dirty) {
		RelCatEntry relCatBuffer;
		RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatBuffer);
		Attribute relCatRecord[RELCAT_NO_ATTRS];
		RelCacheTable::relCatEntryToRecord(&relCatBuffer, relCatRecord);
		RecId recId = RelCacheTable::relCache[ATTRCAT_RELID]->recId;
		RecBuffer relCatBlock(recId.block);
		relCatBlock.setRecord(relCatRecord, recId.slot);
	}
	free(RelCacheTable::relCache[ATTRCAT_RELID]);

	if (RelCacheTable::relCache[RELCAT_RELID]->dirty) {
		RelCatEntry relCatBuffer;
		RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatBuffer);
		Attribute relCatRecord[RELCAT_NO_ATTRS];
		RelCacheTable::relCatEntryToRecord(&relCatBuffer, relCatRecord);
		RecId recId = RelCacheTable::relCache[RELCAT_RELID]->recId;
		RecBuffer relCatBlock(recId.block);
		relCatBlock.setRecord(relCatRecord, recId.slot);
	}
	free(RelCacheTable::relCache[RELCAT_RELID]);

	for (int relId = ATTRCAT_RELID; relId >= RELCAT_RELID; relId--) {
		AttrCacheEntry* curr = AttrCacheTable::attrCache[relId], * next = nullptr;
		for (int attrIndex = 0; attrIndex < 6; attrIndex++) {
			next = curr->next;
			if (curr->dirty) {
				AttrCatEntry attrCatBuffer;
				AttrCacheTable::getAttrCatEntry(relId, attrIndex, &attrCatBuffer);
				Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
				AttrCacheTable::attrCatEntryToRecord(&attrCatBuffer, attrCatRecord);
				RecId recId = curr->recId;
				RecBuffer attrCatBlock(recId.block);
				attrCatBlock.setRecord(attrCatRecord, recId.slot);
			}
			free(curr);
			curr = next;
		}
	}
}

int OpenRelTable::getFreeOpenRelTableEntry() {
	for (int relId = 0; relId < MAX_OPEN; relId++) {
		if (tableMetaInfo[relId].free) {
			return relId;
		}
	}
	return E_CACHEFULL;
}

int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {
	for (int relId = 0; relId < MAX_OPEN; relId++) {
		if (strcmp(tableMetaInfo[relId].relName, relName) == 0 && !tableMetaInfo[relId].free) {
			return relId;
		}
	}
	return E_RELNOTOPEN;
}

int OpenRelTable::openRel(char relName[ATTR_SIZE]) {
	int relId = getRelId(relName);
	if (relId >= 0) {
		return relId;
	}

	relId = OpenRelTable::getFreeOpenRelTableEntry();
	if (relId == E_CACHEFULL) return E_CACHEFULL;

	Attribute attrVal;
	strcpy(attrVal.sVal, relName);
	RelCacheTable::resetSearchIndex(RELCAT_RELID);
	RecId relcatRecId = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, attrVal, EQ);

	if (relcatRecId == RecId{ -1, -1 }) {
		return E_RELNOTEXIST;
	}

	RecBuffer relationBuffer(relcatRecId.block);
	Attribute relationRecord[RELCAT_NO_ATTRS];
	RelCacheEntry* relCacheBuffer = nullptr;
	relationBuffer.getRecord(relationRecord, relcatRecId.slot);
	relCacheBuffer = (RelCacheEntry*)malloc(sizeof(RelCacheEntry));
	RelCacheTable::recordToRelCatEntry(relationRecord, &(relCacheBuffer->relCatEntry));
	relCacheBuffer->recId.block = relcatRecId.block;
	relCacheBuffer->recId.slot = relcatRecId.slot;
	RelCacheTable::relCache[relId] = relCacheBuffer;

	Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
	AttrCacheEntry* attrCacheEntry = nullptr, * head = nullptr;
	int numberOfAttributes = RelCacheTable::relCache[relId]->relCatEntry.numAttrs;
	head = createAttrCacheEntryList(numberOfAttributes);
	attrCacheEntry = head;

	RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
	while (numberOfAttributes--) {
		RecId attrcatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, RELCAT_ATTR_RELNAME, attrVal, EQ);
		RecBuffer attrCatBlock(attrcatRecId.block);
		attrCatBlock.getRecord(attrCatRecord, attrcatRecId.slot);
		AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &(attrCacheEntry->attrCatEntry));
		attrCacheEntry->recId.block = attrcatRecId.block;
		attrCacheEntry->recId.slot = attrcatRecId.slot;
		attrCacheEntry = attrCacheEntry->next;
	}
	AttrCacheTable::attrCache[relId] = head;

	tableMetaInfo[relId].free = false;
	strcpy(tableMetaInfo[relId].relName, relName);

	return relId;
}

int OpenRelTable::closeRel(int relId) {
	if (relId == RELCAT_RELID || relId == ATTRCAT_RELID) return E_NOTPERMITTED;
	if (0 > relId || relId >= MAX_OPEN) return E_OUTOFBOUND;
	if (tableMetaInfo[relId].free) return E_RELNOTOPEN;

	if (RelCacheTable::relCache[relId]->dirty) {
		Attribute relCatBuffer[RELCAT_NO_ATTRS];
		RelCacheTable::relCatEntryToRecord(&(RelCacheTable::relCache[relId]->relCatEntry), relCatBuffer);
		RecId recId = RelCacheTable::relCache[relId]->recId;
		RecBuffer relCatBlock(recId.block);
		relCatBlock.setRecord(relCatBuffer, recId.slot);
	}
	free(RelCacheTable::relCache[relId]);

	AttrCacheEntry* head = AttrCacheTable::attrCache[relId];
	AttrCacheEntry* next = head->next;

	while (head) {
		if (head->dirty) {
			Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
			AttrCacheTable::attrCatEntryToRecord(&(head->attrCatEntry), attrCatRecord);
			RecBuffer attrCatBlockBuffer(head->recId.block);
			attrCatBlockBuffer.setRecord(attrCatRecord, head->recId.slot);
		}
		free(head);
		head = next;
		if (head) next = head->next;
	}

	tableMetaInfo[relId].free = true;
	RelCacheTable::relCache[relId] = nullptr;
	AttrCacheTable::attrCache[relId] = nullptr;

	return SUCCESS;
}
