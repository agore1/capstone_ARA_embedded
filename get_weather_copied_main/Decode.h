/*
 * Decode.h
 *
 *  Created on: Dec 4, 2014
 *      Author: austin
 */

#ifndef DECODE_H_
#define DECODE_H_


#define SIZE 128

typedef struct {
	int size;
	int id;
	long pulses[SIZE];
} IRCode;

void parseCode(IRCode *code, char *codeStr);
void stringCopy(int start, int end, char* str, char* res);
void markEmpty(char * arrayToEmpty);


#endif /* DECODE_H_ */
