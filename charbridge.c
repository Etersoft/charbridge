/*
 *  charbridge.c - Пример создания символьного устройства
 *              	доступного на запись/чтение
 *
 * Пример взят с: http://www.opennet.ru/docs/RUS/lkmpg26/
 * и модифицирован
 */

#include <linux/module.h> /* Необходимо для любого модуля */
#include <linux/kernel.h> /* Все-таки мы работаем с ядром! */
#include <linux/fs.h>
#include <linux/sched.h>    /* Взаимодействие с планировщиком */
#include <linux/poll.h>
#include <asm/uaccess.h>    /* определения функций get_user и put_user */
#include <linux/pipe_fs_i.h>
#include <linux/version.h>

#include "charbridge.h"
#define SUCCESS 0

#define BUF_LEN 255
#define NAM_LEN 10

// #define WAIT_TIMEOUT_MSEC 300

#define DRIVER_AUTHOR "Pavel Vainerman <pv@etersoft.ru>"
#define DRIVER_DESC   "A sample driver"

//#define DEBUG 1

/* --------------------------------------------------------------------------- */
// Реализован циклический буфер:
// При записи происходит сдвиг wpos (позиции записи)
// При чтении сдвигается rpos (позиция чтения)
// При достижении конца буфера, данные опять пишутся в начало...
// WARNING! Данные БУДУТ затёрты (потеряны) если запись происходит быстрее чем чтение!
// При этом rpos принудительно сдвигается до текущей позиции записи (см. do_write).
// И получается, что читающий процесс теряет часть затёртых данных,
// которые не успел прочесть...
/* --------------------------------------------------------------------------- */
/*
 * Информация об устройстве
 *
 */
struct Device_Info
{
	int open;
	char buf[BUF_LEN];
	unsigned int wpos;	// очередная позиция для записи...
	unsigned int rpos;	// очередная позиция для чтения...
	dev_t rdev;
	spinlock_t lock;
//	long int timeout;
	char name[NAM_LEN];
	wait_queue_head_t waitq;
	wait_queue_head_t pollq;
	
} deviceA[MAX_DEV],deviceB[MAX_DEV];

static int dev_major = 0;

/* --------------------------------------------------------------------------- */
inline static int get_index( int minor )
{
// Миноры от 0..99 		- для sideA0...sideA99
// Миноры от 100..199	- для sideB0...sideB99
	if( minor >= MAX_PAIR )
		minor -= MAX_PAIR;

	if( (minor >= MAX_DEV) || (minor < 0) )
		return -1;

	return minor;
}
/* --------------------------------------------------------------------------- */
inline static unsigned char check_sideB( int minor )
{
	if( minor >= MAX_PAIR )
		return 1;

	return 0;
}
/* --------------------------------------------------------------------------- */
inline static int check_empty( struct Device_Info* idev )
{
	return ( idev->wpos == idev->rpos );
}

/* --------------------------------------------------------------------------- */
static int do_wait_empty( struct Device_Info* idev )
{
	int ret=0;
#ifdef DEBUG
  printk("(charbridge): (%s) do_wait_empty...\n", idev->name);
#endif

	while( check_empty(idev) ) {
		int i, is_sig = 0;

		ret = wait_event_interruptible(idev->waitq, !check_empty(idev) );
//		ret = wait_event_interruptible_timeout(idev->waitq,!check_empty(idev),idev->timeout);

#ifdef DEBUG
		printk("(charbridge): (%s) do_wait_empty interrupt... ret=%d\n", idev->name, ret);
#endif
/*
		if( ret == idev->timeout )
		{
//			wake_up_interruptible(&idev->pollq);
#ifdef DEBUG
			printk("(charbridge): (%s) do_wait_empty timeout (%ld)\n", idev->name, idev->timeout);
#endif
			return -EAGAIN;
		}
*/
		if( ret < 0 )
		{
#ifdef DEBUG
			printk("(charbridge): (%s) do_wait_empty RET<0...\n", idev->name);
#endif
			return -EINTR;
		}

		for (i = 0; i < _NSIG_WORDS && !is_sig; i++)
			is_sig = current->pending.signal.sig[i] & ~current->blocked.sig[i];

		if (is_sig)
		{
#ifdef DEBUG
			printk("(charbridge): (%s) do_wait_empty EINTR...\n", idev->name);
#endif
			return -EINTR;
		}
		
#ifdef DEBUG
		printk("(charbridge): (%s) do_wait_empty continue... rpos=%d wpos=%d\n", idev->name, idev->rpos, idev->wpos );
#endif
	}

#ifdef DEBUG
		printk("(charbridge): (%s) do_wait_empty OK rpos=%d wpos=%d\n", idev->name, idev->rpos, idev->wpos);
#endif

	return SUCCESS;
}
/* --------------------------------------------------------------------------- */
/* 
 * Вызывается когда процесс, открывший файл устройство A(1)
 * пытается считать из него данные.
 */
