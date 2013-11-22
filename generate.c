#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>

#define FILENAME "matrix"
#define RESTRICT 100

int main(int argc, char* argv[]){
	
	if (argc != 2){
		printf("Error: invalid number of arguments\n");
		exit(-1);
	}

	errno = 0; // getting the number from argument
	int step = sizeof(char);
	char** unvalid = (char**)malloc(step);
	int n = strtol(argv[1], unvalid, 10);
	if ((errno != 0) || (**unvalid != '\0')){
		printf("Error: invalid matrix size\n");
		exit(-1);
	}
	
	int fd;

	(void)umask(0);

	if((fd = open(FILENAME, O_WRONLY|O_CREAT, 0666)) < 0){
		printf("Error: cannot create the matrix file\n");
		exit(-1);
	}

	int i;
	int lim = 2*n*n;
	int current;
	for (i=0; i< lim; i++){
		current = random()%RESTRICT; // just so that numbers won't be too big
		if((i%n) == 0) printf("\n");
		if(i != 0) {
			if((i%(lim/2)) == 0) printf("\n");
		}
		printf("%d ", current);
		if(write(fd, &current, sizeof(int)) != sizeof(int)){
			printf("Error: cannot write in the matrix file\n");
			exit(-1);
		}
	}

	printf("\n\n");

	if(close(fd) < 0){
		printf("Error: cannot close the matrix file\n");
		exit(-1);
	}

	return 0;
}
