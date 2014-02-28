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
#include <sys/dir.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include "info.h"

int port;
struct seeder_info* busyChunks;
void initialize(char* args[], int* sockfd, struct sigaction* sa) {
    
	struct addrinfo hints, *servinfo, *p;
    
	memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int truVal = 1;
    int status = 0;
    
    while((status = getaddrinfo(NULL, args[1], &hints, &servinfo)) != 0 ) {
    	errx(-1, "%s",gai_strerror(status));
    }
    
    for(p = servinfo; p != NULL; p = p->ai_next) {
    	if (((*sockfd) = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
    		continue;
    	}
        
    	if ((status = setsockopt((*sockfd), SOL_SOCKET, SO_REUSEADDR, &truVal, sizeof(int))) == -1) {
    		errx(-2, "%s",gai_strerror(status));
    	}
        
    	if (bind((*sockfd), p->ai_addr, p->ai_addrlen) == -1) {
    		close((*sockfd));
    		continue;
    	}
    	break;
    }
    
    if (p == NULL)  {
    	errx(-3, "%s", "Initialization failed\n");
    }
    
    //Start listening
  	if (listen(*sockfd, BACKLOG) == -1) {
    	perror("Listen error");
        errx(-4, "%s", "Listening Failed\n");
    }
    (*sa).sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&((*sa).sa_mask));
    (*sa).sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, sa, NULL) == -1) {
    	errx(-5, "%s", "Handler Failed\n");
    }
}

int RequestPort(struct seeder_info* seed, char* request) {
    MakeStrID(GET, request, REQ_LEN);
    while (send(seed->port, request, BUFF_SIZE, MSG_DONTWAIT) < 0) {
        if(errno == EAGAIN || errno == EINTR || errno == ENOBUFS || errno == EWOULDBLOCK) {
            continue;
        }
        return -1;
        break;
    }
    return 0;
}
void Response(int socket, int status, char* data) {
    MakeStrID(status, data, REQ_LEN);
    while(send(socket, data, BUFF_SIZE, MSG_DONTWAIT) < 0) {
        if(errno == EAGAIN || errno == EINTR || errno == ENOBUFS) {
            continue;
        }
        break;
    }
}

void FindSeeder(struct file_info* file, int chunk, char* request, int requester) {
    
    //fprintf(stderr, "[DBG] %s, %d, %d, %d\n", file->name, file->chunks, chunk, file->seeders);
    
    struct seeder_info* p;
    
    if (chunk > file->chunks) {
        Response(requester, CHNK_ERR, request);
    }
    
    for (p = &(file->seeders)[chunk]; p != NULL; p = p->next) {
        if (p->busy == false) {
            break;
        }
    }
    if (p == NULL) {
        Response(requester, NO_SEED, request);
    }    
    
    else {
        char peerID[INT_LEN];
        memset(peerID, '\0', INT_LEN);
        itoa(requester, peerID);
        strncpy(request+REQ_LEN+NAME_SIZE+INT_LEN, peerID, INT_LEN);
        if (RequestPort(p, request) != 0) {
            p->busy = true;
            Response(requester, NO_SEED, request);
        }
        else {
            p->busy = true;
            p->nextBusy = busyChunks->nextBusy;
            busyChunks->nextBusy = p;
        }
    }
    
}
struct file_info* FindFile(struct file_info* files, char request[]) {
    struct file_info* p;
    for (p = files->next; p != NULL; p = p->next) {
        if (!strncasecmp(request, p->name, NAME_SIZE)) {
            return p;
        }
    }
    return NULL;
};

