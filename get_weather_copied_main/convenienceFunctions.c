/*
 * convenienceFunctions.c
 *
 *  Created on: Nov 28, 2014
 *      Author: austin
 */
#define FALSE	0
#define TRUE 	1

#include "convenienceFunctions.h"
#include "string.h"
// Implementation of itoa() - from http://en.wikibooks.org/wiki/C_Programming/C_Reference/stdlib.h/itoa
/* itoa:  convert n to characters in s */
 void itoa(int n, char s[])
 {
     int i, sign;

     if ((sign = n) < 0)  /* record sign */
         n = -n;          /* make n positive */
     i = 0;
     do {       /* generate digits in reverse order */
         s[i++] = n % 10 + '0';   /* get next digit */
     } while ((n /= 10) > 0);     /* delete it */
     if (sign < 0)
         s[i++] = '-';
     s[i] = '\0';
     reverse(s);
 }

/* A utility function to reverse a string - from http://en.wikibooks.org/wiki/C_Programming/C_Reference/stdlib.h/itoa */
  void reverse(char s[])
  {
      int i, j;
      char c;

      for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
          c = s[i];
          s[i] = s[j];
          s[j] = c;
      }
  }

