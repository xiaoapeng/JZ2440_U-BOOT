/*
 * (C) Copyright 2006 OpenMoko, Inc.
 * Author: Harald Welte <laforge@openmoko.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
+ * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>

#include <nand.h>
#include <asm/arch/s3c24x0_cpu.h>
#include <asm/io.h>


#define S3C2440_NFCONF_EN          (1<<15)
#define S3C2440_NFCONF_512BYTE     (1<<14)
#define S3C2440_NFCONF_4STEP       (1<<13)
#define S3C2440_NFCONF_INITECC     (1<<12)
#define S3C2440_NFCONF_nFCE        (1<<11)
#define S3C2440_NFCONF_TACLS(x)    ((x)<<8)
#define S3C2440_NFCONF_TWRPH0(x)   ((x)<<4)
#define S3C2440_NFCONF_TWRPH1(x)   ((x)<<0)

#define S3C2440_ADDR_NALE 4
#define S3C2440_ADDR_NCLE 8

#ifdef CONFIG_NAND_SPL

/* in the early stage of NAND flash booting, printf() is not available */
#define printf(fmt, args...)

static void nand_read_buf(struct mtd_info *mtd, u_char *buf, int len)
{
	int i;
	struct nand_chip *this = mtd->priv;

	for (i = 0; i < len; i++)
		buf[i] = readb(this->IO_ADDR_R);
}
#endif

static void s3c2440_nand_select(struct mtd_info *mtd, int chipnr)
{
	struct s3c2440_nand *nand = s3c2440_get_base_nand();
	switch (chipnr) {
	case -1:/*取消选中*/
		writel(readl(&nand->nfcont) | (1<<1),
			       &nand->nfcont);
		break;
	case 0:
	//使能选中
		writel(readl(&nand->nfcont) & ~(1<<1),
		       &nand->nfcont);
		break;

	default:
		BUG();
	}
}




static void s3c2440_hwcontrol(struct mtd_info *mtd, int cmd_dat, unsigned int ctrl)
{
	struct s3c2440_nand *nand = s3c2440_get_base_nand();
#if 0
	debug("hwcontrol(): 0x%02x 0x%02x\n", cmd, ctrl);

	if (ctrl & NAND_CTRL_CHANGE) {
		ulong IO_ADDR_W = (ulong)nand;

		if (!(ctrl & NAND_CLE))
			IO_ADDR_W |= S3C2440_ADDR_NCLE;
		if (!(ctrl & NAND_ALE))
			IO_ADDR_W |= S3C2440_ADDR_NALE;

		chip->IO_ADDR_W = (void *)IO_ADDR_W;

		if (ctrl & NAND_NCE)
			//使能选中
			writel(readl(&nand->nfcont) & ~(1<<1),
			       &nand->nfcont);
		else
			//取消选中
			writel(readl(&nand->nfcont) | (1<<1),
			       &nand->nfcont);
	}

	if (cmd != NAND_CMD_NONE)
		writeb(cmd, chip->IO_ADDR_W);
#endif
	if(ctrl& NAND_CLE)
	{
		//发命令
		writeb(cmd_dat,&nand->nfcmd);
	}
	else
	if(ctrl & NAND_ALE)
	{
		//发地址
		writeb(cmd_dat,&nand->nfaddr);
	}
}

static int s3c2440_dev_ready(struct mtd_info *mtd)
{
	struct s3c2440_nand *nand = s3c2440_get_base_nand();
	debug("dev_ready\n");
	return readl(&nand->nfstat) & 0x01;
}

#ifdef CONFIG_S3C2440_NAND_HWECC
void s3c2440_nand_enable_hwecc(struct mtd_info *mtd, int mode)
{
	struct s3c2440_nand *nand = s3c2440_get_base_nand();
	debug("s3c2440_nand_enable_hwecc(%p, %d)\n", mtd, mode);
	writel(readl(&nand->nfconf) | S3C2440_NFCONF_INITECC, &nand->nfconf);
}

