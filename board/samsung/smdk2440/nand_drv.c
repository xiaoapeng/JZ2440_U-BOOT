#include <config.h>

/*NAND Flash*/
#define     NFCONF                   __REG(0x4E000000)  //NAND flash configuration             
#define     NFCONT                   __REG(0x4E000004)  //NAND flash control                   
#define     NFCCMD                    __REG(0x4E000008)  //NAND flash command                   
#define     NFADDR                   __REG(0x4E00000C)  //NAND flash address                   
#define     NFDATA                   __REG(0x4E000010)  //NAND flash data                      
#define     NFMECC0                  __REG(0x4E000014)  //NAND flash main area ECC0/1          
#define     NFMECC1                  __REG(0x4E000018)  //NAND flash main area ECC2/3          
#define     NFSECC                   __REG(0x4E00001C)  //NAND flash spare area ECC            
#define     NFSTAT                   __REG(0x4E000020)  //NAND flash operation status          
#define     NFESTAT0                 __REG(0x4E000024)  //NAND flash ECC status for I/O[7:0]   
#define     NFESTAT1                 __REG(0x4E000028)  //NAND flash ECC status for I/O[15:8]  
#define     NFMECC0_STATUS           __REG(0x4E00002C)  //NAND flash main area ECC0 status     
#define     NFMECC1_STATUS           __REG(0x4E000030)  //NAND flash main area ECC1 status     
#define     NFSECC_STATUS            __REG(0x4E000034)  //NAND flash spare area ECC status     
#define     NFSBLK                   __REG(0x4E000038)  //NAND flash start block address       
#define     NFEBLK                   __REG(0x4E00003C)  //NAND flash end block address       
#define     __REG(x)					((volatile unsigned int)(x))

#define dgpio(u_i_gpio)	(*((volatile unsigned int *)(u_i_gpio)))
#define dgpio_bety(u_i_gpio)	(*((volatile unsigned char *)(u_i_gpio)))

static void wait_ready(void)
{
	while (!(dgpio_bety(NFSTAT) & 1));
}

static void nand_init(void)
{
#define  TACLS   0
#define  TWRPH0  1
#define  TWRPH1  0
	/*设置NAND FLASH的时序*/
	dgpio(NFCONF) = (TACLS<<12) | (TWRPH0<<8) | (TWRPH1<<4);
	/*使能NAND FLASH控制器,初始化ECC，禁止片选*/
	dgpio(NFCONT) = (1<<4) | (1<<1) | (1<<0);
}

static void nand_select(void)
{
	/*使能片选*/
	dgpio(NFCONT) &=~(1<<1);
}

static void nand_deselect(void)
{
	/*禁止片选*/
	dgpio(NFCONT) |= (1<<1);
}

static void nand_cmd(unsigned char cmd)
{
	volatile int i;
	dgpio_bety(NFCCMD) = cmd;
	for(i=0; i<10; i++);
}

static void nand_addr_byte(unsigned char addr)
{
	volatile int i;
	dgpio_bety(NFADDR) = addr;
	for(i=0; i<10; i++);
}

static unsigned char nand_data(void)
{
	return	dgpio_bety(NFDATA);
}

static void nand_w_data(unsigned char val)
{
	dgpio_bety(NFDATA) = val;
}

static void nand_read(unsigned int addr, unsigned char *buf, unsigned int len)
{
	int i = 0;
	int page = addr / 2048;
	int col  = addr & (2048 - 1);
	
	nand_select(); 

	while (i < len)
	{
		/* 发出00h命令 */
		nand_cmd(00);

		/* 发出地址 */
		/* col addr */
		nand_addr_byte(col & 0xff);
		nand_addr_byte((col>>8) & 0xff);

		/* row/page addr */
		nand_addr_byte(page & 0xff);
		nand_addr_byte((page>>8) & 0xff);
		nand_addr_byte((page>>16) & 0xff);

		/* 发出30h命令 */
		nand_cmd(0x30);

		/* 等待就绪 */
		wait_ready();

		/* 读数据 */
		for (; (col < 2048) && (i < len); col++)
		{
			buf[i++] = nand_data();			
		}
		if (i == len)
			break;

		col = 0;
		page++;
	}
	
	nand_deselect(); 	
}
int  __nand_boot(void)
{
	//判断为nand启动返回1
	volatile int *p=(int*)0x0;
	int b;
	b=*p;
	*p=b+1;
	if(b+1==*p)
	{
		*p=b;
		return 1;
	}
	return 0;
}
#define SET_GPIO_VAL(GpioReg,val) 		((dgpio((GpioReg))|=(val)))
#define RESER_GPIO_VAL(GpioReg,mask) 	((dgpio((GpioReg))&=(~(mask))))

typedef unsigned int gpiocon_t;
int gpio_set(unsigned int gpio_addr,unsigned int mask,unsigned int val)
{
	unsigned int gpiocon = dgpio(gpio_addr);
	//如果超出要操作的范围 说明该调用不合法
	if((~mask)&val)
		return -1;
	//先清除要操作的位 在对该位赋值 
	RESER_GPIO_VAL(&gpiocon, mask);
	SET_GPIO_VAL(&gpiocon, val);
	dgpio(gpio_addr)=gpiocon;
	return 0;
}
#define     GPFCON                   __REG(0x56000050)  //Port F control                                   
#define     GPFDAT                   __REG(0x56000054)  //Port F data                                      
#define led1(val) gpio_set(GPFDAT,(1<<4),(val<<4))
#define led2(val) gpio_set(GPFDAT,(1<<5),(val<<5))
#define led3(val) gpio_set(GPFDAT,(1<<6),(val<<6))

void led_config(void)
{
	gpio_set(GPFCON,(3<<8)|(3<<10)|(3<<12),(1<<8)|(1<<10)|(1<<12));
	gpio_set(GPFDAT,(1<<4)|(1<<5)|(1<<6),(1<<4)|(1<<5)|(1<<6));
}

void led1on(void)
{
	led1(0);
}

void led1off(void)
{
	led1(1);
}

extern unsigned long __bss_start, __bss_end__;
void relocation(void)
{
	unsigned long* lma_start_p=(unsigned long* )0;
	unsigned long* code_start_p=(unsigned long*)CONFIG_SYS_TEXT_BASE;
	unsigned long* bss_start_p=&__bss_start;
	unsigned long* end_p=&__bss_end__;
	led_config();
	led1(__nand_boot());
	//while(1);
	nand_init();
	if(__nand_boot())
	{
		nand_read(0,(unsigned char*)code_start_p,((unsigned int)bss_start_p-(unsigned int)code_start_p));
	}
	else
	{
		while(code_start_p<bss_start_p)
		{
			*code_start_p=*lma_start_p;
			code_start_p++;
			lma_start_p++;
		}
	}
	while(bss_start_p<end_p)
	{
		*bss_start_p=0;
		bss_start_p++;
	}
}



