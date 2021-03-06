#include "stm32f10x.h"
#include "ds1302.h"
#include "SysTick.h"
#include "printf.h"


#define NOP() __NOP

#define DS1302_CLK_H()	(GPIOB->BSRR=GPIO_Pin_12)
#define DS1302_CLK_L()	(GPIOB->BRR=GPIO_Pin_12)

#define DS1302_RST_H()	(GPIOB->BSRR=GPIO_Pin_14)
#define DS1302_RST_L()	(GPIOB->BRR=GPIO_Pin_14)

#define DS1302_OUT_H()	(GPIOB->BSRR=GPIO_Pin_13)
#define DS1302_OUT_L()	(GPIOB->BRR=GPIO_Pin_13)
											
#define DS1302_IN_X		(GPIOB->IDR&GPIO_Pin_13)

#define Time_24_Hour	0x00	//24时制控制	
#define Time_Start		0x00	//开始走时

#define DS1302_SECOND	0x80	//DS1302各寄存器操作命令定义
#define DS1302_MINUTE	0x82
#define DS1302_HOUR		0x84
#define DS1302_DAY		0x86
#define DS1302_MONTH	0x88
#define DS1302_WEEK		0x8A  
#define DS1302_YEAR		0x8C
#define DS1302_WRITE	0x8E
#define DS1302_POWER	0x90


unsigned char tt[7];
unsigned char year,month,day,week,hour,minute,second;

static GPIO_InitTypeDef GPIO_InitStructure;

