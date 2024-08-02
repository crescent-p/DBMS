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

  unsigned char buffer1[BLOCK_SIZE];
  Disk::readBlock(buffer1, 7000);
  unsigned char message2[6];
  memcpy(message2, buffer1 + 20, 6);
  std::cout<<message2<<std::endl;
  // std::cout<<buffer<<std::endl;

  // StaticBuffer buffer;
  // OpenRelTable cache;

  return 0;
  // FrontendInterface::handleFrontend(argc, argv);
}
// int main(int argc, char *argv[]) {
//   Disk disk_run;

//   unsigned char buffer[BLOCK_SIZE];
//   Disk::readBlock(buffer, 7000);
//   char message[] = "hello";
//   memcpy(buffer + 20, message, 6);
//   Disk::writeBlock(buffer, 7000);

//   unsigned char buffer2[BLOCK_SIZE];
//   char message2[6];
//   Disk::readBlock(buffer2, 7000);
//   memcpy(message2, buffer2 + 20, 6);
//   std::cout << message2;

//   return 0;
// }