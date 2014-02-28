#ifndef __FUNCL___
#include "defines.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>


void split(char*, int, char, char*);
void split2(char*, char, char, char*);
void split3(char*, int, char, char*);
void c_error(char*);
void c_log(char*);
void itoa(int num, char* str);
void MakeStrID(int idNo, char* str, int strSize);
void Trim (char* trimmed, int size);

#define __FUNCL___
#endif