#include "BPlusTree.h"
#include "../shared.h"
#include <cstring>
#include <iostream>

int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE]) {

    // if relId is either RELCAT_RELID or ATTRCAT_RELID:
    //     return E_NOTPERMITTED;
    if(relId == RELCAT_RELID || relId == ATTRCAT_RELID)
        return E_NOTPERMITTED;

    // get the attribute catalog entry of attribute `attrName`
    // using AttrCacheTable::getAttrCatEntry()
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if(ret != SUCCESS)
        return ret;
    // if getAttrCatEntry fails
    //     return the error code from getAttrCatEntry

    if (attrCatEntry.rootBlock != -1/* an index already exists for the attribute (check rootBlock field) */) {
        return SUCCESS;
    }

    /******Creating a new B+ Tree ******/

    // get a free leaf block using constructor 1 to allocate a new block
    IndLeaf rootBlockBuf;

    // (if the block could not be allocated, the appropriate error code
    //  will be stored in the blockNum member field of the object)

    // declare rootBlock to store the blockNumber of the new leaf block
    int rootBlock = rootBlockBuf.getBlockNum();

    // if there is no more disk space for creating an index
    if (rootBlock == E_DISKFULL) {
        return E_DISKFULL;
    }
    //NOT MENTIONED IN DOCUMENTATION
    attrCatEntry.rootBlock = rootBlock;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

    RelCatEntry relCatEntry;

    // load the relation catalog entry into relCatEntry
    // using RelCacheTable::getRelCatEntry().

    ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    if(ret != SUCCESS)
        return ret;

    int block = relCatEntry.firstBlk/* first record block of the relation  */;

    /***** Traverse all the blocks in the relation and insert them one
           by one into the B+ Tree *****/
    while (block != -1) {

        // declare a RecBuffer object for `block` (using appropriate constructor)
        RecBuffer recBuffer = RecBuffer(block);

        unsigned char slotMap[relCatEntry.numSlotsPerBlk];

        // load the slot map into slotMap using RecBuffer::getSlotMap().
        recBuffer.getSlotMap(slotMap);
        // for every occupied slot of the block
        for(int slotNum = 0; slotNum < relCatEntry.numSlotsPerBlk; slotNum++)
        {
            if(slotMap[slotNum] == SLOT_UNOCCUPIED) continue;
            Attribute record[relCatEntry.numAttrs];
            // load the record corresponding to the slot into `record`
            // using RecBuffer::getRecord().
            recBuffer.getRecord(record, slotNum);
            // declare recId and store the rec-id of this record in it
            RecId recId{block, slotNum};

            // insert the attribute value corresponding to attrName from the record
            // into the B+ tree using bPlusInsert.
            // (note that bPlusInsert will destroy any existing bplus tree if
            // insert fails i.e when disk is full)
            int retVal = bPlusInsert(relId, attrName, record[attrCatEntry.offset], recId);

            if (retVal == E_DISKFULL) {
                // (unable to get enough blocks to build the B+ Tree.)
                return E_DISKFULL;
            }
        }

        // get the header of the block using BlockBuffer::getHeader()
        HeadInfo headInfo; 
        int ret = recBuffer.getHeader(&headInfo);
        if(ret != SUCCESS)
            return ret;
        // set block = rblock of current block (from the header)
        block = headInfo.rblock;
    }

    return SUCCESS;
}

