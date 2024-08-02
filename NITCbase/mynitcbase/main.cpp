#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"
#include <bits/stdc++.h>

int main(int argc, char *argv[]) {
  /* Initialize the Run Copy of Disk */
  Disk disk_run;

  //creating a string
  unsigned char buffer[BLOCK_SIZE];
  //reading the information at 7000th block.
  Disk::readBlock(buffer, 7000);
  //writing a new message to that block.
  unsigned char message[] = "hello";
  std::memcpy(buffer + 20, message, 6);
  Disk::writeBlock(buffer, 7000);

  std::cout<<buffer<<std::endl;
  // StaticBuffer buffer;
  // OpenRelTable cache;

  return FrontendInterface::handleFrontend(argc, argv);
}