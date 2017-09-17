#ifndef _T_READ_H_
#define _T_READ_H_

#include "cstr.h"

extern int t_nextfield P((FILE *, char *, unsigned));
extern int t_nextcstrfield P((FILE *, cstr *));
extern int t_nextline P((FILE *));

#endif // _T_READ_H_