int BPlusTree::bPlusDestroy(int rootBlockNum) {
    if (rootBlockNum < 0 || rootBlockNum >= DISK_BLOCKS/*rootBlockNum lies outside the valid range [0,DISK_BLOCKS-1]*/) {
        return E_OUTOFBOUND;
    }

    int type = StaticBuffer::getStaticBlockType(rootBlockNum)/* type of block (using StaticBuffer::getStaticBlockType())*/;

    if (type == IND_LEAF) {
        // declare an instance of IndLeaf for rootBlockNum using appropriate
        // constructor
        IndLeaf indLeaf = IndLeaf(rootBlockNum);

        // release the block using BlockBuffer::releaseBlock().
        indLeaf.releaseBlock();
        return SUCCESS;

    } else if (type == IND_INTERNAL) {
        // declare an instance of IndInternal for rootBlockNum using appropriate
        // constructor
        IndInternal indInternal = IndInternal(rootBlockNum);
        // load the header of the block using BlockBuffer::getHeader().
        HeadInfo headInfo;
        indInternal.getHeader(&headInfo);
    
        InternalEntry internalEntry;
        indInternal.getEntry(&internalEntry, 0);
        BPlusTree::bPlusDestroy(internalEntry.lChild);

        for(int entry = 0; entry < headInfo.numEntries; entry++){
            internalEntry;
            indInternal.getEntry(&internalEntry, entry);
            int ret = BPlusTree::bPlusDestroy(internalEntry.rChild);
            if(ret != SUCCESS)
                return ret;
        }
        /*iterate through all the entries of the internalBlk and destroy the lChild
        of the first entry and rChild of all entries using BPlusTree::bPlusDestroy().
        (the rchild of an entry is the same as the lchild of the next entry.
         take care not to delete overlapping children more than once ) */

        // release the block using BlockBuffer::releaseBlock().
        indInternal.releaseBlock();
        return SUCCESS;

    } else {
        // (block is not an index block.)
        return E_INVALIDBLOCK;
    }
}

RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op){
    
    // declare searchIndex which will be used to store search index for attrName.
    IndexId searchIndex;

    /* get the search index corresponding to attribute with name attrName
       using AttrCacheTable::getSearchIndex(). */
    int res = AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);

    if(res != SUCCESS)
        return {-1, -1};
    AttrCatEntry attrCatEntry;
    /* load the attribute cache entry into attrCatEntry using
     AttrCacheTable::getAttrCatEntry(). */

    res = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    int attrType = attrCatEntry.attrType;

    // declare variables block and index which will be used during search
    int block, index;

    if (searchIndex.block == -1 && searchIndex.index == -1) {
        // (search is done for the first time)

        // start the search from the first entry of root.
        block = attrCatEntry.rootBlock;
        index = 0;

        if (block == -1) {
            return RecId{-1, -1};
        }

    } else {
        /*a valid searchIndex points to an entry in the leaf index of the attribute's
        B+ Tree which had previously satisfied the op for the given attrVal.*/

        block = searchIndex.block;
        index = searchIndex.index + 1;  // search is resumed from the next index.

        // load block into leaf using IndLeaf::IndLeaf().
        IndLeaf leaf(block);

        // declare leafHead which will be used to hold the header of leaf.
        HeadInfo leafHead;

        // load header into leafHead using BlockBuffer::getHeader().
        res = leaf.getHeader(&leafHead);

        if(res != SUCCESS)
            return {-1, -1};

        if (index >= leafHead.numEntries) {
            /* (all the entries in the block has been searched; search from the
            beginning of the next leaf index block. */

            // update block to rblock of current block and index to 0.
            block = leafHead.rblock;
            index = 0;

            if (block == -1) {
                // (end of linked list reached - the search is done.)
                return RecId{-1, -1};
            }
        }
    }

    /******  Traverse through all the internal nodes according to value
             of attrVal and the operator op                             ******/

    /* (This section is only needed when
        - search restarts from the root block (when searchIndex is reset by caller)
        - root is not a leaf
        If there was a valid search index, then we are already at a leaf block
        and the test condition in the following loop will fail)
    */

    while(StaticBuffer::getStaticBlockType(block) == IND_INTERNAL) {  //use StaticBuffer::getStaticBlockType()
        
        // load the block into internalBlk using IndInternal::IndInternal().
        IndInternal internalBlk(block);

        HeadInfo intHead;

        // load the header of internalBlk into intHead using BlockBuffer::getHeader()
        internalBlk.getHeader(&intHead);

        // declare intEntry which will be used to store an entry of internalBlk.
        InternalEntry intEntry;
        


        if (op == NE || op == LT || op == LE) {
            /*
            - NE: need to search the entire linked list of leaf indices of the B+ Tree,
            starting from the leftmost leaf index. Thus, always move to the left.

            - LT and LE: the attribute values are arranged in ascending order in the
            leaf indices of the B+ Tree. Values that satisfy these conditions, if
            any exist, will always be found in the left-most leaf index. Thus,
            always move to the left.
            */

            // load entry in the first slot of the block into intEntry
            // using IndInternal::getEntry().
            internalBlk.getEntry(&intEntry, 0);
            BplusTreeComparisons++;
            block = intEntry.lChild;

        } else {
            /*
            - EQ, GT and GE: move to the left child of the first entry that is
            greater than (or equal to) attrVal
            (we are trying to find the first entry that satisfies the condition.
            since the values are in ascending order we move to the left child which
            might contain more entries that satisfy the condition)
            */

            /*
             traverse through all entries of internalBlk and find an entry that
             satisfies the condition.
             if op == EQ or GE, then intEntry.attrVal >= attrVal
             if op == GT, then intEntry.attrVal > attrVal
             Hint: the helper function compareAttrs() can be used for comparing
            */
           index = 0;
           bool entryFound = false;
           while(index < intHead.numEntries){
            int ret = internalBlk.getEntry(&intEntry, index);
            if(ret != SUCCESS)
                break;
            int cmpVal = compareAttrs(intEntry.attrVal, attrVal, attrType);
            BplusTreeComparisons++;
            if (
                    (op == EQ && cmpVal == 0) ||
                    (op == GE && cmpVal >= 0) ||
                    (op == GT && cmpVal > 0)
                ){
                    entryFound = true;
                    break;
                }
            index++;
           }
           //modified in stage 10
            if (entryFound) {
                // move to the left child of that entry
                block =  intEntry.lChild;// left child of the entry

            } else {
                // move to the right child of the last entry of the block
                // i.e numEntries - 1 th entry of the block

                block =  intEntry.rChild;// right child of last entry
            }
        }
    }

    // NOTE: `block` now has the block number of a leaf index block.

    /******  Identify the first leaf index entry from the current position
                that satisfies our condition (moving right)             ******/
    
    while (block != -1) {
        // load the block into leafBlk using IndLeaf::IndLeaf().
        IndLeaf leafBlk(block);
        HeadInfo leafHead;

        // load the header to leafHead using BlockBuffer::getHeader().
        leafBlk.getHeader(&leafHead);

        // declare leafEntry which will be used to store an entry from leafBlk
        Index leafEntry;

        while (index < leafHead.numEntries /*index < numEntries in leafBlk*/) {
            // load entry corresponding to block and index into leafEntry
            // using IndLeaf::getEntry().
            leafBlk.getEntry(&leafEntry, index);

            int cmpVal = compareAttrs(leafEntry.attrVal, attrVal, attrType); /* comparison between leafEntry's attribute value
                            and input attrVal using compareAttrs()*/
            BplusTreeComparisons++;
            if (
                (op == EQ && cmpVal == 0) ||
                (op == LE && cmpVal <= 0) ||
                (op == LT && cmpVal < 0) ||
                (op == GT && cmpVal > 0) ||
                (op == GE && cmpVal >= 0) ||
                (op == NE && cmpVal != 0)
            ) {
                // (entry satisfying the condition found)
                // set search index to {block, index}
                searchIndex.block = block;
                searchIndex.index = index;

                AttrCacheTable::setSearchIndex(relId, attrName, &searchIndex);

                return {leafEntry.block, leafEntry.slot};

                // return the recId {leafEntry.block, leafEntry.slot}.

            } else if ((op == EQ || op == LE || op == LT) && cmpVal > 0) {
                /*future entries will not satisfy EQ, LE, LT since the values
                    are arranged in ascending order in the leaves */
                return RecId {-1, -1};
            }

            // search next index.
            ++index;
        }

        /*only for NE operation do we have to check the entire linked list;
        for all the other op it is guaranteed that the block being searched
        will have an entry, if it exists, satisying that op. */
        if (op != NE) {
            break;
        }

        // block = next block in the linked list, i.e., the rblock in leafHead.
        // update index to 0.
        block = leafHead.rblock;
        index = 0;
    }

    // no entry satisying the op was found; return the recId {-1,-1}
    return {-1, -1};
}

