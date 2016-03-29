/*
 * Sample disk driver, from the beginning.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/timer.h>
#include <linux/types.h>	/* size_t */
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/hdreg.h>	/* HDIO_GETGEO */
#include <linux/kdev_t.h>
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>	/* invalidate_bdev */
#include <linux/bio.h>

MODULE_LICENSE("Dual BSD/GPL")
struct sbull_dev {
      int size ;  // device in sectors
      u8 *data; //data array
      short users; //how many users
      short media_changes; //Flag media
      spinlock_t lock; //mutual exculsion
      struct request_queue *queue; //device request queue
      struct gendisk *gd ; //disk structure
      struct timer_list timer;
    }

// Handles IO request
static void sbull_transfer(struct sbull_dev *dev , unsigned long sector,
                          unsigned long nsect, char *buffer , int write)
    {
  unsigned long offser = secotr * KERNEL_SECTOR_SIZE;
  unsigned long nybtes = nsec*KERNEL_SECOTR_SIZE;
  if((offset +nbytes)>dev->size){
    printk(KERN_NOTICE "Beyond end write(%ld %ld) \n", offset ,nbytes);
    return;
  }
  if (write)
      memcpy(dev->data+offset, buffer, nbytes);
  else
      memcpy(buffer , dev->data+offset, nbytes);
}
static void sbull_request(struct request_queue *q){
  struct request *req;
  while ((req=blk_fetch_request(q)) !=NULL) {
    struct sbull_dev *dev =req->rq_disk->private_data;
    if(req->cmd_type !=REQ_TYPE_FS){
      printk (KERN_NOTICE "skipnon-fs request\n");
      _blk_end_request_cur(req,-EIO);
      continue
    }
    sbull_transfer(dev,blk_rq_pos(req),blk_rq_cur_sectors(req),
                  req->buffer, rq_data_dir(req));
    __blk_end_request_cur(req,0);
  }
}
static int sbull_open(struct block_device *bdev , fmode_t mode)
    {
  struct sbull_dev *dev = bdev->bd_disk->private_data;
  del_timer_sync(&dev->timer);
  spin_lock(&dev->timer);
  if(! dev->users)
      check_disk_change(bdev);
  dev->users++;
  spin_unlock(&dev->lock);
  return 0;
}

static void sbull_release(struct gendisk *disk, fmode_t mode)
{
  struct sbull_dev *dev = disk->private_data;


  spin_lock(&dev->lock);
  dev->users--;

  if (!dev->users) {
    dev->timer.expires = jiffies + INVALIDATE_DELAY;
    add_timer(&dev->timer);
	}
  spin_unlock(&dev->lock);
}

/*
 * Look for a (simulated) media change.
 */
int sbull_media_changed(struct gendisk *gd)
{
  struct sbull_dev *dev = gd->private_data;

  return dev->media_change;
}

/*
 * Revalidate.  WE DO NOT TAKE THE LOCK HERE, for fear of deadlocking
 * with open.  That needs to be reevaluated.
 */
int sbull_revalidate(struct gendisk *gd)
{
	struct sbull_dev *dev = gd->private_data;

  if (dev->media_change) {
    dev->media_change = 0;
    memset (dev->data, 0, dev->size);
	}
  return 0;
}

/*
 * The "invalidate" function runs out of the device timer; it sets
 * a flag to simulate the removal of the media.
 */
void sbull_invalidate(unsigned long ldev)
{
  struct sbull_dev *dev = (struct sbull_dev *) ldev;

  spin_lock(&dev->lock);
  if (dev->users || !dev->data)
      printk (KERN_WARNING "sbull: timer sanity check failed\n");
  else
      dev->media_change = 1;
  spin_unlock(&dev->lock);
}

/*
 * The ioctl() implementation
 */

int sbull_ioctl (struct block_device *bdev, fmode_t mode,
                 unsigned int cmd, unsigned long arg)
{
  long size;
  struct hd_geometry geo;
  struct sbull_dev *dev = bdev->bd_disk->private_data;

  switch(cmd) {
    case HDIO_GETGEO:
      size = dev->size*(hardsect_size/KERNEL_SECTOR_SIZE);
      geo.cylinders = (size & ~0x3f) >> 6;
      geo.heads = 4;
      geo.sectors = 16;
      geo.start = 4;
      if (copy_to_user((void __user *) arg, &geo, sizeof(geo)))
          return -EFAULT;
	    return 0;
  }

  return -ENOTTY; /* unknown command */
}


// device operations
static struct block_device_operations sbull_ops ={
  .owner =THIS_MODULE,
  .open = sbull_open ,
  .release =sbull_release,
  .media_changed =sbull_media_changed,
  .revalidate_disk =sbull_revalidate,
  .ioctl =sbull_ioctl
}
static void setup_device(struct sbull_dev *dev,int which)
    {
         memset(dev ,0 ,sizeof(struct sbull_dev);
         dev->size =ndevices*hardsect_size
         dev->data=vmalloc(dev->size);
         if (dev->data == NULL)
         {
           printk(KERN_NOTICE "vmalloc failure \n");
           return
         }
         spin_lock_init(&dev->lock);

         dev->queue = blk_init_queue(sbull_request, &dev->lock);

         dev->gd = alloc_disk(SBULL_MINORS);
         if (! dev->gd ) {
           printk(KERN_NOTICE "alloc disk failure");
           goto out_vfree;
         }
          dev->gd->major =sbull_MAJOR;
          dev->gd->first_minor =which *SBULL_MINORS;
          dev->gd->fops= &scull_ops;
          dev->gd->private_data=dev;
          dev->gd->queue=dev->queue;
          snprintf(dev->gd->disk_name,32, "sbull%c",which +'a');
          set_capacity(dev->gd,nsectors*(hardsect_size/KERNEL_SECTOR_SIZE);
          add_disk(dev->gd);

}

static int __init sbull_init(void) {
  int i;
  // Get registered
  sbull_major=register_blkdev(sbull_major,"sbull")
    if (sbull_major <0)
        {
      printk(KERN_WARNING "SBULL:unable to get major number\n");
      return -EBUSY;
    }

  //Allocate device array and intializw
  Devices=kmalloc(ndevices*sizeof(struct sbull_dev), GFP_KERNEL)
    if(Devices ==NULL)
        {
      goto out_unregister
    }
  for (i=0; i <ndevices; i++)
  {
    setup_device(Devices +i,i)
  }
}
