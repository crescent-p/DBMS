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

  return FrontendInterface::handleFrontend(argc, argv);
  // FrontendInterface::handleFrontend(argc, argv);
}