int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], Attribute attrVal, RecId recId) {
    // get the attribute cache entry corresponding to attrName
    // using AttrCacheTable::getAttrCatEntry().
    AttrCatEntry attrCatEntry;

    int res = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    // if getAttrCatEntry() failed
    //     return the error code

    if(res != SUCCESS)
        return res;

    int rootblockNum = attrCatEntry.rootBlock/* rootBlock of B+ Tree (from attrCatEntry) */;

    if (rootblockNum == -1/* there is no index on attribute (rootBlock is -1) */) {
        return E_NOINDEX;
    }

    // find the leaf block to which insertion is to be done using the
    // findLeafToInsert() function

    int leafBlkNum = BPlusTree::findLeafToInsert(rootblockNum, attrVal, attrCatEntry.attrType)/* findLeafToInsert(root block num, attrVal, attribute type) */;

    // insert the attrVal and recId to the leaf block at blockNum using the
    // insertIntoLeaf() function.

    // declare a struct Index with attrVal = attrVal, block = recId.block and
    // slot = recId.slot to pass as argument to the function.
    // insertIntoLeaf(relId, attrName, leafBlkNum, Index entry)
    
    // NOTE: the insertIntoLeaf() function will propagate the insertion to the
    //       required internal nodes by calling the required helper functions
    //       like insertIntoInternal() or createNewRoot()

    
    Index index;
    index.attrVal = attrVal;
    index.block = recId.block;
    index.slot = recId.slot;
    int ret = BPlusTree::insertIntoLeaf(relId, attrName, leafBlkNum, index);


    if (ret == E_DISKFULL/*insertIntoLeaf() returns E_DISKFULL */) {
        // destroy the existing B+ tree by passing the rootBlock to bPlusDestroy().
        BPlusTree::bPlusDestroy(rootblockNum);
        // update the rootBlock of attribute catalog cache entry to -1 using
        // AttrCacheTable::setAttrCatEntry().
        attrCatEntry.rootBlock = -1;
        AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);
        return E_DISKFULL;
    }

    return SUCCESS;
}

int BPlusTree::findLeafToInsert(int rootBlock, Attribute attrVal, int attrType) {
    int blockNum = rootBlock;

    while (StaticBuffer::getStaticBlockType(blockNum) == IND_INTERNAL/*block is not of type IND_LEAF */) {  // use StaticBuffer::getStaticBlockType()

        // declare an IndInternal object for block using appropriate constructor
        IndInternal  indInternal(blockNum);
        // get header of the block using BlockBuffer::getHeader()
        HeadInfo headInfo;
        indInternal.getHeader(&headInfo);
        /* iterate through all the entries, to find the first entry whose
             attribute value >= value to be inserted.
             NOTE: the helper function compareAttrs() declared in BlockBuffer.h
                   can be used to compare two Attribute values. */
        int index = 0;
        for(; index < headInfo.numEntries; index++){
            InternalEntry internalEntry;
            indInternal.getEntry(&internalEntry, index);
            int cmp = compareAttrs(internalEntry.attrVal, attrVal, attrType);
            if(cmp >= 0){
                break;
            }
        }

        if (index == headInfo.numEntries/*no such entry is found*/) {
            // set blockNum = rChild of (nEntries-1)'th entry of the block
            // (i.e. rightmost child of the block)  
            InternalEntry internalEntry;
            indInternal.getEntry(&internalEntry, headInfo.numEntries - 1);
            blockNum = internalEntry.rChild;

        } else {
            InternalEntry internalEntry;
            indInternal.getEntry(&internalEntry, index);
            blockNum = internalEntry.lChild;
            // set blockNum = lChild of the entry that was found
        }
    }

    return blockNum;
}