void DS1302_Configuration(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	/* PE4,5,6输出 */
	GPIO_ResetBits(GPIOB,GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	//推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	//50M时钟速度
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}


static void DelayNOP(u32 count)
{
	while(count--) ;
}

static void DS1302_OUT(void)
{
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

static void DS1302_IN(void)
{
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

void DS1302SendByte(u8 byte)
{
	u8	i;

	for(i=0x01;i;i<<=1)
	{
		if(byte&i)	DS1302_OUT_H();
		else	DS1302_OUT_L();
		DS1302_CLK_H();
		DelayNOP(50);		//加延时
		DS1302_CLK_L();
	}
}


u8 DS1302ReceiveByte(void)
{
	u8	i,byte=0;

	for(i=0x01;i;i<<=1)
	{
		if(DS1302_IN_X)	byte |= i;
		DS1302_CLK_H();
		DelayNOP(50);		//加延时
		DS1302_CLK_L();
	}
	return(byte);
}


void Write1302(u8 addr,u8 data)
{
  DS1302_OUT();
	DS1302_RST_L();
	DS1302_CLK_L();
	DS1302_RST_H();
	DelayNOP(100);
	DS1302SendByte(addr);
	DS1302SendByte(data);
	DelayNOP(100);
	DS1302_RST_L();
}

u8 Read1302(u8 addr)
{
    u8 data=0;

  DS1302_OUT();
	DS1302_RST_L();
	DS1302_CLK_L();
	DS1302_RST_H();
	DelayNOP(100);
	DS1302SendByte(addr|0x01);
	DS1302_IN();
	data = DS1302ReceiveByte();
	DelayNOP(100);
	DS1302_RST_L();
	return(data);
}

//读取时间函数
void DS1302_GetTime(u8 *time)
{
	time[0] = Read1302(DS1302_YEAR);
	time[1] = Read1302(DS1302_WEEK);
	time[2] = Read1302(DS1302_MONTH);
	time[3] = Read1302(DS1302_DAY);
	time[4] = Read1302(DS1302_HOUR);
	time[5] = Read1302(DS1302_MINUTE);
	time[6] = Read1302(DS1302_SECOND);	
}

/*
读取DS1302中的RAM
addr:地址,从0到30,共31个字节的空间
返回为所读取的数据
*/
u8 ReadDS1302Ram(u8 addr)
{
	u8	tmp,res;

	tmp = (addr<<1)|0xc0;
	res = Read1302(tmp);
	return(res);
}

/*
写DS1302中的RAM
addr:地址,从0到30,共31个字节的空间
data:要写的数据
*/
void WriteDS1302Ram(u8 addr,u8 data)
{
	u8	tmp;

	Write1302(DS1302_WRITE,0x00);		//关闭写保护
	tmp = (addr<<1)|0xc0;
	Write1302(tmp,data);
	Write1302(DS1302_WRITE,0x80);		//打开写保护
}

void ReadDSRam(u8 *p,u8 add,u8 cnt)
{
	u8 i;
	
	if(cnt>30) return;
	for(i=0;i<cnt;i++)
	{
		*p = ReadDS1302Ram(add+1+i);
		p++;
	}
}

void WriteDSRam(u8 *p,u8 add,u8 cnt)
{
	u8 i;
	
	if(cnt>30) return;
	for(i=0;i<cnt;i++)
	{
		WriteDS1302Ram(add+1+i,*p++);
	}
}
  
/*
读时间函数,顺序为:年周月日时分秒
*/
void ReadDS1302Clock(u8 *p)
{
	DS1302_OUT();
	DS1302_RST_L();
	DS1302_CLK_L();
	DS1302_RST_H();
	DelayNOP(100);
	DS1302SendByte(0xbf);			//突发模式
	DS1302_IN();
	p[6] = DS1302ReceiveByte();		//秒
	p[5] = DS1302ReceiveByte();		//分
	p[4] = DS1302ReceiveByte();		//时
	p[3] = DS1302ReceiveByte();		//日
	p[2] = DS1302ReceiveByte();		//月
	p[1] = DS1302ReceiveByte();	  //周
	p[0] = DS1302ReceiveByte();		//年
	DS1302ReceiveByte();			//保护标志字节
	DelayNOP(100);
	DS1302_RST_L();
}

/*
写时间函数,顺序为:年周月日时分秒
*/
void WriteDS1302Clock(u8 *p)
{
	Write1302(DS1302_WRITE,0x00);		//关闭写保护
	DS1302_OUT();
	DS1302_RST_L();
	DS1302_CLK_L();
	DS1302_RST_H();
	DelayNOP(100);
	DS1302SendByte(0xbe);				//突发模式
	DS1302SendByte(p[6]);				//秒
	DS1302SendByte(p[5]);				//分
	DS1302SendByte(p[4]);				//时
	DS1302SendByte(p[3]);				//日
	DS1302SendByte(p[2]);				//月
	DS1302SendByte(p[1]);				//周，设置成周一，没有使用
	DS1302SendByte(p[0]);				//年
	DS1302SendByte(0x80);				//保护标志字节
	DelayNOP(100);
	DS1302_RST_L();
}



void InitClock(void)
{
	u8	tmp;
	DS1302_Configuration();
	tmp = ReadDS1302Ram(0);
	if(tmp^0xa5)
	{
		WriteDS1302Ram(0,0xa5);
		Write1302(DS1302_WRITE,0x00);		//关闭写保护
		Write1302(0x90,0x03);				//禁止涓流充电
		Write1302(DS1302_HOUR,0x00);		//设置成24小时制
		Write1302(DS1302_SECOND,0x00);		//使能时钟运行
		Write1302(DS1302_WRITE,0x80);		//打开写保护
	}
}


void TestDS1302(void)
{
	u8 i,tt[7],dd1[30],dd2[30];
	
	DS1302_Configuration();
	InitClock();
	tt[0] = 0x17;
	tt[1] = 0x08;
	tt[2] = 0x24;
	tt[3] = 0x15;
	tt[4] = 0x06;
	tt[5] = 0x00;
	WriteDS1302Clock(tt);
	for(i=0;i<30;i++)
	{
		dd1[i] = i;
		dd2[i] = 0;
	}
	WriteDSRam(dd1,0,30);
	ReadDSRam(dd2,0,30);
	while(1) 
	{
		ReadDS1302Clock(tt);
		printf("%d",tt[5]);
		delay_ms(1000);
		
	}
}
#define DS1302_SECOND	0x80	//DS1302各寄存器操作命令定义
#define DS1302_MINUTE	0x82
#define DS1302_HOUR		0x84
#define DS1302_DAY		0x86
#define DS1302_MONTH	0x88
#define DS1302_WEEK		0x8A  
#define DS1302_YEAR		0x8C
#define DS1302_WRITE	0x8E
#define DS1302_POWER	0x90

void ds1302init(void)
{	
	InitClock();
  /*if(Read1302(0x86)==0x14 && Read1302(0x84)==0x13 && Read1302(0x82)==0x30 && Read1302(0x80)==0x00)
	{	
	*/
	tt[0] = 0x17;    //年设定
	tt[1] = 0x06;    //周设定
	tt[2] = 0x10;    //月设定
	tt[3] = 0x14;    //日设定
	tt[4] = 0x13;    //时设定
	tt[5] = 0x40;    //分设定
	tt[6] = 0x00;    //秒设定
	//}
	WriteDS1302Clock(tt);
}
	

void	ds1302while1(void) 
	{
		ReadDS1302Clock(tt);	
//	printf("20%d%d年%d%d月%d%d日 星期%d   %d%d : %d%d : %d%d \n",tt[0]/16,tt[0]%16,tt[2]/16,tt[2]%16,tt[3]/16,tt[3]%16,tt[1],tt[4]/16,tt[4]%16,tt[5]/16,tt[5]%16,tt[6]/16,tt[6]%16);
//  delay_ms(1000);
		year   = tt[0]/16*10+tt[0]%16;
		month  = tt[2]/16*10+tt[2]%16;
		day    = tt[3]/16*10+tt[3]%16;
		week   = tt[1];
		hour   = tt[4]/16*10+tt[4]%16;
		minute = tt[5]/16*10+tt[5]%16;
		second = tt[6]/16*10+tt[6]%16;
	}		
