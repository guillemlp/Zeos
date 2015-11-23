/*
 * libc.c 
 */

#include <libc.h>

#include <types.h>

int errno;

int perror() {
    return errno;
}
// 1
void exit() {
    __asm__ __volatile__(
        "int $0x80\n\t"
        :
        :"a" (1));
}

// 2
int fork(void) {
    int ret = -1;
    asm("movl $2, %%eax;"
        "int $0x80;"
        : "=r" (ret));
    
    if (ret >= 0) return ret;
    else {
        errno = ret;
        return -1; 
    }
}

// 4
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

// 10
int gettime() {
    int ret = -1;
    asm("movl $10, %%eax;"
        "int $0x80;"
        : "=r" (ret));
    
    if (ret >= 0) return ret;
    else {
        errno = ret;
        return -1; 
    }
}

// 19
int clone(void (*function)(void), void *stack) {
    int ret = -1;
    asm("int $0x80;"
        : "=r" (ret)
        : "a" (19), "b"(function), "c" (stack) );
    
    if (ret >= 0) return ret;
    else {
        errno = ret;
        return -1; 
    }
}

// 20
int getpid(void) {
   int ret = -1;
    asm("movl $20, %%eax;"
        "int $0x80;"
        : "=r" (ret));
    
    if (ret >= 0) return ret;
    else {
        errno = ret;
        return -1; 
    } 
}

// 21
int sem_init(int n_sem, unsigned int value) {
    //n_sem: identifier of the sempahore to be initialized
    //value: initial value of the counter of the sempahore
    //returns -1 if error, 0 if OK
    int ret = -1;
    asm("int $0x80;"
        : "=r" (ret)
        : "a" (21), "b"(n_sem), "c" (value) );
    
    if (ret >= 0) return ret;
    else {
        errno = ret;
        return -1; 
    }

}

// 22
int sem_wait(int n_sem) {
    //n_sem: identifier of the sempahore to be initialized
    //returns -1 if error, 0 if OK
    int ret = -1;
    asm("int $0x80;"
        : "=r" (ret)
        : "a" (21), "b"(n_sem) );
    
    if (ret >= 0) return ret;
    else {
        errno = ret;
        return -1; 
    }
}

// 23
int sem_signal(int n_sem) {
    //n_sem: identifier of the sempahore to be initialized
    //returns -1 if error, 0 if OK
    int ret = -1;
    asm("int $0x80;"
        : "=r" (ret)
        : "a" (21), "b"(n_sem) );
    
    if (ret >= 0) return ret;
    else {
        errno = ret;
        return -1; 
    }
}

// 24
int sem_destroy(int n_sem) {
    //n_sem: identifier of the sempahore to be initialized
    //returns -1 if error, 0 if OK
    int ret = -1;
    asm("int $0x80;"
        : "=r" (ret)
        : "a" (21), "b"(n_sem) );
    
    if (ret >= 0) return ret;
    else {
        errno = ret;
        return -1; 
    }
}

// 35
int get_stats(int pid, struct stats *st) {
  int result;
  __asm__ __volatile__ (
    "int $0x80\n\t"
    :"=a" (result)
    :"a" (35), "b" (pid), "c" (st) );
  if (result<0) {
    errno = -result;
    return -1;
  }
  errno=0;
  return result;
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