int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE], int blockNum, Index indexEntry) {
    // get the attribute cache entry corresponding to attrName
    // using AttrCacheTable::getAttrCatEntry().
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    // declare an IndLeaf instance for the block using appropriate constructor
    IndLeaf indLeaf = IndLeaf(blockNum);
    HeadInfo blockHeader;
    // store the header of the leaf index block into blockHeader
    // using BlockBuffer::getHeader()
    indLeaf.getHeader(&blockHeader);
    // the following variable will be used to store a list of index entries with
    // existing indices + the new index to insert
    Index indices[blockHeader.numEntries + 1];
    bool notInserted = true;
    for(int i = 0; i < blockHeader.numEntries; i++){
        Index leafEntry;
        indLeaf.getEntry(&leafEntry, i);
        int cmp = compareAttrs(leafEntry.attrVal, indexEntry.attrVal, attrCatEntry.attrType);
        indices[i] = leafEntry;
        if(cmp <= 0 && notInserted){
            notInserted = false;
            i++;
            indices[i] = indexEntry;
        }
    }
    if(notInserted){
        indices[blockHeader.numEntries] = indexEntry;
    }
    /*
    Iterate through all the entries in the block and copy them to the array indices.
    Also insert `indexEntry` at appropriate position in the indices array maintaining
    the ascending order.
    - use IndLeaf::getEntry() to get the entry
    - use compareAttrs() declared in BlockBuffer.h to compare two Attribute structs
    */

    if (blockHeader.numEntries != MAX_KEYS_LEAF) {
        // (leaf block has not reached max limit)

        // increment blockHeader.numEntries and update the header of block
        // using BlockBuffer::setHeader().
        blockHeader.numEntries++;
        indLeaf.setHeader(&blockHeader);
        // iterate through all the entries of the array `indices` and populate the
        // entries of block with them using IndLeaf::setEntry().
        for(int i = 0; i < blockHeader.numEntries; i++){
            indLeaf.setEntry(&indices[i], i);
        }
        return SUCCESS;
    }

    // If we reached here, the `indices` array has more than entries than can fit
    // in a single leaf index block. Therefore, we will need to split the entries
    // in `indices` between two leaf blocks. We do this using the splitLeaf() function.
    // This function will return the blockNum of the newly allocated block or
    // E_DISKFULL if there are no more blocks to be allocated.

    int newRightBlk = splitLeaf(blockNum, indices);

    if (newRightBlk == E_DISKFULL) 
        return E_DISKFULL;

    if (blockHeader.pblock != -1 /* the current leaf block was not the root */) {  // check pblock in header
        // insert the middle value from `indices` into the parent block using the
        // insertIntoInternal() function. (i.e the last value of the left block)
        // IndInternal parentBlk = IndInternal(blockHeader.pblock);
        InternalEntry internalEntry;
        internalEntry.attrVal = indices[MIDDLE_INDEX_LEAF].attrVal;
        internalEntry.lChild = blockNum;
        internalEntry.rChild = newRightBlk;
        // the middle value will be at index 31 (given by constant MIDDLE_INDEX_LEAF)

        // create a struct InternalEntry with attrVal = indices[MIDDLE_INDEX_LEAF].attrVal,
        // lChild = currentBlock, rChild = newRightBlk and pass it as argument to
        // the insertIntoInternalFunction as follows


        // insertIntoInternal(relId, attrName, parent of current block, new internal entry)        
        int res = BPlusTree::insertIntoInternal(relId, attrName, blockHeader.pblock, internalEntry);
        if(res != SUCCESS)
            return res;

    } else {
        // the current block was the root block and is now split. a new internal index
        // block needs to be allocated and made the root of the tree.
        // To do this, call the createNewRoot() function with the following arguments

        int res = createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal, blockNum, newRightBlk);
        if(res != SUCCESS)
            return res;
    }

    // if either of the above calls returned an error (E_DISKFULL), then return that
    // else return SUCCESS

    return SUCCESS;
}

