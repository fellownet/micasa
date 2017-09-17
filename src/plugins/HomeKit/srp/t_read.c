#include <stdio.h>

#include "t_read.h"

#define FSEPARATOR	':'

int
t_nextfield(fp, s, max)
FILE * fp;
char * s;
unsigned max;
{
  int c, count = 0;

  while((c = getc(fp)) != EOF) {
    if(c == '\r' || c == '\n') {
      ungetc(c, fp);
      break;
    }
    else if(c == FSEPARATOR)
      break;
    if(count < max - 1) {
      *s++ = c;
      ++count;
    }
  }
  *s++ = '\0';
  return count;
}

int
t_nextcstrfield(fp, s)
FILE * fp;
cstr * s;
{
  int c, count = 0;
  char cc;

  cstr_set_length(s, 0);
  while((c = getc(fp)) != EOF) {
    if(c == '\r' || c == '\n') {
      ungetc(c, fp);
      break;
    }
    else if(c == FSEPARATOR)
      break;

    cc = c;
    cstr_appendn(s, &cc, 1);
    ++count;
  }
  return count;
}

int
t_nextline(fp)
FILE * fp;
{
  int c;

  while((c = getc(fp)) != '\n')
    if(c == EOF)
      return -1;
  return 0;
}
