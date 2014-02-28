#include "info.h"

//File Info Section -----------------

struct file_info* MakeNewFile (char name[], size_t fileSize, struct sockaddr_storage* client, int fd) {
    
    struct file_info* newFile =  (struct file_info*)calloc(1, sizeof(struct file_info));
    
    strncpy(newFile->name, name, NAME_SIZE);
    
    newFile->chunks = (fileSize%CHNK_SIZE)? fileSize/CHNK_SIZE+1 : fileSize/CHNK_SIZE;
    newFile->seeders = (struct seeder_info*)calloc(newFile->chunks, sizeof(struct seeder_info));
    newFile->next = NULL;
    newFile->totalSize = fileSize;
    //Initial first seeder
    (newFile->seeders)[0].busy = false;
    (newFile->seeders)[0].ip = ((struct sockaddr_in *) client)->sin_addr;
    (newFile->seeders)[0].port = fd;
    (newFile->seeders)[0].chnk = 0;
    (newFile->seeders)[0].file = newFile;
    (newFile->seeders)[0].next = NULL;
    (newFile->seeders)[0].nextBusy = NULL;
    
    //printf("[DBG] newly created: %s, %d\n", newFile->name, newFile->chunks);

    int i;
    for( i = 1; i < newFile->chunks; i++) {
        memcpy(&(newFile->seeders)[i], &(newFile->seeders)[0], sizeof(struct seeder_info));
        (newFile->seeders)[i].chnk = i;
    }
    return newFile;
}

void StoreFileInfo(struct file_info* fileList, struct file_info* newFile) {
    
    struct file_info* oldHead = (*fileList).next;
    (*fileList).next = newFile;
    (*newFile).next = oldHead;    
};

size_t SendFilesList(int socket, struct file_info *list) {
    
    char fileList[BUFF_SIZE];
    memset(fileList, '\0', BUFF_SIZE);
    struct file_info* p;
    size_t i = 0;
    for ( p = (*list).next; p != NULL; p = (*p).next) {
        if (i*NAME_SIZE >= BUFF_SIZE) break;
        strncpy(fileList+(i*NAME_SIZE), (*p).name, NAME_SIZE);
        i++;
    }
    while (send(socket, fileList, BUFF_SIZE, MSG_DONTWAIT) < 0 ) {
        if(errno == EAGAIN || errno == EINTR || errno == ENOBUFS) {
            continue;
        }
        break;
    }

    return i;
}

struct file_info* GetFileInfo(char* name, struct file_info* list) {
    struct file_info* pre, *curr;
    pre = list;
    for (curr = (*list).next; curr != NULL; curr = (*curr).next) {
        if(strncasecmp(name, (*curr).name, NAME_SIZE) == 0){
            return curr;
        }
        pre = curr;
    }
    return NULL;
}

void PackAddress(char* dataPack, struct seeder_info* seeder) {
    inet_ntop(AF_INET, &(seeder->ip), dataPack+REQ_LEN+NAME_SIZE+INT_LEN, INET_ADDRSTRLEN);
    itoa(seeder->port,dataPack+REQ_LEN+NAME_SIZE+INT_LEN+INET_ADDRSTRLEN);
    
}