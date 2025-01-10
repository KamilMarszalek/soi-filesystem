#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>   

#define MAGIC_STR "MYFS"
#define BLOCK_SIZE 512
#define MAX_INODES 128
#define MAX_FRAGS 16
#define MAX_NAME_LEN 128


typedef struct {
    int startBlock;   
    int blockCount;   
} Fragment;

typedef struct {
    int isUsed;                            
    char fileName[MAX_NAME_LEN]; 
    int  fileSize;               
    Fragment fragments[MAX_FRAGS];  
    int   fragmentsCount;             
} Inode;

typedef struct {
    char signature[4];      
    int  blockCount;    
    int  inodeCount;
    int  freeBlocks;    
    int  inodeTableOffset;  
    int  blockBitmapOffset; 
    int  dataOffset;        
} SuperBlock;


int readSuperBlock(FILE *fp, SuperBlock *sb) {
    fseek(fp, 0, SEEK_SET);
    if (fread(sb, sizeof(SuperBlock), 1, fp) != 1) {
        return -1;
    }
    if (memcmp(sb->signature, MAGIC_STR, 4) != 0) {
        fprintf(stderr, "Błędna sygnatura superbloku (nie 'MYFS').\n");
        return -1;
    }
    return 0;
}

int writeSuperBlock(FILE *fp, const SuperBlock *sb) {
    fseek(fp, 0, SEEK_SET);
    if (fwrite(sb, sizeof(SuperBlock), 1, fp) != 1) {
        return -1;
    }
    return 0;
}

int readInode(FILE *fp, const SuperBlock *sb, int idx, Inode *ino) {
    if (idx < 0 || idx >= sb->inodeCount) return -1;
    long offset = sb->inodeTableOffset + (long)idx * sizeof(Inode);
    fseek(fp, offset, SEEK_SET);
    if (fread(ino, sizeof(Inode), 1, fp) != 1) {
        return -1;
    }
    return 0;
}

int writeInode(FILE *fp, const SuperBlock *sb, int idx, const Inode *ino) {
    if (idx < 0 || idx >= sb->inodeCount) return -1;
    long offset = sb->inodeTableOffset + (long)idx * sizeof(Inode);
    fseek(fp, offset, SEEK_SET);
    if (fwrite(ino, sizeof(Inode), 1, fp) != 1) {
        return -1;
    }
    return 0;
}

int loadBlockMap(FILE *fp, const SuperBlock *sb, unsigned char *blockMap) {
    fseek(fp, sb->blockBitmapOffset, SEEK_SET);
    size_t r = fread(blockMap, 1, sb->blockCount, fp);
    return (r == (size_t)sb->blockCount) ? 0 : -1;
}

int saveBlockMap(FILE *fp, const SuperBlock *sb, const unsigned char *blockMap) {
    fseek(fp, sb->blockBitmapOffset, SEEK_SET);
    size_t w = fwrite(blockMap, 1, sb->blockCount, fp);
    return (w == (size_t)sb->blockCount) ? 0 : -1;
}

long getBlockOffset(const SuperBlock *sb, int blockNum) {
    return sb->dataOffset + (long)blockNum * BLOCK_SIZE;
}

