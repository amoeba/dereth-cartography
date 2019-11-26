// exp.c
//
// David Simpson
// http://www.ugcs.caltech.edu/~dsimpson
//
// ExP extracts the "file" with the matching id out of PORTAL.DAT.
// The id must be specified in hexadecimal.
//
// This program does essentially the same thing for PORTAL.DAT as
// exc does for CELL.DAT.  Ids are of the form ttnnnnnn, where tt
// is the type of data in that file and nnnnnn is the id number
// of that type of data.  There are currently 22 types of data in
// PORTAL.DAT.  The types of data, without further explanation,
// are as follows:
//
// 01 Simple Objects
// 02 Complex Objects (comprised of simple objects)
// 03 Animations (?)
// 04 CLUTs
// 05 Textures
// 06 UI Graphics
// 08 Texture Information
// 09 Animation strips (?)
// 0A ?
// 0D Dungeon Blocks
// 0E ?
// 0F ? (Lists of CLUTs)
// 10 ?
// 11 ?
// 12 ?
// 13 ?
// 20 ?
// 30 ?
// 31 Help
// 32 ?
// 33 ?
// 34 ?
//
// See acbmp.c for some information for the graphics.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define uint  unsigned int
#define uchar unsigned char

#define SECSIZE 256
#define NUMFILELOC    0x03F
#define ROOTDIRPTRLOC 0x148

void PrintUsage()
{
  printf("usage: exp <PORTAL FILE> <ID>\n");
}

int FetchFile(FILE *inFile, uint filePos, uint len, uchar *buf)
{
  int  read, doChain;
  uint sec[SECSIZE];

  if (filePos == 0) {
    printf("ERROR: NULL file pointer found!\n");
    return 0;
  }

  doChain = 1;
  while (doChain) {

    read = fseek(inFile, filePos, SEEK_SET);
    if (read != 0) {
      printf("ERROR: Seek to %08X failed!\n", filePos);
      return 0;
    }
    read = fread(sec, sizeof(uint), SECSIZE, inFile);
    if (read != SECSIZE) {
      printf("ERROR: Sector is only %d words!\n", read);
      return 0;
     }
    
    filePos = sec[0] & 0x7FFFFFFF;

    if (len > (SECSIZE  - 1) * sizeof(uint)) {
      memcpy(buf, &sec[1], (SECSIZE - 1) * sizeof(uint));
      buf += (SECSIZE - 1) * sizeof(uint);
      len -= (SECSIZE - 1) * sizeof(uint);
    }
    else {
      memcpy(buf, &sec[1], len);
      len = 0;
    }

    if (filePos == 0)
      doChain = 0;
  }

  return 1;
}

int FetchFilePos(FILE *inFile, uint dirPos, uint id, uint *filePos, uint *len)
{
  uint dir[SECSIZE];
  uint i;
  uint numFiles;
  int  read;

  while (1) {
    if (dirPos == 0) {
      printf("ERROR: NULL directory entry found!\n");
      return 0;
    }

    read = fseek(inFile, dirPos, SEEK_SET);
    if (read != 0) {
      printf("ERROR: Seek to %08X is beyond end of file!\n", filePos);
      return 0;
    }

    read = fread(dir, sizeof(uint), SECSIZE, inFile);
    if (read != SECSIZE) {
      printf("ERROR: Sector only contains %d words!\n", read);
      return 0;
    }

    numFiles = dir[NUMFILELOC];
    if (numFiles >= NUMFILELOC) {
      printf("ERROR: Number of files exceeds directory entries!\n");
      return 0;
    }

    i = 0;
    while ((i < numFiles) && (id > dir[i * 3 + NUMFILELOC + 1])) {
      i++;
    }
    if (i < numFiles) {
      if (id == dir[i * 3 + NUMFILELOC + 1]) {
        *filePos = dir[i * 3 + NUMFILELOC + 2];
        *len = dir[i * 3 + NUMFILELOC + 3];
        return 1;
      }
    }

    if (dir[1] == 0) {
      filePos = 0;
      len = 0;
      return 0;
    }
   
    dirPos = dir[i + 1];
  }

  return 0;
}

int main(int argc, char *argv[])
{
  FILE  *inFile, *outFile;
  int   read;
  uint  rootDirPtr;
  uint  filePos, len;
  uchar *buf;
  uint  id;

  if (argc != 3) {
    printf("ERROR: Incorrect number of arguments!\n");
    PrintUsage();
    return -1;
  }

  inFile = fopen(argv[1], "rb");
  if (inFile == NULL) {
    printf("ERROR: File %s failed to open!\n", argv[1]);
    return -1;
  }

  read = fseek(inFile, ROOTDIRPTRLOC, SEEK_SET);
  if (read != 0) {
    printf("ERROR: Seek to %08X is beyond end of file!\n", ROOTDIRPTRLOC);
    fclose(inFile);
    return -1;
  }

  read = fread(&rootDirPtr, sizeof(uint), 1, inFile);
  if (read != 1) {
    printf("ERROR: End of file reached!\n");
    fclose(inFile);
    return 0;
  }

  id = strtol(argv[2], NULL, 16);

  if (!FetchFilePos(inFile, rootDirPtr, id, &filePos, &len)) {
    printf("ERROR: File %08X does not exist!\n", id);
    return -1;
  }

  buf = (uchar *)malloc(len);
  if (!FetchFile(inFile, filePos, len, buf)) {
    free(buf);
    fclose(inFile);
    return -1;
  }

  outFile = fopen(argv[2], "wb");
  if (outFile == NULL) {
    printf("ERROR: File %s failed to open!\n", argv[2]);
    return -1;
  }

  fwrite(buf, 1, len, outFile);
  fclose(outFile);
  free(buf);
  fclose(inFile);

  return 0;
}

