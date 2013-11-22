#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

int main(int argc, char* argv[]){

	if(argc < 2){
		printf("Error: invalid number of arguments\n");
		exit(-1);
	}
	
	errno = 0; // getting the client number from argument
	int step = sizeof(char);
	char* unvalid;
	int clnum = strtol(argv[1], &unvalid, 10);
	if ((errno != 0) || (*unvalid != '\0')){
		printf("Error: invalid matrix size\n");
		exit(-1);
	}

	int i;
	char* num = (char*)malloc(10);
	int res;

	for(i=0; i<clnum; i++){
		num = (char*)realloc(num, strlen(num) + 1);
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