int BPlusTree::insertIntoInternal(int relId, char attrName[ATTR_SIZE], int intBlockNum, InternalEntry intEntry) {
    // get the attribute cache entry corresponding to attrName
    // using AttrCacheTable::getAttrCatEntry().
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    // declare intBlk, an instance of IndInternal using constructor 2 for the block
    // corresponding to intBlockNum
    IndInternal intBlk = IndInternal(intBlockNum);

    HeadInfo blockHeader;
    // load blockHeader with header of intBlk using BlockBuffer::getHeader().
    intBlk.getHeader(&blockHeader);

    // declare internalEntries to store all existing entries + the new entry
    InternalEntry internalEntries[blockHeader.numEntries + 1];

    /*
    Iterate through all the entries in the block and copy them to the array
    `internalEntries`. Insert `indexEntry` at appropriate position in the
    array maintaining the ascending order.
        - use IndInternal::getEntry() to get the entry
        - use compareAttrs() to compare two structs of type Attribute

    Update the lChild of the internalEntry immediately following the newly added
    entry to the rChild of the newly added entry.
    */
   bool notInserted = true;
   for(int index = 0; index < blockHeader.numEntries; index++){
        InternalEntry internalEntry;
        intBlk.getEntry(&internalEntry, index);
        int cmp = compareAttrs(internalEntries->attrVal, intEntry.attrVal, attrCatEntry.attrType);
        internalEntries[index] = internalEntry;
        if(cmp <= 0  && notInserted){
            notInserted = false;
            index++;
            internalEntries[index] = intEntry;
            index++;
            intBlk.getEntry(&internalEntry, index);
            internalEntry.lChild = intEntry.rChild;
            internalEntries[index] = internalEntry;
        } 
   }
   if(notInserted){
      internalEntries[blockHeader.numEntries] = intEntry;
    }


    if (blockHeader.numEntries != MAX_KEYS_INTERNAL) {
        // (internal index block has not reached max limit)

        // increment blockheader.numEntries and update the header of intBlk
        // using BlockBuffer::setHeader().
        blockHeader.numEntries++;
        intBlk.setHeader(&blockHeader);
        // iterate through all entries in internalEntries array and populate the
        // entries of intBlk with them using IndInternal::setEntry().
        for(int i = 0; i < blockHeader.numEntries; i++){
            intBlk.setEntry(&internalEntries[i], i);
        }
        return SUCCESS;
    }

    // If we reached here, the `internalEntries` array has more than entries than
    // can fit in a single internal index block. Therefore, we will need to split
    // the entries in `internalEntries` between two internal index blocks. We do
    // this using the splitInternal() function.
    // This function will return the blockNum of the newly allocated block or
    // E_DISKFULL if there are no more blocks to be allocated.

    int newRightBlk = splitInternal(intBlockNum, internalEntries);

    if (newRightBlk == E_DISKFULL/* splitInternal() returned E_DISKFULL */) {
        BPlusTree::bPlusDestroy(intEntry.rChild);
        // Using bPlusDestroy(), destroy the right subtree, rooted at intEntry.rChild.
        // This corresponds to the tree built up till now that has not yet been
        // connected to the existing B+ Tree
        return E_DISKFULL;
    }

    if (blockHeader.pblock != -1/* the current block was not the root */) {  // (check pblock in header)
        // insert the middle value from `internalEntries` into the parent block
        // using the insertIntoInternal() function (recursively).
        BPlusTree::insertIntoInternal(relId, attrName, blockHeader.pblock, internalEntries[MIDDLE_INDEX_INTERNAL]);
        // the middle value will be at index 50 (given by constant MIDDLE_INDEX_INTERNAL)

        // create a struct InternalEntry with lChild = current block, rChild = newRightBlk
        // and attrVal = internalEntries[MIDDLE_INDEX_INTERNAL].attrVal
        // and pass it as argument to the insertIntoInternalFunction as follows
        InternalEntry internalEntry;
        internalEntry.attrVal = internalEntries[MIDDLE_INDEX_INTERNAL].attrVal;
        internalEntry.lChild = intBlockNum;
        internalEntry.rChild = newRightBlk;
        
        int ret = BPlusTree::insertIntoInternal(relId, attrName, blockHeader.pblock, internalEntry);
        // insertIntoInternal(relId, attrName, parent of current block, new internal entry)
        if(ret != SUCCESS)
            return ret;
    } else {
        // the current block was the root block and is now split. a new internal index
        // block needs to be allocated and made the root of the tree.
        // To do this, call the createNewRoot() function with the following arguments
        int ret = BPlusTree::createNewRoot(relId, attrName, internalEntries[MIDDLE_INDEX_INTERNAL].attrVal, intBlockNum, newRightBlk);
        // createNewRoot(relId, attrName,
        //               internalEntries[MIDDLE_INDEX_INTERNAL].attrVal,
        //               current block, new right block)
        if(ret != SUCCESS)
            return ret;
    }

    // if either of the above calls returned an error (E_DISKFULL), then return that
    // else return SUCCESS
    return SUCCESS;
}