static int s3c2440_nand_calculate_ecc(struct mtd_info *mtd, const u_char *dat,
				      u_char *ecc_code)
{
	struct s3c2440_nand *nand = s3c2440_get_base_nand();
	ecc_code[0] = readb(&nand->nfecc);
	ecc_code[1] = readb(&nand->nfecc + 1);
	ecc_code[2] = readb(&nand->nfecc + 2);
	debug("s3c2440_nand_calculate_hwecc(%p,): 0x%02x 0x%02x 0x%02x\n",
	       mtd , ecc_code[0], ecc_code[1], ecc_code[2]);

	return 0;
}

static int s3c2440_nand_correct_data(struct mtd_info *mtd, u_char *dat,
				     u_char *read_ecc, u_char *calc_ecc)
{
	if (read_ecc[0] == calc_ecc[0] &&
	    read_ecc[1] == calc_ecc[1] &&
	    read_ecc[2] == calc_ecc[2])
		return 0;

	printf("s3c2440_nand_correct_data: not implemented\n");
	return -1;
}
#endif

int board_nand_init(struct nand_chip *nand)
{
	u_int32_t cfg;
	u_int8_t tacls, twrph0, twrph1;
	struct s3c24x0_clock_power *clk_power = s3c24x0_get_base_clock_power();
	struct s3c2440_nand *nand_reg = s3c2440_get_base_nand();

	debug("board_nand_init()\n");

	writel(readl(&clk_power->clkcon) | (1 << 4), &clk_power->clkcon);

	/* initialize hardware */
#if defined(CONFIG_S3C24XX_CUSTOM_NAND_TIMING)
	tacls  = CONFIG_S3C24XX_TACLS;
	twrph0 = CONFIG_S3C24XX_TWRPH0;
	twrph1 =  CONFIG_S3C24XX_TWRPH1;
#else
	tacls = 4;
	twrph0 = 8;
	twrph1 = 8;
#endif

#if 0
	cfg = S3C2440_NFCONF_EN;
	cfg |= S3C2440_NFCONF_TACLS(tacls - 1);
	cfg |= S3C2440_NFCONF_TWRPH0(twrph0 - 1);
	cfg |= S3C2440_NFCONF_TWRPH1(twrph1 - 1);
#endif
	/*初始化cong寄存器 配置时序*/
	cfg = ((tacls-1)<<12)|((twrph0-1)<<8)|((twrph1-1)<<4);
	writel(cfg, &nand_reg->nfconf);

	/*使能NAND FLASH 控制器 ，初始化RCC，禁止片选*/
	writel((1<<4)|(1<<1)|(1<<0),&nand_reg->nfcont);
	

	/*设置读写寄存器的地址*/
	nand->IO_ADDR_R = (void *)&nand_reg->nfdata;
	nand->IO_ADDR_W = (void *)&nand_reg->nfdata;

	//自己写	如果写为null 那么就调用 nand_select_chip
	nand->select_chip = s3c2440_nand_select;

	/* read_buf and write_buf are default */
	/* read_byte and write_byte are default */
#ifdef CONFIG_NAND_SPL
	nand->read_buf = nand_read_buf;
#endif

	/* hwcontrol always must be implemented */
	nand->cmd_ctrl = s3c2440_hwcontrol;
	

	nand->dev_ready = s3c2440_dev_ready;

#ifdef CONFIG_S3C2440_NAND_HWECC
	nand->ecc.hwctl = s3c2440_nand_enable_hwecc;
	nand->ecc.calculate = s3c2440_nand_calculate_ecc;
	nand->ecc.correct = s3c2440_nand_correct_data;
	nand->ecc.mode = NAND_ECC_HW;
	nand->ecc.size = CONFIG_SYS_NAND_ECCSIZE;
	nand->ecc.bytes = CONFIG_SYS_NAND_ECCBYTES;
#else
	nand->ecc.mode = NAND_ECC_SOFT;
#endif

#ifdef CONFIG_S3C2440_NAND_BBT
	nand->options = NAND_USE_FLASH_BBT;
#else
	nand->options = 0;
#endif

	debug("end of nand_init\n");

	return 0;
}