static ssize_t do_read(struct file *file, /* см. include/linux/fs.h   */
            char __user * buffer,             /* буфер для сообщения */
            size_t length,                    /* размер буфера       */
            loff_t * offset,
			struct Device_Info* idev)
{
	ssize_t ret = 0;
	int bytes_read = 0; // Количество байт, фактически записанных в буфер

 /* 
   * Если установлен флаг O_NONBLOCK, то процесс не должен приостанавливаться
   * В этом случае, если файл уже открыт, необходимо вернуть код ошибки
   * -EAGAIN, что означает "попробуйте в другой раз"
   */
	if ((file->f_flags & O_NONBLOCK) && check_empty(idev) )
		return -EAGAIN;

#ifdef DEBUG
	printk("(charbridge): begin read from %s rpos=%d wpos=%d\n", idev->name, idev->rpos, idev->wpos);
#endif

	ret = do_wait_empty(idev);
	if( ret < 0 )
		return ret;

	// захватываем ресурс
	spin_lock(&idev->lock);

#ifdef DEBUG
	printk("(charbridge): read from %s: (%p,%p,%d)\n", idev->name, file, buffer, length);
#endif

	// запись данных в буфер пользователя
	// while( length && *(idev->beg) ) { 
	while( length && idev->rpos!=idev->wpos ) 
	{
		put_user(idev->buf[idev->rpos++], buffer++);
		if( idev->rpos >= BUF_LEN )
			idev->rpos = 0;

   		length--;
		bytes_read++;
	}

	// освобождаем ресурс
	spin_unlock(&idev->lock);

#ifdef DEBUG
	printk("(charbridge): Read %d bytes, %d left\n", bytes_read, length);
#endif
	
	return bytes_read;
}
/* --------------------------------------------------------------------------- */
/* 
 * Вызывается при попытке записи в файл устройства A
 */
static ssize_t
do_write(struct file *file,
            const char __user * buffer, size_t length, loff_t * offset,
			struct Device_Info *idev)
{
	ssize_t i 		= 0;	

 /* 
   * Если установлен флаг O_NONBLOCK, то процесс не должен приостанавливаться
   * В этом случае, если файл уже открыт, необходимо вернуть код ошибки
   * -EAGAIN, что означает "попробуйте в другой раз"
   */
	if ((file->f_flags & O_NONBLOCK) && !check_empty(idev) )
		return -EAGAIN;

#ifdef DEBUG
	printk("(charbridge): begin write to %s\n", idev->name);
#endif

	spin_lock(&idev->lock);

#ifdef DEBUG
	printk("(charbridge): write to %s(%p,%d)\n", idev->name, file, length);
#endif

	for( i=0; i<length; i++ )
	{
    	get_user(idev->buf[idev->wpos], buffer + i);
		idev->wpos++;
#warning НАДО ОБДУМАТЬ ДАННУЮ ЛОГИКУ!!!
		// если достигли конца буфера, то пишем
		// опять в начало...
		if( idev->wpos >= BUF_LEN )
			idev->wpos = 0;
		
		// сдвигаем rpos, имитируя потерю данных
		// которые не успели вычитать...
		if( idev->wpos == idev->rpos )
		{
#ifdef DEBUG
			  printk("(charbridge): DATA LOSS!! in (%s)...\n", idev->name);
#endif
			idev->rpos = idev->wpos;
		}			
	}

	spin_unlock(&idev->lock);

	// Возобновить работу процессов из WaitQ.
#ifdef DEBUG
 	printk("(charbridge): write to %s beg=%d end=%d wake_up...\n", idev->name, idev->rpos, idev->wpos );
#endif
	wake_up_interruptible(&idev->waitq);
	wake_up_interruptible(&idev->pollq);
	
	// Вернуть количество записанных байт
	return i;
}


/* --------------------------------------------------------------------------- */
static unsigned int do_poll( struct file *file, struct poll_table_struct *wait,
								struct Device_Info *idev )
{
#ifdef DEBUG
	printk("(charbridge): (%s) do_poll \n", idev->name);
#endif

	poll_wait(file, &idev->pollq, wait);

	if( !check_empty(idev) )
		return POLLIN | POLLRDNORM;

#ifdef DEBUG
	printk("(charbridge): (%s) do_poll empty...\n", idev->name);
#endif

