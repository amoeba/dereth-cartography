// mapac.c
//
// David Simpson
// http://www.ugcs.caltech.edu/~dsimpson
//
// MapAC searches the Asheron's Call cell.dat file for landblock data and
// stores the results in the designated map file.
//
// A new map file is created with the NEWMAP argument.  From then on, data
// is read from the map file first and is then overwritten by any new data
// found in cell.dat.  For instance, you may find it useful to first
// generate a map file from the cell.dat located on the AC CD-ROM and
// subsequently overwrite this base data with any in your cell.dat.
//
// mapac NEWMAP my.map
// mapac d:\cell.dat my.map
// mapac \progra~1\micros~1\ashero~1\cell.dat my.map
//
// If you want to get the most data possible from the servers, use
// /render radius 25 (the default is 8). With this, you will get a 51x51
// landblock square from the servers, which is quite large.  This will also
// create a fair amount of lag everytime you cross a landblock boundry, so be
// careful in dangerous areas.  If you want to collect data, I recommend
// setting your render radius to 25, wait for the data to download, reset
// your render radius to like 6 or so, travel (say 51 landblocks away), set
// your render radius to 25 again, collect data, and repeat.  Definitely safer
// this way.
//
// If you want pretty graphics from this map data, see graphac.

// CELL.DAT
//
// Cell.dat, along with portal.dat, contains the majority of the data used in
// Asheron's Call.  Cell.dat is the storage area for Turbine's own file system.
// The file is essentially a collection of sectors (or cells).  Each sector is
// 256 bytes in length.  The first 1KB of the file is different though.  Each
// "file" is made up of multiple sectors, though the sectors are not necessarily
// contiguous.  The "files" are indexed from a directory.  The first word of
// each sector is a pointer to the next sector of that "file."  A NULL pointer
// indicates that this is the last sector in that file.

// The Map
//
// The map in Asheron's Call is 254 by 254 landblocks.  Each landblock contains
// a 9 by 9 grid of data points which makes for an 8 by 8 group of land squares
// in game.  Each landblock has a unique id, which is a word in length, and
// has the format xxyyFFFF.  In game, xx is the east-west position, and yy is the
// north-south position.  Landblock 0000FFFF is located in the southwest corner of
// the map.  Use /loc to find out which landblock you are on.  Each square in a
// landblock is 0.1 wide and tall, making each landblock 0.8 by 0.8.  Although
// each landblock contains 9 by 9 points, the points on the edges are redundant
// with adjacent landblocks.  The entire map is 2041 by 2041 data points, making
// 2040 by 2040 squares.  Lastly, each square is 24.0 by 24.0 units, whatever
// they may be.

// Landblock Data
//
// The topography data for each landblock is stored in just one sector.  It's
// data format is as follows:
//
// uint   Next sector pointer (will be 0)
// uint   Landblock id (xxyyFFFF)
// uint   Object Block Present
// ushort Topo[9][9]
// uchar  Z[9][9]
// uchar  Pad (always 0, I think)
//
// Object Block Present is a flag which signifies that there is an object block
// associated with this landblock.  The id of the object block is xxyyFFFE.
// The Z data is exactly what it sounds like: height information.  In game,
// the height of a point is 2.0 * Z.
// The Topo contains the land type, road information, and what I believe is
// vegetation type and/or density.  It is as follows:
//
// bits
// 0    Road
// 1    Unknown (draws a road though)
// 2-6  Land Type
// 7    Unknown (may be more land type, always 0 though)
// 8-15 Vegetation
//
// Roads always have an underlying land type.  Bit 1 is very rare in the data,
// but in tests, just draws a road like bit 0.  Currently, most of the land
// types are used, except those of 0x60, 0x70, 0x74, 0x78, and 0x7C which are
// not currently found in game.  Recently, 0x6C was added and 0x68 was changed.
// I don't know much about the vegetation bits other than that is what they
// seem to be.  Go ahead, play with them.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define uchar  unsigned char
#define ushort unsigned short
#define uint   unsigned int
#define ulong  unsigned long

#define LANDSIZE       2041
#define CELLSECSIZE      64
#define NUMFILELOC    0x03F
#define ROOTDIRPTRLOC 0x148

typedef struct {
  ushort type;
  uchar  z;
  uchar  used;
} landData;

landData land[LANDSIZE][LANDSIZE];

void PrintUsage()
{
  printf("usgae:\n");
  printf("mapac <CELL DATA FILE> <MAP FILE>\n");
  printf("mapac NEWMAP <MAP FILE>\n");
  printf("   WARNING: Argument NEWMAP creates a new map, erasing all previous data!\n");
}

void writeLandData(uchar *sec, uint blockX, uint blockY)
{
  uint   startX, startY;
  uint   x, y;
  ushort *typeData;
  uchar  *zData;
  ushort oldType, newType;
  uchar  oldZ, newZ;

  startX = blockX * 8;
  startY = LANDSIZE - blockY * 8 - 1;
  typeData = (ushort *)(&sec[12]);
  zData = &sec[174];

  for (x = 0; x < 9; x++) {
    for (y = 0; y < 9; y++) {
      oldType = land[startY - y][startX + x].type;
      oldZ = land[startY - y][startX + x].z;
      newType = typeData[x * 9 + y];
      newZ = zData[x * 9 + y];

      // If the new data point is different than the old data point, then tell the user
      if (land[startY - y][startX + x].used && ((oldType != newType) || (oldZ != newZ)))
        printf("(%4d, %4d) was %04X, %3d.  Now %04X, %3d.\n", startX + x, startY - y, oldType, oldZ, newType, newZ);

      // Write new data point
      land[startY - y][startX + x].type = newType;
      land[startY - y][startX + x].z = newZ;
      land[startY - y][startX + x].used = 1;
    }
  }
}

