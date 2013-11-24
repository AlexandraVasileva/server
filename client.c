#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <math.h>
#include <unistd.h>
#include <string.h>

#define FILENAME "matrix"
#define NAME "log.txt"
#define IPCNAME "/bin/bash"
#define BUF 2
#define TNUM 2
#define ST_OUT 1
#define PART 8

struct info_coord {
	long mtype;
	int repeat;
	int line;
	int column;
	int n;
};

struct info_partmatrix {
	long mtype;
	int partmatrix[0];
};


struct array {
	long mtype;
	int result[0];
};

struct mymsgbuf {
	long mtype;
	char mtext[1];
};

struct mysembuf {
	short sem_num;
	short sem_op;
	short sem_flg;
};

void semprint(struct sembuf mybuf, int descr, int mynum, char* message){

	mybuf.sem_op = -1;
	if(semop(descr, &mybuf, 1) < 0){ // waiting to print
		printf("Error: cannot operate the semaphore\n");
		exit(-1);
	}

	printf("Client %d %s\n", mynum, message); // critical section

	mybuf.sem_op = 1;

	if(semop(descr, &mybuf, 1) < 0){ // opening the file for other clients
		printf("Error: cannot operate the semaphore\n");
		exit(-1);
	}
	
	return;
}


int* clientf (int rp, int ln, int cl, int n, int* matrix){ // the client function
	int i, j, res;
	int integer = sizeof(int);
	int* result = (int*)malloc(rp*integer);

	for(j = 0; j < rp; j++){
		res = 0;	
		for(i=0; i<n; i++){
			res += (matrix[n*ln+i] * matrix[n*n+cl+n*i]);
		}
		result[j] = res;
		cl++;
		if(cl == n){
			cl = 0;
			ln ++;
		} // order: line and all columns
	}
	return result;
}

int main(int argc, char* argv[]){
	
	if (argc != 2){
		printf("Error: invalid number of arguments\n");
		exit(-1);
	}

	errno = 0; // getting the client number from argument
	int step = sizeof(char);
	char** unvalid = (char**)malloc(step);
	int mynum = strtol(argv[1], unvalid, 10);
	if ((errno != 0) || (**unvalid != '\0')){
		printf("Error: invalid matrix size\n");
		exit(-1);
	}

	int fd;
	(void)umask(0);
	if(close(ST_OUT) == -1){
		printf("Error: cannot close standard output\n");
		exit(-1);
	}
	if((fd = open(NAME, O_WRONLY|O_APPEND|O_CREAT, 0666)) != 1){
		printf("Error: cannot create the log file\n");
		exit(-1);
	}

	key_t key = ftok(IPCNAME, 0); // creating the semaphore
	if(key < 0){
		printf("Error: cannot generate the key\n");
		exit(-1);
	}
	int descr = semget(key, 1, 0666|IPC_CREAT);
	if(descr < 0){
		printf("Error: cannot create the semaphore\n");
		exit(-1);
	}

	struct sembuf mybuf;
	mybuf.sem_num = 0;
	mybuf.sem_flg = 0;
	semprint (mybuf, descr, mynum, "here and ready to count");

	key_t mkey = ftok(IPCNAME, 2); // creating the messagebox
	if(mkey < 0){
		printf("Error: cannot generate the key\n");
		exit(-1);
	}

	int mesid = msgget (mkey, 0666|IPC_CREAT); // messagebox ID
	if(mesid < 0){
		printf("Error: cannot create the messagebox\n");
		exit(-1);
	}


	struct mymsgbuf signal;
	signal.mtype = 5*mynum+1;
	signal.mtext[0] = 's';

	if(msgsnd(mesid, &signal, sizeof(char), 0) == -1){
		printf("Error: cannot send the start message\n");
		exit(-1);
	}

	int part = PART;
	struct info_coord info_coord;
	if(msgrcv(mesid, &info_coord, 5*sizeof(int), 5*mynum+2, 0) != 5*sizeof(int)){
		printf("Error: cannot read the task message\n");
		exit(-1);
	}

	int* matrix = malloc(info_coord.n*sizeof(int));

	struct info_partmatrix *info_partmatrix = malloc(sizeof(struct info_partmatrix) + part*sizeof(int));
	int partnum = 2*info_coord.n*info_coord.n/part;
	if((2*info_coord.n*info_coord.n % part) != 0) partnum++;
	int read;

	int i;
	for(i=0; i<partnum; i++){
		memcpy(info_partmatrix->partmatrix, matrix+i*part, part*sizeof(int));
		if((read = msgrcv(mesid, info_partmatrix, part*sizeof(int), 5*mynum+3, 0)) == -1){
			printf("Error: cannot read the task message (partmatrix)\n");
			exit(-1);
		}
		memcpy(matrix+i*part, info_partmatrix->partmatrix, read);	
	}

	semprint (mybuf, descr, mynum, "got the data and is counting");

	int* result = clientf(info_coord.repeat, info_coord.line, info_coord.column, info_coord.n, matrix);

	struct array *ready = malloc(sizeof(struct array) + part*sizeof(int));
	ready->mtype = 5*mynum+4;

	partnum = info_coord.repeat/part;
	if((info_coord.repeat%part) != 0) partnum ++;
	
	for(i=0; i<(partnum-1); i++){	
		memcpy(ready->result, result+i*part, part*sizeof(int));
		if(msgsnd(mesid, ready, part*sizeof(int), 0) == -1){
			printf("Error: cannot send the result message\n");
			exit(-1);
		}
	}
	
	int left = info_coord.repeat%part;
	if(left == 0) left = part;

	memcpy(ready->result, result+(partnum-1)*part, left*sizeof(int));
	if(msgsnd(mesid, ready, left*sizeof(int), 0) == -1){
		printf("Error: cannot send the result message\n");
		exit(-1);
	}
		printf("RES %d\n", result[i*part]);

	semprint (mybuf, descr, mynum, "has sent his result data and is ready to quit");

	struct mymsgbuf endsignal;
	if(msgrcv(mesid, &endsignal, sizeof(char), 5*mynum+5, 0) != sizeof(char)){
		printf("Error: cannot receive the end signal\n");
		exit(-1);
	}

	return 0;
}