	return 0;
}
/* --------------------------------------------------------------------------- */
/*
	Инициализация списка устройств
*/
static void device_init( struct Device_Info* idev, char* name )
{
	memset( idev->buf,0,sizeof(idev->buf) );
	idev->wpos	= 0;
	idev->rpos	= 0;
	idev->open = 0;
	idev->rdev = 0;
	
	spin_lock_init(&idev->lock); // SPIN_LOCK_UNLOCKED;
//	idev->timeout = msecs_to_jiffies(WAIT_TIMEOUT_MSEC);
	init_waitqueue_head(&idev->waitq);
	init_waitqueue_head(&idev->pollq);
	
	memcpy(idev->name,name,sizeof(idev->name));
}	
/* --------------------------------------------------------------------------- */
/* 
 * Вызывается когда процесс пытается открыть файл устройства
 */
static int device_open(struct inode *inode, struct file *file)
{
	unsigned int minor = MINOR(inode->i_rdev);
	unsigned int ind = get_index( minor );
	
	if( ind < 0 )
		return -ENODEV;

	if( !check_sideB(minor) )
	{
//		if( deviceA[ind].open )
//			return -EBUSY;

		deviceA[ind].open++;
		deviceA[ind].rdev = inode->i_rdev;
#ifdef DEBUG
 		printk("(charbridge): open device %s %d count\n", deviceA[ind].name, deviceA[ind].open);
#endif
	}
	else
	{
//		if( deviceB[ind].open )
//			return -EBUSY;

		deviceB[ind].open++;
		deviceB[ind].rdev = inode->i_rdev;
#ifdef DEBUG
		printk("(charbridge): open device %s %d count\n", deviceB[ind].name, deviceB[ind].open);
#endif
	}
		
	try_module_get(THIS_MODULE); // MOD_INC_USE_COUNT
	return SUCCESS;
}
/* --------------------------------------------------------------------------- */
static int device_release(struct inode *inode, struct file *file)
{
	unsigned int minor = MINOR(inode->i_rdev);
	unsigned int ind = get_index( minor );
	
	if( ind < 0 )
		return -ENODEV;

	if( !check_sideB(minor) )
	{
		if( deviceA[ind].open )
			deviceA[ind].open--;
#ifdef DEBUG
 		printk("(charbridge): release device %s (cnt=%d)\n",deviceA[ind].name,deviceA[ind].open);
#endif
	}
	else
	{
		if( deviceB[ind].open )
			deviceB[ind].open--;
#ifdef DEBUG
 		printk("(charbridge): release device %s (cnt=%d)\n",deviceB[ind].name,deviceB[ind].open);
#endif
	}

	module_put(THIS_MODULE); // MOD_DEC_USE_COUNT;
	return SUCCESS;
}

/* --------------------------------------------------------------------------- */
/* 
 * Вызывается когда процесс, открывший файл устройства
 * пытается считать из него данные.
 */
static ssize_t device_read(struct file *file, /* см. include/linux/fs.h   */
            char __user * buffer,             /* буфер для сообщения */
            size_t length,                    /* размер буфера       */
            loff_t * offset)
{
	unsigned int minor = MINOR(file->f_mapping->host->i_rdev);
	unsigned int ind = get_index( minor );
	
	if( ind < 0 )
	{
#ifdef DEBUG
		printk("(charbridge): read from BAD file pointer...\n");
#endif
		return -EBADFD;
	}

	// читаем из буфера ДРУГОГО устройства
	if( !check_sideB(minor) )
		return do_read(file,buffer, length,offset,&deviceB[ind]);
	
	return do_read(file,buffer,length,offset,&deviceA[ind]);
}

/* --------------------------------------------------------------------------- */

/* 
 * Вызывается при попытке записи в файл устройства
 */
static ssize_t
device_write(struct file *file,
            const char __user * buffer, size_t length, loff_t * offset)
{
	unsigned int minor = MINOR(file->f_mapping->host->i_rdev);
	unsigned int ind = get_index( minor );
	if( ind < 0 )
	{
#ifdef DEBUG
		printk("(charbridge): write to BAD file pointer...\n");
#endif
		return -EBADFD;
	}

	if( !check_sideB(minor) )
		return do_write(file,buffer,length,offset,&deviceA[ind]);
	
	return do_write(file,buffer,length,offset,&deviceB[ind]);
}
/* --------------------------------------------------------------------------- */
static unsigned int device_poll( struct file *file, struct poll_table_struct *ptable )
{
	// ждём данных в другом устройстве
	unsigned int minor = MINOR(file->f_mapping->host->i_rdev);
	unsigned int ind = get_index( minor );
	if( ind < 0 )
	{
#ifdef DEBUG
		printk("(charbridge): poll to BAD file pointer...\n");
#endif
		return POLLNVAL;
	}

	if( !check_sideB(minor) )
		return do_poll(file,ptable,&deviceB[ind]);
	
	return do_poll(file,ptable,&deviceA[ind]);
}
/* --------------------------------------------------------------------------- */
/* 
 * Вызывается, когда процесс пытается выполнить операцию ioctl над файлом устройства.
 * Кроме inode и структуры file функция получает два дополнительных параметра:
 * номер ioctl и дополнительные аргументы.
 *
 */