int main(int argc, char* argv[]) {
    
	if(argc != 2) {
        c_error("Usage : <PortNumber>");
		return 1;
	}
    
	if((port = atoi(argv[1])) <= 1024 ) {
		c_error("[Error] Invlid Port.");
		return 2;
	}
    
	int listener, incoming_socket;
	struct sigaction sa;
  	
    initialize(argv, &listener, &sa);
    
    fprintf(stderr, "[INIT] Server Initilized on port %d.\n", port);
  	//Select
  	fd_set client_fds, safeSet;
    FD_ZERO(&client_fds);
    FD_ZERO(&safeSet);
    
    int maxSocket = listener;
    
    ssize_t recievedBytes;
    
    FD_SET(listener, &safeSet);
    FD_SET(0, &safeSet);
    struct sockaddr_storage clientAddress;
    
    //struct seeder_info head, *clients;
    
    struct file_info m;
    memset(&m, 0, sizeof(m));
    struct file_info *files = &m;
    
    struct seeder_info n;
    memset(&n, 0, sizeof(n));
    busyChunks = &n;
    
    char buffer[BUFF_SIZE];
    fprintf(stderr, "[INIT] Server is up and runnig...\n\n");
    
    while(true) {
        memset(buffer, 0, BUFF_SIZE);
    	client_fds = safeSet;
    	if(select(maxSocket+1, &client_fds, NULL, NULL, NULL) == -1) {
    		//Error select
            c_error("Error select()");
    		return 3;
    	}
        int i = 0;
        for(i = 0; i <= maxSocket; i++) {
            if(FD_ISSET(i, &client_fds)) {
                
                if(i == listener) {
                    //c_log("Incoming connection...");
                    socklen_t storeSize = sizeof clientAddress ;
                    incoming_socket = accept(listener, (struct sockaddr*) &clientAddress, &storeSize);
                    if(incoming_socket < 0) {
                      //  c_error("Couldn't establish new connection. accept() error.\n\n");
                        continue;
                    }
                    
                    //arrangements for new client
                    //--add to listening set
                    FD_SET(incoming_socket, &safeSet);
                    //fprintf(stderr, "[LOG] New client established a connection. [%d client(s)]\n", ++numOfClients);
                    
                    maxSocket = (incoming_socket > maxSocket) ? incoming_socket : maxSocket;
                }
                
                else if(i == 0) {
                    //server commands
                    recievedBytes = read(i, buffer, BUFF_SIZE);
                    printf(">>WHO U TYPING AT?! MIND YOUR OWN BIZ!! K?!<<\n");
                    
                }
                
                else {
                    memset(buffer, '\0', BUFF_SIZE);
                    bool error = false;
                    while((recievedBytes = recv(i, buffer, BUFF_SIZE, MSG_DONTWAIT)) < 0 ) {
                        //Error or DC
                        if (errno == EAGAIN || errno == EINTR) {
                            continue;
                        }
                        
                        else {
                            error = true;
                            break;
                        }
                    }
                    
                    if (error) {
                        continue;
                    }
                    
                    if (recievedBytes ==  0) {
                        FD_CLR(i, &safeSet);
                        //printf("[LOG] A client has been disconnected. [%d client(s)]\n", --numOfClients);
                        //Handle disconnected clients
                        break;
                    }
                    
                    char req[REQ_LEN];
                    strncpy(req, buffer, REQ_LEN);
                    int request = atoi(req);                    
                    
                    //Get requset sender information
                    struct sockaddr_storage address;
                    socklen_t len = sizeof(address);
                    getpeername(i, (struct sockaddr*)&address, &len);
                    
                    //Process Request
                    switch (request) {
                            
                            //Share Request==============================
                        case SHARE: {
                            size_t size = atoi(buffer+REQ_LEN+NAME_SIZE);
                            struct file_info* newFile = MakeNewFile(buffer+REQ_LEN, size, &address, i);
                            StoreFileInfo(files, newFile);
                            printf("[LOG] File \"%s\" has been added.\n", newFile->name);
                            break;
                        }
                            
                            //File List Request==========================
                        case LS: {
                            SendFilesList(i, files);
                            break;
                            
                        }
                        case GETSIZE: {
                            struct file_info* file = FindFile(files, buffer+REQ_LEN);
                            uint32_t size = file->totalSize;
                            if (file == NULL) {
                                size = htonl(0);
                                
                            }
                            else {
                                size = htonl(size);
                            }
                            while (send(i, &size, sizeof(uint32_t), MSG_DONTWAIT) < 0) {
                                if(errno == EAGAIN || errno == EINTR || errno == ENOBUFS) {
                                    continue;
                                }
                                break;
                            }
                            break;
                        
                        }
                            //Get File Request==========================
                        case GET: {
                            struct file_info* file = FindFile(files, buffer+REQ_LEN);
                            
                            if (file == NULL) {
                                Response(i, NOT_FOUND, buffer);
                                break;
                            }
                            
                            FindSeeder(file, atoi(buffer+REQ_LEN+NAME_SIZE), buffer, i);
                            
                            break;
                        }
                        case SEED_INF: {
                            struct sockaddr_storage peer;
                            socklen_t adlen = sizeof(peer);
                            
                            char peerFD[INT_LEN];
                            memset(peerFD, '\0', INT_LEN);
                            strncpy(peerFD, buffer+REQ_LEN+NAME_SIZE+INT_LEN, INT_LEN);
                            
                            int fdes = atoi(peerFD);
                            getpeername(fdes, (struct sockaddr*)&peer, &adlen);
                            inet_ntop(AF_INET, &((struct sockaddr_in*)&peer)->sin_addr, buffer+REQ_LEN+NAME_SIZE+INT_LEN+INT_LEN, INET_ADDRSTRLEN);
                            Response(fdes, INIT_TRANS, buffer);
                            break;
                        }
                        case FREE: {
                            struct seeder_info* curr, *pre;
                            char fileName[NAME_SIZE];
                            char chunkNum[INT_LEN];
                            memcpy(fileName, buffer+REQ_LEN, NAME_SIZE);
                            memcpy(chunkNum, buffer+NAME_SIZE+REQ_LEN, INT_LEN);
                            
                            int chnk = atoi(chunkNum);
                            
                            pre = busyChunks;
                            for (curr = busyChunks->nextBusy; curr != NULL; curr = curr->nextBusy) {
                                if (strncasecmp(buffer+REQ_LEN, (curr->file)->name, NAME_SIZE) == 0) {
                                    if (curr->port == i) {
                                        if (curr->chnk == chnk) {
                                            curr->busy = false;
                                            pre->nextBusy = curr->nextBusy;
                                            break;
                                        }
                                    }
                                }
                                pre = curr;
                            }
                            break;
                        }
                    
                        case SEED: {
                            char fileName[NAME_SIZE];
                            char chunkNum[INT_LEN];
                            memcpy(fileName, buffer+REQ_LEN, NAME_SIZE);
                            memcpy(chunkNum, buffer+NAME_SIZE+REQ_LEN, INT_LEN);
                            int chnk = atoi(chunkNum);

                            struct file_info* file = FindFile(files, buffer+REQ_LEN);
                            if(file == NULL) {
                                c_error("Invalid seed request.");
                                break;
                            }
                            
                            struct seeder_info* newSeed = calloc(1, sizeof(struct seeder_info));
                            
                            newSeed->ip = ((struct sockaddr_in*)&address)->sin_addr;
                            newSeed->port = i;
                            newSeed->file = file;
                            newSeed->chnk = chnk;
                            newSeed->busy = false;
                            newSeed->nextBusy = NULL;
                            newSeed->next = file->seeders->next;
                            file->seeders->next = newSeed;
                            break;
                            
                        }
                    }
                }
    		}
    	}
    }
    return 0;
}

