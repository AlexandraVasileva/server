#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <string.h>

#define FILENAME "matrix"
#define IPCNAME "/bin/bash"
#define ST_OUT 1
#define ANSWERNAME "server_answer"
#define BUF 2
#define PART 8

struct targ {
	int repeat; // how many operations this thread has to carry out
	int line; // first line (numeration starts with 0)
	int column; // first column (numeration starts with 0)
	int clnum;
	int* final_res;
	int n;
	int mesid;
	int* matrix;
};

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

void * threadf (void * temporal){ // the thread function
	struct targ * arguments = (struct targ *) temporal;
	int res, i, j;
	int rp = (*arguments).repeat;
	int ln = (*arguments).line;
	int cl = (*arguments).column;
	int mynum = (*arguments).clnum;
	int n = (*arguments).n;
	int mesid = (*arguments).mesid;
	int* matrix = malloc(2*n*n*sizeof(int));
	matrix = (*arguments).matrix;
	
	int integer = sizeof(int);
	int part = PART;

	struct mymsgbuf signal, endsignal;

	if(msgrcv(mesid, &signal, sizeof(char), 5*mynum+1, 0) < 0){
		printf("Error: cannot receive the start message\n");
		exit(-1);
	}
	struct info_coord *info_coord = malloc(sizeof (struct info_coord));
	info_coord->mtype = 5*mynum+2;
	info_coord->repeat = rp;
	info_coord->line = ln;
	info_coord->column = cl;
	info_coord->n = n;
	if(msgsnd(mesid, info_coord, 5*sizeof(int), 0) == -1){
		printf("Error: cannot send the task message (coordinates)\n");
		exit(-1);
	}
	int partnum = 2*n*n/part;
	if(2*n*n % part != 0) partnum ++;

	struct info_partmatrix *info_partmatrix = malloc(sizeof(struct info_partmatrix) + part*sizeof(int));
	info_partmatrix->mtype = 5*mynum+3;

	for(i=0; i<(partnum-1); i++){
		memcpy(info_partmatrix->partmatrix, matrix+i*part, part*sizeof(int));
		if(msgsnd(mesid, info_partmatrix, part*sizeof(int), 0) == -1){
			printf("Error: cannot send the task message (partmatrix)\n");
			exit(-1);
		}
	}

	int left = 2*n*n % part;
	if(left == 0) left = part;
	memcpy(info_partmatrix->partmatrix, matrix+(partnum-1)*part, left*sizeof(int));

	if(msgsnd(mesid, info_partmatrix, left*sizeof(int), 0) == -1){
		printf("Error: cannot send the task message (partmatrix)\n");
		exit(-1);
	}
	
	struct array *ready = malloc(sizeof(struct array) + part*sizeof(int));
	int* ready_final = malloc(rp*sizeof(int));
	partnum = rp/part;
	if(rp%part != 0) partnum ++;

	int received;
	
	for(i=0; i<partnum; i++){
		if((received = msgrcv(mesid, ready, part*sizeof(int), 5*mynum+4, 0)) < 0){
			printf("Error: cannot receive the result message\n");
			exit(-1);
		}
		memcpy(ready_final+i*part, ready->result, received);
	}

	endsignal.mtype = 5*mynum+5;
	endsignal.mtext[0] = 'f';

	if(msgsnd(mesid, &endsignal, sizeof(char), 0) == -1){
		printf("Error: cannot send the end signal\n");
		exit(-1);
	}
	// writing the results
	memcpy((arguments->final_res)+n*ln+cl, ready_final, rp*sizeof(int));

	return;
}

