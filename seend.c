#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>

struct timespec start, end;

volatile sig_atomic_t run = 1;

void skill(int sig){
    run = 0;
}

int main(int argc, char *argv[]){
    signal(SIGINT, skill);
    signal(SIGTERM, skill);

    int open_flags  = O_RDONLY;
    int write_flags  = O_WRONLY | O_CREAT;
    int page_size  = getpagesize();
    int intervals = 10;
    int interval_time = 2;
    long long total_bytes;
    double bandwidth;
    int r_file;
    int w_file;
    off_t size;
    long long elapsed;
    int count = 0;
    size_t ret;
    int opt;
    double elapsed_sec;
    char filename[PATH_MAX] = {0};

    while((opt = getopt(argc, argv, "f:")) != -1){
        switch(opt){
		case 'f':
                    strncpy(filename, optarg, PATH_MAX -1);
		    break;
	} 
    
    }


    r_file = open(filename, open_flags);
    if(r_file < 0){
   	perror("open");
	return 1;
    }
    w_file = open("copy1.1", write_flags);
    if(w_file < 0){
   	perror("open");
	return 1;
    }

    size = lseek(r_file,0, SEEK_END);



    lseek(r_file, 0, SEEK_SET);
    printf("\nfile size:%ld bytes \n\n", size);
    clock_gettime(CLOCK_MONOTONIC, &start);
    sendfile(w_file, r_file, NULL, size);
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed  = (end.tv_sec - start.tv_sec) * 1000000000LL + 
                       (end.tv_nsec - start.tv_nsec);
    elapsed_sec = elapsed / 1000000000.0;
    total_bytes = (long long) (size);
    bandwidth = total_bytes / elapsed_sec;
    printf("\r bw: %lf MiB/s", bandwidth / (1024 * 1024));
    fflush(stdout);
    count = 0;
    printf("\n\n\n elapsed: %ld s", elapsed);
    printf("\n bw: %lf KiB/s", bandwidth/ 1024);
    printf("\n bw: %lf MiB/s", bandwidth / (1024*1024));
    printf("\n bw: %lf GiB/s\n", bandwidth / (1024 * 1024 * 1024));
    close(r_file);
    close(w_file);

}
