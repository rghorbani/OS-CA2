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











