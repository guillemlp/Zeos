/*
 * libc.c 
 */

#include <libc.h>

#include <types.h>

int errno;

int perror() {
    return errno;
}

/* 
    asm("movl %%eax, %%ebx"
         : 
         : "a" (fd));
    asm("movl %%eax, %%ecx"
         : 
         : "a" (buffer));
    asm("movl %%eax, %%edx"
         : 
         : "a" (size));
    asm("movl $4, %%eax;"
        "int $0x80;"
        : "=r" (ret));
*/

int write(int fd, char *buffer, int size) {
    int ret = -1;
    asm("movl $4, %%eax;"
        "int $0x80;"
        : "=r" (ret)
        : "b"(fd), "c" (buffer), "d" (size) );
    
    if (ret >= 0) return ret;
    else {
        errno = ret;
        return -1; 
    }
}

void itoa(int a, char *b) {
    int i, i1;
    char c;
  
    if (a==0) { b[0]='0'; b[1]=0; return ;}
  
    i=0;
    while (a>0) {
        b[i]=(a%10)+'0';
        a=a/10;
        i++;
    }

    for (i1=0; i1<i/2; i1++) {
        c=b[i1];
        b[i1]=b[i-i1-1];
        b[i-i1-1]=c;
    }
    b[i]=0;
}

int strlen(char *a) {
    int i;

    i=0;

    while (a[i]!=0) i++;

    return i;
}

