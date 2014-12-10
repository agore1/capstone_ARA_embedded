/*
 * Decode.c
 *	Andrew's new, non-dynamic decoder.
 *  Created on: Dec 4, 2014
 *      Author: austin
 */


//#include <stdio.h>
#include <stdlib.h>
#include "Decode.h"
//#include <string.h>

//#define SIZE 256

/*
typedef struct {
	int size;
	int id;
	long pulses[SIZE];
} IRCode;


void parseCode(IRCode *code, char *codeStr);
void stringCopy(int start, int end, char* str, char* res);

int main() {
	char *codeStr = "code start:37-4-500,1000,500,1000|";
	IRCode Code;

	char *codePtr = strstr(codeStr, "code start");
	codePtr += strlen("code start:");


	parseCode(&Code, codePtr);
	printf("id: %d\n", Code.id);
	printf("size: %d\n", Code.size);
	int i = 0;
	for (i; i < Code.size; i++) {
		printf("pulse %d: %d\n", i, Code.pulses[i]);
	}
	return 0;
}
*/
void parseCode(IRCode *code, char *codeStr) {
	int i = 0;
	char one[1] = {0};
	char two[2] = {0};
	char three[3] = {0};
	char four[4] = {0};
	char five[5] = {0};
	char c = codeStr[0];
	int start = 0;
	int end = 0;
	int code_index = 0;
	int len = 0;
	long result = 0;
	int id_set = 0;
	int size_set = 0;
	for (i; c != '|'; i++) {
		c = codeStr[i];
		switch (c) {
			case '-':
				len = end-start;
				switch (len) {
					case 1:
						stringCopy(start, end-1,codeStr,one);
						result = atol(one);
						break;
					case 2:
						stringCopy(start, end-1,codeStr,two);
						result = atol(two);
						break;
					case 3:
						stringCopy(start, end-1,codeStr,three);
						result = atol(three);
						break;
					case 4:
						stringCopy(start, end-1,codeStr,four);
						result = atol(four);
						break;
					case 5:
						stringCopy(start, end-1,codeStr,five);
						result = atol(five);
						/*Place a null character in the first position of five to prevent altol overflow*/
						five[0] = '\0';
						break;
					default:
//						printf("oh no!\n");
				}
				start = i+1;
				end = i+1;
				if (!id_set) {
					id_set = 1;
					code->id = result;
				} else {
					size_set = 1;
					code->size = result;
				}
				break;
			case ',':
				len = end-start;
				switch (len) {
					case 1:
						stringCopy(start, end-1,codeStr,one);
						result = atol(one);
						break;
					case 2:
						stringCopy(start, end-1,codeStr,two);
						result = atol(two);
						break;
					case 3:
						stringCopy(start, end-1,codeStr,three);
						result = atol(three);
						break;
					case 4:
						stringCopy(start, end-1,codeStr,four);
						result = atol(four);
						break;
					case 5:
						stringCopy(start, end-1,codeStr,five);
						result = atol(five);
						/*Place a null character in the first position of five to prevent altol overflow*/
//						five[0] = '\0';
						markEmpty(five);
						break;
					default:
//						printf("oh no!\n");
				}
				start = i+1;
				end = i+1;
				code->pulses[code_index++] = result;
				break;
			case '|':
				len = end-start;
				switch (len) {
					case 1:
						stringCopy(start, end-1,codeStr,one);
						result = atol(one);
						break;
					case 2:
						stringCopy(start, end-1,codeStr,two);
						result = atol(two);
						break;
					case 3:
						stringCopy(start, end-1,codeStr,three);
						result = atol(three);
						break;
					case 4:
						stringCopy(start, end-1,codeStr,four);
						result = atol(four);
						break;
					case 5:
						stringCopy(start, end-1,codeStr,five);
						result = atol(five);
						/*Place a null character in the first position of five to prevent altol overflow*/
						five[0] = '\0';
						break;
					default:
//						printf("oh no!\n");
				}
				start = i+1;
				end = i+1;
				code->pulses[code_index++] = result;
				break;
			default:
				end++;
				break;
		}
	}
	code->pulses[code_index] = '\0';
}

void stringCopy(int start, int end, char* str, char* res) {
	int i = start;
	for (i;i<= end; i++){
		res[i-start] = str[i];

	}
//	res[++i] = '\0';
}

void markEmpty(char * arrayToEmpty){
	arrayToEmpty[0] = '\0';
}


