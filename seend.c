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
#include <sys/statvfs.h>
#include <sys/stat.h>

struct timespec start, end;

volatile sig_atomic_t run = 1;

void skill(int sig){
    run = 0;
}

double copy_file_chunk(int r_file, int w_file, int page_size){
    size_t ret;
    long long elapsed;
    double elapsed_sec;
    void * buffer;

    if(posix_memalign(&buffer, page_size, page_size)){
   	perror("posix_memalign");
	return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &start);
    while((ret = read(r_file, buffer, page_size )) > 0){
        write(w_file, buffer, ret);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed  = (end.tv_sec - start.tv_sec) * 1000000000LL + 
                       (end.tv_nsec - start.tv_nsec);
    free(buffer);
    return elapsed;

}

double copy_file_sendfile(int r_file, int w_file, off_t size){
    long long elapsed;
    double elapsed_sec;

    clock_gettime(CLOCK_MONOTONIC, &start);
    sendfile(w_file, r_file, NULL, size);
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed  = (end.tv_sec - start.tv_sec) * 1000000000LL + 
                       (end.tv_nsec - start.tv_nsec);
    return elapsed;
}


double copy_ap_esync(int r_file, int w_file, int page_size){
    size_t ret;
    long long elapsed;
    double elapsed_sec;
    void * buffer;

    if(posix_memalign(&buffer, page_size, page_size)){
   	perror("posix_memalign");
	return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &start);
    while((ret = read(r_file, buffer, page_size )) > 0){
        write(w_file, buffer, ret);
	fsync(w_file);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed  = (end.tv_sec - start.tv_sec) * 1000000000LL + 
                       (end.tv_nsec - start.tv_nsec);
    free(buffer);
    return elapsed;

}


double copy_ap_estat(int r_file, int w_file, int page_size, char * w_file_name){
    size_t ret;
    long long elapsed;
    double elapsed_sec;
    void * buffer;
    struct stat file_stat;

    if(posix_memalign(&buffer, page_size, page_size)){
   	perror("posix_memalign");
	return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &start);
    while((ret = read(r_file, buffer, page_size )) > 0){
	
        if(stat(w_file_name, &file_stat) != 0){
            perror("stat");
    	return -1;
        }
        write(w_file, buffer, ret);

    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed  = (end.tv_sec - start.tv_sec) * 1000000000LL + 
                       (end.tv_nsec - start.tv_nsec);
    free(buffer);
    return elapsed;

}





void print_stats(off_t size, long long elapsed){
    double elapsed_sec = elapsed / 1000000000.0;
    long long total_bytes = (long long) (size);
    double bandwidth = total_bytes / elapsed_sec;
    printf("\n\n\n elapsed: %ld s", elapsed);
    printf("\n bw: %lf KiB/s", bandwidth/ 1024);
    printf("\n bw: %lf MiB/s", bandwidth / (1024*1024));
    printf("\n bw: %lf GiB/s\n", bandwidth / (1024 * 1024 * 1024));

}

int check_disk_space(const char * path, off_t size){
    struct statvfs stat;
    if(statvfs(path, &stat) != 0){
        perror("statvfs");
	return -1;
    }
    off_t available = (off_t)stat.f_bavail * stat.f_frsize;
    if (available > size) return 1;
    else return 0;
}

void drop_caches(){
    sync();
    int dc_file = open("/proc/sys/vm/drop_caches", O_WRONLY);
    if (dc_file >= 0){
        write(dc_file, "3", 1);
    } else {
        perror("open");

    }
    close(dc_file);
}

int main(int argc, char *argv[]){
    signal(SIGINT, skill);
    signal(SIGTERM, skill);

    int open_flags  = O_RDONLY;
    int write_flags  = O_WRONLY | O_CREAT;
    int page_size  = getpagesize();
    int r_file;
    int w_file;
    off_t size;
    long long elapsed;
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

    size = lseek(r_file,0, SEEK_END);
    lseek(r_file, 0, SEEK_SET);
    printf("\nfile size:%ld bytes \n\n", size);

    if(check_disk_space(".", size) <= 0){
	printf("\ncould not allocate space \n");
	close(r_file);
        return -1;
    }

    w_file = open("copy1.1", write_flags);
    if(w_file < 0){
   	perror("open");
	return 1;
    }

    printf("\nuser space stats\n", size);
    elapsed = copy_file_chunk(r_file, w_file, page_size);
    print_stats(size, elapsed);

    lseek(r_file, 0, SEEK_SET);
    lseek(w_file, 0, SEEK_SET);
    printf("\nsend stats\n", size);
    elapsed = copy_file_sendfile(r_file, w_file, size);
    print_stats(size, elapsed);

    lseek(r_file, 0, SEEK_SET);
    lseek(w_file, 0, SEEK_SET);
    drop_caches();
    printf("\nno cache send stats\n", size);
    elapsed = copy_file_sendfile(r_file, w_file, size);
    print_stats(size, elapsed);

    lseek(r_file, 0, SEEK_SET);
    lseek(w_file, 0, SEEK_SET);
    drop_caches();
    printf("\nno cache user space stats\n", size);
    elapsed = copy_file_chunk(r_file, w_file, page_size);
    print_stats(size, elapsed);


    lseek(r_file, 0, SEEK_SET);
    lseek(w_file, 0, SEEK_SET);
    printf("\nantipattern: excessive fsync\n", size);
    elapsed =  copy_ap_esync(r_file, w_file, page_size);
    print_stats(size, elapsed);


    lseek(r_file, 0, SEEK_SET);
    lseek(w_file, 0, SEEK_SET);
    printf("\nantipattern: excessive stat\n", size);
    elapsed =  copy_ap_estat(r_file, w_file, page_size, "copy1.1");
    print_stats(size, elapsed);

    close(r_file);
    close(w_file);
}
