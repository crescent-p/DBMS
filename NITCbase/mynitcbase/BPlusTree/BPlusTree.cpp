#include "BPlusTree.h"
#include <cstring>

RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int op) {
    IndexId searchIndex;
    int ret = AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);
    AttrCatEntry attrCatEntry;
    ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    int block = -1, index = -1;

    if (searchIndex == IndexId{-1, -1}) {
        block = attrCatEntry.rootBlock;
        index = 0;
        if (block == -1) return RecId{-1, -1};
    } else {
        block = searchIndex.block, index = searchIndex.index + 1;
        IndLeaf leaf(block);
        HeadInfo leafHead;
        leaf.getHeader(&leafHead);
        if (index >= leafHead.numEntries) {
            block = leafHead.rblock, index = 0;
            if (block == -1) return RecId{-1, -1};
        }
    }

    while (StaticBuffer::getStaticBlockType(block) == IND_INTERNAL) {
        IndInternal internalBlk(block);
        HeadInfo intHead;
        internalBlk.getHeader(&intHead);
        InternalEntry intEntry;

        if (op == NE || op == LT || op == LE) {
            internalBlk.getEntry(&intEntry, 0);
            block = intEntry.lChild;
        } else {
            int entryindex = 0;
            while (entryindex < intHead.numEntries) {
                ret = internalBlk.getEntry(&intEntry, entryindex);
                int cmpVal = compareAttrs(intEntry.attrVal, attrVal, attrCatEntry.attrType);
                if ((op == EQ && cmpVal >= 0) || (op == GE && cmpVal >= 0) || (op == GT && cmpVal > 0)) break;
                entryindex++;
            }
            if (entryindex < intHead.numEntries) {
                block = intEntry.lChild;
            } else {
                block = intEntry.rChild;
            }
        }
    }

    while (block != -1) {
        IndLeaf leafBlk(block);
        HeadInfo leafHead;
        leafBlk.getHeader(&leafHead);
        Index leafEntry;

        while (index < leafHead.numEntries) {
            leafBlk.getEntry(&leafEntry, index);
            int cmpVal = compareAttrs(leafEntry.attrVal, attrVal, attrCatEntry.attrType);

            if ((op == EQ && cmpVal == 0) || (op == LE && cmpVal <= 0) || (op == GE && cmpVal >= 0) ||
                (op == LT && cmpVal < 0) || (op == GT && cmpVal > 0) || (op == NE && cmpVal != 0)) {
                searchIndex = IndexId{block, index};
                AttrCacheTable::setSearchIndex(relId, attrName, &searchIndex);
                return RecId{leafEntry.block, leafEntry.slot};
            } else if ((op == EQ || op == LE || op == LT) && cmpVal > 0) {
                return RecId{-1, -1};
            }
            ++index;
        }

        if (op != NE) break;
        block = leafHead.rblock, index = 0;
    }

    return RecId{-1, -1};
}

int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE]) {
    if (relId == RELCAT_RELID || relId == ATTRCAT_RELID) return E_NOTPERMITTED;

    AttrCatEntry attrCatEntryBuffer;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntryBuffer);
    if (ret != SUCCESS) return ret;

    if (attrCatEntryBuffer.rootBlock != -1) return SUCCESS;

    IndLeaf rootBlockBuf;
    int rootBlock = rootBlockBuf.getBlockNum();
    if (rootBlock == E_DISKFULL) return E_DISKFULL;

    attrCatEntryBuffer.rootBlock = rootBlock;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntryBuffer);

    RelCatEntry relCatEntryBuffer;
    RelCacheTable::getRelCatEntry(relId, &relCatEntryBuffer);
    int block = relCatEntryBuffer.firstBlk;

    while (block != -1) {
        RecBuffer blockBuffer(block);
        unsigned char slotmap[relCatEntryBuffer.numSlotsPerBlk];
        blockBuffer.getSlotMap(slotmap);

        for (int slot = 0; slot < relCatEntryBuffer.numSlotsPerBlk; slot++) {
            if (slotmap[slot] == SLOT_OCCUPIED) {
                Attribute record[relCatEntryBuffer.numAttrs];
                blockBuffer.getRecord(record, slot);
                RecId recId = RecId{block, slot};
                ret = bPlusInsert(relId, attrName, record[attrCatEntryBuffer.offset], recId);
                if (ret == E_DISKFULL) return E_DISKFULL;
            }
        }

        HeadInfo blockHeader;
        blockBuffer.getHeader(&blockHeader);
        block = blockHeader.rblock;
    }

    return SUCCESS;
}

