#include <libc.h>

char buff[24];

int pid;

void test_write() {
	char buffer[3] = "abc";
	write(1,buffer,3);
	write(1,"abc",3);
	write(1,"AAA",3);
}

void test_getpid() {
	char a[10];
	int ticks = getpid();
	itoa(ticks, a);
	write(1,a,1);
}

void test_gettime() {
	char a[10];
	int ticks = gettime();
	itoa(ticks, a);
	write(1,a,3);
}

long inner(long n) 
{
	int i;
	long suma;
	suma = 0;
	for (i=0; i<n; i++) suma = suma + i;
	return suma;	
}
long outer(long n) 
{
	int i;
	long acum;
	acum = 0;
	for (i=0; i<n; i++) acum = acum + inner(i);
	return acum;	
}

int add1(int par1, int par2) {
	return par1+par2;
}
/*
//no input/ouput inline
int add2(int par1, int par2) {
	//  no fa falta ja ho fa
	//  "pushl %ebp;"
	//	 "movl %esp, %ebp;"
	asm("movl 8(%ebp), %eax;"
		 "movl 12(%ebp), %ecx;"
		 "addl %ecx, %eax;");
	// "movl %ebp, %esp;"
	//	 "popl %ebp;"
	//	 "ret;"
}*/
//input/ouput inline
int add_v2(int par1, int par2) {
	int var;
	asm("addl %%ecx, %%eax"
		 : "=r" (var)
		 : "a" (par1), "c" (par2) );
	return var;
}

int __attribute__ ((__section__(".text.main")))
   main(void)
{
   /* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
   /* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */

	//long count, acum;
	//count = 75;
	//acum = 0;
	//acum = outer(count);
	//int num = add1(1,2);
	//int a = add2(2,3);
	//int n = a;

	//test_write();
	test_getpid();
	int j = 0;
    while(1) {
    	if ( j == 10000000) {
    		//test_gettime();
    	}
    	++j;
    }
    return 0;	
}
