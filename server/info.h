#ifndef _INFO__
#include "defines.h"
#include <sys/stat.h>
#include <unistd.h>
#include <libc.h>
#include <sys/socket.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include "func_lib.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>

struct seeder_info;

struct file_info {
    char name[NAME_SIZE];
    size_t totalSize;
    uint32_t chunks;
    struct seeder_info* seeders;
    struct file_info* next;
};

struct seeder_info {
    struct in_addr ip;
    int port;
    int chnk;
    bool busy;
    struct seeder_info *next, *nextBusy;
    struct file_info* file;
    
};

struct file_info* MakeNewFile (char name[], size_t fileSize, struct sockaddr_storage* client, int fd);

void StoreFileInfo(struct file_info* fileList, struct file_info* newFile);

size_t SendFilesList(int socket, struct file_info *list);

struct file_info* GetFileInfo(char* name, struct file_info* lists);

void PackAddress(char* dataPack, struct seeder_info* seeder);

#define _INFO__
#endif