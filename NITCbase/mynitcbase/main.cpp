#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"
#include <bits/stdc++.h>

using namespace std;

int main(int argc, char *argv[]) {
  /* Initialize the Run Copy of Disk */
  Disk disk_run;
  StaticBuffer buffer;
  OpenRelTable cache;

   // RecBuffer mybuffer(6);
   // HeadInfo* header = new HeadInfo;
   // mybuffer.getHeader(header);

   // printf("%d", header->numAttrs);

  return FrontendInterface::handleFrontend(argc, argv);
  // FrontendInterface::handleFrontend(argc, argv);
}