int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], Attribute attrVal, RecId recId) {
    AttrCatEntry attrCatEntryBuffer;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntryBuffer);
    if (ret != SUCCESS) return ret;

    int rootBlock = attrCatEntryBuffer.rootBlock;
    if (rootBlock == -1) return E_NOINDEX;

    int leafBlkNum = findLeafToInsert(rootBlock, attrVal, attrCatEntryBuffer.attrType);

    Index indexEntry;
    indexEntry.attrVal = attrVal, indexEntry.block = recId.block, indexEntry.slot = recId.slot;

    if (insertIntoLeaf(relId, attrName, leafBlkNum, indexEntry) == E_DISKFULL) {
        BPlusTree::bPlusDestroy(rootBlock);
        attrCatEntryBuffer.rootBlock = -1;
        AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntryBuffer);
        return E_DISKFULL;
    }

    return SUCCESS;
}

int BPlusTree::findLeafToInsert(int rootBlock, Attribute attrVal, int attrType) {
    int blockNum = rootBlock;

    while (StaticBuffer::getStaticBlockType(blockNum) != IND_LEAF) {
        IndInternal internalBlock(blockNum);
        HeadInfo blockHeader;
        internalBlock.getHeader(&blockHeader);

        int index = 0;
        while (index < blockHeader.numEntries) {
            InternalEntry entry;
            internalBlock.getEntry(&entry, index);
            if (compareAttrs(attrVal, entry.attrVal, attrType) <= 0) break;
            index++;
        }

        if (index == blockHeader.numEntries) {
            InternalEntry entry;
            internalBlock.getEntry(&entry, blockHeader.numEntries - 1);
            blockNum = entry.rChild;
        } else {
            InternalEntry entry;
            internalBlock.getEntry(&entry, index);
            blockNum = entry.lChild;
        }
    }

    return blockNum;
}

int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE], int leafBlockNum, Index indexEntry) {
    AttrCatEntry attrCatEntryBuffer;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntryBuffer);

    IndLeaf leafBlock(leafBlockNum);
    HeadInfo blockHeader;
    leafBlock.getHeader(&blockHeader);

    Index indices[blockHeader.numEntries + 1];
    bool inserted = false;
    for (int entryindex = 0; entryindex < blockHeader.numEntries; entryindex++) {
        Index entry;
        leafBlock.getEntry(&entry, entryindex);
        if (compareAttrs(entry.attrVal, indexEntry.attrVal, attrCatEntryBuffer.attrType) <= 0) {
            indices[entryindex] = entry;
        } else {
            indices[entryindex] = indexEntry;
            inserted = true;
            for (entryindex++; entryindex <= blockHeader.numEntries; entryindex++) {
                leafBlock.getEntry(&entry, entryindex - 1);
                indices[entryindex] = entry;
            }
            break;
        }
    }
    if (!inserted) indices[blockHeader.numEntries] = indexEntry;

    if (blockHeader.numEntries < MAX_KEYS_LEAF) {
        blockHeader.numEntries++;
        leafBlock.setHeader(&blockHeader);
        for (int indicesIt = 0; indicesIt < blockHeader.numEntries; indicesIt++)
            leafBlock.setEntry(&indices[indicesIt], indicesIt);
        return SUCCESS;
    }

    int newRightBlk = splitLeaf(leafBlockNum, indices);
    if (newRightBlk == E_DISKFULL) return E_DISKFULL;

    if (blockHeader.pblock != -1) {
        InternalEntry middleEntry;
        middleEntry.attrVal = indices[MIDDLE_INDEX_LEAF].attrVal, middleEntry.lChild = leafBlockNum, middleEntry.rChild = newRightBlk;
        return insertIntoInternal(relId, attrName, blockHeader.pblock, middleEntry);
    } else {
        if (createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal, leafBlockNum, newRightBlk) == E_DISKFULL)
            return E_DISKFULL;
    }

    return SUCCESS;
}

