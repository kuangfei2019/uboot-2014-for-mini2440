/*
 * (C) Copyright 2006 OpenMoko, Inc.
 * Author: Harald Welte <laforge@openmoko.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>

#include <nand.h>
#include <asm/arch/s3c24x0_cpu.h>
#include <asm/io.h>

#define NF_BASE 0x4e000000

#define S3C2440_NFCONT_EN          (1<<0)
#define S3C2440_NFCONT_INITECC     (1<<4)
#define S3C2440_NFCONT_nFCE        (1<<1)
#define S3C2440_NFCONT_MAINECCLOCK (1<<5)
#define S3C2440_NFCONF_TACLS(x)    ((x)<<12)
#define S3C2440_NFCONF_TWRPH0(x)   ((x)<<8)
#define S3C2440_NFCONF_TWRPH1(x)   ((x)<<4)

#define S3C2440_ADDR_NALE 0x08
#define S3C2440_ADDR_NCLE 0x0c

ulong IO_ADDR_W = NF_BASE;

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

static void s3c2440_hwcontrol(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
//	struct nand_chip *chip = mtd->priv;
	struct s3c2440_nand *nand = s3c2440_get_base_nand();

	debug("hwcontrol(): 0x%02x 0x%02x\n", cmd, ctrl);

	if (ctrl & NAND_CTRL_CHANGE) {
		IO_ADDR_W = (ulong)nand;

		if (!(ctrl & NAND_CLE))
			IO_ADDR_W |= S3C2440_ADDR_NCLE;
		if (!(ctrl & NAND_ALE))
			IO_ADDR_W |= S3C2440_ADDR_NALE;

//		chip->IO_ADDR_W = (void *)IO_ADDR_W;

		if (ctrl & NAND_NCE)
			writel(readl(&nand->nfcont) & ~S3C2440_NFCONT_nFCE,
			       &nand->nfcont);
		else
			writel(readl(&nand->nfcont) | S3C2440_NFCONT_nFCE,
			       &nand->nfcont);
	}

	if (cmd != NAND_CMD_NONE)
		writeb(cmd, (void *)IO_ADDR_W);
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
	twrph0 = 2;
	twrph1 = 0;
#endif

	cfg = 0;
	cfg |= S3C2440_NFCONF_TACLS(tacls - 1);
	cfg |= S3C2440_NFCONF_TWRPH0(twrph0 - 1);
	cfg |= S3C2440_NFCONF_TWRPH1(twrph1 - 1);
	writel(cfg, &nand_reg->nfconf);

	/* initialize nand_chip data structure */
	nand->IO_ADDR_R = (void *)&nand_reg->nfdata;
	nand->IO_ADDR_W = (void *)&nand_reg->nfdata;

	nand->select_chip = NULL;

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
	nand->ecc.strength = 1;
#else
	nand->ecc.mode = NAND_ECC_SOFT;
#endif

#ifdef CONFIG_S3C2440_NAND_BBT
	nand->bbt_options |= NAND_BBT_USE_FLASH;
#else
	nand->bbt_options = 0;
#endif

	debug("end of nand_init\n");

	return 0;
}
