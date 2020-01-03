#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/poll.h>
#define GLOBALFIFO_SIZE  60

static int globalfifo_major;

struct globalfifo_dev{
	struct cdev cdev;
	unsigned int current_len;
	unsigned char mem[GLOBALFIFO_SIZE];
	struct mutex mutex;
	struct mutex poll_mutex;
	wait_queue_head_t r_wait;
	wait_queue_head_t w_wait;
	struct class *fifo_class;
};

static struct globalfifo_dev *globalfifo_devp = NULL;

static int globalfifo_open (struct inode *node, struct file *filp)
{
	/*
	struct cdev *dev = node->i_cdev;
	printk("node->i_cdev:%p\n",node->i_cdev);
	struct globalfifo_dev *globalfifo_devp = container_of(dev, struct globalfifo_dev, cdev);
	printk("globalfifo_devp:%p\n",globalfifo_devp);
	filp->private_data = globalfifo_devp;
	*/
	printk(KERN_INFO "globalfifo_open\n");
	return 0;
}

static ssize_t globalfifo_read (struct file *filp, char __user *buf, size_t size, loff_t *off)
{
	int ret;
	//struct globalfifo_dev *globalfifo_devp = filp->private_data;
	DECLARE_WAITQUEUE(wait, current);
	
	mutex_lock(&globalfifo_devp->mutex);
	add_wait_queue(&globalfifo_devp->r_wait, &wait);
	
	while(globalfifo_devp->current_len == 0) {
		if(filp->f_flags & O_NONBLOCK){
			ret = -EAGAIN;
			goto out;
		}
		__set_current_state(TASK_INTERRUPTIBLE);
		
		/*切换进程之前释放互斥锁*/
		mutex_unlock(&globalfifo_devp->mutex);
		
		schedule();
		
		if(signal_pending(current)){
			ret = -ERESTARTSYS;
			goto out2;
		}
		mutex_lock(&globalfifo_devp->mutex);			
	}
	
	if(size > globalfifo_devp->current_len)
		size = globalfifo_devp->current_len;
	if(copy_to_user(buf, globalfifo_devp->mem, size)){
		ret = -EFAULT;
		goto out;
	}else{
		memcpy(globalfifo_devp->mem, globalfifo_devp->mem + size, globalfifo_devp->current_len - size);
		globalfifo_devp->current_len -= size;
		printk(KERN_INFO"Read %d bytes, current_len:%d\n", size, globalfifo_devp->current_len);
		
		wake_up_interruptible(&globalfifo_devp->w_wait);
		ret = size;
	}
	
out:
	mutex_unlock(&globalfifo_devp->mutex);
out2:
	remove_wait_queue(&globalfifo_devp->r_wait, &wait);
	set_current_state(TASK_RUNNING);
	return ret;
}

static ssize_t globalfifo_write (struct file *filp, const char __user *buf, size_t size, loff_t *off)
{
	int ret;
	//struct globalfifo_dev *globalfifo_devp = filp->private_data;
	DECLARE_WAITQUEUE(wait, current);
	
	mutex_lock(&globalfifo_devp->mutex);
	add_wait_queue(&globalfifo_devp->w_wait, &wait);
	
	while(globalfifo_devp->current_len == GLOBALFIFO_SIZE) {
		if(filp->f_flags & O_NONBLOCK){
			ret = -EAGAIN;
			goto out;
		}
		__set_current_state(TASK_INTERRUPTIBLE);
		
		/*切换进程之前释放互斥锁*/
		mutex_unlock(&globalfifo_devp->mutex);
		
		schedule();
		
		if(signal_pending(current)){
			ret = -ERESTARTSYS;
			goto out2;
		}
		mutex_lock(&globalfifo_devp->mutex);			
	}
	
	if(size > (GLOBALFIFO_SIZE - globalfifo_devp->current_len))
		size = GLOBALFIFO_SIZE - globalfifo_devp->current_len;
	if(copy_from_user(globalfifo_devp->mem + globalfifo_devp->current_len, buf, size)){
		ret = -EFAULT;
		goto out;
	}else{
		
		globalfifo_devp->current_len += size;
		printk(KERN_INFO"write %d bytes, current_len:%d\n", size, globalfifo_devp->current_len);
		
		wake_up_interruptible(&globalfifo_devp->r_wait);
		ret = size;
	}
	
out:
	mutex_unlock(&globalfifo_devp->mutex);
out2:
	remove_wait_queue(&globalfifo_devp->w_wait, &wait);
	set_current_state(TASK_RUNNING);
	return ret;
}
static int globalfifo_release (struct inode *node, struct file *filp)
{
	/*
	struct globalfifo_dev *globalfifo_devp = filp->private_data;
	cdev_del(&globalfifo_devp->dev);
	unregister_chrdev_region(globalfifo_devp->devno, 1);
	
	*/
	return 0;
}
static unsigned int globalfifo_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	mutex_lock(&globalfifo_devp->poll_mutex);
	
	poll_wait(filp, &globalfifo_devp->r_wait, wait); //不会休眠！将设备结构体中的r_wait和w_wait添加到轮询表中意味着因调用select而阻塞的进程可以被r_wait和w_wait唤醒
	poll_wait(filp, &globalfifo_devp->w_wait, wait);
	
	if(globalfifo_devp->current_len != 0){
		mask |= (POLLIN | POLLRDNORM);	//POLLIN:普通或优先级带数据可;POLLRDNORM:普通数据可读
	}
	if(globalfifo_devp->current_len != GLOBALFIFO_SIZE){
		mask |= (POLLOUT | POLLWRNORM);	 //POLLWRNORM:普通数据可写;POLLOUT:普通数据可写
	}
	
	mutex_unlock(&globalfifo_devp->poll_mutex);
	return mask;
}
static const struct file_operations globalfifo_ops = {
	.owner = THIS_MODULE,
	.open  = globalfifo_open,
	.write = globalfifo_write,
	.read  = globalfifo_read,
	.poll  = globalfifo_poll,
	.release = globalfifo_release,
};

