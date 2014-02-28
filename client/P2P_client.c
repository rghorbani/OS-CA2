//
//  P2P_client.c
//  P2P_Client
//
//  Created by Farzad Shafiee on 4/23/13.
//  Copyright (c) 2013 Farzad Shafiee. All rights reserved.
//

#include "defines.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/param.h>

#include "func_lib.h"

int server_fd;
//char* path;
fd_set client_fds, safeSet;
int maxSocket;
char* serverIP;
char* serverPort;
bool connected;
int thr;
volatile int threadTrack;
volatile int seeds;
pthread_mutex_t readMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t writeMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t seedMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t portMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t servMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t trackMutex = PTHREAD_MUTEX_INITIALIZER;
volatile bool *done;

int connect_server(char* ip, char* port){
    int fd;
    struct addrinfo hints ,*server, *p;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int rv;
    if((rv =getaddrinfo(ip, port, &hints, &server)) != 0 ) {
        c_error("Error connecting to server.\n");
        fprintf(stderr, "code: %s.\n", gai_strerror(rv));
        return -1;
    }
    for(p = server; p != NULL; p = p->ai_next) {
        if ((fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            continue;
        }
        if (connect(fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(fd);
            continue;
        }
        break;
    }
    if (p == NULL) {
        return 2;
    }
    if(fd == -1){
        fprintf(stderr, "[ERR] Couldn't Create socket.\n");
        exit(1);
    }
    
    FD_SET(fd, &safeSet);
    if(fd > maxSocket)
        maxSocket = fd;
    connected = true;
    printf("[LOG] Connected to a server at %s:%s\n", ip, port);
    return fd;
}


void* GetChunk(void* arg){
    char* info = (char*) arg;
    char ip[INET_ADDRSTRLEN];
    char portstr[INT_LEN];
    char chnk[INT_LEN];
    char fileName[NAME_SIZE];
    
    memset(ip, '\0', sizeof(ip));
    memset(portstr, '\0', sizeof(portstr));
    memset(chnk, '\0', sizeof(chnk));
    memset(fileName, '\0', sizeof(fileName));
    strncpy(ip, info+REQ_LEN+NAME_SIZE+INT_LEN+INT_LEN, INET_ADDRSTRLEN);
    strncpy(portstr, info+REQ_LEN+NAME_SIZE+INT_LEN+INT_LEN+INET_ADDRSTRLEN, INT_LEN);
    strncpy(chnk, info+REQ_LEN+NAME_SIZE, INT_LEN);
    strncpy(fileName, info+REQ_LEN, NAME_SIZE);
    int chunk = atoi(chnk);
    char* wrm = "r+";
    int fd;
    struct addrinfo hints ,*server, *p;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int rv;
    
    if((rv =getaddrinfo(ip, portstr, &hints, &server)) != 0 ) {
        c_error("Error connecting to server.\n");
        done[chunk] = false;
        pthread_exit(&thr);
    }
    
    for(p = server; p != NULL; p = p->ai_next) {
        if ((fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            continue;
        }
        if (connect(fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(fd);
            continue;
        }
        break;
    }

    if (p == NULL) {
        done[chunk] = false;
        pthread_exit(&thr);
    }
    if(fd == -1){
        done[chunk] = false;
        pthread_exit(&thr);
    }
    char buf[BUFF_SIZE];
    memset(buf, '\0', BUFF_SIZE);
    bool error=false;
    while(recv(fd, buf, BUFF_SIZE, MSG_DONTWAIT) < 0){
        if (errno == EAGAIN || errno == EINTR) {
            continue;
        }
        else{
            error=true;
            break;
        }
    }
    if(error){
        done[chunk] = false;
        pthread_exit(&thr);
    }
    pthread_mutex_lock(&writeMutex);
    FILE* file = fopen(fileName, wrm);
    fseek(file, chunk*CHNK_SIZE, SEEK_SET);
    fwrite(buf, sizeof(char), CHNK_SIZE, file);
    fclose(file);
    pthread_mutex_unlock(&writeMutex);
    close(fd);
    
    printf("[LOG]Packet number %d of file \"%s\" has been received through %s:%s successfully.\n", chunk+1, fileName, ip, portstr);
    MakeStrID(SEED, buf, REQ_LEN);
    strncpy(buf+REQ_LEN, fileName, NAME_SIZE);
    strncpy(buf+REQ_LEN+NAME_SIZE, chnk, INT_LEN);
    while(send(server_fd, buf, BUFF_SIZE, MSG_DONTWAIT) < 0){
        if(errno == EAGAIN || errno == EINTR || errno == ENOBUFS) {
            continue;
        }
        else{
            error=true;
            break;
        }
    }
    if (error) {
    }
    
    //==================closing fd
    done[chunk] = true;
    pthread_mutex_lock(&trackMutex);
    threadTrack--;
    pthread_mutex_unlock(&trackMutex);
    close(fd);
    freeaddrinfo(server);
    pthread_exit(&thr);
    
}

void GetFile(char* name){
    FD_ZERO(&safeSet);
    int serverSocket = connect_server(serverIP, serverPort);
    
    //GETSIZE
    char reqseed[BUFF_SIZE];
    memset(reqseed, '\0', BUFF_SIZE);
    MakeStrID(GETSIZE, reqseed, REQ_LEN);
    //GETSIZE
    strncpy(reqseed+REQ_LEN, name, NAME_SIZE);
    while(send(serverSocket, reqseed, BUFF_SIZE, MSG_DONTWAIT) < 0){
        if(errno == EAGAIN || errno == EINTR || errno == ENOBUFS) {
            continue;
        }
        else{
            printf( "[ERR] failed sending request to server");
            exit(-1);
        }
    }
    
    uint32_t parts;
    bool err = false;
    while(recv(serverSocket, &parts, sizeof(uint32_t), MSG_WAITALL) < 0 ) {
        if (errno == EAGAIN || errno == EINTR) {
            continue;
        }
        err = true;
        break;
    }
    
    if (err) {
        printf("[ERR] Couldn't recieve file size for file: %s\n", name);
        exit(-2);
    }
    parts = ntohl(parts);
    if (parts == 0) {
        printf("[ERR] File NOT found: %s\n", name);
        exit(-1);
    }
    parts = (parts%CHNK_SIZE)? parts/CHNK_SIZE+1 : parts/CHNK_SIZE;
    done = calloc(parts, sizeof(bool));
    
    FILE* newFile = fopen(name, "w");
    fclose(newFile);
    pthread_t threads[parts];
    bool skip = false;
    
    threadTrack = parts;
    
    int i;
    for(i=0;i<parts;++i){
        if(done[i] == true) continue;
        
        //===============recieves port from server
        memset(reqseed, '\0', BUFF_SIZE);
        MakeStrID(GET, reqseed, REQ_LEN);
        strncpy(reqseed+REQ_LEN, name, NAME_SIZE);
        itoa(i, reqseed+REQ_LEN+NAME_SIZE);
        
        
        bool err = false;
        
        while (send(serverSocket, reqseed, BUFF_SIZE, MSG_DONTWAIT) < 0 ) {
            if(errno == EAGAIN || errno == EINTR || errno == ENOBUFS) {
                continue;
            }
            err = true;
            break;
        }
        
        if (err) {
            skip = true;
            continue;
        }
        
        
        memset(reqseed, '\0', BUFF_SIZE);
        while (recv(serverSocket, reqseed, BUFF_SIZE, MSG_DONTWAIT) < 0 ) {
            if (errno == EAGAIN || errno == EINTR) {
                continue;
            }
            err = true;
            break;
        }
        
        if (err) {
            skip = true;
            continue;
        }
        char stat[REQ_LEN];
        strncpy(stat, reqseed, REQ_LEN);
        int reqID = atoi(stat);
        if (reqID == NOT_FOUND) {
            exit(-1);
        }
        else if(reqID == CHNK_ERR) {
            continue;
        }
        
        else if(reqID == NO_SEED) {
            skip = true;
            continue;
        }
        
        else if(reqID == INIT_TRANS){
            int state;
            char* str = calloc(BUFF_SIZE, sizeof(char));
            memcpy(str, reqseed, BUFF_SIZE);
            while((state = pthread_create(&threads[i], NULL, GetChunk, (void*)str)) != 0) {
                if (state == EAGAIN) {
                    continue;
                }
                break;
            }
            
        }
        
        if(skip && i == parts-1){
            i=-1;
            skip=false;
        }
        else {
            if (skip == false) {
                int j = 0;
                for ( j = 0; j < parts; j++) {
                    if (done[i] == false) {
                        i = -1;
                        break;
                    }
                }
            }
        }
    }    
    while (threadTrack > 0) {
        //fprintf(stderr, "threadTrack:%d\n", threadTrack);
        
    }
    free((void*)done);
    printf("[LOG]File \"%s\" has been successfully downloaded.\n", name);
    exit(1);
}

int get_command(){
    char reqType[REQCOMMLEN], reqArg[2*NAME_SIZE];
    char buffer[BUFF_SIZE];
    char segment[2][BUFF_SIZE];
    memset(buffer, '\0', BUFF_SIZE);
    memset(segment, '\0', 2*BUFF_SIZE);
    memset(reqType, '\0', REQCOMMLEN);
    memset(reqArg, '\0', 2*NAME_SIZE);
    
    scanf("%s", reqType);
    
    if(strcasecmp(reqType, "connect") == 0){
        scanf("%s", reqArg);
        GetIP_Port(reqArg, segment[0], segment[1]);
        serverIP = calloc(2*INT_LEN, sizeof(char));
        serverPort = calloc(2*INT_LEN, sizeof(char));
        strncpy(serverIP, segment[0], 2*INT_LEN);
        strncpy(serverPort, segment[1], 2*INT_LEN);
        
        server_fd = connect_server(segment[0], segment[1]);
        fflush(stdin);
        return 1;
        
    }else if (strcasecmp(reqType, "get_files_list") == 0) {
        if ( connected == false) {
            c_error("Not connected to any servers at the moment!\n");
            fflush(stdin);
            return -1;
            
        }
        fprintf(stderr, "-----shared files-----\n");
        
        MakeStrID(LS, buffer, REQ_LEN);

        while(send(server_fd, buffer, BUFF_SIZE, MSG_DONTWAIT) < 0){
            if(errno == EAGAIN || errno == EINTR || errno == ENOBUFS) {
                continue;
            }
            else{
                //error = true;
                return -1;
            }
        }
        
        memset(buffer, '\0', BUFF_SIZE);
        while(recv(server_fd, buffer, BUFF_SIZE, MSG_DONTWAIT) < 0 ) {
            if (errno == EAGAIN || errno == EINTR) {
                continue;
            }
            else {
                //error = true;
                break;
            }
        }

        int i;
        for(i=0;i<(BUFF_SIZE/NAME_SIZE);++i){
            if(buffer[i*NAME_SIZE] == '\0'){
                break;
            }
            printf("%s\n", buffer+(i*NAME_SIZE));
            
        }
        fflush(stdin);
        return 1;
        
    }else if(strcasecmp(reqType, "sharefile") == 0){
        scanf("%s", reqArg);
        if ( connected == false) {
            c_error("Not connected to any servers at the moment!");
            fflush(stdin);
            return -1;
            
        }
        MakeStrID(SHARE, buffer, REQ_LEN);
        strncpy(buffer+REQ_LEN, reqArg, NAME_SIZE);
        //send file size
        unsigned int size = GetFileSize(reqArg);
        //TODO
        
        if (size == 0) {
            c_error("No such file.");
            fflush(stdin);
            return -1;
        }
        
        char file_size[INT_LEN];
        itoa(size, file_size);
        strncpy(buffer+REQ_LEN+NAME_SIZE, file_size, INT_LEN);

        while(send(server_fd, buffer, BUFF_SIZE, MSG_DONTWAIT) < 0){
            if(errno == EAGAIN || errno == EINTR || errno == ENOBUFS) {
                continue;
            }
            else{
                fflush(stdin);
                return -1;
            }
        }

        
    }else if(strcasecmp(reqType, "getfile") == 0){
        scanf("%s", reqArg);
        if ( connected == false) {
            fflush(stdin);
            c_error("Not connected to any servers at the moment!");
            return -1;
        }
        
        int f = fork();
        if(f < 0){
            fprintf(stderr, "[Error fork().]\n");
        }else if(f == 0){ // in child
            GetFile(reqArg);
        }
        //TODO Thread
    }else{
        printf("[LOG]Wrong command!\n\n");
    }
    fflush(stdin);
    return 1;
}

int ReadChunk(char* storage, char* req) {
    
    char chnk[INT_LEN];
    char fileName[NAME_SIZE];
    memset(chnk, '\0', sizeof(chnk));
    memset(fileName, '\0', sizeof(fileName));
    strncpy(chnk, req+REQ_LEN+NAME_SIZE, INT_LEN);
    strncpy(fileName, req+REQ_LEN, NAME_SIZE);
    int pos = atoi(chnk);
    FILE* currentChunk  = fopen(fileName, "r");
    fseek(currentChunk, pos*CHNK_SIZE, SEEK_SET);
    fread(storage, sizeof(char), CHNK_SIZE, currentChunk);
    fclose(currentChunk);
    return pos;
}

void* SeedFile(void* arg) {
    char* req = (char*)arg;
    int port = 1025;
    char dataChunk[BUFF_SIZE];
    memset(dataChunk, '\0', BUFF_SIZE);
    
    pthread_mutex_lock(&readMutex);
    int pack = ReadChunk(dataChunk,req)+1;
    pthread_mutex_unlock(&readMutex);
    
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int truVal = 1;
    int status = 0;
    pthread_mutex_lock(&portMutex);
    char listenPort[INT_LEN];
    memset(listenPort, '\0', INT_LEN);

    if((status = getaddrinfo(NULL, "0", &hints, &servinfo)) != 0 ) {
        pthread_exit(&thr);
        fprintf(stderr, "Seed error (1):%s\n", listenPort);
        freeaddrinfo(servinfo);
        
    }
    
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            continue;
        }
        
        if ((status = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &truVal, sizeof(int))) == -1) {
            fprintf(stderr,"Seed error (2)");
            break;
        }
        
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }
        else {
            break;
        }
    }
    struct sockaddr connection;
    socklen_t s = sizeof(connection);
    getsockname(sockfd, &connection, &s);
    port = htons(((struct sockaddr_in*)&connection)->sin_port);
    itoa(port, listenPort);
    freeaddrinfo(servinfo);
    if (p == NULL)  {
        pthread_mutex_unlock(&portMutex);
    }

    //Start listening
    if (listen(sockfd, 1) == -1) {
        
        pthread_mutex_unlock(&portMutex);
        pthread_exit(&thr);
    }
    
    pthread_mutex_unlock(&portMutex);

    itoa(port, listenPort);
    strncpy(req+REQ_LEN+NAME_SIZE+INT_LEN+INT_LEN+INET_ADDRSTRLEN, listenPort, INT_LEN);
    MakeStrID(SEED_INF, req, REQ_LEN);
    bool err = false;
    while (send(server_fd, req, BUFF_SIZE, MSG_DONTWAIT) < 0) {
        if(errno == EAGAIN || errno == EINTR || errno == ENOBUFS) {
            continue;
        }
        err = true;
        break;
    }
    if (err == true) {
        seeds--;
        pthread_exit(&thr);
    }
    
    fd_set safe, peer;
    FD_ZERO(&safe);
    FD_ZERO(&peer);
    FD_SET(sockfd, &safe);
    int incoming_socket = -1;
    struct sockaddr_storage peerAddress;
    while (true) {
        peer = safe;
        if(select(sockfd+1, &peer, NULL, NULL, NULL) == -1) {
            //Error select
            seeds--;
            pthread_exit(&thr);
            break;
        }
        else if (FD_ISSET(sockfd, &peer)) {
            socklen_t storeSize = sizeof peerAddress;
            incoming_socket = accept(sockfd, (struct sockaddr*) &peerAddress, &storeSize);
            FD_SET(incoming_socket, &safe);
            break;
            
        }
    }
    
    while (send(incoming_socket, dataChunk, BUFF_SIZE, MSG_DONTWAIT) < 0) {
        if(errno == EAGAIN || errno == EINTR || errno == ENOBUFS) {
            continue;
        }
        break;
    }
    char fileName[NAME_SIZE], IP[INET_ADDRSTRLEN];
    close(sockfd);  close(incoming_socket);
    
    memset(fileName, '\0', sizeof(fileName));    memset(IP, '\0', sizeof(IP));
    strncpy(fileName, arg+REQ_LEN, NAME_SIZE); inet_ntop(AF_INET, &((struct sockaddr_in*)&connection)->sin_addr, IP, INET_ADDRSTRLEN);
    printf("[LOG]Packet number %d of file \"%s\" has been send through %s:%s successfully.\n", pack, fileName, IP, listenPort);
    MakeStrID(FREE, req, REQ_LEN);

    while (send(server_fd, req, BUFF_SIZE, MSG_DONTWAIT) < 0) {
        if(errno == EAGAIN || errno == EINTR || errno == ENOBUFS) {
            continue;
        }
        break;
    }
    
    
    free(arg);
    pthread_mutex_lock(&seedMutex);
    seeds--;
    pthread_mutex_unlock(&seedMutex);
    pthread_exit(&thr);
    
}

