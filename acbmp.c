// acbmp.c
//
// David Simpson
// http://www.ugcs.caltech.edu/~dsimpson
//
// ACBMP seacrhes the directories in PORTAL.DAT for graphic files.
// If it finds one, the graphics data is extracted and then saved
// as a BMP file.  An index is output to stdout listing each BMP
// file and the source(s) for its data.
//
// Texture files have an id of 0x0500nnnn.  Texture files contain
// 8 bit image information.  A list of CLUTs follows the image
// information.  The first CLUT in the list is used to build the
// 24 bit BMP image which is saved.
//
// UI graphics have an id of 0x0600nnnn.  UI graphics are 24 bit
// images.
//
// Running the program will generate about 5600 files.  About one
// third are textures while the rest are UI graphics.  Have fun!

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
  printf("usage: acbmp <PORTAL FILE>\n");
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
  uint  i;
  uchar *buf, *pal;
  uint  *palPtrs;
  uint  imageW, imageH, imageType, imageId;
  uint  fileNum;
  char  fileName[16];
  uchar *image, *r, *g, *b;
  short outShort;
  int   outInt;
  int   x, y;
  uchar color;

  if (argc != 2) {
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

  fileNum = 0;
  for (i = 0; i < 65536; i++) {
    if (FetchFilePos(inFile, rootDirPtr, 0x05000000 | i, &filePos, &len)) {
      buf = (uchar *)malloc(len);
      if (!FetchFile(inFile, filePos, len, buf)) {
        free(buf);
        fclose(inFile);
        return -1;
      }
      palPtrs = (uint *)buf;
      image = buf + 4 * sizeof(uint);

      imageId = palPtrs[0];
      imageType = palPtrs[1];
      imageW = palPtrs[2];
      imageH = palPtrs[3];
      palPtrs += imageH * imageW / sizeof(uint) + 4;

      // imageType 4 is a bump map, I think.  I don't really know its format.
      if (imageType == 2) {

        if (!FetchFilePos(inFile, rootDirPtr, palPtrs[0], &filePos, &len)) {
          printf("ERROR: Palette %08X could not be found!\n", palPtrs[0]);
          free(buf);
          fclose(inFile);
          return -1;
        }
        pal = (uchar *)malloc(len);
        if (!FetchFile(inFile, filePos, len, pal)) {
          free(pal);
          free(buf);
          fclose(inFile);
          return -1;
        }

        sprintf(fileName, "gr%04d.bmp", fileNum);
        outFile = fopen(fileName, "wb");
        if (outFile == NULL) {
          printf("ERROR: File %s did not open!\n", fileName);
          free(pal);
          free(buf);
          fclose(inFile);
          return -1;
        }

        outShort = 19778;
        fwrite(&outShort, sizeof(short), 1, outFile);
        outInt = imageW * imageH * 3 + 54 + (imageW & 3) * imageH;
        fwrite(&outInt, sizeof(int), 1, outFile);
        outShort = 0;
        fwrite(&outShort, sizeof(short), 1, outFile);
        fwrite(&outShort, sizeof(short), 1, outFile);
        outInt = 54;
        fwrite(&outInt, sizeof(int), 1, outFile);
        outInt = 40;
        fwrite(&outInt, sizeof(int), 1, outFile);
        outInt = imageW;
        fwrite(&outInt, sizeof(int), 1, outFile);
        outInt = imageH;
        fwrite(&outInt, sizeof(int), 1, outFile);
        outShort = 1;
        fwrite(&outShort, sizeof(short), 1, outFile);
        outShort = 24;
        fwrite(&outShort, sizeof(short), 1, outFile);
        outInt = 0;
        fwrite(&outInt, sizeof(int), 1, outFile);
        outInt = imageW * imageH * 3 + (imageW & 3) * imageH;
        fwrite(&outInt, sizeof(int), 1, outFile);
        outInt = 0;
        fwrite(&outInt, sizeof(int), 1, outFile);
        outInt = 0;
        fwrite(&outInt, sizeof(int), 1, outFile);
        outInt = 0;
        fwrite(&outInt, sizeof(int), 1, outFile);
        outInt = 0;
        fwrite(&outInt, sizeof(int), 1, outFile);

        for (y = imageH - 1; y >= 0; y--) {
          for (x = 0; x < imageW; x++) {
            b = &pal[image[y * imageW + x] * 4 + 8];
            g = &pal[image[y * imageW + x] * 4 + 9];
            r = &pal[image[y * imageW + x] * 4 + 10];
            fwrite(b, 1, 1, outFile);
            fwrite(g, 1, 1, outFile);
            fwrite(r, 1, 1, outFile);
          }
          for (x = 0; x < (imageW & 3); x++) {
            color = 0;
            fwrite(&color, 1, 1, outFile);
          }
        }
        fclose(outFile);

        printf("%4d %08X %08X %3d %3d\n", fileNum, imageId, palPtrs[0],
            imageW, imageH);
        free(pal);
        free(buf);
        fileNum++;
      }
      else
        free(buf);
    }
  }

  for (i = 0; i < 65536; i++) {
    if (FetchFilePos(inFile, rootDirPtr, 0x06000000 | i, &filePos, &len)) {
      buf = (uchar *)malloc(len);
      if (!FetchFile(inFile, filePos, len, buf)) {
        free(buf);
        fclose(inFile);
        return -1;
      }
      palPtrs = (uint *)buf;
      image = buf + 3 * sizeof(uint);

      imageId = palPtrs[0];
      imageW = palPtrs[1];
      imageH = palPtrs[2];

      sprintf(fileName, "gr%04d.bmp", fileNum);
      outFile = fopen(fileName, "wb");
      if (outFile == NULL) {
        printf("ERROR: File %s did not open!\n", fileName);
        free(buf);
        fclose(inFile);
        return -1;
      }

      outShort = 19778;
      fwrite(&outShort, sizeof(short), 1, outFile);
      outInt = imageW * imageH * 3 + 54 + (imageW & 3) * imageH;
      fwrite(&outInt, sizeof(int), 1, outFile);
      outShort = 0;
      fwrite(&outShort, sizeof(short), 1, outFile);
      fwrite(&outShort, sizeof(short), 1, outFile);
      outInt = 54;
      fwrite(&outInt, sizeof(int), 1, outFile);
      outInt = 40;
      fwrite(&outInt, sizeof(int), 1, outFile);
      outInt = imageW;
      fwrite(&outInt, sizeof(int), 1, outFile);
      outInt = imageH;
      fwrite(&outInt, sizeof(int), 1, outFile);
      outShort = 1;
      fwrite(&outShort, sizeof(short), 1, outFile);
      outShort = 24;
      fwrite(&outShort, sizeof(short), 1, outFile);
      outInt = 0;
      fwrite(&outInt, sizeof(int), 1, outFile);
      outInt = imageW * imageH * 3 + (imageW & 3) * imageH;
      fwrite(&outInt, sizeof(int), 1, outFile);
      outInt = 0;
      fwrite(&outInt, sizeof(int), 1, outFile);
      outInt = 0;
      fwrite(&outInt, sizeof(int), 1, outFile);
      outInt = 0;
      fwrite(&outInt, sizeof(int), 1, outFile);
      outInt = 0;
      fwrite(&outInt, sizeof(int), 1, outFile);

      for (y = imageH - 1; y >= 0; y--) {
        for (x = 0; x < imageW; x++) {
          b = &image[(y * imageW + x) * 3 + 2];
          g = &image[(y * imageW + x) * 3 + 1];
          r = &image[(y * imageW + x) * 3];
          fwrite(b, 1, 1, outFile);
          fwrite(g, 1, 1, outFile);
          fwrite(r, 1, 1, outFile);
        }
        for (x = 0; x < (imageW & 3); x++) {
          color = 0;
          fwrite(&color, 1, 1, outFile);
        }
      }
      fclose(outFile);

      printf("%4d %08X %08X %3d %3d\n", fileNum, imageId, palPtrs[0],
          imageW, imageH);
      free(buf);
      fileNum++;
    }
  }

  fclose(inFile);

  return 0;
}

