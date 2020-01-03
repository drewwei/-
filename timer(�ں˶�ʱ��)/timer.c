#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
static int timer_major;

struct timer_dev{
	struct cdev cdev;
	spinlock_t  lock;
	struct class *fifo_class;
	int count;
	struct timer_list timer;
};

static struct timer_dev *timer_devp = NULL;


static void do_timer(unsigned long data)
{
	struct timer_dev * devp = (struct timer_dev *)data;
	
	printk(KERN_INFO "count:%d\n", devp->count);
	spin_lock(&timer_devp->lock);
	
	devp->count += 1;
	
	spin_unlock(&timer_devp->lock);
	
	mod_timer(&devp->timer, jiffies + HZ);
}

static int timer_open (struct inode *node, struct file *filp)
{

	init_timer(&timer_devp->timer);
	timer_devp->timer.function = do_timer;
	timer_devp->timer.data = (unsigned long)timer_devp;
	timer_devp->timer.expires = jiffies + HZ;
	add_timer(&timer_devp->timer);
	return 0;
}
static int timer_release (struct inode *node, struct file *filp)
{
	del_timer(&timer_devp->timer);
	timer_devp->count = 0;
	return 0;
}
static ssize_t timer_read (struct file *filp, char __user *buf, size_t size, loff_t *off)
{
	int data = timer_devp->count;
	ssize_t ret;
	ret = copy_to_user(buf, &data, sizeof(data));
	return ret;
}
static const struct file_operations timer_ops = {
	.owner = THIS_MODULE,
	.open  = timer_open,
	.read  = timer_read,
	.release = timer_release,
};

static void timer_setup_cdev(void)
{
	dev_t devno = MKDEV(timer_major, 0);
	/*
	struct cdev dev = lobalfifo_devp->cdev;
	cdev_init(&dev, &timer_ops);//注意！ 错误，初始话的是临时变量dev,该函数结束后就自动消除了
	*/
	struct cdev *dev = &timer_devp->cdev; //正确做法, 千万不用用值来传递，而是用指针来传递
	cdev_init(dev, &timer_ops);
	dev->owner = THIS_MODULE;
	if(cdev_add(dev, devno, 1)){
		printk("Error adding test_setup_cdev");
	}	
}

static int __init timer_init(void)
{
	int ret;
	dev_t devno = MKDEV(timer_major, 0);
	
	if(timer_major)
		ret =register_chrdev_region(devno, 1, "timer");
	else{
		ret = alloc_chrdev_region(&devno, 0, 1, "timer");
		timer_major = MAJOR(devno);
	}	
	if(ret < 0)
		return ret;
	
	timer_devp = kzalloc(sizeof(struct timer_dev), GFP_KERNEL);
	if(!timer_devp) {
		ret = -ENOMEM;
		printk(KERN_ALERT "malloc fail...\n");
		goto fail_malloc;
	}
	memset(timer_devp, 0, sizeof(*timer_devp));
	
	timer_setup_cdev();
	/*
	cdev_init(&timer_devp->cdev, &timer_ops);
	timer_devp->cdev.owner = THIS_MODULE;
	
	if(cdev_add(&timer_devp->cdev, devno, 1)){
		printk("Error adding test_setup_cdev");
	}
	*/
	timer_devp->fifo_class = class_create(THIS_MODULE, "timer");
	if(IS_ERR(timer_devp->fifo_class)) {
    printk(KERN_ALERT "Failed to create class.\n");
    goto fail_malloc;
	}
	device_create(timer_devp->fifo_class, NULL, devno, NULL, "globalmem");
	
		
		
	spin_lock_init(&timer_devp->lock);
	
	return 0;

fail_malloc:
	unregister_chrdev_region(devno, 1);
	return ret;
	
}
static void timer_exit(void)
{
	dev_t devno = MKDEV(timer_major, 0);
	cdev_del(&timer_devp->cdev);
	device_destroy(timer_devp->fifo_class, devno);
	class_destroy(timer_devp->fifo_class);
	unregister_chrdev_region(devno, 1);
	
	kfree(timer_devp);
	timer_devp = NULL;	
}


module_init(timer_init);
module_exit(timer_exit);
MODULE_LICENSE("GPL");


