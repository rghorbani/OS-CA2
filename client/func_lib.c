#include "func_lib.h"

void c_error(char *str){
	printf("[ERR] %s\n", str);
}

void c_log(char *str){
	printf("[LOG] %s\n", str);
}
void itoa(int num, char* str) {
	char tmp[INT_LEN];
	int i=0,j;
	memset(tmp, '\0', INT_LEN);
	while(num>0){
		tmp[i] = (char)((num%10)+'0');
		num/=10;
		++i;
	}
	for(j=i;j>=0;--j)
		str[i-j-1]=tmp[j];
}

void MakeStrID(int idNo, char* str, int strSize) {
    int mp = 10;
    int pre_mp = 1;
    int i = strSize-1;
    for (i = strSize-1; i >= 0; i--) {
        str[i] = (idNo%mp - idNo%pre_mp)/pre_mp + ('0');
        pre_mp = mp;
        mp*=10;
    }
}

void Trim (char* trimmed, int size) {
    int i = 0;
    for (i = 0; i < size; i++) {
        if (trimmed[0] == '0') {
            int j = 0;
            for (j = 1; j < size; j++) {
                trimmed[j-1] = trimmed[j];
            }
            trimmed[size-1] = '\0';
        }
    }
}
int calcSpeed(char* ret, time_t start, time_t finish, int bytes) {
    int bps = (int)bytes/((finish - start)*1024);
    if(bps == 0) bps = 1;
    itoa(bps, ret);
    return bps;
}
int str_size(char* str){
    int i=0;
    while(i<256 && str[i]!='\0') ++i;
    if(i == -1)
        return -1;
    else
        return i;
}
void split2(char* from, char del1, char del2, char* to){
    int len = str_size(from);
    int i=0, j;
    int begin=0;
    int first=0;
    //printf("dels=%c,%c,%c,\n", del1, del2, from[17]);
    while(i<len){
        if(from[i]==del1){
            //printf("f=%c,%d\n", from[i], i);
            begin=i+1;
            first=1;
        }else if(first!=0 && (from[i]==del2 || from[i]=='\n')){
            //printf("fuck=%c,%d,%d\n", from[i],begin,i);
            break;
        }
        ++i;
    }
    for(j=begin;j<i;++j){
        to[j-begin] = from[j];
    }
}

void GetIP_Port(char* str, char* ip, char* port) {
    int len = str_size(str);
    int i = 0;
    int j = 0;
    while (str[i] == ' ' && i < len) {
        i++;
    }
    
    while (str[i+j] != ' ' && str[i+j] != ':' && str[i+j] != '\n' && str[i+j] != '\0' && j <len) {
        j++;
    }
    strncpy(ip, str+i, j);
    i = j;
    j = 0;
    while ((str[i] == ' ' || str[i] == ':') && i < len) {
        i++;
    }
    
    while (str[i+j] != ' ' && str[i+j] != ':' && str[i+j] != '\n' && str[i+j] != '\0' && j <len) {
        j++;
    }
    strncpy(port, str+i, j);
}


unsigned int GetFileSize(char* file) {
    
    struct stat inf;
    if (stat(file, &inf) == -1) {
        return 0;
    }
    return (unsigned int)inf.st_size;
    
}