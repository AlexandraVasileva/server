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

struct inf { // received
	long mtype;
	int repeat;
	int line;
	int column;
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

	// AP: нужно писать в файл честно через write
	printf("Client %d %s\n", mynum, message); // critical section

	mybuf.sem_op = 1;

	if(semop(descr, &mybuf, 1) < 0){ // opening the file for other clients
		printf("Error: cannot operate the semaphore\n");
		exit(-1);
	}
	
	return;
}


int* clientf ( int rp, int ln, int cl){ // the client function
	int i, j, res, n, fd;
	int integer = sizeof(int);
	char* filename = FILENAME;

	int* matrix = (int*)malloc(integer);
	int* result = (int*)malloc(rp*integer);

	// AP: матрицы для умножения нцжно получить от сервера через сообщения
	if((fd = open(filename, O_RDONLY, 0)) < 0){
		printf("Error: cannot open the matrix file\n");
		exit(-1);
	}

	size_t size;
	int counter = 0;
	while((size = read(fd, matrix+counter, BUF*integer)) != 0){ // reading the matrix from shared memory into one big array
		if(size < 0){
			printf("Error:cannot read the matrix file\n");
			exit(-1);
		}
		counter += size/integer;
		matrix = (int*)realloc(matrix, (counter + 1)*integer);
	}

	counter += size/integer;
	matrix = (int*)realloc(matrix, (counter + 1)*integer);
	counter /= 2;
	n = (int)sqrt((double)counter); // matrix size


	for(j = 0; j < rp; j++){
		res = 0;	
		for(i=0; i<n; i++){
			res += (matrix[n*ln+i] * matrix[counter+cl+n*i]);
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
	signal.mtype = 4*mynum+1;
	signal.mtext[0] = 's';

	
	if(msgsnd(mesid, &signal, sizeof(char), 0) == -1){
		printf("Error: cannot send the start message\n");
		exit(-1);
	}

	struct inf info;
	if(msgrcv(mesid, &info, 3*sizeof(int), 4*mynum+2, 0) != 3*sizeof(int)){
		printf("Error: cannot read the task message\n");
		exit(-1);
	}

	semprint (mybuf, descr, mynum, "got the data and is counting");

	int* result = clientf(info.repeat, info.line, info.column);

	struct array *ready = malloc(sizeof(struct array) + info.repeat * sizeof(int));
	ready->mtype = 4*mynum+3;
	memcpy(ready->result, result, info.repeat * sizeof(int));

	// AP: результат может быть больше макс размера сообщения - это нужно учесть
	if(msgsnd(mesid, ready, info.repeat*sizeof(int), 0) == -1){
		printf("Error: cannot send the result message\n");
		exit(-1);
	}

	semprint (mybuf, descr, mynum, "has sent his result data and is ready to quit");

	struct mymsgbuf endsignal;
	if(msgrcv(mesid, &endsignal, sizeof(char), 4*mynum+4, 0) != sizeof(char)){
		printf("Error: cannot receive the end signal\n");
		exit(-1);
	}

	return 0;
}