int formatDisk(const char *diskFile, long diskSize) {
    FILE *fp = fopen(diskFile, "wb");
    if (!fp) {
        perror("formatDisk fopen");
        return -1;
    }
    int inodeCount = MAX_INODES;
    int blockSize = BLOCK_SIZE;
    long blocks = diskSize / blockSize;
    if (blocks < 1) {
        fprintf(stderr, "Za mały rozmiar dysku\n");
        fclose(fp);
        return -1;
    }
    size_t superBlockSize = sizeof(SuperBlock);
    size_t inodeTableSize = inodeCount * sizeof(Inode);
    size_t bitmapSize = blocks;
    long overhead = (long)superBlockSize + (long)inodeTableSize + (long)bitmapSize;
    if (diskSize <= overhead) {
        fprintf(stderr, "Za mały rozmiar dysku\n");
        fclose(fp);
        return -1;
    }
    int dataSpace = diskSize - overhead;
    long blockCount = dataSpace / blockSize;
    if (blockCount < 1) {
        fprintf(stderr, "Za mały rozmiar dysku\n");
        fclose(fp);
        return -1;
    }

    SuperBlock sb;
    memset(&sb, 0, sizeof(sb));
    memcpy(sb.signature, MAGIC_STR, 4);
    sb.blockCount = (int)blockCount;   
    sb.inodeCount = inodeCount;   
    sb.freeBlocks = (int)blockCount;   

    sb.inodeTableOffset = (int)sizeof(SuperBlock);
    int inodeTableOffset = sb.inodeTableOffset;
    int inodeTableBytes = (int)inodeTableSize;
    
    sb.blockBitmapOffset = inodeTableOffset + inodeTableBytes;
    int bitmapOffset = sb.blockBitmapOffset;
    int bitmapBytes = (int)blockCount;
    sb.dataOffset = sb.blockBitmapOffset + bitmapBytes;

    long totalSize = sb.dataOffset + (long)blockCount * blockSize;
    if (totalSize > diskSize) {
    }

    fseek(fp, totalSize - 1, SEEK_SET);
    fputc('\0', fp);

    fseek(fp, 0, SEEK_SET);
    fwrite(&sb, sizeof(sb), 1, fp);

    Inode emptyInode;
    memset(&emptyInode, 0, sizeof(emptyInode));
    for (int i = 0; i < MAX_FRAGS; i++) {
        emptyInode.fragments[i].startBlock = -1;
        emptyInode.fragments[i].blockCount = 0;
    }
    emptyInode.fragmentsCount = 0;
    emptyInode.isUsed = 0;

    fseek(fp, sb.inodeTableOffset, SEEK_SET);
    for (int i = 0; i < inodeCount; i++) {
        fwrite(&emptyInode, sizeof(emptyInode), 1, fp);
    }

    unsigned char zero = 0;
    fseek(fp, sb.blockBitmapOffset, SEEK_SET);
    for (int i = 0; i < blockCount; i++) {
        fwrite(&zero, 1, 1, fp);
    }

    fclose(fp);
    printf("Utworzono wirtualny dysk: %s\n", diskFile);
    printf("Liczba bloków = %d, rozmiar pliku = %ld bajtów\n", (int)blockCount, totalSize);
    return 0;
}

int allocateExtents(unsigned char *blockMap, SuperBlock *sb, Inode *ino, int blocksNeeded) {
    if (blocksNeeded <= 0) return 0; 

    int allocated = 0;
    ino->fragmentsCount = 0;
    for (int i = 0; i < MAX_FRAGS; i++) {
        ino->fragments[i].startBlock = -1;
        ino->fragments[i].blockCount = 0;
    }

    int fragIndex = 0;
    int i = 0;
    while (i < sb->blockCount && allocated < blocksNeeded) {
        if (blockMap[i] == 0) {
            int start = i;
            int length = 0;
            while (i < sb->blockCount && blockMap[i] == 0 && allocated + length < blocksNeeded) {
                i++;
                length++;
            }
            if (fragIndex >= MAX_FRAGS) {
                return -1;
            }
            ino->fragments[fragIndex].startBlock = start;
            ino->fragments[fragIndex].blockCount = length;
            fragIndex++;
            for (int b = start; b < start + length; b++) {
                blockMap[b] = 1; 
            }

            allocated += length;
        } else {
            i++;
        }
    }

    if (allocated < blocksNeeded) {
        return -1;
    }

    ino->fragmentsCount = fragIndex;
    sb->freeBlocks -= allocated;
    return 0;
}

