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

#define FILENAME "matrix"
#define IPCNAME "/bin/bash"
#define ST_OUT 1
#define ANSWERNAME "server_answer"
#define BUF 2

struct targ {
	int repeat; // how many operations this thread has to carry out
	int line; // first line (numeration starts with 0)
	int column; // first column (numeration starts with 0)
	int clnum;
	int* final_res;
	int n;
};

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

void * threadf (void * temporal){ // the thread function
	struct targ * arguments = (struct targ *) temporal;
	int res, i, j;
	int rp = (*arguments).repeat;
	int ln = (*arguments).line;
	int cl = (*arguments).column;
	int mynum = (*arguments).clnum;
	int n = (*arguments).n;

	int integer = sizeof(int);


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

	struct mymsgbuf signal, endsignal;


	if(msgrcv(mesid, &signal, sizeof(char), 4*mynum+1, 0) < 0){
		printf("Error: cannot receive the start message\n");
		exit(-1);
	}

	struct inf info;
	info.mtype = 4*mynum+2;
	info.repeat = rp;
	info.line = ln;
	info.column = cl;

	if(msgsnd(mesid, &info, 3*sizeof(int), 0) == -1){
		printf("Error: cannot send the task message\n");
		exit(-1);
	}

	struct array *ready = malloc(sizeof(struct array) + sizeof(int) * rp);

	if(msgrcv(mesid, ready, rp * sizeof(int), 4*mynum+3, MSG_NOERROR) < 0){
		printf("Error: cannot receive the result message\n");
		exit(-1);
	}
	endsignal.mtype = 4*mynum+4;
	endsignal.mtext[0] = 'f';

	if(msgsnd(mesid, &endsignal, sizeof(char), 0) == -1){
		printf("Error: cannot send the end signal\n");
		exit(-1);
	}

	// writing the results
	for(i=0; i<rp; i++) arguments->final_res[n*ln+cl+i] = ready->result[i];

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
	
	struct targ arg[m];
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


	for(k=0; k<(m-uneven); k++){ // creating threads
		arg[k].repeat = portion;
		arg[k].n = n;
		arg[k].final_res = result;
		arg[k].line = (portion*k)/n;
		arg[k].column = (portion*k)%n;
		arg[k].clnum = k;
		arg[k].n = n;
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
	

	if(close(ST_OUT) < 0){
		printf("Error: cannot close standard output\n");
		exit(-1);
	}
	

	if((fd = open(ANSWERNAME, O_WRONLY|O_CREAT, 0666)) < 0){
		printf("Error: cannot create the answer file\n");
		exit(-1);
	}

	for(k = 0; k < counter; k++){
		printf("%d ", *(result + k));
		if((k+1)%n == 0){
			printf("\n");
		}
	}

	if(semctl(descr, 0, IPC_RMID, NULL) < 0){
		printf("Error: cannot delete the semaphore\n");
		exit(-1);
	} 

	return 0;
}