int ReadDir(FILE *inFile, uint dirPos)
{
  uint dir[4 * CELLSECSIZE];
  uint sec[CELLSECSIZE];
  uint numFiles;
  uint i, found;

  // Read the directory
  assert(dirPos != 0);
  assert(fseek(inFile, dirPos, SEEK_SET) == 0);
  assert(fread(dir, sizeof(uint) * CELLSECSIZE, 1, inFile) == 1);

  dirPos = dir[0];

  if (dirPos != 0) {
    assert(fseek(inFile, dirPos, SEEK_SET) == 0);
    assert(fread(&dirPos, sizeof(uint), 1, inFile) == 1);
    assert(fread(&dir[CELLSECSIZE], sizeof(uint) * (CELLSECSIZE - 1), 1, inFile) == 1);
  }
  if (dirPos != 0) {
    assert(fseek(inFile, dirPos, SEEK_SET) == 0);
    assert(fread(&dirPos, sizeof(uint), 1, inFile) == 1);
    assert(fread(&dir[CELLSECSIZE  * 2 - 1], sizeof(uint) * (CELLSECSIZE - 1), 1, inFile) == 1);
  }

  if (dirPos != 0) {
    assert(fseek(inFile, dirPos, SEEK_SET) == 0);
    assert(fread(&dirPos, sizeof(uint), 1, inFile) == 1);
    assert(fread(&dir[CELLSECSIZE * 3 - 2], sizeof(uint) * (CELLSECSIZE - 1), 1, inFile) == 1);
  }

  numFiles = dir[NUMFILELOC];
  assert(numFiles < NUMFILELOC);

  found = 0;
  for (i = 0; i < numFiles; i++) {
    if ((dir[i * 3 + NUMFILELOC + 1] & 0x0000FFFF) == 0x0000FFFF) {
      // File in directory is a landblock, so read it and write it into the land data
      assert(dir[i * 3 + NUMFILELOC + 3] == 252);
      assert((dir[i * 3 + NUMFILELOC + 1] & 0xFF000000) != 0xFF000000);
      assert((dir[i * 3 + NUMFILELOC + 1] & 0x00FF0000) != 0x00FF0000);
      assert(fseek(inFile, dir[i * 3 + NUMFILELOC + 2], SEEK_SET) == 0);
      assert(fread(sec, sizeof(uint) * CELLSECSIZE, 1, inFile) == 1);
      writeLandData((uchar *)sec, sec[1] >> 24, (sec[1] & 0x00FF0000) >> 16);
      found++;
    }
  }

  // If subdirectories exist, recurse into them
  if (dir[1] != 0) {
    for (i = 0; i <= numFiles; i++)
      found += ReadDir(inFile, dir[i + 1]);
  }

  return found;
}

int main(int argc, char *argv[])
{
  FILE *mapFile;
  FILE *cellFile;
  uint cellDirPtr;
  int  found;
  int  x, y;
  int  count[256];

  if (argc != 3) {
    printf("ERROR: Incorrect number of arguments!\n");
    PrintUsage();
    return -1;
  }

  // If the NEWMAP argument is given, write out a new, blank map and exit
  if (!strcmp("NEWMAP", argv[1])) {
    printf("Writing new map\n");
    mapFile = fopen(argv[2], "wb");
    if (mapFile == NULL) {
      printf("ERROR: File %s could not be opened!\n", argv[2]);
      return -1;
    }
    for (y = 0; y < LANDSIZE; y++) {
      for (x = 0; x < LANDSIZE; x++) {
        land[y][x].type = 0;
        land[y][x].z = 0;
        land[y][x].used = 0;
      }
    }
    fwrite(land, sizeof(landData), LANDSIZE * LANDSIZE, mapFile);
    fclose(mapFile);
    return 0;
  }

  // Read old map data
  mapFile = fopen(argv[2], "rb");
  if (mapFile == NULL) {
    printf("ERROR: File %s could not be opened!\n", argv[2]);
    return -1;
  }
  fread(land, sizeof(landData), LANDSIZE * LANDSIZE, mapFile);
  fclose(mapFile);

  // Open CELL.DAT and read pointer to root directory
  cellFile = fopen(argv[1], "rb");
  if (cellFile == NULL) {
    printf("ERROR: File %s could not be opened!\n", argv[1]);
    return -1;
  }
  assert(fseek(cellFile, ROOTDIRPTRLOC, SEEK_SET) == 0);
  assert(fread(&cellDirPtr, sizeof(uint), 1, cellFile) == 1);

  // Read and process sectors until the end of the file is reached
  found = ReadDir(cellFile, cellDirPtr);
  fclose(cellFile);
  printf("Total land blocks found: %d\n", found);

  // Count the number of each land type in the map data, and print it out.
  for (x = 0; x < 256; x++)
    count[x] = 0;

  for (y = 0; y < LANDSIZE; y++) {
    for (x = 0; x < LANDSIZE; x++) {
      if (land[y][x].used)
        count[land[y][x].type & 0x00FF]++;
    }
  }

  for (x = 0; x < 256; x++) {
    if (count[x] > 0) {
//      printf("%02X %7d\n", x, count[x]);
    }
  }
  
  // Write out the map data
  mapFile = fopen(argv[2], "wb");
  if (mapFile == NULL) {
    printf("ERROR: File %s could not be opened!\n", argv[2]);
    return -1;
  }
  fwrite(land, sizeof(landData), LANDSIZE * LANDSIZE, mapFile);
  fclose(mapFile);

  return 0;
}
 
