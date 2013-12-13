#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>
#include<dirent.h>
#include<fcntl.h>
#include<errno.h>
#include<sys/mman.h>
#include<sys/ipc.h>

#define MAPFILENAME "map"
#define MESSAGEFILENAME "/bin/bash"
#define MAXNAMELENGTH 255

struct mymsgbuf{ // for the counter message
	long mtype;
	int counter;
};

int main(int argc, char* argv[]) {

	struct filedata{ // the main structure which we use in the map file
		char name[MAXNAMELENGTH];
		mode_t mode;
		uid_t uid;
		gid_t gid;
		off_t size;
		time_t ctime;
		time_t mtime;
	} *begin, *temp;

	if(argc != 2){
		printf("Error: invalid number of arguments\n");
		exit(-1);
	}
	
	char* source = argv[1];

	struct dirent *s;
	struct stat bufs;
	DIR *dirs;

	if(!(dirs = opendir(source))) {
		printf("Error: incorrect source directory\n");
		exit(1);
	}
	
	struct stat buf;
	struct filedata data;

	int fd = open(MAPFILENAME, O_RDWR|O_CREAT, 0666);
	if (fd < 0){
		printf("Error: cannot open the map file\n");
		exit(-1);
	}

	int counter = 0;
	char* currname;

	s = readdir(dirs);

	while(s != NULL) {

		counter++;
		if(ftruncate(fd, counter*sizeof(struct filedata)) < 0){
			printf("Error: cannot change the file length\n");
			exit(-1);
		}
		
		begin = (struct filedata*)mmap(NULL, counter*sizeof(struct filedata), PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0); // mapping
		if(begin == MAP_FAILED){
			printf("Error: cannot map\n");
			exit(-1);
		}

		currname = s->d_name;

		if((!strcmp(currname, ".")) || (!strcmp(currname, ".."))) { // skipping the pointers
			s = readdir(dirs);
			continue;
		}

		char* current = malloc(strlen(source)+strlen(currname)+2); // forming the full name
		current = strcpy(current, source);
		current = strcat(current, "/");
		current = strcat(current, currname);

		if(lstat(current, &buf) == -1){
			printf("Error: cannot lstat a file\n");
			exit(-1);
		}

		temp = begin+counter-1;

		strcpy(temp->name, currname); // forming the filedata structure
		temp->mode = buf.st_mode;
		temp->uid = buf.st_uid;
		temp->gid = buf.st_gid;
		temp->size = buf.st_size;
		temp->ctime = buf.st_ctime;
		temp->mtime = buf.st_mtime;

		if(munmap((void*)begin, counter*sizeof(struct filedata)) < 0){
			printf("Error: cannot unmap\n");
			exit(-1);
		}
		
		s = readdir(dirs);
	}

	key_t key;
	if ((key = ftok(MESSAGEFILENAME, 0)) < 0){
		printf("Error: cannot generate the key\n");
		exit(-1);
	}

	int msgdes;
	if((msgdes = msgget(key, 0666|IPC_CREAT)) < 0){
		printf("Error: cannot create the messagebox\n");
		exit(-1);
	}

	struct mymsgbuf mybuf;
	mybuf.mtype = 1;
	mybuf.counter = counter;

	if(msgsnd(msgdes, (struct msgbuf*) &mybuf, sizeof(int), 0) < 0){ // sending the counter message
		printf("Error: cannot send the message\n");
		exit(-1);
	}

	return 0;
}
