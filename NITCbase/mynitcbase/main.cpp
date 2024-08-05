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

  for(int i = 0; i < 2; i++){
    RelCatEntry* relCatEntry = new RelCatEntry;
    RelCacheTable::getRelCatEntry(i, relCatEntry); 
    printf("%s\n", relCatEntry->relName);
    for(int j = 0; j < 6; j++){
      AttrCatEntry* attrCatEntry = new AttrCatEntry;
      AttrCacheTable::getAttrCatEntry(i, j, attrCatEntry);
      const char* type = (NUMBER == attrCatEntry->attrType) ? "NUM" : "STR";
      printf(" %s: %s\n", attrCatEntry->attrName, type);
    }
    printf("\n");
  }

  return 0;
  // FrontendInterface::handleFrontend(argc, argv);
}