static void globalfifo_setup_cdev(void)
{
	dev_t devno = MKDEV(globalfifo_major, 0);
	/*
	struct cdev dev = lobalfifo_devp->cdev;
	cdev_init(&dev, &globalfifo_ops);//注意！ 错误，初始话的是临时变量dev,该函数结束后就自动消除了
	*/
	struct cdev *dev = &globalfifo_devp->cdev; //正确做法, 千万不用用值来传递，而是用指针来传递
	cdev_init(dev, &globalfifo_ops);
	dev->owner = THIS_MODULE;
	if(cdev_add(dev, devno, 1)){
		printk("Error adding test_setup_cdev");
	}	
}

static int __init globalfifo_init(void)
{
	int ret;
	dev_t devno = MKDEV(globalfifo_major, 0);
	
	if(globalfifo_major)
		ret =register_chrdev_region(devno, 1, "globalfifo");
	else{
		ret = alloc_chrdev_region(&devno, 0, 1, "globalfifo");
		globalfifo_major = MAJOR(devno);
	}	
	if(ret < 0)
		return ret;
	
	globalfifo_devp = kzalloc(sizeof(struct globalfifo_dev), GFP_KERNEL);
	if(!globalfifo_devp) {
		ret = -ENOMEM;
		printk(KERN_ALERT "malloc fail...\n");
		goto fail_malloc;
	}
	memset(globalfifo_devp, 0, sizeof(*globalfifo_devp));
	
	globalfifo_setup_cdev();
	/*
	cdev_init(&globalfifo_devp->cdev, &globalfifo_ops);
	globalfifo_devp->cdev.owner = THIS_MODULE;
	
	if(cdev_add(&globalfifo_devp->cdev, devno, 1)){
		printk("Error adding test_setup_cdev");
	}
	*/
	globalfifo_devp->fifo_class = class_create(THIS_MODULE, "globalfifo");
	if(IS_ERR(globalfifo_devp->fifo_class)) {
    printk(KERN_ALERT "Failed to create class.\n");
    goto fail_malloc;
	}
	device_create(globalfifo_devp->fifo_class, NULL, devno, NULL, "globalmem");
	
		
		
	mutex_init(&globalfifo_devp->mutex);
	mutex_init(&globalfifo_devp->poll_mutex);
	init_waitqueue_head(&globalfifo_devp->r_wait);
	init_waitqueue_head(&globalfifo_devp->w_wait);
	
	return 0;

fail_malloc:
	unregister_chrdev_region(devno, 1);
	return ret;
	
}
static void globalfifo_exit(void)
{
	dev_t devno = MKDEV(globalfifo_major, 0);
	cdev_del(&globalfifo_devp->cdev);
	device_destroy(globalfifo_devp->fifo_class, devno);
	class_destroy(globalfifo_devp->fifo_class);
	unregister_chrdev_region(devno, 1);
	
	kfree(globalfifo_devp);
	globalfifo_devp = NULL;	
}
module_init(globalfifo_init);
module_exit(globalfifo_exit);
MODULE_LICENSE("GPL");











