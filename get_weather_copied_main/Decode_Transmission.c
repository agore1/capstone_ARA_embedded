/*
 * Decode_Transmission.c
 *
 *  Created on: Nov 30, 2014
 *      Author: austin
 */
#include <msp430.h>
//#include <stdio.h>
#include <stdlib.h>
#include "Decode_Transmission.h"
#include "sl_common.h"

//#define SIZE 128

/*
int main() {
    WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer

	char *codeStr = "37-5-500,1000,37000,235,18|";
	IRCode Code;

	parseCode(&Code, codeStr);
//	printf("id: %d\n", Code.id);
//	printf("size: %d\n", Code.size);
//	int i = 0;
//	for (i; i < Code.size; i++) {
//		printf("pulse %d: %d\n", i, Code.pulses[i]);
//	}
	return 0;
}
*/

void parseCode(IRCode *code, char *codeStr) {
	int i = 0;
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
//				len = end-start;
				//Debugging
				len = 4;
				char *res1 = calloc(sizeof(char),len);
				stringCopy(start, end-1,(char*)codeStr,res1);
//				pal_Strncpy(res1,codeStr, len);
				result = atol(res1);
				/*Clear res1 to allow it to be reused - somehow this causes result to produce an incorrect conversion*/
				free(res1);
				_nop();
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
			/*A comma indicates the end of a number, convert it */
			case ',':
				len = end-start;
				char *res2 = calloc(sizeof(char),len);
				stringCopy(start, end-1,codeStr,res2);
//				pal_Strncpy(res2,codeStr, len);
				result = atol(res2);
				/*Clear res2 to allow it to be reused*/
				free(res2);
				_nop();
				start = i+1;
				end = i+1;
				code->pulses[code_index++] = result;
				break;
			case '|':
				len = end-start;
				char *res3 = calloc(sizeof(char),len);
				stringCopy(start, end-1,codeStr,res3);
//				pal_Strncpy(res3,codeStr, len);
				result = atol(res3);
				/*Clear res3 to allow it to be reused*/
				free(res3);
				_nop();
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
	res[++i] = '\0';
	_nop();
}
