
#define MEOSCHRDV_DEVICE_NAME "meoschrdv"
#define MEOSCHRDV_BUFSIZE (8 * 1024) 
#define MEOSCHRDV_NUM_DEVICES (1)
#define MEOSCHRDV_MINOR_BASE (0)

struct meoschrdv {
  unsigned char *data;
  unsigned long bufsize;
  struct cdev cdev;
};
