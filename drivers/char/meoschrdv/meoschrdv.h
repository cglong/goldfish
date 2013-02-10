
#define MCHR_DEVICE_NAME "mchr"
#define MCHR_BUFSIZE (8 * 1024) 
#define MCHR_NUM_DEVICES (1)
#define MCHR_MINOR_BASE (0)

struct mchr {
  unsigned char *data;
  unsigned long bufsize;
  struct cdev cdev;
};