int main(int argc, char* argv[]){
	
	errno = 0; // getting the client number from argument
	int step = sizeof(char);
	char* unvalid;
	int m = strtol(argv[1], &unvalid, 10);
	if ((errno != 0) || (*unvalid != '\0')){
		printf("Error: invalid matrix size\n");
		exit(-1);
	}
	m++;

	int fd;

	(void)umask(0);

	if((fd = open(FILENAME, O_RDONLY)) < 0){
		printf("Error: cannot open the matrix file\n");
		exit(-1);
	}

	int* matrix;
	int integer = sizeof(int);
	matrix = (int*)malloc(BUF*integer);

	size_t size;
	int counter = 0;
	while((size = read(fd, matrix+counter, BUF*integer)) != 0){ // reading the matrix file into one big array
		if(size < 0){
			printf("Error: cannot read the matrix file\n");
			exit(-1);
		}
		counter += size/integer;
		matrix = (int*)realloc(matrix, (counter + BUF)*integer);
	}

	counter += size/integer;
	matrix = (int*)realloc(matrix, (counter + BUF)*integer);

	counter /= 2;
	int n = (int)sqrt((double)counter);

	if(m > counter) {
		printf("Warning: the number of threads is excessive for this task; only %d threads will be used\n", counter);
		m = counter;
	}
	
	int* result = (int*) malloc(counter*integer);

	int uneven = counter % m;
	int portion = (counter - uneven) / m; // uneven threads will have portion+1 repeats, m-uneven will have portion repeats
	
	struct targ arg[m];// = malloc(sizeof(struct targ) + 2*n*n*sizeof(int));
	pthread_t names[m];
	int k, rest;


	key_t skey = ftok(IPCNAME, 0); // creating the semaphore
	if(skey < 0){
		printf("Error: cannot generate the key\n");
		exit(-1);
	}
	int descr = semget(skey, 1, 0666|IPC_CREAT);
	if(descr < 0){
		printf("Error: cannot create the semaphore\n");
		exit(-1);
	}

	struct sembuf mybuf;
	mybuf.sem_num = 0;
	mybuf.sem_flg = 0;
	mybuf.sem_op = 1;

	if(semop(descr, &mybuf, 1) < 0){ // opening the file for clients
		printf("Error: cannot operate the semaphore\n");
		exit(-1);
	}

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


	for(k=0; k<(m-uneven); k++){ // creating threads
//		arg+k = malloc(sizeof(struct targ) + 2*n*n*sizeof(int));
		arg[k].repeat = portion;
		arg[k].n = n;
		arg[k].final_res = result;
		arg[k].line = (portion*k)/n;
		arg[k].column = (portion*k)%n;
		arg[k].clnum = k;
		arg[k].n = n;
		arg[k].mesid = mesid;
		arg[k].matrix = matrix;
		if(pthread_create(&(names[k]), NULL, threadf, (void *)(arg+k)) != 0){
			printf("Error: cannot create the new thread\n");
			exit(-1);
		}
	}

	for(k=m-uneven; k<m; k++){ // creating threads
		arg[k].repeat = portion + 1;
		arg[k].n = n;
		arg[k].final_res = result;
		arg[k].line = ((m-uneven)*portion + (portion+1)*(k-m+uneven))/n;
		arg[k].column = ((m-uneven)*portion + (portion+1)*(k-m+uneven))%n;
		arg[k].clnum = k;
		arg[k].n = n;
		arg[k].mesid = mesid;
		arg[k].matrix = matrix;
		if(pthread_create(&(names[k]), NULL, threadf, (void *)(arg+k)) != 0){
			printf("Error: cannot create the new thread\n");
			exit(-1);
		}	
	}
	
	for(k=0; k<m; k++){
		if(pthread_join(names[k], NULL) != 0){
			printf("Error: cannot join the thread\n");
			exit(-1);
		}
	}

	if(close(fd) < 0){
		printf("Error: cannot close the matrix file\n");
		exit(-1);
	}
	
	if((fd = open(ANSWERNAME, O_WRONLY|O_CREAT, 0666)) < 0){
		printf("Error: cannot create the answer file\n");
		exit(-1);
	}
	char* num = (char*)malloc(10);
	char space = ' ';
	char enter = '\n';
	
	for(k = 0; k < counter; k++){
		sprintf(num, "%d", *(result+k));
		num = (char*)realloc(num, strlen(num)+1);
		if(write(fd, num, strlen(num)) != strlen(num)){
			printf("Error: cannot print the result array\n");
			exit(-1);
		}
	
		if(write(fd, &space, sizeof(char)) != sizeof(char)){
			printf("Error: cannot print the result array\n");
			exit(-1);
		}
	
		if(((k+1)%n) == 0){
			if(write(fd, &enter, sizeof(char)) != sizeof(char)){
				printf("Error: cannot print the result array\n");
				exit(-1);
			}
		}
	
	}

	if(semctl(descr, 0, IPC_RMID, NULL) < 0){
		printf("Error: cannot delete the semaphore\n");
		exit(-1);
	} 

	return 0;
}