//modified in stage11
int BPlusTree::splitInternal(int intBlockNum, InternalEntry internalEntries[]) {
    // declare rightBlk, an instance of IndInternal using constructor 1 to obtain new
    // internal index block that will be used as the right block in the splitting
    IndInternal rightBlk = IndInternal();

    // declare leftBlk, an instance of IndInternal using constructor 2 to read from
    // the existing internal index block
    IndInternal leftBlk = IndInternal(intBlockNum);

    int rightBlkNum = rightBlk.getBlockNum()/* block num of right blk */;
    int leftBlkNum = leftBlk.getBlockNum()/* block num of left blk */;

    if (rightBlkNum == E_DISKFULL/* newly allocated block has blockNum E_DISKFULL */) {
        //(failed to obtain a new internal index block because the disk is full)
        return E_DISKFULL;
    }

    HeadInfo leftBlkHeader, rightBlkHeader;
    // get the headers of left block and right block using BlockBuffer::getHeader()
    rightBlk.getHeader(&rightBlkHeader);
    leftBlk.getHeader(&leftBlkHeader);
    // set rightBlkHeader with the following values
    // - number of entries = (MAX_KEYS_INTERNAL)/2 = 50
    // - pblock = pblock of leftBlk
    // and update the header of rightBlk using BlockBuffer::setHeader()
    rightBlkHeader.pblock = leftBlkHeader.pblock;
    rightBlkHeader.numEntries = MAX_KEYS_INTERNAL/2;
    rightBlk.setHeader(&rightBlkHeader);

    // set leftBlkHeader with the following values
    // - number of entries = (MAX_KEYS_INTERNAL)/2 = 50
    // and update the header using BlockBuffer::setHeader()
    leftBlkHeader.numEntries = MAX_KEYS_INTERNAL/2;
    /*
    - set the first 50 entries of leftBlk = index 0 to 49 of internalEntries
      array
    - set the first 50 entries of newRightBlk = entries from index 51 to 100
      of internalEntries array using IndInternal::setEntry().
      (index 50 will be moving to the parent internal index block)
    */
   for(int i = 0; i < 50; i++){
        leftBlk.setEntry(&internalEntries[i], i);
   }
   for(int i = 51; i <= 100; i++){
        rightBlk.setEntry(&internalEntries[i], i - 51);
   }

    int type = StaticBuffer::getStaticBlockType(internalEntries[0].lChild)/* block type of a child of any entry of the internalEntries array */;
    //            (use StaticBuffer::getStaticBlockType())

    for (int i = 51; i <= 100; i++/* each child block of the new right block */) {
        // declare an instance of BlockBuffer to access the child block using
        // constructor 2
        if(type == IND_INTERNAL){
            IndInternal indInternal = IndInternal(internalEntries[i].rChild);
            HeadInfo headInfo;
            indInternal.getHeader(&headInfo);
            headInfo.pblock = rightBlkNum;
            indInternal.setHeader(&headInfo);
        }else if(type == IND_LEAF){
            IndLeaf indLeaf = IndLeaf(internalEntries[i].rChild);
            HeadInfo headInfo;
            indLeaf.getHeader(&headInfo);
            headInfo.pblock = rightBlkNum;
            indLeaf.setHeader(&headInfo);
        }
        // update pblock of the block to rightBlkNum using BlockBuffer::getHeader()
        // and BlockBuffer::setHeader().
    }
    return rightBlkNum;
}

