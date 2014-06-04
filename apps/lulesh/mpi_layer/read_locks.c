#include <stdint.h>
#include <stdio.h>

#include <errno.h>
#include <sys/types.h> // for fstat, open, struct stat
#include <sys/stat.h> // for fstat, open, struct stat
#include <fcntl.h> // for open
#include <unistd.h> // for fstat, read, struct stat

// start definitions

struct time {
  uint64_t tv_sec;
  uint64_t tv_nsec;
};

struct lock_record {
  int tid;
  int action;
  int src;
  int dest;
  struct time start_time;
  struct time end_time;
};

#define I_INIT 0
#define COMM_P_IN 1
#define COMM_P_OUT 2
#define COMM_V_IN 3
#define COMM_V_OUT 4
#define P2P_P_IN 1
#define P2P_P_OUT 2
#define P2P_V_IN 3
#define P2P_V_OUT 4

// end

double time_diff_ns(struct time start, struct time end) {
  int64_t tv_sec;
  int64_t tv_nsec;
  if ((end.tv_nsec-start.tv_nsec)<0) {
    tv_sec = end.tv_sec-start.tv_sec-1;
    tv_nsec = (1000000000+end.tv_nsec)-start.tv_nsec;
  } else {
    tv_sec = end.tv_sec-start.tv_sec;
    tv_nsec = end.tv_nsec-start.tv_nsec;
  }

  unsigned long elapsed = (tv_sec*1e9) + tv_nsec;
  return (double)elapsed;
}

double time_diff_us(struct time start_time, struct time end_time) {
  double elapsed = time_diff_ns(start_time, end_time);
  return (elapsed/1e3);
}

#if 0
double time_diff_ns(struct time start_time, struct time end_time) {
  unsigned long elapsed = ((end_time.tv_sec-start_time.tv_sec)*1e9)+(end_time.tv_nsec-start_time.tv_nsec);
  return (double)elapsed;
}

double time_diff_us(struct time start_time, struct time end_time) {
  double elapsed = time_diff_ns(start_time, end_time);
  return (elapsed/1e3);
}
#endif

int open_log_file(char* filename) {
  struct lock_record t;

  int fd = open(filename, O_RDONLY, 0);
  if (fd == -1) {
    perror("file open");
    return -1;
  }

  return fd;
}


int main(int argc, char** argv) {
  int success;
  if (argc < 2) {
    return -1;
  }

  //  printf("Opening %s\n", argv[1]);
  int fd = open_log_file(argv[1]);

  struct stat filestat;

  success = fstat(fd, &filestat);
  if (success != 0) {
    perror("fstat");
    printf("error getting file size\n");
    return -1;
  }
  if (filestat.st_size % sizeof(struct lock_record) != 0) {
    printf("filesize indicates mismatched record size or corrupt file\n");
    return -1;
  }
  
  ssize_t red;

  printf("tid\taction\tsrc\tdest\tstart_time_ns\tend_time_ns\tduration_ns\n");

  struct lock_record rec;
  red = read(fd, (void*)&rec, sizeof(rec));

  struct time time_zero = rec.start_time;

  printf("%d\t%d\t%d\t%d\t%f\t%f\t%f\n", 
	 rec.tid, 
	 rec.action,
	 rec.src,
	 rec.dest,
	 time_diff_ns(time_zero, rec.start_time),
	 time_diff_ns(time_zero, rec.end_time),
	 time_diff_ns(rec.start_time, rec.end_time));
  
  while (red != 0) {
    red = read(fd, (void*)&rec, sizeof(rec));

    printf("%d\t%d\t%d\t%d\t%f\t%f\t%f\n", 
	   rec.tid, 
	   rec.action,
	   rec.src,
	   rec.dest,
	   time_diff_ns(time_zero, rec.start_time),
	   time_diff_ns(time_zero, rec.end_time),
	   time_diff_ns(rec.start_time, rec.end_time));
  }

  return 0;
}