int BPlusTree::splitLeaf(int leafBlockNum, Index indices[]) {
    IndLeaf rightBlock;
    IndLeaf leftBlock(leafBlockNum);

    int rightBlockNum = rightBlock.getBlockNum();
    int leftBlockNum = leftBlock.getBlockNum();
    if (rightBlockNum == E_DISKFULL) return E_DISKFULL;

    HeadInfo leftBlockHeader, rightBlockHeader;
    leftBlock.getHeader(&leftBlockHeader);
    rightBlock.getHeader(&rightBlockHeader);

    rightBlockHeader.blockType = leftBlockHeader.blockType;
    rightBlockHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2;
    rightBlockHeader.pblock = leftBlockHeader.pblock;
    rightBlockHeader.lblock = leftBlockNum;
    rightBlockHeader.rblock = leftBlockHeader.rblock;
    rightBlock.setHeader(&rightBlockHeader);

    leftBlockHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2;
    leftBlockHeader.rblock = rightBlockNum;
    leftBlock.setHeader(&leftBlockHeader);

    for (int entryindex = 0; entryindex <= MIDDLE_INDEX_LEAF; entryindex++) {
        leftBlock.setEntry(&indices[entryindex], entryindex);
        rightBlock.setEntry(&indices[entryindex + MIDDLE_INDEX_LEAF + 1], entryindex);
    }

    return rightBlockNum;
}

int BPlusTree::insertIntoInternal(int relId, char attrName[ATTR_SIZE], int intBlockNum, InternalEntry intEntry) {
    AttrCatEntry attrCatEntryBuffer;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntryBuffer);

    IndInternal internalBlock(intBlockNum);
    HeadInfo blockHeader;
    internalBlock.getHeader(&blockHeader);

    InternalEntry internalEntries[blockHeader.numEntries + 1];
    int insertedIndex = -1;
    for (int entryindex = 0; entryindex < blockHeader.numEntries; entryindex++) {
        InternalEntry internalBlockEntry;
        internalBlock.getEntry(&internalBlockEntry, entryindex);
        if (compareAttrs(internalBlockEntry.attrVal, intEntry.attrVal, attrCatEntryBuffer.attrType) <= 0) {
            internalEntries[entryindex] = internalBlockEntry;
        } else {
            internalEntries[entryindex] = intEntry;
            insertedIndex = entryindex;
            for (entryindex++; entryindex <= blockHeader.numEntries; entryindex++) {
                internalBlock.getEntry(&internalBlockEntry, entryindex - 1);
                internalEntries[entryindex] = internalBlockEntry;
            }
            break;
        }
    }
    if (insertedIndex == -1) {
        internalEntries[blockHeader.numEntries] = intEntry;
        insertedIndex = blockHeader.numEntries;
    }

    if (insertedIndex > 0) internalEntries[insertedIndex - 1].rChild = intEntry.lChild;
    if (insertedIndex < blockHeader.numEntries) internalEntries[insertedIndex + 1].lChild = intEntry.rChild;

    if (blockHeader.numEntries < MAX_KEYS_INTERNAL) {
        blockHeader.numEntries++;
        internalBlock.setHeader(&blockHeader);
        for (int entryindex = 0; entryindex < blockHeader.numEntries; entryindex++)
            internalBlock.setEntry(&internalEntries[entryindex], entryindex);
        return SUCCESS;
    }

    int newRightBlk = splitInternal(intBlockNum, internalEntries);
    if (newRightBlk == E_DISKFULL) {
        BPlusTree::bPlusDestroy(intEntry.rChild);
        return E_DISKFULL;
    }

    if (blockHeader.pblock != -1) {
        InternalEntry middleEntry;
        middleEntry.lChild = intBlockNum, middleEntry.rChild = newRightBlk;
        middleEntry.attrVal = internalEntries[MIDDLE_INDEX_INTERNAL].attrVal;
        return insertIntoInternal(relId, attrName, blockHeader.pblock, middleEntry);
    } else {
        return createNewRoot(relId, attrName, internalEntries[MIDDLE_INDEX_INTERNAL].attrVal, intBlockNum, newRightBlk);
    }

    return SUCCESS;
}