int copyIn(const char *diskName, const char *srcFile, const char *destName) {
    FILE *fSrc = fopen(srcFile, "rb");
    if (!fSrc) {
        fprintf(stderr, "Nie mogę otworzyć pliku źródłowego %s\n", srcFile);
        return -1;
    }
    fseek(fSrc, 0, SEEK_END);
    long fileSize = ftell(fSrc);
    fseek(fSrc, 0, SEEK_SET);
    FILE *fp = fopen(diskName, "rb+");
    if (!fp) {
        fclose(fSrc);
        fprintf(stderr, "Nie można otworzyć dysku %s\n", diskName);
        return -1;
    }
    SuperBlock sb;
    if (readSuperBlock(fp, &sb) < 0) {
        fclose(fp);
        fclose(fSrc);
        return -1;
    }
    unsigned char *blockMap = calloc(sb.blockCount, 1);
    loadBlockMap(fp, &sb, blockMap);
    int freeInodeIdx = -1;
    for (int i = 0; i < sb.inodeCount; i++) {
        Inode temp;
        if (readInode(fp, &sb, i, &temp) == 0) {
            if (temp.isUsed == 0) {
                freeInodeIdx = i;
                break;
            }
        }
    }
    if (freeInodeIdx < 0) {
        fprintf(stderr, "Brak wolnych i-węzłów, katalog pełny.\n");
        free(blockMap);
        fclose(fp);
        fclose(fSrc);
        return -1;
    }

    Inode newIno;
    memset(&newIno, 0, sizeof(newIno));
    newIno.isUsed = 1;
    strncpy(newIno.fileName, destName, MAX_NAME_LEN - 1);
    newIno.fileName[MAX_NAME_LEN - 1] = '\0';
    newIno.fileSize = fileSize;
    newIno.fragmentsCount = 0;
    int blocksNeeded = (fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (allocateExtents(blockMap, &sb, &newIno, blocksNeeded) < 0) {
        fprintf(stderr, "Brak miejsca na dysku albo za dużo fragmentów.\n");
        free(blockMap);
        fclose(fp);
        fclose(fSrc);
        return -1;
    }
    long bytesLeft = fileSize;
    char *buf = malloc(BLOCK_SIZE);

    for (int f = 0; f < newIno.fragmentsCount; f++) {
        int start = newIno.fragments[f].startBlock;
        int cnt   = newIno.fragments[f].blockCount; 
        for (int b = 0; b < cnt; b++) {
            if (bytesLeft <= 0) break;
            size_t toRead = (bytesLeft > BLOCK_SIZE) ? BLOCK_SIZE : bytesLeft;
            size_t r = fread(buf, 1, toRead, fSrc);
            if (r != toRead) {
            }
            long off = getBlockOffset(&sb, start + b);
            fseek(fp, off, SEEK_SET);
            fwrite(buf, 1, toRead, fp);

            bytesLeft -= toRead;
        }
        if (bytesLeft <= 0) break;
    }

    free(buf);
    writeInode(fp, &sb, freeInodeIdx, &newIno);

    saveBlockMap(fp, &sb, blockMap);
    writeSuperBlock(fp, &sb);

    free(blockMap);
    fclose(fp);
    fclose(fSrc);
    printf("Skopiowano plik %s do FS jako '%s' (inode=%d, rozmiar=%ld).\n",
           srcFile, destName, freeInodeIdx, fileSize);
    return 0;
}

int copyOut(const char *diskName, const char *fileName, const char *outFile) {
    FILE *fp = fopen(diskName, "rb");
    if (!fp) {
        fprintf(stderr, "Nie można otworzyć dysku %s\n", diskName);
        return -1;
    }
    SuperBlock sb;
    if (readSuperBlock(fp, &sb) < 0) {
        fclose(fp);
        return -1;
    }
    int foundInode = -1;
    Inode ino;
    for (int i = 0; i < sb.inodeCount; i++) {
        if (readInode(fp, &sb, i, &ino) == 0) {
            if (ino.isUsed == 1) {
                if (strncmp(ino.fileName, fileName, MAX_NAME_LEN) == 0) {
                    foundInode = i;
                    break;
                }
            }
        }
    }
    if (foundInode < 0) {
        fprintf(stderr, "Nie ma takiego pliku '%s' na dysku.\n", fileName);
        fclose(fp);
        return -1;
    }
    FILE *fOut = fopen(outFile, "wb");
    if (!fOut) {
        fprintf(stderr, "Nie można utworzyć pliku wyjściowego %s\n", outFile);
        fclose(fp);
        return -1;
    }
    long bytesLeft = ino.fileSize;
    char *buf = malloc(BLOCK_SIZE);

    for (int f = 0; f < ino.fragmentsCount; f++) {
        int start = ino.fragments[f].startBlock;
        int cnt   = ino.fragments[f].blockCount;
        for (int b = 0; b < cnt; b++) {
            if (bytesLeft <= 0) break;
            size_t toRead = (bytesLeft > BLOCK_SIZE) ? BLOCK_SIZE : bytesLeft;
            long off = getBlockOffset(&sb, start + b);
            fseek(fp, off, SEEK_SET);
            size_t rr = fread(buf, 1, toRead, fp);
            fwrite(buf, 1, rr, fOut);
            bytesLeft -= rr;
        }
        if (bytesLeft <= 0) break;
    }
    free(buf);

    fclose(fOut);
    fclose(fp);
    printf("Skopiowano plik '%s' (inode=%d) z FS do '%s'.\n", fileName, foundInode, outFile);
    return 0;
}
int removeFile(const char *diskName, const char *fileName) {
    FILE *fp = fopen(diskName, "rb+");
    if (!fp) {
        fprintf(stderr, "Nie można otworzyć %s\n", diskName);
        return -1;
    }
    SuperBlock sb;
    if (readSuperBlock(fp, &sb) < 0) {
        fclose(fp);
        return -1;
    }
    int foundInode = -1;
    Inode ino;
    for (int i = 0; i < sb.inodeCount; i++) {
        if (readInode(fp, &sb, i, &ino) == 0) {
            if (ino.isUsed == 1) {
                if (strncmp(ino.fileName, fileName, MAX_NAME_LEN) == 0) {
                    foundInode = i;
                    break;
                }
            }
        }
    }
    if (foundInode < 0) {
        fprintf(stderr, "Nie znaleziono pliku '%s'.\n", fileName);
        fclose(fp);
        return -1;
    }
    unsigned char *blockMap = calloc(sb.blockCount, 1);
    loadBlockMap(fp, &sb, blockMap);

    int totalBlocksFreed = 0;
    for (int f = 0; f < ino.fragmentsCount; f++) {
        int start = ino.fragments[f].startBlock;
        int cnt   = ino.fragments[f].blockCount;
        for (int b = start; b < start + cnt; b++) {
            blockMap[b] = 0; // wolny
        }
        totalBlocksFreed += cnt;
    }
    sb.freeBlocks += totalBlocksFreed;
    Inode empty;
    memset(&empty, 0, sizeof(empty));
    writeInode(fp, &sb, foundInode, &empty);
    saveBlockMap(fp, &sb, blockMap);
    writeSuperBlock(fp, &sb);

    free(blockMap);
    fclose(fp);
    printf("Plik '%s' (inode=%d) usunięty.\n", fileName, foundInode);
    return 0;
}
int listFiles(const char *diskName) {
    FILE *fp = fopen(diskName, "rb");
    if (!fp) {
        fprintf(stderr, "Nie można otworzyć %s\n", diskName);
        return -1;
    }
    SuperBlock sb;
    if (readSuperBlock(fp, &sb) < 0) {
        fclose(fp);
        return -1;
    }
    printf("Katalog:\n");
    for (int i = 0; i < sb.inodeCount; i++) {
        Inode ino;
        if (readInode(fp, &sb, i, &ino) == 0) {
            if (ino.isUsed == 1) {
                printf("  inode=%d, nazwa='%s', rozmiar=%d bajtów, fragmentsCount=%d\n",
                       i, ino.fileName, ino.fileSize, ino.fragmentsCount);
            }
        }
    }
    fclose(fp);
    return 0;
}
int printMap(const char *diskName) {
    FILE *fp = fopen(diskName, "rb");
    if (!fp) {
        fprintf(stderr, "Nie można otworzyć pliku dysku\n");
        return -1;
    }
    SuperBlock sb;
    if (readSuperBlock(fp, &sb) < 0) {
        fclose(fp);
        return -1;
    }

    printf("STRUKTURA DYSKU '%s':\n", diskName);
    printf("Offset superbloku: %ld\n", 0L);
    printf("Offset tabeli i-węzłów: %d\n", sb.inodeTableOffset);
    printf("Offset bitmapy bloków: %d\n", sb.blockBitmapOffset);
    printf("Offset danych: %d\n", sb.dataOffset);

    unsigned char *blockMap = calloc(sb.blockCount, 1);
    loadBlockMap(fp, &sb, blockMap);
    printf("Mapa bloków:\n");
    int start = 0;
    int current = blockMap[0];
    for (int i = 1; i < sb.blockCount; i++) {
        if ((int)blockMap[i] != current) {
            printf("Bloki [%d..%d] -> %s\n", start, i - 1, (current == 0) ? "WOLNE" : "ZAJĘTE");
            start = i;
            current = blockMap[i];
        }
    }
    printf("Bloki [%d..%d] -> %s\n", start, sb.blockCount - 1,
           (current == 0) ? "WOLNE" : "ZAJĘTE");

    free(blockMap);
    fclose(fp);
    return 0;
}
int removeDisk(const char *diskName) {
    if (unlink(diskName) == 0) {
        printf("Plik dysku '%s' usunięty.\n", diskName);
        return 0;
    } else {
        perror("unlink");
        return -1;
    }
}
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, 
            "Użycie: %s <polecenie> [argumenty]\n"
            "Dostępne polecenia:\n"
            "  create <diskFile> <blockCount>\n"
            "  copyin <diskFile> <srcFile> <destName>\n"
            "  copyout <diskFile> <fileName> <outFile>\n"
            "  ls <diskFile>\n"
            "  rm <diskFile> <fileName>\n"
            "  map <diskFile>\n"
            "  rmdisk <diskFile>\n",
            argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "create") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Użycie: create <diskFile> <diskSize>\n");
            return 1;
        }
        const char *diskFile = argv[2];
        int diskSize = atoi(argv[3]);
        return formatDisk(diskFile, diskSize);

    } else if (strcmp(cmd, "copyin") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Użycie: copyin <diskFile> <srcFile> <destName>\n");
            return 1;
        }
        return copyIn(argv[2], argv[3], argv[4]);

    } else if (strcmp(cmd, "copyout") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Użycie: copyout <diskFile> <fileName> <outFile>\n");
            return 1;
        }
        return copyOut(argv[2], argv[3], argv[4]);

    } else if (strcmp(cmd, "ls") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Użycie: ls <diskFile>\n");
            return 1;
        }
        return listFiles(argv[2]);

    } else if (strcmp(cmd, "rm") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Użycie: rm <diskFile> <fileName>\n");
            return 1;
        }
        return removeFile(argv[2], argv[3]);

    } else if (strcmp(cmd, "map") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Użycie: map <diskFile>\n");
            return 1;
        }
        return printMap(argv[2]);

    } else if (strcmp(cmd, "rmdisk") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Użycie: rmdisk <diskFile>\n");
            return 1;
        }
        return removeDisk(argv[2]);

    } else {
        fprintf(stderr, "Nieznane polecenie: %s\n", cmd);
        return 1;
    }
    return 0;
}