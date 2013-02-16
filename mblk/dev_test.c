#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

struct mklb_stat
{
	unsigned total_read_ops;
	unsigned total_write_ops;
	unsigned total_read_size;  // in bytes
	unsigned total_write_size; // in bytes
};

#define IOCTL_GET_STATISTICS 48
#define BUF_SIZE 100

int main(int argc, char *argv[])
{
  int fd;
  int count = 0;
  char readbuf[BUF_SIZE];
  char *test_string = "Hello, Android Device Driver!";
  int size = strlen(test_string);

  memset(readbuf, 0, BUF_SIZE);

  printf("%s: entered\n", argv[0]);

  fd = open("/dev/block/mblk0", O_RDWR);
  if (fd == -1) {
    perror("open failed.\n");
    count = fd;
    exit(-1);
  }
  printf("%s: open: successful\n", argv[0]);

  count = write(fd, test_string, size);
  if (count == -1) {
    perror("write failed");
    close(fd);
    exit(-1);
  }
  printf("%s: write: returning %d bytes!\n", argv[0], count);

  if (lseek(fd, SEEK_SET, 0) != 0)
    {
      perror("seek failed");
      close(fd);
      exit(-1);
    }

  count = read(fd, readbuf, size);
  if (count == -1) {
    perror("read failed");
    close(fd);
    exit(-1);
  }
  printf("%s: read: returning %d bytes!\n", argv[0], count);
  
  if (memcmp(readbuf, test_string, count) != 0)
    {
      int i;
      printf("Comparison failed.\n");
      printf("Expected: ");
      for (i = 0; i < count; i++)
	{
	  printf("%c", test_string[i]);
	}
      printf("\n");

      printf("Actual : ");
      for (i = 0; i < count; i++)
	{
	  printf("%c", readbuf[i]);
	}
      printf("\n");

      close(fd);
      exit(-1);
    }
  else
    {
      printf("Comparison Success.\n");
    }
  
  struct mklb_stat mblk;
  ioctl(fd, IOCTL_GET_STATISTICS, &mblk);
  
  printf("Read ops: %d\n", mblk.total_read_ops);
  printf("Write ops: %d\n", mblk.total_write_ops);
  printf("Read size: %d\n", mblk.total_read_size);
  printf("Write size: %d\n", mblk.total_write_ops);
  
  close(fd);

  return 0;
}