int BPlusTree::splitInternal(int intBlockNum, InternalEntry internalEntries[]) {
    IndInternal rightBlock;
    IndInternal leftBlock(intBlockNum);

    int leftBlockNum = leftBlock.getBlockNum();
    int rightBlockNum = rightBlock.getBlockNum();
    if (rightBlockNum == E_DISKFULL) return E_DISKFULL;

    HeadInfo leftBlockHeader, rightBlockHeader;
    leftBlock.getHeader(&leftBlockHeader);
    rightBlock.getHeader(&rightBlockHeader);

    rightBlockHeader.numEntries = MAX_KEYS_INTERNAL / 2;
    rightBlockHeader.pblock = leftBlockHeader.pblock;
    rightBlock.setHeader(&rightBlockHeader);

    leftBlockHeader.numEntries = MAX_KEYS_INTERNAL / 2;
    leftBlockHeader.rblock = rightBlockNum;
    leftBlock.setHeader(&leftBlockHeader);

    for (int entryindex = 0; entryindex < MIDDLE_INDEX_INTERNAL; entryindex++) {
        leftBlock.setEntry(&internalEntries[entryindex], entryindex);
        rightBlock.setEntry(&internalEntries[entryindex + MIDDLE_INDEX_INTERNAL + 1], entryindex);
    }

    int type = StaticBuffer::getStaticBlockType(internalEntries[0].lChild);
    BlockBuffer blockbuffer(internalEntries[MIDDLE_INDEX_INTERNAL + 1].lChild);
    HeadInfo blockHeader;
    blockbuffer.getHeader(&blockHeader);
    blockHeader.pblock = rightBlockNum;
    blockbuffer.setHeader(&blockHeader);

    for (int entryindex = 0; entryindex < MIDDLE_INDEX_INTERNAL; entryindex++) {
        BlockBuffer blockbuffer(internalEntries[entryindex + MIDDLE_INDEX_INTERNAL + 1].rChild);
        blockbuffer.getHeader(&blockHeader);
        blockHeader.pblock = rightBlockNum;
        blockbuffer.setHeader(&blockHeader);
    }

    return rightBlockNum;
}

int BPlusTree::createNewRoot(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int lChild, int rChild) {
    AttrCatEntry attrCatEntryBuffer;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntryBuffer);

    IndInternal newRootBlock;
    int newRootBlkNum = newRootBlock.getBlockNum();
    if (newRootBlkNum == E_DISKFULL) {
        BPlusTree::bPlusDestroy(rChild);
        return E_DISKFULL;
    }

    HeadInfo blockHeader;
    newRootBlock.getHeader(&blockHeader);
    blockHeader.numEntries = 1;
    newRootBlock.setHeader(&blockHeader);

    InternalEntry internalentry;
    internalentry.lChild = lChild, internalentry.rChild = rChild, internalentry.attrVal = attrVal;
    newRootBlock.setEntry(&internalentry, 0);

    BlockBuffer leftChildBlock(lChild);
    BlockBuffer rightChildBlock(rChild);
    HeadInfo leftChildHeader, rightChildHeader;
    leftChildBlock.getHeader(&leftChildHeader);
    rightChildBlock.getHeader(&rightChildHeader);
    leftChildHeader.pblock = newRootBlkNum;
    rightChildHeader.pblock = newRootBlkNum;
    leftChildBlock.setHeader(&leftChildHeader);
    rightChildBlock.setHeader(&rightChildHeader);

    attrCatEntryBuffer.rootBlock = newRootBlkNum;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntryBuffer);

    return SUCCESS;
}

int BPlusTree::bPlusDestroy(int rootBlockNum) {
    if (rootBlockNum < 0 || rootBlockNum >= DISK_BLOCKS) return E_OUTOFBOUND;

    int type = StaticBuffer::getStaticBlockType(rootBlockNum);
    if (type == IND_LEAF) {
        IndLeaf leafBlock(rootBlockNum);
        leafBlock.releaseBlock();
        return SUCCESS;
    } else if (type == IND_INTERNAL) {
        IndInternal internalBlock(rootBlockNum);
        HeadInfo blockHeader;
        internalBlock.getHeader(&blockHeader);

        InternalEntry blockEntry;
        internalBlock.getEntry(&blockEntry, 0);
        BPlusTree::bPlusDestroy(blockEntry.lChild);

        for (int entry = 0; entry < blockHeader.numEntries; entry++) {
            internalBlock.getEntry(&blockEntry, entry);
            BPlusTree::bPlusDestroy(blockEntry.rChild);
        }

        internalBlock.releaseBlock();
        return SUCCESS;
    } else {
        return E_INVALIDBLOCK;
    }
}
