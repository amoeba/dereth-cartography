// exc.c
//
// David Simpson
// http://www.ugcs.caltech.edu/~dsimpson
//
// ExC extracts the "file" with the matching id out of CELL.DAT.
// The id must be specified in hexadecimal.
//
// For example,
// exc \progra~1\micros~2\ashero~1\cell.dat 7F7FFFFF
// would extract the file which contains the topography data for the
// landblock at the center of the map.
//
// There are three kinds of files located in CELL.DAT.
//
// Surface topography:  These files contain the main landblock information
// for Dereth.  Their id's are of the form xxyyFFFF.  See mapac.c for more
// information of landblocks and their format.
//
// Surface object information:  These files contain a list of the objects
// located on the surface world.  Their id's are of the form xxyyFFFE.
// The format for these blocks is generally as follows:
//   uint id (xxyyFFFE)
//   uint unknown (perhaps something to do with number of dungeon blocks)
//   uint # of objects
//   Object objects[# of objects]
//   uint # of other objects
//   Object2 object2s[# of other objects]
//
// Objects have the following data format:
//   uint object id
//   float x
//   float y
//   float z
//   float a
//   float b
//   float c
//   float d
// The object id is the id for the file which describes that object in
// PORTAL.DAT.  The triplet (x, y, z) is the translation (position) of the
// specified object within the landblock.  The quartet (a, b, c, d) is the
// unit quaternion a + bi + cj + dk which specifies the rotation of the
// given object.
//
// The first eight words of the other objects in the file start the same
// way as the regular objects, but have a bunch of other information
// afterwards of varying length.  I think it has something to do with
// visibility, but I haven't looked at it in great detail.
//
// Dungeon information:  Each of these files contains information for one
// block in a dungeon:  Their id's are of the form xxyynnnn.  nnnn starts
// at 0x0100 and counts up, one for each block in the dungeon.
// The data format is as follows:
//   uint type
//   uint id
//   uchar # of textures
//   uchar # of connections
//   ushort # of visible dungeon blocks(?)
//   ushort texture ids[# of textures]
//   ushort pad (if # of textures ids is odd)
//   uint dungeon block geometry id
//   float x
//   float y
//   float z
//   float a
//   float b
//   float c
//   float d
//   dword connectivity information[# of connections]
//   ushort visible blocks[# of visible dungeon blocks](?)
//   ushort pad (if # of visible dungeon blocks is off)
//   uint # of objects
//   Object objects[# of objects]
// If bit 1 of type is set, then the dungeon is a surface structure.  If
// bit 2 of type is set, then this dungeon block contains objects.  If
// bit 2 of type is not set, then the # of objects and the object data is
// not present at all in this block!
//
// The texture ids are actually 0x08000000 + texture id.  These ids
// reference texture information files in PORTAL.DAT.
//
// The dungeon block geometry id is actually 0x0D000000 + geometry id.  It
// references the file with this id in PORTAL.DAT.
//
// The triplet (x, y, z) is the translation of this dungeon block while
// the unit quaternion specified by (a, b, c, d) is the rotation.
//
// I've haven't taken the time yet to figure out how the connection data
// works, but I'm pretty sure that it is connection data.
//
// I don't actually know that the next list is the visibility list (and
// drawing order), but I assume that it is.
//
//
//
// The following code does a directory lookup in CELL.DAT for the file
// you are looking for, reads it, and then saves it a file whose name
// is the id of the file you are fetching.  Sorry, I didn't bother to
// comment it.  :)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define uint  unsigned int
#define uchar unsigned char

#define SECSIZE 64
#define NUMFILELOC    0x03F
#define ROOTDIRPTRLOC 0x148

void PrintUsage()
{
  printf("usage: exc <CELL FILE> <ID>\n");
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
  uint dir[4 * SECSIZE];
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
      printf("ERROR: Seek to %08X is beyond end of file!\n", dirPos);
      return 0;
    }

    read = fread(dir, sizeof(uint), SECSIZE, inFile);
    if (read != SECSIZE) {
      printf("ERROR: Sector only contains %d words!\n", read);
      return 0;
    }

    dirPos = dir[0];

    if (dirPos != 0) {
      read = fseek(inFile, dirPos, SEEK_SET);
      if (read != 0) {
        printf("ERROR: Seek to %08X is beyond end of file!\n", dirPos);
        return 0;
      }

      read = fread(&dirPos, sizeof(uint), 1, inFile);
      if (read != 1) {
        printf("ERROR: Sector does not exist!\n", read);
        return 0;
      }

      read = fread(&dir[SECSIZE], sizeof(uint), SECSIZE - 1, inFile);
      if (read != SECSIZE - 1) {
        printf("ERROR: Sector only contains %d words!\n", read + 1);
        return 0;
      }
    }

    if (dirPos != 0) {
      read = fseek(inFile, dirPos, SEEK_SET);
      if (read != 0) {
        printf("ERROR: Seek to %08X is beyond end of file!\n", dirPos);
        return 0;
      }

      read = fread(&dirPos, sizeof(uint), 1, inFile);
      if (read != 1) {
        printf("ERROR: Sector does not exist!\n", read);
        return 0;
      }

      read = fread(&dir[SECSIZE  * 2 - 1], sizeof(uint), SECSIZE - 1, inFile);
      if (read != SECSIZE - 1) {
        printf("ERROR: Sector only contains %d words!\n", read + 1);
        return 0;
      }
    }

    if (dirPos != 0) {
      read = fseek(inFile, dirPos, SEEK_SET);
      if (read != 0) {
        printf("ERROR: Seek to %08X is beyond end of file!\n", dirPos);
        return 0;
      }

      read = fread(&dirPos, sizeof(uint), 1, inFile);
      if (read != 1) {
        printf("ERROR: Sector does not exist!\n", read);
        return 0;
      }

      read = fread(&dir[SECSIZE * 3 - 2], sizeof(uint), SECSIZE - 1, inFile);
      if (read != SECSIZE - 1) {
        printf("ERROR: Sector only contains %d words!\n", read + 1);
        return 0;
      }
    }

    numFiles = dir[NUMFILELOC];
    if (numFiles >= NUMFILELOC) {
      printf("ERROR: Number of files, %d,  exceeds directory entries!\n", numFiles);
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
  char  fileName[16];

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

  id = strtoul(argv[2], NULL, 16);

  if (!FetchFilePos(inFile, rootDirPtr, id, &filePos, &len)) {
		printf("ERROR: File %08X not found!\n", id);
		fclose(inFile);
		return -1;
	}

  buf = (uchar *)malloc(len);
  if (!FetchFile(inFile, filePos, len, buf)) {
    free(buf);
    fclose(inFile);
    return -1;
  }

  sprintf(fileName, "%08X", id);
  outFile = fopen(fileName, "wb");
  if (outFile == NULL) {
    printf("ERROR: File %s failed to open!\n", argv[1]);
		free(buf);
		fclose(inFile);
    return -1;
  }

  fwrite(buf, 1, len, outFile);
  free(buf);
  fclose(outFile);
  fclose(inFile);

  return 0;
}

