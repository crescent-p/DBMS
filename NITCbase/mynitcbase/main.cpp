#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"
#include <bits/stdc++.h>

using namespace std;

int main(int argc, char *argv[]) {
  /* Initialize the Run Copy of Disk */
  Disk disk_run;

  RecBuffer relCatBuffer(RELCAT_BLOCK);
  RecBuffer attrCatBuffer(ATTRCAT_BLOCK);

  HeadInfo relCatHeader;
  HeadInfo attrCatHeader;

  relCatBuffer.getHeader(&relCatHeader);
  attrCatBuffer.getHeader(&attrCatHeader);
  
  for(int i = 0; i < relCatHeader.numEntries; i++){
      
      Attribute relCatRecord[RELCAT_NO_ATTRS];

      relCatBuffer.getRecord(relCatRecord, i);

      printf("Relation: %s\n", relCatRecord[RELCAT_REL_NAME_INDEX].sVal);
 
      for(int j = 0; j < attrCatHeader.numEntries; j++){

        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

        attrCatBuffer.getRecord(attrCatRecord, i*relCatHeader.numEntries + j);

        if(strcmp(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal) == 0){
          const char *attrType = attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER ? "NUM" : "STR";
          printf("  %s: %s\n", attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrType);
        }
      }
      printf("\n");
  }

  return 0;
  // FrontendInterface::handleFrontend(argc, argv);
}
