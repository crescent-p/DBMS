#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"
#include <bits/stdc++.h>

using namespace std;

int main(int argc, char *argv[]) {
  /* Initialize the Run Copy of Disk */
  StaticBuffer buffer;
  Disk disk_run;
  OpenRelTable cache;



  RelCatEntry* relCatEntry = new RelCatEntry;
  RelCacheTable::getRelCatEntry(0, relCatEntry);

  RecBuffer::BlockBuffer relBlockBuffer(RELCAT_BLOCK);
  RecBuffer::BlockBuffer attrBlockBuffer(ATTRCAT_BLOCK);

  HeadInfo relCatHeadInfo;
  relBlockBuffer.getHeader(&relCatHeadInfo);

  for(int i = 0; i < relCatHeadInfo.numEntries; i++){
    RelCacheTable::getRelCatEntry(i, relCatEntry); 
    printf("\n %s\n Relation: ", relCatEntry->relName);
    HeadInfo attrCatHeadInfo;
    attrBlockBuffer.getHeader(&attrCatHeadInfo);
    for(int j = 0; j < relCatHeadInfo.numAttrs; j++){

      AttrCatEntry* attrCatEntry = new AttrCatEntry;
      AttrCacheTable::getAttrCatEntry(i, j, attrCatEntry);
      if(j == 0){
        printf("%s\n", attrCatEntry->relName);
      }
      if(strcmp(relCatEntry->relName, attrCatEntry->relName) != 0) continue;
      const char* type = (NUMBER == attrCatEntry->attrType) ? "NUM" : "STR";
      printf(" %s: %s\n", attrCatEntry->attrName, type);
    }
    printf("\n");
  }

  return 0;
  // FrontendInterface::handleFrontend(argc, argv);
}
