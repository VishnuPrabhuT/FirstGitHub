#include<linux/init.h>
#include<linux/kernel.h>
#include<linux/kernel_stat.h>

#include <linux/ioctl.h>
#include <linux/cdev.h> // for struct cdev
#include <linux/delay.h> // for udelay
#include <linux/fs.h> // for struct file_operations
#include <linux/mm_types.h> // for u32 and u64
#include <linux/mod_devicetable.h> // for struct pci_device_id
#include <linux/module.h> // for MODULE_LICENSE and MODULE_AUTHOR
#include <linux/pci.h> // for struct pci_driver
#include <linux/sched.h> // for schedule
#include <asm/io.h> // for ioremap
#include <asm/uaccess.h> // for copy_from_user

#define PCI_VENDOR_ID 0x1234
#define PCI_DEVICE_ID 0x1113
#define DEVICE_RAM 0x0020
#define CONTROL_SIZE 65536

/*Registers*/
#define FifoStart 0x1020
#define FifoEnd 0x1024
#define FifoHead 0x4010
#define FifoTail 0x4014
#define FIFO_ENTRIES 1024

/*IOCTL*/
#define VMODE      _IOW (0xCC, 0, unsigned long)
#define BIND_DMA   _IOW (0xCC, 1, unsigned long)
#define START_DMA  _IOWR(0xCC, 2, unsigned long)
#define FIFO_QUEUE _IOWR(0xCC, 3, unsigned long)
#define FIFO_FLUSH _IO  (0xCC, 4)
#define UNBIND_DMA _IOW (0xCC, 5, unsigned long)

/*Graphics Constants*/
#define FrameColumns 0x8000
#define FrameRows 0x8004
#define FrameRowPitch 0x8008
#define FramePixelFormat 0x800C
#define FrameStartAddress 0x8010

#define EncoderWidth 0x9000
#define EncoderHeight 0x9004
#define EncoderOffsetX 0x9008
#define EncoderOffsetY 0x900C
#define EncoderFrame 0x9010

#define DrawClearColor4fBlue 0x5100
#define DrawClearColor4fGreen 0x5104
#define DrawClearColor4fRed 0x5108
#define DrawClearColor4fAlpha 0x510C

#define RasterClear 0x3008
#define RasterFlush 0x3FFC

#define ConfigAcceleration 0x1010
#define ConfigModeSet 0x1008

#define GRAPHICS_OFF 0
#define GRAPHICS_ON 1

MODULE_LICENSE("Proprietary");
MODULE_AUTHOR("Vishnu");

struct fifo_entry{
	u32 command;
	u32 value;
};

struct fifo{
	u64 p_base;
	struct fifo_entry *k_base;
	u32 head;
	u32 tail;
};

struct kyouko3{
	struct cdev kyouko3v;
	struct pci_dev *pci_saved_dev;
	unsigned long p_control;
	unsigned long p_ram;
	unsigned int *k_control;
	unsigned int *k_ram;
	struct fifo fifo;
	_Bool graphics_ON;
	_Bool dma_Mapped;
}kyouko3;

int K_READ_REG(unsigned int reg){
	unsigned int value;
	udelay(1);
	rmb();
	value=*(kyouko3.k_control+(reg>>2));
	return (value);
}

void K_WRITE_REG(unsigned int reg,unsigned int value){
	udelay(1);
	*(kyouko3.k_control+(reg>>2))=value;
}

void FIFO_WRITE(unsigned int reg,unsigned int value){
	kyouko3.fifo.k_base[kyouko3.fifo.head].command=reg;
	kyouko3.fifo.k_base[kyouko3.fifo.head].value=value;
	++kyouko3.fifo.head;
	if (kyouko3.fifo.head>=FIFO_ENTRIES)
		kyouko3.fifo.head=0;
}

long fifo_queue(unsigned int command,unsigned long arg){
	long ret;
	struct fifo_entry fifo_entry;
	ret=copy_from_user(&fifo_entry,(struct fifo_entry*)arg,sizeof(struct fifo_entry));
	FIFO_WRITE(fifo_entry.command,fifo_entry.value);
	return ret;
}

void fifo_flush(void){
	K_WRITE_REG(FifoHead,kyouko3.fifo.head);
	while(kyouko3.fifo.tail!=kyouko3.fifo.head){
		kyouko3.fifo.tail=K_READ_REG(FifoTail);
		schedule();
	}
}

void graphics_on(void) {
   // clear color of blue
   float red = 0.0f, green = 0.0f, blue = 1.0f, alpha = 0.0f;

   K_WRITE_REG(FrameColumns, 1024);
   K_WRITE_REG(FrameRows, 768);
   K_WRITE_REG(FrameRowPitch, 1024*4);
   K_WRITE_REG(FramePixelFormat, 0xf888);
   K_WRITE_REG(FrameStartAddress, 0);

   K_WRITE_REG(ConfigAcceleration, 0x40000000);

   K_WRITE_REG(EncoderWidth, 1024);
   K_WRITE_REG(EncoderHeight, 768);
   K_WRITE_REG(EncoderOffsetX, 0);
   K_WRITE_REG(EncoderOffsetY, 0);
   K_WRITE_REG(EncoderFrame, 0);

   K_WRITE_REG(ConfigModeSet, 0);

   msleep(10);

   FIFO_WRITE(DrawClearColor4fRed, *(unsigned int*)(&red));
   FIFO_WRITE(DrawClearColor4fGreen, *(unsigned int*)(&green));
   FIFO_WRITE(DrawClearColor4fBlue, *(unsigned int*)(&blue));
   FIFO_WRITE(DrawClearColor4fAlpha, *(unsigned int*)(&alpha));

   FIFO_WRITE(RasterClear, 0x03);
   FIFO_WRITE(RasterFlush, 0x0);

   fifo_flush();

   kyouko3.graphics_ON = 1;
}

