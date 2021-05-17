// init: The initial user-level program

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/fcntl.h"

char *argv[] = { "sh", 0 };

void
handletty(int n)
{
       int pid;
       char devname[] = "/dev/ttyN";

       devname[strlen(devname)-1] = n + '0';
       pid = fork();
       if(pid < 0){
               printf("init: fork failed\n");
               exit();
       }
       if(pid == 0){
               close(0);
               close(1);
               close(2);

               if(open(devname, O_RDWR) < 0){
                       mknod(devname, 1, n);
                       open(devname, O_RDWR);
               }
               dup(0);
               dup(0);

			//    if(n == 1){
               	printf("Starting sh on %s\n", devname);
			//    }
               exec("/bin/sh", argv);  // ucita se program shell
               printf("init: exec sh failed\n");
               exit();
       }
}

int
main(void)
{
	int i, wpid;

	if(getpid() != 1){
		fprintf(2, "init: already running\n");
		exit();
	}

	if(open("/dev/console", O_RDWR) < 0){
		mknod("/dev/console", 1, 1);
		open("/dev/console", O_RDWR);
	}
	dup(0);  // stdout
	dup(0);  // stderr
        //printf("Welcome to console!");
	for (i = 1; i <= 6; i++){
		handletty(i);
	}

	while((wpid=wait()) >= 0)
		;
}