int device_ioctl(struct inode *inode, /* см. include/linux/fs.h */
      struct file *file,              /* то же самое */
      unsigned int ioctl_num,         /* номер и аргументы ioctl */
      unsigned long ioctl_param)
{
  int i;
  char *temp;
  char ch;

  /* 
   * Реакция на различные команды ioctl
   */
  switch (ioctl_num) {
  case IOCTL_SET_MSG:
    /* 
     * Принять указатель на сообщение (в пространстве пользователя)
     * и переписать в буфер. Адрес которого задан в дополнительно аргументе.
     */
    temp = (char *)ioctl_param;

    /* 
     * Найти длину сообщения
     */
    get_user(ch, temp);
    for (i = 0; ch && i < BUF_LEN; i++, temp++)
      get_user(ch, temp);

    device_write(file, (char *)ioctl_param, i, 0);
    break;

  case IOCTL_GET_MSG:
    /* 
     * Передать текущее сообщение вызывающему процессу -
     * записать по указанному адресу.
     */
    i = device_read(file, (char *)ioctl_param, 99, 0);

    /* 
     * Вставить в буфер завершающий символ \0
     */
    put_user('\0', (char *)ioctl_param + i);
    break;

  case IOCTL_GET_NTH_BYTE:
    /* 
     * Этот вызов является вводом (ioctl_param) и
     * выводом (возвращаемое значение функции) одновременно
     */
//    return Message[ioctl_param];
	return 0;
    break;
  }

  return SUCCESS;
}

/* Объявлнеия */
/* --------------------------------------------------------------------------- */
/* 
 * В этой структуре хранятся адреса функций-обработчиков
 * операций, производимых процессом над устройством.
 * Поскольку указатель на эту структуру хранится в таблице устройств,
 * она не может быть локальной для init_module.
 * Отсутствующие указатели в структуре забиваются значением NULL.
 */
struct file_operations Fops = {
  .read = device_read,
  .write = device_write,
//  .ioctl = device_ioctl,
  .open = device_open,
  .release = device_release,    /* оно же close */
  .poll = device_poll
};
/* --------------------------------------------------------------------------- */
/* 
 * Инициализация модуля - Регистрация символьного устройства
 */
int init_module()
{
	int i=0;
	char nm1[NAM_LEN];
	char nm2[NAM_LEN];

  /* 
   * Регистрация символьного устройства (по крайней мере - попытка регистрации)
   */
   dev_major = register_chrdev(0, DEVICE_FILE_NAME, &Fops);

  /* 
   * Отрицательное значение означает ошибку
   */
  if (dev_major < 0) {
    printk("(charbridge): %s failed with %d\n",
           "Sorry, registering the character device1 ", dev_major);
    return dev_major;
  }

	printk("(charbridge): %s The major device=%d.\n",
         "Registeration is a success\n", dev_major);

	for( ; i<MAX_DEV; i++ )
	{
		sprintf(nm1,"cbsideA%d",i);
		sprintf(nm2,"cbsideB%d",i);
		device_init( &deviceA[i], nm1 );
		device_init( &deviceB[i], nm2 );
	}
	
	return 0;
}
/* --------------------------------------------------------------------------- */
/* 
 * Завершение работы модуля - дерегистрация файла в /proc
 */
void cleanup_module()
{
	int ret = 0;

	// Дерегистрация устройства
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23)
	unregister_chrdev(dev_major, DEVICE_FILE_NAME);
#else
	ret = unregister_chrdev(dev_major, DEVICE_FILE_NAME);
#endif

	// Если обнаружена ошибка -- вывести сообщение
	if (ret < 0)
    	printk("Error in module_unregister_chrdev: %d\n", ret);
}
/* --------------------------------------------------------------------------- */
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);    /* Автор модуля */
MODULE_DESCRIPTION(DRIVER_DESC); /* Назначение модуля */

/*  
 *  Этот модуль использует устройство /dev/cbsideAxx(Bxx).
 *  В будущих версиях ядра
 *  макрос MODULE_SUPPORTED_DEVICE может быть использован
 *  для автоматической настройки модуля, но пока
 *  он служит исключительно в описательных целях.
 */
MODULE_SUPPORTED_DEVICE("cbside");
/* --------------------------------------------------------------------------- */