void graphics_off(void){
	fifo_flush();
	K_WRITE_REG(ConfigAcceleration, 0x80000000);
	K_WRITE_REG(ConfigModeSet, 0);
	msleep(10);
	kyouko3.graphics_ON=0;
}

int k_open(struct inode *inode,struct file *fp){
	int ram_size;
	kyouko3.k_control=ioremap(kyouko3.p_control,CONTROL_SIZE);
	ram_size=K_READ_REG(DEVICE_RAM);
	ram_size=ram_size*(1024*1024);
	kyouko3.k_ram=ioremap(kyouko3.p_ram,DEVICE_RAM);

	kyouko3.fifo.k_base= pci_alloc_consistent(kyouko3.pci_saved_dev, 8192u, &kyouko3.fifo.p_base);
	K_WRITE_REG(FifoStart,kyouko3.fifo.p_base);
	K_WRITE_REG(FifoEnd,kyouko3.fifo.p_base+8192u);

	kyouko3.fifo.head=0;
	kyouko3.fifo.tail=0;
	kyouko3.graphics_ON=0;
	kyouko3.dma_Mapped=0;

	printk(KERN_ALERT "THIS IS OPEN\n");
	return 0;
}

int k_release(struct inode *inode,struct file *fp){
	graphics_off();
	pci_free_consistent(kyouko3.pci_saved_dev,8192u,kyouko3.fifo.k_base,kyouko3.fifo.p_base);

	iounmap(kyouko3.k_control);
	iounmap(kyouko3.k_ram);
	printk(KERN_ALERT "YOU ARE FREE!\n");
	return 0;
}

long k_ioctl(struct file *fp, unsigned int cmd, unsigned long arg){
	long ret=0;

	switch(cmd){
		case FIFO_QUEUE:
			ret=fifo_queue(cmd,arg);
			break;
		case FIFO_FLUSH:
			fifo_flush();
			break;
		case VMODE:
			if ((int)arg==GRAPHICS_OFF)
				graphics_off();
			else
				graphics_on();
			break;
	}
	return ret;
}

int k_probe(struct pci_dev *pci_dev,const struct pci_device_id *pci_id){
	int ret;
	kyouko3.pci_saved_dev=pci_dev;
	kyouko3.p_control=pci_resource_start(pci_dev,1);
	kyouko3.p_ram=pci_resource_start(pci_dev,2);

	printk(KERN_ALERT "INSIDE PROBE GOT PHYSICAL WITH THE CARD!\n");
	ret=pci_enable_device(kyouko3.pci_saved_dev);
	pci_set_master(kyouko3.pci_saved_dev);
	return ret;
}

void k_remove(struct pci_dev *pci_dev){
	pci_disable_device(pci_dev);
}


int k_mmap(struct file *fp,struct vm_area_struct *vma){
	int ret=0;
	switch(vma->vm_pgoff << PAGE_SHIFT){
		case 0:
			printk(KERN_ALERT "Mapped the control memory!\n");
			ret= io_remap_pfn_range(vma,vma->vm_start,kyouko3.p_control>>PAGE_SHIFT,vma->vm_end-vma->vm_start,vma->vm_page_prot);
		break;

		case 0x80000000:
			printk(KERN_ALERT "Mapped the RAM!\n");
			ret= io_remap_pfn_range(vma,vma->vm_start,kyouko3.p_ram>>PAGE_SHIFT,vma->vm_end-vma->vm_start,vma->vm_page_prot);
		break;

		default:
        	ret = 1;
        	printk(KERN_ALERT "Error!\n");
        break;
	}
	printk(KERN_ALERT "MMAP RETURN VALUE - %d\n",ret);
	return ret;
}

struct file_operations fops={
	.open=k_open,
	.mmap=k_mmap,
	.unlocked_ioctl=k_ioctl,
	.release=k_release,
	.owner=THIS_MODULE
};

struct pci_device_id ids[]={
	{
		PCI_DEVICE(
			PCI_VENDOR_ID,
			PCI_DEVICE_ID)
	},{0}
};

struct pci_driver drv={
	.name="kyouko3v",
	.id_table=ids,
	.probe=k_probe,
	.remove=k_remove
};


int k_init(void){
	int i;
	cdev_init(&kyouko3.kyouko3v,&fops);
	cdev_add(&kyouko3.kyouko3v,MKDEV(500,127),1);
	i=pci_register_driver(&drv);
	printk(KERN_ALERT "THIS IS INIT\n");
	return 0;
}
void k_exit(void){
	pci_unregister_driver(&drv);
	cdev_del(&kyouko3.kyouko3v);
	printk(KERN_ALERT "THIS IS EXIT\n");
}

module_init(k_init);
module_exit(k_exit);