int main(int argc, char* argv[]) {
    
    if (argc != 2) {
        c_error("Usage : <Directory>");
        return -1;
    }
    
    mkdir(argv[1], S_IRWXO | S_IRWXG | S_IRWXU);
    
    if(chdir(argv[1]) < 0) {
        c_error("Unable to set working directory.");
        return -2;
    }
    
    char path[MAXPATHLEN];
    printf("[LOG] Files will be stored in: \n>>%s\n", getwd(path));
    
    int stat = 1;
    char from_server[BUFF_SIZE];
    FD_SET(0, &safeSet);
    pthread_t seeders[MAX_SEED];
    while (stat) {
        client_fds = safeSet;
        if(select(maxSocket+1, &client_fds, NULL, NULL, NULL) == -1){
            //Error select
            fprintf(stderr, "Error select() \n");
            return 0;
        }
        int i;
        char reqstr[REQ_LEN];
        
        
        for(i = 0; i <= maxSocket; i++){
            if(FD_ISSET(i, &client_fds)){
                if(i == 0){
                    stat = get_command();
                    printf("\n");
                }else if(i == server_fd){
                    ssize_t rb;
                    while ((rb = recv(i, from_server, sizeof(from_server), MSG_DONTWAIT)) < 0) {
                        if (errno == EAGAIN || errno == EINTR) {
                            continue;
                        }
                        break;
                    }
                    if (rb == 0) {
                        FD_CLR(i, &safeSet);
                        continue;
                    }
                    memset(reqstr, '\0', REQ_LEN);
                    memcpy(reqstr, from_server, REQ_LEN*sizeof(char));
                    int request = atoi(reqstr);
                    switch(request){
                        case GET: {
                            while (seeds >= MAX_SEED);
                            
                            char* arg = calloc(BUFF_SIZE, sizeof(char));
                            memcpy(arg, from_server, BUFF_SIZE);
                            
                            pthread_mutex_lock(&seedMutex);
                            seeds++;
                            pthread_mutex_unlock(&seedMutex);
                            pthread_create(&seeders[seeds], NULL, SeedFile, (void*)arg);
                            
                            break;
                        }
                        default:
                            fprintf(stderr, "Unknown Request\n");
                            break;
                    }
                }
            }
        }
    }
    
    return 0;
}