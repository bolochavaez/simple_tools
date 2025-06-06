#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

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
    void * buffer;
    int intervals = 10;
    int interval_time = 2;
    long long total_bytes;
    long long bandwidth;
    int r_file;
    int w_file;
    off_t size;
    time_t start;
    time_t end;
    time_t elapsed;
    int count = 0;
    size_t ret;
    int opt;
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

    if(posix_memalign(&buffer, page_size, page_size)){
   	perror("posix_memalign");
	return 1;
    }

    lseek(r_file, 0, SEEK_SET);
    printf("\nfile size:%ld bytes \n\n", size);
    start = time(NULL);
    while(ret = read(r_file, buffer, page_size ) > 0){
        write(w_file, buffer, ret);
    }
    end = time(NULL);
    elapsed  = end - start;
    total_bytes = (long long) (size);
    bandwidth = total_bytes / elapsed;
    printf("\r bw: %ld MiB/s", bandwidth / (1024 * 1024));
    fflush(stdout);
    count = 0;
    start = time(NULL);

    printf("\n\n\n elapsed: %ld s", elapsed);
    printf("\n bw: %ld KiB/s", bandwidth/ 1024);
    printf("\n bw: %ld MiB/s", bandwidth / (1024*1024));
    printf("\n bw: %ld GiB/s\n", bandwidth / (1024 * 1024 * 1024));
    close(r_file);
    close(w_file);
    free(buffer);

}
