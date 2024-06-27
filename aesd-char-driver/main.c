/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("JozoP"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    if ((buf == NULL) ||(filp == NULL) || (f_pos == NULL))
    {
        return -EFAULT;
    }


    
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */

    struct aesd_dev *dev;
	ssize_t rd_count = 0;
    ssize_t bytes_rd = 0;
	ssize_t rd_offset = 0;
    struct aesd_buffer_entry *rd_idx = NULL;

    dev = (struct aesd_dev *)filp->private_data;
    if (mutex_lock_interruptible(&dev->lock))
    {
        PDEBUG(KERN_ERR "mutex lock failed");
        return -EFAULT;
    }
    rd_idx = aesd_circular_buffer_find_entry_offset_for_fpos(&(dev->circular_buffer), *f_pos, &rd_offset);
    if (rd_idx == NULL)
    {
        mutex_unlock(&(dev->lock));
		return bytes_rd;
    }
    else
    {
        count = (count > (rd_idx->size - rd_offset))? (rd_idx->size - rd_offset): count;
    }
    if((rd_count = copy_to_user(buf,(rd_idx->buffptr + rd_offset), count))==0)
		PDEBUG("read Complete");
    bytes_rd = count - rd_count;
    *f_pos += bytes_rd;
	mutex_unlock(&(dev->lock));
    return bytes_rd;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    if (count == 0)
        return 0;
    if ((buf == NULL) ||(filp == NULL) || (f_pos == NULL))
        return -EFAULT;

    struct aesd_dev *dev;
	ssize_t bytes_written = -ENOMEM;
    ssize_t bytes_count = 0;

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    dev = (struct aesd_dev *)filp->private_data;
	
    if (mutex_lock_interruptible(&(dev->lock)))
    {
        PDEBUG(KERN_ERR "mutex lock failed");
        return -EFAULT;
    }

    if (dev->circular_buffer_entry.size == 0)
    {
        PDEBUG("Allocating buffer");
        dev->circular_buffer_entry.buffptr = kmalloc(count * sizeof(char), GFP_KERNEL);
        if (dev->circular_buffer_entry.buffptr == NULL)
        {
            PDEBUG("memory alloc failure");
            mutex_unlock(&dev->lock);
			return bytes_written;
        }
    }
    else
    {
        dev->circular_buffer_entry.buffptr = krealloc(dev->circular_buffer_entry.buffptr, (dev->circular_buffer_entry.size + count), GFP_KERNEL);
        if (dev->circular_buffer_entry.buffptr == NULL)
        {
            PDEBUG("memory alloc failure");
            mutex_unlock(&dev->lock);
			return bytes_written;
        }
    }
    PDEBUG("write from user space buffer to kernel buffer");
    if((bytes_count = copy_from_user((void *)(dev->circular_buffer_entry.buffptr + dev->circular_buffer_entry.size),buf, count))== 0)
		PDEBUG("write Complete");
	
    bytes_written = count - bytes_count;
    dev->circular_buffer_entry.size += bytes_written;
    if (memchr(dev-> circular_buffer_entry.buffptr, '\n', dev->circular_buffer_entry.size))
    {
        aesd_circular_buffer_add_entry(&dev->circular_buffer, &dev->circular_buffer_entry)))
        
        dev-> circular_buffer_entry.buffptr = NULL;
        dev->circular_buffer_entry.size = 0;
    }
    mutex_unlock(&dev->lock);
    return bytes_written;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */

    mutex_init(&aesd_device.lock);
    aesd_circular_buffer_init(&aesd_device.circular_buffer);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    uint8_t index;
    struct aesd_buffer_entry *entry;

    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.circular_buffer, index){
        if (entry->buffptr != NULL)
            kfree(entry->buffptr);
    }

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
