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

struct generic_record {
  int tid;
  int action;
  struct time start_time;
  struct time end_time;
};

enum {G_INIT, G_DURATION};

// end


void time_diff(struct timespec start, struct timespec end, struct timespec* elapsed)
{
}

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

int open_log_file(char* filename) {
  struct generic_record t;

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

  int fd = open_log_file(argv[1]);

  struct stat filestat;

  success = fstat(fd, &filestat);
  if (success != 0) {
    perror("fstat");
    printf("error getting file size\n");
    return -1;
  }
  if (filestat.st_size % sizeof(struct generic_record) != 0) {
    printf("filesize indicates mismatched record size or corrupt file\n");
    return -1;
  }
  
  ssize_t red;

  printf("tid\taction\tsrc\tdest\tstart (ns)\tend (ns)\tduration (ns)\n");

  struct generic_record rec;
  red = read(fd, (void*)&rec, sizeof(rec));

  struct time time_zero = rec.start_time;

  printf("%d\t%d\t%g\t%g\t%g\n", 
	 rec.tid, 
	 rec.action,
	 time_diff_ns(time_zero, rec.start_time),
	 time_diff_ns(time_zero, rec.end_time),
	 time_diff_ns(rec.start_time, rec.end_time));

  
  while (red != 0) {
    red = read(fd, (void*)&rec, sizeof(rec));

  printf("%d\t%d\t%g\t%g\t%g\n", 
	 rec.tid, 
	 rec.action,
	 time_diff_ns(time_zero, rec.start_time),
	 time_diff_ns(time_zero, rec.end_time),
	 time_diff_ns(rec.start_time, rec.end_time));
  }

  return 0;
}