int BPlusTree::createNewRoot(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int lChild, int rChild) {
    // get the attribute cache entry corresponding to attrName
    // using AttrCacheTable::getAttrCatEntry().
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    // declare newRootBlk, an instance of IndInternal using appropriate constructor
    // to allocate a new internal index block on the disk

    IndInternal indInternal;

    int newRootBlkNum = indInternal.getBlockNum()/* block number of newRootBlk */;

    if (newRootBlkNum == E_DISKFULL) {
        // (failed to obtain an empty internal index block because the disk is full)
        BPlusTree::bPlusDestroy(rChild);
        // Using bPlusDestroy(), destroy the right subtree, rooted at rChild.
        // This corresponds to the tree built up till now that has not yet been
        // connected to the existing B+ Tree

        return E_DISKFULL;
    }

    // update the header of the new block with numEntries = 1 using
    // BlockBuffer::getHeader() and BlockBuffer::setHeader()
    HeadInfo headInfo;
    indInternal.getHeader(&headInfo);
    headInfo.numEntries = 1;
    // create a struct InternalEntry with lChild, attrVal and rChild from the
    // arguments and set it as the first entry in newRootBlk using IndInternal::setEntry()

    InternalEntry internalEntry;
    internalEntry.attrVal = attrVal;
    internalEntry.lChild = lChild;
    internalEntry.rChild = rChild;

    indInternal.setEntry(&internalEntry, 0);
    // declare BlockBuffer instances for the `lChild` and `rChild` blocks using
    BlockBuffer leftChildBlock = BlockBuffer(lChild);
    BlockBuffer rightChildBlock = BlockBuffer(rChild);

    HeadInfo leftHead, rightHead;
    leftChildBlock.getHeader(&leftHead);
    rightChildBlock.getHeader(&rightHead);

    leftHead.pblock = newRootBlkNum;
    rightHead.pblock = newRootBlkNum;
    
    leftChildBlock.setHeader(&leftHead);
    rightChildBlock.setHeader(&rightHead);
    // appropriate constructor and update the pblock of those blocks to `newRootBlkNum`
    // using BlockBuffer::getHeader() and BlockBuffer::setHeader()

    // update rootBlock = newRootBlkNum for the entry corresponding to `attrName`
    // in the attribute cache using AttrCacheTable::setAttrCatEntry().
    attrCatEntry.rootBlock = newRootBlkNum;
    int ret = AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);
    if(ret != SUCCESS)
        return ret;
    return SUCCESS;
}


int BPlusTree::splitLeaf(int leafBlockNum, Index indices[]) {
    // declare rightBlk, an instance of IndLeaf using constructor 1 to obtain new
    // leaf index block that will be used as the right block in the splitting
    IndLeaf rightBlk = IndLeaf();
    // declare leftBlk, an instance of IndLeaf using constructor 2 to read from
    // the existing leaf block

    IndLeaf leftBlk = IndLeaf(leafBlockNum);

    int rightBlkNum = rightBlk.getBlockNum()/* block num of right blk */;
    int leftBlkNum = leftBlk.getBlockNum()/* block num of left blk */;

    if (rightBlkNum == E_DISKFULL || leftBlkNum == E_DISKFULL/* newly allocated block has blockNum E_DISKFULL */) {
        //(failed to obtain a new leaf index block because the disk is full)
        return E_DISKFULL;
    }

    HeadInfo leftBlkHeader, rightBlkHeader;
    // get the headers of left block and right block using BlockBuffer::getHeader()

    leftBlk.getHeader(&leftBlkHeader);
    rightBlk.getHeader(&rightBlkHeader);

    // set rightBlkHeader with the following values
    // - number of entries = (MAX_KEYS_LEAF+1)/2 = 32,
    // - pblock = pblock of leftBlk
    // - lblock = leftBlkNum
    // - rblock = rblock of leftBlk
    // and update the header of rightBlk using BlockBuffer::setHeader()

    rightBlkHeader.numEntries = (MAX_KEYS_LEAF + 1)/2;
    rightBlkHeader.pblock = leftBlkHeader.pblock;
    rightBlkHeader.lblock = leftBlkNum;
    rightBlkHeader.rblock = leftBlkHeader.rblock;

    rightBlk.setHeader(&rightBlkHeader);

    // set leftBlkHeader with the following values
    // - number of entries = (MAX_KEYS_LEAF+1)/2 = 32
    // - rblock = rightBlkNum
    // and update the header of leftBlk using BlockBuffer::setHeader() */

    leftBlkHeader.numEntries = (MAX_KEYS_LEAF + 1)/2;
    leftBlkHeader.rblock = rightBlkNum;

    leftBlk.setHeader(&leftBlkHeader);
    // set the first 32 entries of leftBlk = the first 32 entries of indices array
    // and set the first 32 entries of newRightBlk = the next 32 entries of
    // indices array using IndLeaf::setEntry().
    for(int i = 0; i < 32; i++){
       int res = leftBlk.setEntry(&indices[i], i);
        if(res != SUCCESS)
            return res;
    }
    for(int i = 0; i < 32; i++){
        int res = rightBlk.setEntry(&indices[i + 32], i);
        if(res != SUCCESS)
            return res;
    }


    return rightBlkNum;
}






