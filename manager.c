#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#define CLNUM 3 // threads number from the previous task, m

int main(){

	int i;
	// AP: было же скаазано принимать количество клиентов через аргументы - исправить
	int clnum = CLNUM;
	char* num = (char*)malloc(10);
	int res;

	for(i=0;i<clnum; i++){
		// AP: либо не используйте нестандартную функцию, либо исопльзуйте ее правильно - лишняя память не должна тратиться, 10 значных чисел у вас нет - исправить
		sprintf(num, "%d", i);
		if((res = fork()) < 0){
			printf("Error: cannot fork\n");
			exit(-1);
		}
		if(res == 0){

			if(execl("client", "client", num, NULL) == -1){
				printf("Error: cannot exec the client program\n");
				exit(-1);
			}
		}
	}

	if(execl("server", "server", num, NULL) == -1){
		printf("Error: cannot exec the server program\n");
		exit(-1);
	}
	
	return 0;
}
