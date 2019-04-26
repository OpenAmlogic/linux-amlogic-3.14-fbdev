/*
 * drivers/amlogic/mtd/aml_nand.c
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/

#include "aml_mtd.h"


uint8_t nand_boot_flag;

/*define nand oob layout expose to outsize*/
static struct nand_ecclayout aml_nand_uboot_oob = {
	.eccbytes = 84,
	.oobfree = {
		{
			.offset = 0,
			.length = 6
		}
	}
};

static struct nand_ecclayout aml_nand_oob_64 = {
	.eccbytes = 56,
	.oobfree = {
		{ .offset = 0,
		 .length = 8 }
		 }
};

static struct nand_ecclayout aml_nand_oob_128 = {
	.eccbytes = 120,
	.oobfree = {
		{.offset = 0,
		 .length = 8} }
};

static struct nand_ecclayout aml_nand_oob_218 = {
	.eccbytes = 200,
	.oobfree = {
		{.offset = 0,
		 .length = 8} }
};

static struct nand_ecclayout aml_nand_oob_224 = {
	.eccbytes = 208,
	.oobfree = {
		{.offset = 0,
		 .length = 8} }
};

static struct nand_ecclayout aml_nand_oob_256 = {
	.eccbytes = 240,
	.oobfree = {
		{.offset = 0,
		 .length = 16} }
};

static struct nand_ecclayout aml_nand_oob_376 = {
	.eccbytes = 352,
	.oobfree = {
		{.offset = 0,
		 .length = 16} }
};

static struct nand_ecclayout aml_nand_oob_436 = {
	.eccbytes = 352,
	.oobfree = {
		{.offset = 0,
		 .length = 16} }
};

static struct nand_ecclayout aml_nand_oob_448 = {
	.eccbytes = 416,
	.oobfree = {
		{.offset = 0,
		 .length = 16} }
};

static struct nand_ecclayout aml_nand_oob_640 = {
	.eccbytes = 608,
	.oobfree = {
		{.offset = 0,
		 .length = 16} }
};

static struct nand_ecclayout aml_nand_oob_744 = {
	.eccbytes = 700,
	.oobfree = {
		{.offset = 0,
		 .length = 16} }
};

static struct nand_ecclayout aml_nand_oob_1280 = {
	.eccbytes = 1200,
	.oobfree = {
		{.offset = 0,
		 .length = 32} }
};

static struct nand_ecclayout aml_nand_oob_1664 = {
	.eccbytes = 1584,
	.oobfree = {
		{.offset = 0,
		 .length = 32} }
};

void aml_platform_get_user_byte(struct aml_nand_chip *aml_chip,
	unsigned char *oob_buf, int byte_num)
{
	int read_times = 0;
	unsigned int len = PER_INFO_BYTE / sizeof(unsigned int);

	while (byte_num > 0) {
		*oob_buf++ = (aml_chip->user_info_buf[read_times*len] & 0xff);
		byte_num--;
		if (aml_chip->user_byte_mode == 2) {
			*oob_buf++ =
			((aml_chip->user_info_buf[read_times*len] >> 8) & 0xff);
			byte_num--;
		}
		read_times++;
	}
	/*
	 *printk("oob_buf:");
	 *for (i = 0; i < PER_INFO_BYTE; i++)
	 *	printk("%d ", oob_buf_tmp[i]);
	 *printk("\n");
	*/
}

void aml_platform_set_user_byte(struct aml_nand_chip *aml_chip,
	unsigned char *oob_buf, int byte_num)
{
	int write_times = 0;
	unsigned int len = PER_INFO_BYTE/sizeof(unsigned int);

	while (byte_num > 0) {
		aml_chip->user_info_buf[write_times*len] = *oob_buf++;
		byte_num--;
		if (aml_chip->user_byte_mode == 2) {
			aml_chip->user_info_buf[write_times*len] |=
				(*oob_buf++ << 8);
			byte_num--;
		}
		write_times++;
	}
}

int aml_nand_block_bad_scrub_update_bbt(struct mtd_info *mtd)
{
	return 0;
}

static int aml_repair_bbt(struct aml_nand_chip *aml_chip,
	unsigned int *bad_blk_addr, int cnt)
{
	return 0;
}

static int aml_nand_add_partition(struct aml_nand_chip *aml_chip)
{
	struct mtd_info *mtd = &aml_chip->mtd;
	struct aml_nand_platform *plat = aml_chip->platform;
#ifdef CONFIG_MTD_PARTITIONS
	struct mtd_partition *temp_parts = NULL;
	struct mtd_partition *parts;
	int nr, i, error = 0, part_save_in_env = 1, phys_erase_shift;
	u8 part_num = 0;
	loff_t offset;
	int bad_block_cnt = 0;
	loff_t adjust_offset = 0;
	uint64_t last_size = 0, start_blk = 0;
	uint64_t mini_part_size;
	int reserved_part_blk_num = RESERVED_BLOCK_NUM;
	unsigned int bad_blk_addr[128];

	mini_part_size =
	(mtd->erasesize > MINI_PART_SIZE) ? mtd->erasesize : MINI_PART_SIZE;
	phys_erase_shift = fls(mtd->erasesize) - 1;
	parts = plat->platform_nand_data.chip.partitions;
	nr = plat->platform_nand_data.chip.nr_partitions;
	if (!strncmp((char *)plat->name,
		NAND_BOOT_NAME, strlen((const char *)NAND_BOOT_NAME))) {
		if (nr == 0) {
			parts = kzalloc(sizeof(struct mtd_partition),
					GFP_KERNEL);
			if (!parts)
				return -ENOMEM;
		}
		parts->name = NAND_BOOT_NAME;
		parts->offset = 0;
		parts->size = (mtd->writesize * 1024);
		nr = 1;
		nand_boot_flag = 1;
	} else {
		if (nand_boot_flag)
			adjust_offset =
				(1024 * mtd->writesize / aml_chip->plane_num);
		part_num++;
		start_blk = 0;
		do {
			offset = adjust_offset + start_blk * mtd->erasesize;
			error = mtd->_block_isbad(mtd, offset);
			if (error == FACTORY_BAD_BLOCK_ERROR) {
				pr_info("%s:%d factory bad addr =%llx\n",
					__func__, __LINE__,
					(uint64_t)(offset >> phys_erase_shift));
				adjust_offset += mtd->erasesize;
				continue;
			}
			start_blk++;
		} while (start_blk < reserved_part_blk_num);
		adjust_offset += reserved_part_blk_num * mtd->erasesize;

		/*normal mtd device divide part from here(adjust_offset)*/
		if (nr == 0) {
			part_save_in_env = 0;
			if (nand_boot_flag)
				nr = NAND_MINI_PART_NUM + 1;
			else
				nr = 2;
			parts = kzalloc((nr * sizeof(struct mtd_partition)),
				GFP_KERNEL);
			if (!parts)
				return -ENOMEM;
			mini_part_size =
	(mtd->erasesize > MINI_PART_SIZE) ? mtd->erasesize : MINI_PART_SIZE;
		}
		for (i = 0; i < nr; i++) {
			temp_parts = parts + i;
			bad_block_cnt = 0;
			memset((u8 *)bad_blk_addr, 0xff, 128 * sizeof(int));
			if ((temp_parts->size >= mtd->erasesize)
				|| (i == (nr - 1)))
				mini_part_size = temp_parts->size;
			temp_parts->offset = adjust_offset;
			if (i < (nr - 1)) {
				start_blk = 0;
	do {
		offset = adjust_offset + start_blk * mtd->erasesize;
		error = mtd->_block_isbad(mtd, offset);
		if (error == FACTORY_BAD_BLOCK_ERROR) {
			pr_info("%s:%d factory bad addr=%llx\n",
			__func__, __LINE__,
			(uint64_t)(offset >> phys_erase_shift));
			adjust_offset += mtd->erasesize;
			continue;
		} else if (error) {
			if (bad_block_cnt < 128)
				bad_blk_addr[bad_block_cnt] =
				offset >> phys_erase_shift;
			pr_info("%s:%d find %d bad addr =%d\n",
				__func__, __LINE__,
				bad_block_cnt,
				bad_blk_addr[bad_block_cnt]);
				bad_block_cnt++;
		}
		start_blk++;
	} while (start_blk < (mini_part_size >> phys_erase_shift));
	if (mini_part_size > NAND_SYS_PART_SIZE) {
		if (((bad_block_cnt*32) > (mini_part_size >> phys_erase_shift))
		|| (bad_block_cnt > 10))
			aml_repair_bbt(aml_chip, bad_blk_addr, bad_block_cnt);
	}
			} else {
	last_size = mtd->size - adjust_offset;
	start_blk = 0;
	bad_block_cnt = 0;
	memset((unsigned char *)bad_blk_addr, 0xff, 128 * sizeof(int));
	do {
		offset =
		adjust_offset + start_blk * mtd->erasesize;
		error = mtd->_block_isbad(mtd, offset);
		 if (error
		 && (error != FACTORY_BAD_BLOCK_ERROR)) {
			if (bad_block_cnt < 128)
				bad_blk_addr[bad_block_cnt] =
					offset >> phys_erase_shift;
			pr_info("%s:%d find %d bad addr =%d\n",
				__func__, __LINE__,
				bad_block_cnt, bad_blk_addr[bad_block_cnt]);
				bad_block_cnt++;
		}
		start_blk++;
	} while (start_blk < (last_size >> phys_erase_shift));
			}

			if ((i == (nr - 1)) && (part_save_in_env == 0))
				temp_parts->size = NAND_SYS_PART_SIZE;
			else if (mini_part_size != MTDPART_SIZ_FULL)
				temp_parts->size =
			mini_part_size + (adjust_offset - temp_parts->offset);
			adjust_offset += mini_part_size;

			if (temp_parts->name == NULL) {
				temp_parts->name =
				kzalloc(MAX_MTD_PART_NAME_LEN, GFP_KERNEL);
				if (!temp_parts->name)
					return -ENOMEM;
				sprintf((char *)temp_parts->name,
					"mtd%d", part_num++);
			}
		}
	}
	return add_mtd_partitions(mtd, parts, nr);
#else
	return add_mtd_device(mtd);
#endif
}

#ifndef AML_NAND_UBOOT
int nand_idleflag;
/* DEFINE_MUTEX(nand_controller_mutex); */
#define	NAND_CTRL_NONE_RB	(1<<1)
void nand_get_chip(void *chip)
{
	struct aml_nand_chip *aml_chip = (struct aml_nand_chip *)chip;
	struct hw_controller *controller = aml_chip->controller;
	int retry = 0, ret = 0;

	while (1) {
		/* lock controller*/
		/* mutex_lock(&nand_controller_mutex); */
		nand_idleflag = 1;
		if ((aml_chip->options & NAND_CTRL_NONE_RB) == 0) {
			ret = pinctrl_select_state(controller->nand_pinctrl,
				controller->nand_rbstate);
		} else {
			ret = pinctrl_select_state(controller->nand_pinctrl,
				controller->nand_norbstate);
		}
		if (ret < 0)
			pr_info("%s:%d %s can't get pinctrl\n",
				__func__,
				__LINE__,
				dev_name(aml_chip->device));
		else
			break;

		if (retry++ > 10) {
			pr_info("get pin fail over 10 times retry=%d\n",
				retry);
			break;
		}
	}
}

void nand_release_chip(void *chip)
{
	struct aml_nand_chip *aml_chip = (struct aml_nand_chip *)chip;
	struct hw_controller *controller = aml_chip->controller;
	int ret = 0;

	if (nand_idleflag) {
		/* enter standby state. */
		/* controller->enter_standby(controller); */
		/* just enter standby status. */
		NFC_SEND_CMD_STANDBY(controller, 5);/* delay for 5 cycle. */
		ret = pinctrl_select_state(controller->nand_pinctrl,
			controller->nand_idlestate);
		if (ret < 0)
			pr_info("select idle state error\n");
		nand_idleflag = 0;
		/* release controller*/
		/* mutex_unlock(&nand_controller_mutex); */
	}
}
#else
static inline void nand_get_chip(void)
{
	/* pull up enable */
	cpu_id_t cpu_id = get_cpu_id();

	AMLNF_SET_REG_MASK(P_PAD_PULL_UP_EN_REG2, 0x87ff);
	/* pull direction, dqs pull down */
	AMLNF_SET_REG_MASK(P_PAD_PULL_UP_REG2, 0x8700);
	/* switch pinmux */
	if (cpu_id.family_id == MESON_CPU_MAJOR_ID_GXL) {
		AMLNF_CLEAR_REG_MASK(P_PERIPHS_PIN_MUX_7,
			((0x7 << 28) | (0x1FF << 15) | (0xF << 10)));
		AMLNF_SET_REG_MASK(P_PERIPHS_PIN_MUX_7, ((0x1<<31) | 0xff));
	} else if (cpu_id.family_id == MESON_CPU_MAJOR_ID_GXBB) {
		AMLNF_SET_REG_MASK(P_PERIPHS_PIN_MUX_4,
			((0x1<<30) | (0x3ff<<20)));
		AMLNF_CLEAR_REG_MASK(P_PERIPHS_PIN_MUX_0, (0x1 << 19));
		AMLNF_CLEAR_REG_MASK(P_PERIPHS_PIN_MUX_4, (0x3 << 18));
		AMLNF_CLEAR_REG_MASK(P_PERIPHS_PIN_MUX_5, (0xF));
	} else
		pr_info("%s %d: not support cpu type!!!\n",
			__func__, __LINE__);

}

static inline void nand_release_chip(void)
{
	NFC_SEND_CMD_STANDBY(controller, 5);
}
#endif

static void aml_nand_select_chip(struct mtd_info *mtd, int chipnr)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);

	switch (chipnr) {
	case -1:
		nand_release_chip(aml_chip);
			break;
	case 0:
		nand_get_chip(aml_chip);
		aml_chip->aml_nand_select_chip(aml_chip, chipnr);
		break;
	case 1:
	case 2:
	case 3:
		aml_chip->aml_nand_select_chip(aml_chip, chipnr);
		break;

	default:
		/*WARN_ON();*/
		pr_info("%s %d: not support ce num!!!\n",
			__func__, __LINE__);
	}
}

void aml_platform_cmd_ctrl(struct aml_nand_chip *aml_chip,
	int cmd, unsigned int ctrl)
{
	if (cmd == NAND_CMD_NONE)
		return;

	if (ctrl & NAND_CLE)
		cmd = NFC_CMD_CLE(controller->chip_selected, cmd);
	else
		cmd = NFC_CMD_ALE(controller->chip_selected, cmd);

	NFC_SEND_CMD(controller, cmd);
}

int aml_platform_wait_devready(struct aml_nand_chip *aml_chip, int chipnr)
{
	struct nand_chip *chip = &aml_chip->chip;
	struct mtd_info *mtd = &aml_chip->mtd;
	unsigned time_out_cnt = 0;
	int status;

	/* wait until command is processed or timeout occures */
	aml_chip->aml_nand_select_chip(aml_chip, chipnr);

	NFC_SEND_CMD_IDLE(controller, 0);
	NFC_SEND_CMD_IDLE(controller, 0);
	while (NFC_CMDFIFO_SIZE(controller) > 0)
		;

	if (aml_chip->ops_mode & AML_CHIP_NONE_RB) {
		aml_chip->aml_nand_command(aml_chip,
			NAND_CMD_STATUS, -1, -1, chipnr);
		udelay(2);
		NFC_SEND_CMD(controller, controller->chip_selected | IDLE | 0);
		NFC_SEND_CMD(controller, controller->chip_selected | IDLE | 0);
		while (NFC_CMDFIFO_SIZE(controller) > 0)
			;
		do {
			status = (int)chip->read_byte(mtd);
			if (status & NAND_STATUS_READY)
				break;
			udelay(20);
		} while (time_out_cnt++ <= 0x2000); /*10ms max*/

		if (time_out_cnt > 0x2000)
			return 0;
	} else {
		do {
			if (chip->dev_ready(mtd))
				break;
		} while (time_out_cnt++ <= AML_NAND_BUSY_TIMEOUT);

		if (time_out_cnt > AML_NAND_BUSY_TIMEOUT)
			return 0;
	}
	return 1;
}

void aml_nand_cmd_ctrl(struct mtd_info *mtd, int cmd,  unsigned int ctrl)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);

	aml_chip->aml_nand_cmd_ctrl(aml_chip, cmd, ctrl);
}

int aml_nand_wait(struct mtd_info *mtd, struct nand_chip *chip)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	int status[MAX_CHIP_NUM], i = 0, time_cnt = 0;
	struct aml_nand_platform *plat = aml_chip->platform;
	int read_status = 0;
	/* Apply this short delay always to ensure that we do wait tWB in
	 * any case on any machine.
	 */
	ndelay(100);

	/* SET_CBUS_REG_MASK(PREG_PAD_GPIO3_O, 1 << 11); */
	for (i = 0; i < controller->chip_num; i++) {
		if (aml_chip->valid_chip[i]) {
			/* active ce for operation chip and send cmd */
			aml_chip->aml_nand_select_chip(aml_chip, i);

			NFC_SEND_CMD(controller,
				controller->chip_selected | IDLE | 0);
			NFC_SEND_CMD(controller,
				controller->chip_selected | IDLE | 0);
			while (NFC_CMDFIFO_SIZE(controller) > 0)
				;

			/* if ((state == FL_ERASING) */
			/* && (chip->options & NAND_IS_AND)) */
			/* aml_chip->aml_nand_command(aml_chip, */
			/* NAND_CMD_STATUS_MULTI, -1, -1, i); */
			/* else */
			aml_chip->aml_nand_command(aml_chip,
				NAND_CMD_STATUS, -1, -1, i);

			NFC_SEND_CMD(controller,
				controller->chip_selected | IDLE | 0);
			NFC_SEND_CMD(controller,
				controller->chip_selected | IDLE | 0);
			while (NFC_CMDFIFO_SIZE(controller) > 0)
				;

			time_cnt = 0;
retry_status:
			while (time_cnt++ < 0x40000) {
				if (chip->dev_ready) {
					if (chip->dev_ready(mtd))
						break;
					udelay(2);
				} else {
					/* if(time_cnt == 1) */
					udelay(2);
				if (chip->read_byte(mtd) & NAND_STATUS_READY)
					break;
				/* aml_chip->aml_nand_command(aml_chip,*/
					/* NAND_CMD_STATUS, -1, -1, i); */
				/* udelay(50); */

				}
				/* udelay(200); */
			}
			status[i] = (int)chip->read_byte(mtd);
			if ((read_status++ < 3) && (status[i] != 0xe0)) {
				pr_info("after write,read %d status =%d fail\n",
					read_status, status[i]);
				goto retry_status;
			}
			status[0] |= status[i];
		}
	}
	if (!strncmp((char *)plat->name,
		NAND_BOOT_NAME, strlen((const char *)NAND_BOOT_NAME)))
		status[0] = 0xe0;

	return status[0];
}

void aml_nand_base_command(struct aml_nand_chip *aml_chip,
	unsigned command, int column, int page_addr, int chipnr)
{
	struct nand_chip *chip = &aml_chip->chip;
	struct mtd_info *mtd = &aml_chip->mtd;
	unsigned command_temp;
	unsigned pages_per_blk_shift, plane_page_addr = 0, plane_blk_addr = 0;
	uint32_t rst_sts_cnt = CONFIG_SYS_NAND_RESET_CNT;
	struct new_tech_nand_t *new_nand_info = &aml_chip->new_nand_info;
	struct aml_nand_dynamic_read *nand_dynamic_read;

	nand_dynamic_read = &new_nand_info->dynamic_read_info;

	pages_per_blk_shift = (chip->phys_erase_shift - chip->page_shift);

	if (page_addr != -1) {
		page_addr /= aml_chip->plane_num;
		plane_page_addr = (page_addr & ((1 << pages_per_blk_shift)-1));
		plane_blk_addr = (page_addr >> pages_per_blk_shift);
		plane_blk_addr = (plane_blk_addr << 1);
	}

	if (aml_chip->plane_num == 2) {
		switch (command) {
		case NAND_CMD_READ0:
			if ((aml_chip->mfr_type == NAND_MFR_MICRON)
			|| (aml_chip->mfr_type == NAND_MFR_INTEL))
				command_temp = command;
			else {
				command_temp =
				NAND_CMD_TWOPLANE_PREVIOS_READ;
				column = -1;
			}
			plane_page_addr |=
			(plane_blk_addr << pages_per_blk_shift);
			break;

		case NAND_CMD_TWOPLANE_READ1:
			command_temp = NAND_CMD_READ0;
			if ((aml_chip->mfr_type == NAND_MFR_MICRON)
			|| (aml_chip->mfr_type == NAND_MFR_INTEL))
				/*plane_page_addr |=
				 *	((plane_blk_addr + 1) << 8);
				 */
				return;
			plane_page_addr |=
				(plane_blk_addr << pages_per_blk_shift);
			break;

		case NAND_CMD_TWOPLANE_READ2:
			if ((aml_chip->mfr_type == NAND_MFR_MICRON)
			|| (aml_chip->mfr_type == NAND_MFR_INTEL))
				command_temp =
					NAND_CMD_PLANE2_READ_START;
			else
				command_temp = NAND_CMD_READ0;
			plane_page_addr |=
			((plane_blk_addr + 1) << pages_per_blk_shift);
			break;

		case NAND_CMD_SEQIN:
			command_temp = command;
			plane_page_addr |=
			(plane_blk_addr << pages_per_blk_shift);
			break;

		case NAND_CMD_TWOPLANE_WRITE2:
			if ((aml_chip->mfr_type == NAND_MFR_HYNIX)
			|| (aml_chip->mfr_type == NAND_MFR_SAMSUNG))
				command_temp = command;
			else
				command_temp =
					NAND_CMD_TWOPLANE_WRITE2_MICRO;
			plane_page_addr |=
			((plane_blk_addr + 1) << pages_per_blk_shift);
			break;

		case NAND_CMD_ERASE1:
			command_temp = command;
			plane_page_addr |=
			(plane_blk_addr << pages_per_blk_shift);
			break;

		case NAND_CMD_MULTI_CHIP_STATUS:
			command_temp = command;
			plane_page_addr |=
			(plane_blk_addr << pages_per_blk_shift);
			break;

		default:
			command_temp = command;
			break;

		}
		chip->cmd_ctrl(mtd,
			command_temp & 0xff,
			NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);

		/*
		*if ((command_temp == NAND_CMD_SEQIN)
		|| (command_temp == NAND_CMD_TWOPLANE_WRITE2)
		|| (command_temp == NAND_CMD_READ0))
			pr_info("plane_page_addr:%x plane_blk_addr:%x cmd:%x\n",
				plane_page_addr, plane_blk_addr, command);
		*/

		if (column != -1 || page_addr != -1) {
			int ctrl = NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE;
			/* Serially input address */
			if (column != -1) {
				/* Adjust columns for 16 bit buswidth */
				if (chip->options & NAND_BUSWIDTH_16)
					column >>= 1;
				chip->cmd_ctrl(mtd, column, ctrl);
				ctrl &= ~NAND_CTRL_CHANGE;
				chip->cmd_ctrl(mtd, column >> 8, ctrl);
			}
			if (page_addr != -1) {
				chip->cmd_ctrl(mtd, plane_page_addr, ctrl);
				chip->cmd_ctrl(mtd,
				plane_page_addr >> 8, NAND_NCE | NAND_ALE);
				/* One more address cycle for devices > 128MiB*/
				if (chip->chipsize > (128 << 20))
					chip->cmd_ctrl(mtd,
				plane_page_addr >> 16, NAND_NCE | NAND_ALE);
			}
		}

		switch (command) {
		case NAND_CMD_READ0:
			plane_page_addr =
				page_addr % (1 << pages_per_blk_shift);

			if ((aml_chip->mfr_type == NAND_MFR_MICRON)
			|| (aml_chip->mfr_type == NAND_MFR_INTEL)) {
				plane_page_addr |=
			((plane_blk_addr + 1) << pages_per_blk_shift);
				command_temp = command;
				chip->cmd_ctrl(mtd, 0x32,
				NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
		aml_chip->aml_nand_wait_devready(aml_chip, chipnr);
				chip->cmd_ctrl(mtd,
				command_temp & 0xff,
				NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
			} else {
				command_temp =
				NAND_CMD_TWOPLANE_PREVIOS_READ;
				column = -1;
				plane_page_addr |=
			((plane_blk_addr + 1) << pages_per_blk_shift);
				chip->cmd_ctrl(mtd,
				command_temp & 0xff,
				NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
			}

			break;

		case NAND_CMD_TWOPLANE_READ1:
			if ((aml_chip->mfr_type == NAND_MFR_MICRON)
			|| (aml_chip->mfr_type == NAND_MFR_INTEL)) {
				page_addr = -1;
				column = -1;
			} else {
				command_temp = NAND_CMD_RNDOUT;
				page_addr = -1;
				chip->cmd_ctrl(mtd,
				command_temp & 0xff,
				NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
			}
			break;

		case NAND_CMD_TWOPLANE_READ2:
			if ((aml_chip->mfr_type == NAND_MFR_MICRON)
			|| (aml_chip->mfr_type == NAND_MFR_INTEL)) {
				page_addr = -1;
				column = -1;
			} else {
				command_temp = NAND_CMD_RNDOUT;
				page_addr = -1;
				chip->cmd_ctrl(mtd,
				command_temp & 0xff,
				NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
			}
			break;

		case NAND_CMD_ERASE1:
			if ((aml_chip->mfr_type == NAND_MFR_MICRON)
			|| (aml_chip->mfr_type == NAND_MFR_INTEL)) {
				command_temp = NAND_CMD_ERASE1_END;
				chip->cmd_ctrl(mtd,
				command_temp & 0xff,
				NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
			aml_chip->aml_nand_wait_devready(aml_chip,
				chipnr);
			}

			command_temp = command;
			chip->cmd_ctrl(mtd,
				command_temp & 0xff,
				NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
			plane_page_addr =
				page_addr % (1 << pages_per_blk_shift);
			plane_page_addr |=
			((plane_blk_addr + 1) << pages_per_blk_shift);
			break;

		default:
			column = -1;
			page_addr = -1;
			break;
		}

		if (column != -1 || page_addr != -1) {
			int ctrl = NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE;

			/* Serially input address */
			if (column != -1) {
				/* Adjust columns for 16 bit buswidth */
				if (chip->options & NAND_BUSWIDTH_16)
					column >>= 1;
				chip->cmd_ctrl(mtd, column, ctrl);
				ctrl &= ~NAND_CTRL_CHANGE;
				chip->cmd_ctrl(mtd, column >> 8, ctrl);
			}
			if (page_addr != -1) {
				/* plane_page_addr |= */
				/* (1 << (pages_per_blk_shift + 1)); */
				/* BUG_ON((plane_page_addr & 0x7FF) == 0); */

				chip->cmd_ctrl(mtd, plane_page_addr, ctrl);
				chip->cmd_ctrl(mtd, plane_page_addr >> 8,
					NAND_NCE | NAND_ALE);
				/* One more address cycle for devices > 128MiB*/
				if (chip->chipsize > (128 << 20))
					chip->cmd_ctrl(mtd,
				plane_page_addr >> 16, NAND_NCE | NAND_ALE);
			}
		}

		if ((command == NAND_CMD_RNDOUT)
		|| (command == NAND_CMD_TWOPLANE_READ2))
			chip->cmd_ctrl(mtd,
				NAND_CMD_RNDOUTSTART,
				NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
		else if (command == NAND_CMD_TWOPLANE_READ1)
			chip->cmd_ctrl(mtd,
				NAND_CMD_RNDOUTSTART,
				NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);

		else if (command == NAND_CMD_READ0)
			chip->cmd_ctrl(mtd,
				NAND_CMD_READSTART,
				NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);

		chip->cmd_ctrl(mtd, NAND_CMD_NONE, NAND_NCE | NAND_CTRL_CHANGE);
	} else {
		if (aml_chip->support_new_nand == 1) {
			if ((nand_dynamic_read->slc_flag == 1)
			&& (new_nand_info->type == SANDISK_19NM)
			&& ((command == NAND_CMD_ERASE1)
			|| (command == NAND_CMD_READ0)
			|| (command == NAND_CMD_SEQIN))) {
				chip->cmd_ctrl(mtd,
					NAND_CMD_SANDISK_SLC & 0xff,
					NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
				udelay(2);
			}
		}
		chip->cmd_ctrl(mtd,
			command & 0xff,
			NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);

		if (column != -1 || page_addr != -1) {
			int ctrl = NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE;

			/* Serially input address */
			if (column != -1) {
				/* Adjust columns for 16 bit buswidth */
				if (chip->options & NAND_BUSWIDTH_16)
					column >>= 1;
				chip->cmd_ctrl(mtd, column, ctrl);
				ctrl &= ~NAND_CTRL_CHANGE;
				chip->cmd_ctrl(mtd, column >> 8, ctrl);
			}
			if (page_addr != -1) {

				chip->cmd_ctrl(mtd, page_addr, ctrl);
				chip->cmd_ctrl(mtd, page_addr >> 8,
					NAND_NCE | NAND_ALE);
				/* One more address cycle for devices > 128MiB*/
				if (chip->chipsize > (128 << 20))
					chip->cmd_ctrl(mtd,
					page_addr >> 16, NAND_NCE | NAND_ALE);
			}
		}
		if (command == NAND_CMD_RNDOUT)
			chip->cmd_ctrl(mtd,
				NAND_CMD_RNDOUTSTART,
				NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
		else if (command == NAND_CMD_READ0)
			chip->cmd_ctrl(mtd,
				NAND_CMD_READSTART,
				NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);

		chip->cmd_ctrl(mtd, NAND_CMD_NONE, NAND_NCE | NAND_CTRL_CHANGE);
	}

	/*
	 * program and erase have their own busy handlers
	 * status, sequential in, and deplete1 need no delay
	 */
	switch (command) {

	case NAND_CMD_CACHEDPROG:
	case NAND_CMD_PAGEPROG:
	case NAND_CMD_ERASE1:
	case NAND_CMD_ERASE2:
	case NAND_CMD_SEQIN:
	case NAND_CMD_RNDIN:
	case NAND_CMD_STATUS:
	/* case NAND_CMD_DEPLETE1: */
		return;

	/*
	* read error status commands require only a short delay
	*/
	/*
	 *case NAND_CMD_STATUS_ERROR:
	 *case NAND_CMD_STATUS_ERROR0:
	 *case NAND_CMD_STATUS_ERROR1:
	 *case NAND_CMD_STATUS_ERROR2:
	 *case NAND_CMD_STATUS_ERROR3:
	 *	udelay(chip->chip_delay);
	 *	return;
	 */
	case NAND_CMD_RESET:
		if (!aml_chip->aml_nand_wait_devready(aml_chip, chipnr))
			pr_info("couldn`t found selected chip: %d ready\n",
				chipnr);
		 if (chip->dev_ready)
			break;
		udelay(chip->chip_delay);
		chip->cmd_ctrl(mtd,
			NAND_CMD_STATUS,
			NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
		chip->cmd_ctrl(mtd,
			NAND_CMD_NONE, NAND_NCE | NAND_CTRL_CHANGE);
		while (!(chip->read_byte(mtd) & NAND_STATUS_READY) &&
			(rst_sts_cnt--))
			;
		return;

	default:
		/*
		 * If we don't have access to the busy pin, we apply the given
		 * command delay
		 */
		break;
	}

	/* Apply this short delay always to ensure that we do wait tWB in
	 * any case on any machine.
	 */
	ndelay(100);
}

void aml_nand_command(struct mtd_info *mtd,
	unsigned command, int column, int page_addr)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct nand_chip *chip = &aml_chip->chip;
	int i = 0, valid_page_num = 1;/* , internal_chip; */

	if (page_addr != -1) {
		valid_page_num = (mtd->writesize >> chip->page_shift);
		valid_page_num /= aml_chip->plane_num;
		aml_chip->page_addr = page_addr / valid_page_num;
	if (unlikely(aml_chip->page_addr >= aml_chip->internal_page_nums)) {
		/* internal_chip = */
		/* aml_chip->page_addr / aml_chip->internal_page_nums; */
		aml_chip->page_addr -= aml_chip->internal_page_nums;
		aml_chip->page_addr |=
		(1 << aml_chip->internal_chip_shift)*aml_chip->internal_chipnr;
	}
	}

	/* Emulate NAND_CMD_READOOB */
	if (command == NAND_CMD_READOOB) {
		command = NAND_CMD_READ0;
		aml_chip->aml_nand_wait_devready(aml_chip, 0);
		aml_chip->aml_nand_command(aml_chip, command,
			column, aml_chip->page_addr, 0);
		return;
	}
	if (command == NAND_CMD_PAGEPROG)
		return;

	/*
	*if (command == NAND_CMD_SEQIN) {
		aml_chip->aml_nand_select_chip(aml_chip, 0);
		aml_chip->aml_nand_command(aml_chip,
			command, column, page_addr, 0);
		return;
	}
	*/

	for (i = 0; i < controller->chip_num; i++) {
		if (aml_chip->valid_chip[i]) {
			/* active ce for operation chip and send cmd */
			aml_chip->aml_nand_wait_devready(aml_chip, i);
			aml_chip->aml_nand_command(aml_chip,
				command, column, aml_chip->page_addr, i);
		}
	}
}


void aml_nand_erase_cmd(struct mtd_info *mtd, int page)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct nand_chip *chip = mtd->priv;
	unsigned pages_per_blk_shift;
	unsigned vt_page_num, internal_chipnr = 1, page_addr, valid_page_num;
	unsigned i = 0, j = 0;
	unsigned block_addr;

	pages_per_blk_shift = (chip->phys_erase_shift - chip->page_shift);

	vt_page_num = (mtd->writesize / (1 << chip->page_shift));
	vt_page_num *= (1 << pages_per_blk_shift);
	if (page % vt_page_num)
		return;

	/* Send commands to erase a block */
	valid_page_num = (mtd->writesize >> chip->page_shift);

	block_addr = ((page / valid_page_num) >> pages_per_blk_shift);

	if (aml_nand_rsv_erase_protect(mtd, block_addr) == -1)
		return;

	/*
	*if((aml_chip->aml_nandenv_info->valid_node->status)
	&& (block_addr == aml_chip->aml_nandenv_info->valid_node->phy_blk_addr))
		aml_nand_free_valid_env(mtd);
	*/

	valid_page_num /= aml_chip->plane_num;

	aml_chip->page_addr = page / valid_page_num;
	if (unlikely(aml_chip->page_addr >= aml_chip->internal_page_nums)) {
		/* internal_chipnr = */
		/* aml_chip->page_addr / aml_chip->internal_page_nums; */
		aml_chip->page_addr -= aml_chip->internal_page_nums;
		aml_chip->page_addr |=
		(1 << aml_chip->internal_chip_shift)*aml_chip->internal_chipnr;
	}

	if (unlikely(aml_chip->ops_mode & AML_INTERLEAVING_MODE))
		internal_chipnr = aml_chip->internal_chipnr;
	else
		internal_chipnr = 1;

	for (i = 0; i < controller->chip_num; i++) {
		if (aml_chip->valid_chip[i]) {
			aml_chip->aml_nand_select_chip(aml_chip, i);
			page_addr = aml_chip->page_addr;
			for (j = 0; j < internal_chipnr; j++) {
				if (j > 0) {
					page_addr = aml_chip->page_addr;
					page_addr |=
					(1 << aml_chip->internal_chip_shift)*j;
				}
				aml_chip->aml_nand_command(aml_chip,
					NAND_CMD_ERASE1, -1, page_addr, i);
				aml_chip->aml_nand_command(aml_chip,
					NAND_CMD_ERASE2, -1, -1, i);
			}
		}
	}
}

void aml_nand_dma_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);

	aml_chip->aml_nand_dma_read(aml_chip, buf, len, 0);
}

void aml_nand_dma_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);

	aml_chip->aml_nand_dma_write(aml_chip, (unsigned char *)buf, len, 0);
}

int aml_nand_read_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
	uint8_t *buf, int oob_required, int page)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	unsigned nand_page_size = aml_chip->page_size;
	unsigned nand_oob_size = aml_chip->oob_size;
	uint8_t *oob_buf = chip->oob_poi;
	int i, error = 0, j = 0, page_addr, internal_chipnr = 1;

	if (aml_chip->ops_mode & AML_INTERLEAVING_MODE)
		internal_chipnr = aml_chip->internal_chipnr;

	for (i = 0; i < controller->chip_num; i++) {
		if (aml_chip->valid_chip[i]) {
			page_addr = aml_chip->page_addr;
			for (j = 0; j < internal_chipnr; j++) {
				if (j > 0) {
					page_addr = aml_chip->page_addr;
					page_addr |=
				(1 << aml_chip->internal_chip_shift) * j;
					aml_chip->aml_nand_select_chip(aml_chip,
						i);
					aml_chip->aml_nand_command(aml_chip,
					NAND_CMD_READ0, 0, page_addr, i);
				}

				if (!aml_chip->aml_nand_wait_devready(aml_chip,
					i)) {
					pr_info("not found sel chip:%d\n", i);
					error = -EBUSY;
					goto exit;
				}

				if (aml_chip->ops_mode & AML_CHIP_NONE_RB)
					chip->cmd_ctrl(mtd,
					NAND_CMD_READ0 & 0xff,
					NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
				if (aml_chip->plane_num == 2) {
					aml_chip->aml_nand_command(aml_chip,
						NAND_CMD_TWOPLANE_READ1,
						0x00, page_addr, i);
					chip->read_buf(mtd,
						aml_chip->aml_nand_data_buf,
						(nand_page_size+nand_oob_size));
					memcpy(buf, aml_chip->aml_nand_data_buf,
						(nand_page_size+nand_oob_size));
					memcpy(oob_buf,
				aml_chip->aml_nand_data_buf + nand_page_size,
					nand_oob_size);

					oob_buf += nand_oob_size;
					buf += (nand_page_size + nand_oob_size);

					aml_chip->aml_nand_command(aml_chip,
						NAND_CMD_TWOPLANE_READ2,
						0x00, page_addr, i);
					chip->read_buf(mtd,
						aml_chip->aml_nand_data_buf,
						nand_page_size + nand_oob_size);
					memcpy(buf, aml_chip->aml_nand_data_buf,
						nand_page_size + nand_oob_size);
					memcpy(oob_buf,
				aml_chip->aml_nand_data_buf + nand_page_size,
						nand_oob_size);

					oob_buf += nand_oob_size;
					buf += (nand_page_size + nand_oob_size);
				} else if (aml_chip->plane_num == 1) {
					chip->read_buf(mtd,
						aml_chip->aml_nand_data_buf,
						nand_page_size + nand_oob_size);
					memcpy(buf, aml_chip->aml_nand_data_buf,
						nand_page_size);
					memcpy(oob_buf,
				aml_chip->aml_nand_data_buf + nand_page_size,
						nand_oob_size);
					oob_buf += nand_oob_size;
					buf += nand_page_size;
				} else {
					error = -ENODEV;
					goto exit;
				}
			}
		}
	}

exit:
	return error;
}

int aml_nand_write_page_raw(struct mtd_info *mtd,
	struct nand_chip *chip, const uint8_t *buf, int oob_required)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	unsigned nand_page_size = aml_chip->page_size;
	unsigned nand_oob_size = aml_chip->oob_size;
	uint8_t *oob_buf = chip->oob_poi;
	int i, error = 0, j = 0, page_addr, internal_chipnr = 1;

	if (aml_chip->ops_mode & AML_INTERLEAVING_MODE)
		internal_chipnr = aml_chip->internal_chipnr;

	for (i = 0; i < controller->chip_num; i++) {
		if (aml_chip->valid_chip[i]) {
			aml_chip->aml_nand_select_chip(aml_chip, i);
			page_addr = aml_chip->page_addr;
		for (j = 0; j < internal_chipnr; j++) {
			if (j > 0) {
				page_addr = aml_chip->page_addr;
				page_addr |=
				(1 << aml_chip->internal_chip_shift) * j;
				aml_chip->aml_nand_command(aml_chip,
				NAND_CMD_SEQIN, 0, page_addr, i);
			}

			if (aml_chip->plane_num == 2) {
				memcpy(aml_chip->aml_nand_data_buf,
					buf, nand_page_size);
		memcpy(aml_chip->aml_nand_data_buf + nand_page_size,
			oob_buf, nand_oob_size);
				chip->write_buf(mtd,
					aml_chip->aml_nand_data_buf,
					nand_page_size + nand_oob_size);
				aml_chip->aml_nand_command(aml_chip,
				NAND_CMD_DUMMY_PROGRAM, -1, -1, i);

				oob_buf += nand_oob_size;
				buf += nand_page_size;

		if (!aml_chip->aml_nand_wait_devready(aml_chip, i)) {
			pr_info("didn't found selected chip:%d ready\n",
				i);
			error = -EBUSY;
			goto exit;
		}

		memcpy(aml_chip->aml_nand_data_buf,
			buf, nand_page_size);
		memcpy(aml_chip->aml_nand_data_buf + nand_page_size,
			oob_buf, nand_oob_size);
		aml_chip->aml_nand_command(aml_chip,
			NAND_CMD_TWOPLANE_WRITE2, 0x00, page_addr, i);
		chip->write_buf(mtd, aml_chip->aml_nand_data_buf,
			(nand_page_size + nand_oob_size));
		aml_chip->aml_nand_command(aml_chip,
			NAND_CMD_PAGEPROG, -1, -1, i);

				oob_buf += nand_oob_size;
				buf += nand_page_size;
			} else if (aml_chip->plane_num == 1) {
				memcpy(aml_chip->aml_nand_data_buf,
					buf, nand_page_size);
		memcpy(aml_chip->aml_nand_data_buf + nand_page_size,
			oob_buf, nand_oob_size);
				chip->write_buf(mtd,
					aml_chip->aml_nand_data_buf,
					nand_page_size + nand_oob_size);
			if (chip->cmdfunc == aml_nand_command)
				aml_chip->aml_nand_command(aml_chip,
					NAND_CMD_PAGEPROG,
					-1, -1, i);

				oob_buf += nand_oob_size;
				buf += nand_page_size;
			} else {
				error = -ENODEV;
				goto exit;
			}
		}
		}
	}
exit:
	return error;
}

int aml_nand_read_page_hwecc(struct mtd_info *mtd,
	struct nand_chip *chip, uint8_t *buf, int oob_required, int page)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	uint8_t *oob_buf = chip->oob_poi;
	unsigned nand_page_size = (1 << chip->page_shift);
	unsigned pages_per_blk_shift;
	int user_byte_num;
	int error = 0, i = 0, stat = 0, j = 0, page_addr, internal_chipnr = 1;
	int ran_mode = aml_chip->ran_mode;

	struct new_tech_nand_t *new_nand_info = &aml_chip->new_nand_info;
	struct aml_nand_dynamic_read *nand_dynamic_read;
	int page_temp, pages_per_blk, readretry_failed_cnt = 0;
	int retry_cnt = new_nand_info->read_rety_info.retry_cnt;

	nand_dynamic_read = &new_nand_info->dynamic_read_info;

	pages_per_blk_shift = (chip->phys_erase_shift - chip->page_shift);
	user_byte_num = (((nand_page_size + chip->ecc.size - 1)/chip->ecc.size)
		* aml_chip->user_byte_mode);

	pages_per_blk = (1<<(chip->phys_erase_shift-chip->page_shift));
	if (aml_chip->support_new_nand == 1) {
		readretry_failed_cnt = 0;
		if ((new_nand_info->type == HYNIX_20NM_8GB)
			|| (new_nand_info->type == HYNIX_20NM_4GB)
			|| (new_nand_info->type == HYNIX_1YNM_8GB))
			retry_cnt = retry_cnt * retry_cnt;
	}

	if (aml_chip->ops_mode & AML_INTERLEAVING_MODE)
		internal_chipnr = aml_chip->internal_chipnr;

	if (nand_page_size > chip->ecc.steps * chip->ecc.size) {
		nand_page_size = chip->ecc.steps * chip->ecc.size;
		user_byte_num = chip->ecc.steps;
	}

	for (i = 0; i < controller->chip_num; i++) {
		if (aml_chip->valid_chip[i]) {
			readretry_failed_cnt = 0;
read_retry:
		page_addr = aml_chip->page_addr;
		for (j = 0; j < internal_chipnr; j++) {
			if (j > 0) {
				page_addr = aml_chip->page_addr;
			page_addr |= (1 << aml_chip->internal_chip_shift) * j;
			aml_chip->aml_nand_select_chip(aml_chip, i);
			aml_chip->aml_nand_command(aml_chip, NAND_CMD_READ0,
				0, page_addr, i);
			}
		if (!aml_chip->aml_nand_wait_devready(aml_chip, i)) {
			pr_info("couldn`t found selected chip%d ready\n",
				i);
			mdelay(50);
			if (!aml_chip->aml_nand_wait_devready(aml_chip, i)) {
				pr_info("npt found chip%d ready again\n", i);
				mdelay(100);
				error = -EBUSY;
				goto exit;
			}
		}
		if (aml_chip->ops_mode & AML_CHIP_NONE_RB)
			chip->cmd_ctrl(mtd, NAND_CMD_READ0 & 0xff,
				NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
		if (aml_chip->plane_num == 2) {
			aml_chip->aml_nand_command(aml_chip,
				NAND_CMD_TWOPLANE_READ1, 0x00, page_addr, i);
dma_retry_plane0:
			error = aml_chip->aml_nand_dma_read(aml_chip,
				buf, nand_page_size, aml_chip->bch_mode);
			if (error)
				goto exit;

			aml_chip->aml_nand_get_user_byte(aml_chip,
				oob_buf, user_byte_num);
			stat = aml_chip->aml_nand_hwecc_correct(aml_chip,
				buf, nand_page_size, oob_buf);
	if (stat < 0) {
		if (aml_chip->ran_mode
			&& (aml_chip->zero_cnt <  aml_chip->ecc_max)) {
			memset(buf, 0xff, nand_page_size);
			memset(oob_buf, 0xff, user_byte_num);
			goto plane0_ff;
		}

		if (ran_mode && aml_chip->ran_mode) {
			aml_chip->ran_mode = 0;
			ndelay(300);
			aml_chip->aml_nand_command(aml_chip,
			NAND_CMD_RNDOUT, 0, -1, i);
			ndelay(500);
			goto dma_retry_plane0;
		}
		memset(buf, 0xff, nand_page_size);
		memset(oob_buf, 0xff, user_byte_num);

		mtd->ecc_stats.failed++;
		pr_info("read ecc pl0 failed at page%d chip%d\n", page_addr, i);
	} else {
		if (aml_chip->ecc_cnt_cur > aml_chip->ecc_cnt_limit) {
			pr_info("%s %d ", __func__, __LINE__);
			pr_info("uncorrect ecnt:%d limit:%d pg:%d blk:%d c%d\n",
				aml_chip->ecc_cnt_cur, aml_chip->ecc_cnt_limit,
				page_addr, page_addr >> pages_per_blk_shift, i);
			mtd->ecc_stats.corrected++;
		}
		mtd->ecc_stats.corrected += stat;
	}
plane0_ff:
			aml_chip->ran_mode = ran_mode;
			oob_buf += user_byte_num;
			buf += nand_page_size;

			aml_chip->aml_nand_command(aml_chip,
				NAND_CMD_TWOPLANE_READ2, 0x00, page_addr, i);
dma_retry_plane1:
			error = aml_chip->aml_nand_dma_read(aml_chip,
				buf, nand_page_size, aml_chip->bch_mode);
			if (error)
				goto exit;

			aml_chip->aml_nand_get_user_byte(aml_chip,
					oob_buf, user_byte_num);
			stat = aml_chip->aml_nand_hwecc_correct(aml_chip,
					buf, nand_page_size, oob_buf);
	if (stat < 0) {
		if (aml_chip->ran_mode
		&& (aml_chip->zero_cnt <  aml_chip->ecc_max)) {
			memset(buf, 0xff, nand_page_size);
			memset(oob_buf, 0xff, user_byte_num);
			oob_buf += user_byte_num;
			buf += nand_page_size;
			continue;
		}

		if (ran_mode && aml_chip->ran_mode) {
			aml_chip->ran_mode = 0;
			ndelay(300);
			aml_chip->aml_nand_command(aml_chip,
				NAND_CMD_RNDOUT, 0, -1, i);
			ndelay(500);
			goto dma_retry_plane1;
		 }
		memset(buf, 0xff, nand_page_size);
		memset(oob_buf, 0xff, user_byte_num);

		mtd->ecc_stats.failed++;
		pr_info("read ecc pl1 failed at page%d chip%d\n",
			page_addr, i);
	} else {
		if (aml_chip->ecc_cnt_cur > aml_chip->ecc_cnt_limit) {
			pr_info("%s %d ", __func__, __LINE__);
			pr_info("uncorrect ecnt:%d limit:%d pg:%d blk:%d c%d\n",
				aml_chip->ecc_cnt_cur, aml_chip->ecc_cnt_limit,
				page_addr, page_addr >> pages_per_blk_shift, i);
				mtd->ecc_stats.corrected++;
		}
		mtd->ecc_stats.corrected += stat;
	}
			aml_chip->ran_mode = ran_mode;
			oob_buf += user_byte_num;
			buf += nand_page_size;

		} else if (aml_chip->plane_num == 1) {
			error = aml_chip->aml_nand_dma_read(aml_chip,
				buf, nand_page_size, aml_chip->bch_mode);
			if (error)
				goto exit;

			aml_chip->aml_nand_get_user_byte(aml_chip,
				oob_buf, user_byte_num);
			stat = aml_chip->aml_nand_hwecc_correct(aml_chip,
				buf, nand_page_size, oob_buf);
			if (stat < 0) {
				if (aml_chip->ran_mode
				&& (aml_chip->zero_cnt <  aml_chip->ecc_max)) {
					memset(buf, 0xff, nand_page_size);
					memset(oob_buf, 0xff, user_byte_num);
					oob_buf += user_byte_num;
					buf += nand_page_size;
					continue;
				}

	if (aml_chip->support_new_nand == 1) {
		if (new_nand_info->type == SANDISK_19NM) {
			page_temp =  page_addr -
			pages_per_blk * (page_addr >> pages_per_blk_shift);
		if (((page_temp % 2 == 0) && (page_temp != 0))
			|| (page_temp == (pages_per_blk - 1))) {
			if (readretry_failed_cnt++ < DYNAMIC_CNT_UPPER) {
				pr_info("read ecc fail ");
				pr_info("pg:%d blk %d c%d, retrycnt:%d\n",
					page_addr,
					(page_addr >> pages_per_blk_shift),
					i, readretry_failed_cnt);
				nand_dynamic_read->dynamic_read_handle(mtd,
					page_temp, i);
				aml_chip->aml_nand_command(aml_chip,
					NAND_CMD_READ0, 0, page_addr, i);
				goto read_retry;
			}
		} else {
			if (readretry_failed_cnt++ < DYNAMIC_CNT_LOWER) {
				pr_info("read ecc fail ");
				pr_info("pg:%d blk %d c%d,retrycnt:%d\n",
					page_addr,
					(page_addr >> pages_per_blk_shift),
					i, readretry_failed_cnt);
				nand_dynamic_read->dynamic_read_handle(mtd,
					page_temp, i);
				aml_chip->aml_nand_command(aml_chip,
					NAND_CMD_READ0, 0, page_addr, i);
				goto read_retry;
			}
		}
		} else if (new_nand_info->type) {
			if (readretry_failed_cnt++ < retry_cnt) {
				pr_info("read ecc fail ");
				pr_info("pg:%d blk %d c%d, retrycnt:%d\n",
				page_addr, (page_addr >> pages_per_blk_shift),
				i, readretry_failed_cnt);
			new_nand_info->read_rety_info.read_retry_handle(mtd, i);
				aml_chip->aml_nand_command(aml_chip,
					NAND_CMD_READ0, 0, page_addr, i);
				goto read_retry;
			}
		}
	}
				/* memset(buf, 0xff, nand_page_size); */
				memset(oob_buf, 0x22, user_byte_num);
				pr_info("%s %d read ecc failed here at",
					__func__, __LINE__);
				pr_info("page:%d, blk:%d chip[%d]\n",
					page_addr,
					(page_addr >> pages_per_blk_shift),
					i);
				mtd->ecc_stats.failed++;
			if (aml_chip->support_new_nand == 1) {
				if ((new_nand_info->type)
					&& (new_nand_info->type < 10)) {
					aml_chip->aml_nand_command(aml_chip,
						NAND_CMD_RESET, -1, -1, i);
			if (!aml_chip->aml_nand_wait_devready(aml_chip, i)) {
				pr_info("didn't found selected chip%d ready\n",
					i);
				error = -EBUSY;
				goto exit;
			}
				}
			}
			} else {
				aml_chip->ran_mode = ran_mode;
	if (aml_chip->support_new_nand == 1) {
		if (new_nand_info->type == SANDISK_19NM) {
			page_temp =
			page_addr >> pages_per_blk_shift;
		page_temp = pages_per_blk * page_addr;
		page_temp = page_addr - page_temp;
		if (((page_temp % 2 == 0)
		&& (page_temp != 0))
		|| (page_temp == (pages_per_blk - 1))) {
			if (readretry_failed_cnt > DYNAMIC_CNT_UPPER - 2) {
				pr_info("%s %d uncorrected ",
					__func__, __LINE__);
			pr_info("ecnt:%d limit:%d pg:%d blk:%d c%d recnt:%d\n",
				aml_chip->ecc_cnt_cur,
				aml_chip->ecc_cnt_limit,
				page_addr,
				page_addr >> pages_per_blk_shift,
				i, readretry_failed_cnt);
				mtd->ecc_stats.corrected++;
			}
		} else {
			if (readretry_failed_cnt > DYNAMIC_CNT_LOWER - 2) {
				pr_info("%s %d uncorrected ",
					__func__, __LINE__);
			pr_info("ecnt:%d limit:%d pg:%d blk:%d c%d, recnt:%d\n",
				aml_chip->ecc_cnt_cur,
				aml_chip->ecc_cnt_limit,
				page_addr,
				page_addr >> pages_per_blk_shift,
				i, readretry_failed_cnt);
				mtd->ecc_stats.corrected++;
			}
		}
		} else if (new_nand_info->type) {
			if (readretry_failed_cnt > (retry_cnt-2)) {
				pr_info("%s %d uncorrected ",
					__func__, __LINE__);
		pr_info("ecnt:%d limit:%d page:%d blk:%d ce%d, retrycnt:%d\n",
				aml_chip->ecc_cnt_cur,
				aml_chip->ecc_cnt_limit,
				page_addr,
				page_addr >> pages_per_blk_shift, i,
				readretry_failed_cnt);
				mtd->ecc_stats.corrected++;
			}
		}
	}
				mtd->ecc_stats.corrected += stat;
			}
if (aml_chip->support_new_nand == 1) {
	if (readretry_failed_cnt) {
		if ((new_nand_info->type == SANDISK_19NM)
			&& nand_dynamic_read->dynamic_read_exit)
			nand_dynamic_read->dynamic_read_exit(mtd, i);
		else if ((new_nand_info->type)
			&& (new_nand_info->read_rety_info.read_retry_exit))
			new_nand_info->read_rety_info.read_retry_exit(mtd, i);
	}
}
			oob_buf += user_byte_num;
			buf += nand_page_size;
		} else {
			error = -ENODEV;
			goto exit;
		}
		}
	}
	}
exit:
	return error;
}

int aml_nand_write_page_hwecc(struct mtd_info *mtd,
	struct nand_chip *chip, const uint8_t *buf, int oob_required)
{
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	uint8_t *oob_buf = chip->oob_poi;
	unsigned nand_page_size = (1 << chip->page_shift);
	int user_byte_num, temp_value;
	int error = 0, i = 0, j = 0, page_addr, internal_chipnr = 1;

	temp_value = nand_page_size + chip->ecc.size - 1;
	user_byte_num = (temp_value/chip->ecc.size) * aml_chip->user_byte_mode;

	if (aml_chip->ops_mode & AML_INTERLEAVING_MODE)
		internal_chipnr = aml_chip->internal_chipnr;

	memset(oob_buf + mtd->oobavail,
		0xa5, user_byte_num * (mtd->writesize / nand_page_size));

	for (i = 0; i < controller->chip_num; i++) {
		if (aml_chip->valid_chip[i]) {
			page_addr = aml_chip->page_addr;
		for (j = 0; j < internal_chipnr; j++) {
			aml_chip->aml_nand_select_chip(aml_chip, i);
		if (j > 0) {
			page_addr = aml_chip->page_addr;
			page_addr |=
				(1 << aml_chip->internal_chip_shift) * j;
			aml_chip->aml_nand_command(aml_chip,
			NAND_CMD_SEQIN, 0, page_addr, i);
		}
		if (aml_chip->plane_num == 2) {
			aml_chip->aml_nand_set_user_byte(aml_chip,
				oob_buf, user_byte_num);
			error = aml_chip->aml_nand_dma_write(aml_chip,
				(unsigned char *)buf,
				nand_page_size, aml_chip->bch_mode);
			if (error) {
				pr_info("dma write 1 err at page %x\n",
					page_addr);
				goto exit;
			}
			aml_chip->aml_nand_command(aml_chip,
				NAND_CMD_DUMMY_PROGRAM, -1, -1, i);

			oob_buf += user_byte_num;
			buf += nand_page_size;

			if (!aml_chip->aml_nand_wait_devready(aml_chip, i)) {
				pr_info("write couldn't found chip:%d ready\n",
					i);
				error = -EBUSY;
				goto exit;
			}

			aml_chip->aml_nand_command(aml_chip,
				NAND_CMD_TWOPLANE_WRITE2, 0x00, page_addr, i);
			aml_chip->aml_nand_set_user_byte(aml_chip,
				oob_buf, user_byte_num);
			error = aml_chip->aml_nand_dma_write(aml_chip,
				(u8 *)buf,
				nand_page_size, aml_chip->bch_mode);
			if (error) {
				pr_info("aml_nand_dma_write 2 err at page %x\n",
					page_addr);
				goto exit;
			}
			if (aml_chip->cached_prog_status)
				aml_chip->aml_nand_command(aml_chip,
					NAND_CMD_CACHEDPROG, -1, -1, i);
			else
				aml_chip->aml_nand_command(aml_chip,
					NAND_CMD_PAGEPROG, -1, -1, i);

			oob_buf += user_byte_num;
			buf += nand_page_size;
		} else if (aml_chip->plane_num == 1) {
			aml_chip->aml_nand_set_user_byte(aml_chip,
				oob_buf, user_byte_num);
			error = aml_chip->aml_nand_dma_write(aml_chip,
				(unsigned char *)buf,
				nand_page_size, aml_chip->bch_mode);
			if (error) {
				pr_info("aml_nand_dma_write err at page %x\n",
					page_addr);
				goto exit;
			}
			if (chip->cmdfunc == aml_nand_command) {
				if (aml_chip->cached_prog_status)
					aml_chip->aml_nand_command(aml_chip,
						NAND_CMD_CACHEDPROG, -1, -1, i);
				else
					aml_chip->aml_nand_command(aml_chip,
						NAND_CMD_PAGEPROG, -1, -1, i);
			}

			oob_buf += user_byte_num;
			buf += nand_page_size;
		} else {
			error = -ENODEV;
			goto exit;
		}
		}
		}
	}
exit:
	return error;
}

int aml_nand_write_page(struct mtd_info *mtd,
	struct nand_chip *chip, uint32_t offset,
	int data_len,
	const uint8_t *buf,
	int oob_required, int page, int cached, int raw)
{
	int status;
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);

	chip->cmdfunc(mtd, NAND_CMD_SEQIN, 0x00, page);

	if ((cached) && (chip->options & NAND_CACHEPRG))
		aml_chip->cached_prog_status = 1;
	else
		aml_chip->cached_prog_status = 0;
	if (unlikely(raw))
		chip->ecc.write_page_raw(mtd, chip, buf, 0);
	else
		chip->ecc.write_page(mtd, chip, buf, 0);

	if (!cached || !(chip->options & NAND_CACHEPRG)) {
		status = chip->waitfunc(mtd, chip);
		/*
		 * See if operation failed and additional status checks are
		 * available
		 */
		if ((status & NAND_STATUS_FAIL) && (chip->errstat))
			status = chip->errstat(mtd,
				chip, FL_WRITING, status, page);

		if (status & NAND_STATUS_FAIL) {
			pr_info("wr page=0x%x, status =  0x%x\n",
				page, status);
			return -EIO;
		}
	} else
		status = chip->waitfunc(mtd, chip);

	aml_chip->cached_prog_status = 0;
	return 0;
}

int aml_nand_read_oob(struct mtd_info *mtd, struct nand_chip *chip, int page)
{
	int32_t page_addr, user_byte_num, internal_chipnr = 1;
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	unsigned char *nand_buffer = aml_chip->aml_nand_data_buf;
	unsigned char *oob_buffer = chip->oob_poi;
	unsigned pages_per_blk_shift = chip->phys_erase_shift-chip->page_shift;
	unsigned nand_page_size = 1 << chip->page_shift;
	unsigned nand_read_size = mtd->oobavail, dma_once_size;
	unsigned read_chip_num;
	int ran_mode = aml_chip->ran_mode;
	int32_t error = 0, i, stat = 0, j = 0;
	int temp;
	int page_temp, readretry_failed_cnt = 0;
	int pages_per_blk =  (1 << (chip->phys_erase_shift - chip->page_shift));
	int retry_cnt = aml_chip->new_nand_info.read_rety_info.retry_cnt;
	struct new_tech_nand_t *new_nand_info = &aml_chip->new_nand_info;
	struct aml_nand_dynamic_read *nand_dynamic_read;
	struct aml_nand_read_retry *nand_read_retry;

	nand_dynamic_read = &new_nand_info->dynamic_read_info;
	nand_read_retry = &new_nand_info->read_rety_info;

	if (aml_chip->support_new_nand == 1) {
		if ((aml_chip->new_nand_info.type == HYNIX_20NM_8GB)
		|| (aml_chip->new_nand_info.type == HYNIX_20NM_4GB)
		|| (aml_chip->new_nand_info.type == HYNIX_1YNM_8GB)) {
			temp = aml_chip->new_nand_info.read_rety_info.retry_cnt;
			retry_cnt = temp * temp;
		}
	}
	temp = (unsigned)aml_chip->plane_num * nand_page_size;
	read_chip_num = (nand_read_size + temp - 1) / temp;

	if (nand_read_size >= nand_page_size)
		temp = nand_page_size + chip->ecc.size - 1;
	else
		temp = nand_read_size + chip->ecc.size - 1;
	user_byte_num = (temp / chip->ecc.size) * aml_chip->user_byte_mode;
	page_addr = page;
	if (aml_chip->ops_mode & AML_INTERLEAVING_MODE) {
		internal_chipnr = aml_chip->internal_chipnr;
		temp = read_chip_num + aml_chip->internal_chipnr - 1;
		if (read_chip_num < internal_chipnr) {
			internal_chipnr = temp / aml_chip->internal_chipnr;
			read_chip_num = 1;
		} else
			read_chip_num = temp / aml_chip->internal_chipnr;
	}

	if (chip->cmdfunc == aml_nand_command)
		chip->cmdfunc(mtd, NAND_CMD_READOOB, 0, page_addr);
	else {
		aml_chip->aml_nand_select_chip(aml_chip, 0);
		chip->cmdfunc(mtd, NAND_CMD_READ0, 0, page_addr);
	}

	for (i = 0; i < read_chip_num; i++) {
		if (aml_chip->valid_chip[i]) {
			page_addr = aml_chip->page_addr;
			if (i > 0) {
				aml_chip->aml_nand_select_chip(aml_chip, i);
				aml_chip->aml_nand_command(aml_chip,
					NAND_CMD_READ0, 0, page_addr, i);
			}
			readretry_failed_cnt = 0;
read_retry:
			page_addr = aml_chip->page_addr;
			for (j = 0; j < internal_chipnr; j++) {
				if (j > 0) {
					page_addr = aml_chip->page_addr;
					page_addr |=
				(1 << aml_chip->internal_chip_shift) * j;
				aml_chip->aml_nand_select_chip(aml_chip, i);
				aml_chip->aml_nand_command(aml_chip,
					NAND_CMD_READ0, 0, page_addr, i);
				}

			if (!aml_chip->aml_nand_wait_devready(aml_chip, i)) {
				pr_info("read couldn't found c%d ready\n", i);
				error = -EBUSY;
				goto exit;
			}
			if (aml_chip->ops_mode & AML_CHIP_NONE_RB)
				chip->cmd_ctrl(mtd, NAND_CMD_READ0 & 0xff,
					NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);

	if (aml_chip->plane_num == 2) {
		dma_once_size = min(nand_read_size, nand_page_size);
		aml_chip->aml_nand_command(aml_chip,
			NAND_CMD_TWOPLANE_READ1, 0x00, page_addr, i);
dma_retry_plane0:
		error = aml_chip->aml_nand_dma_read(aml_chip,
			nand_buffer, dma_once_size, aml_chip->bch_mode);
		if (error)
			goto exit;

		aml_chip->aml_nand_get_user_byte(aml_chip,
			oob_buffer, user_byte_num);
		stat = aml_chip->aml_nand_hwecc_correct(aml_chip,
			nand_buffer, dma_once_size, oob_buffer);
		if (stat < 0) {
			if (aml_chip->ran_mode
				&& (aml_chip->zero_cnt < aml_chip->ecc_max)) {
				memset(oob_buffer, 0xff, user_byte_num);
				goto plane0_ff;
			}
			if (ran_mode && aml_chip->ran_mode) {
				aml_chip->ran_mode = 0;
				ndelay(300);
				aml_chip->aml_nand_command(aml_chip,
					NAND_CMD_RNDOUT, 0, -1, i);
				ndelay(500);
				goto dma_retry_plane0;
			}
			memset(oob_buffer, 0x22, user_byte_num);
			mtd->ecc_stats.failed++;
			pr_info("rdoob pl0 failed pg%d chip%d\n", page_addr, i);
		} else {
			if (aml_chip->ecc_cnt_cur > aml_chip->ecc_cnt_limit) {
				pr_info("%s %d uncorrect ", __func__, __LINE__);
				pr_info("ecnt:%d limit:%d pg:%d blk:%d c%d\n",
					aml_chip->ecc_cnt_cur,
					aml_chip->ecc_cnt_limit,
					page_addr,
					page_addr >> pages_per_blk_shift,
					i);
				mtd->ecc_stats.corrected++;
			}
			mtd->ecc_stats.corrected += stat;
		}
plane0_ff:
		aml_chip->ran_mode = ran_mode;
		oob_buffer += user_byte_num;
		nand_read_size -= dma_once_size;

		if (nand_read_size > 0) {
			dma_once_size = min(nand_read_size, nand_page_size);
			aml_chip->aml_nand_command(aml_chip,
				NAND_CMD_TWOPLANE_READ2,
				0x00, page_addr, i);
dma_retry_plane1:
			error = aml_chip->aml_nand_dma_read(aml_chip,
					nand_buffer, dma_once_size,
					aml_chip->bch_mode);
			if (error) {
				aml_chip->ran_mode = ran_mode;
				goto exit;
			}

			aml_chip->aml_nand_get_user_byte(aml_chip,
				oob_buffer, user_byte_num);
			stat = aml_chip->aml_nand_hwecc_correct(aml_chip,
				nand_buffer, dma_once_size, oob_buffer);
			if (stat < 0) {
				if (aml_chip->ran_mode
				&& (aml_chip->zero_cnt <  aml_chip->ecc_max)) {
					memset(oob_buffer, 0xff, user_byte_num);
					aml_chip->ran_mode = ran_mode;
					oob_buffer += user_byte_num;
					nand_read_size -= dma_once_size;
					continue;
				}
				if (ran_mode && aml_chip->ran_mode) {
					aml_chip->ran_mode = 0;
					ndelay(300);
					aml_chip->aml_nand_command(aml_chip,
						NAND_CMD_RNDOUT, 0, -1, i);
					ndelay(500);
					goto dma_retry_plane1;
				}
				memset(oob_buffer, 0xff, user_byte_num);
				mtd->ecc_stats.failed++;
				pr_info("read oob pl1 failed page %d chip%d\n",
					page_addr, i);
			} else {
				temp = aml_chip->ecc_cnt_limit;
				if (aml_chip->ecc_cnt_cur > temp) {
					pr_info("%s %d uncorrect ",
						__func__, __LINE__);
				pr_info("ecnt:%d limit:%d pg:%d, blk:%d c%d\n",
					aml_chip->ecc_cnt_cur,
					aml_chip->ecc_cnt_limit,
					page_addr,
					page_addr >> pages_per_blk_shift, i);
					mtd->ecc_stats.corrected++;
				}
				mtd->ecc_stats.corrected += stat;
			}
			aml_chip->ran_mode = ran_mode;
			oob_buffer += user_byte_num;
			nand_read_size -= dma_once_size;
		}
	} else if (aml_chip->plane_num == 1) {
		dma_once_size = min(nand_read_size, nand_page_size);
		error = aml_chip->aml_nand_dma_read(aml_chip,
		nand_buffer, dma_once_size, aml_chip->bch_mode);
		if (error) {
			aml_chip->ran_mode = ran_mode;
			return error;
		}

		aml_chip->aml_nand_get_user_byte(aml_chip,
			oob_buffer, user_byte_num);
		stat = aml_chip->aml_nand_hwecc_correct(aml_chip,
			nand_buffer, dma_once_size, oob_buffer);
		if (stat < 0) {
			if (aml_chip->ran_mode
				&& (aml_chip->zero_cnt  <  aml_chip->ecc_max)) {
				memset(oob_buffer, 0xff, user_byte_num);
				oob_buffer += user_byte_num;
				nand_read_size -= dma_once_size;
				continue;
			}
			aml_chip->ran_mode = ran_mode;
		if (aml_chip->support_new_nand == 1) {
			if (aml_chip->new_nand_info.type == SANDISK_19NM) {
				temp =
				pages_per_blk*(page_addr>>pages_per_blk_shift);
				page_temp = page_addr - temp;
				if (((page_temp % 2 == 0) && (page_temp != 0))
					|| (page_temp == (pages_per_blk - 1))) {
					temp = readretry_failed_cnt;
					if (temp++ < DYNAMIC_CNT_UPPER) {
						pr_info("read ecc failed ");
				pr_info("pg:%d blk %d c%d,retrycnt:%d\n",
					page_addr,
					page_addr>>pages_per_blk_shift,
					i, readretry_failed_cnt);
				nand_dynamic_read->dynamic_read_handle(mtd,
					page_temp, i);
				aml_chip->aml_nand_command(aml_chip,
					NAND_CMD_READ0, 0, page_addr, i);
						goto read_retry;
					}
				} else {
					temp = readretry_failed_cnt;
					if (temp++ < DYNAMIC_CNT_LOWER) {
						pr_info("read ecc failed ");
				pr_info("pg:%d blk%d chip%d, retry_cnt:%d\n",
					page_addr,
					page_addr >> pages_per_blk_shift,
					i, readretry_failed_cnt);
				nand_dynamic_read->dynamic_read_handle(mtd,
					page_temp, i);
				aml_chip->aml_nand_command(aml_chip,
					NAND_CMD_READ0, 0, page_addr, i);
						goto read_retry;
					}
				}
			} else if (aml_chip->new_nand_info.type) {
				if (readretry_failed_cnt++ < retry_cnt) {
					pr_info("read ecc failed ");
					pr_info("pg:%d blk %d c%d retrcnt:%d\n",
						page_addr,
						page_addr>>pages_per_blk_shift,
						i, readretry_failed_cnt);
					nand_read_retry->read_retry_handle(mtd,
						i);
					aml_chip->aml_nand_command(aml_chip,
						NAND_CMD_READ0,
						0, page_addr, i);
					goto read_retry;
				}
			}
		}

			pr_info("%s %d read oob fail at pg:%d, blk:%d c%d\n",
				__func__, __LINE__, page_addr,
				page_addr >> pages_per_blk_shift, i);

			memset(oob_buffer, 0x22, user_byte_num);
			mtd->ecc_stats.failed++;
		if (aml_chip->support_new_nand == 1) {
			if ((aml_chip->new_nand_info.type)
				&& (aml_chip->new_nand_info.type < 10)) {
				aml_chip->aml_nand_command(aml_chip,
						NAND_CMD_RESET, -1, -1, i);
			if (!aml_chip->aml_nand_wait_devready(aml_chip, i)) {
				pr_info("read not found c%d ready\n",
					i);
				error = -EBUSY;
				goto exit;
			}
			}
		}
		} else {
			aml_chip->ran_mode = ran_mode;
		if (aml_chip->support_new_nand == 1) {
			if (aml_chip->new_nand_info.type == SANDISK_19NM) {
				temp = page_addr >> pages_per_blk_shift;
				temp = pages_per_blk * temp;
				page_temp = page_addr - temp;
				temp = readretry_failed_cnt;
				if (((page_temp % 2 == 0) && (page_temp != 0))
					|| (page_temp == (pages_per_blk - 1))) {
					if (temp > DYNAMIC_CNT_UPPER - 2) {
						pr_info("%s %d uncorrect ",
							__func__, __LINE__);
		pr_info("ecccnt:%d limit:%d pg:%d blk:%d C%d rrcnt:%d\n",

			aml_chip->ecc_cnt_cur, aml_chip->ecc_cnt_limit,
			page_addr,
			page_addr >> pages_per_blk_shift,
			i, temp);
						mtd->ecc_stats.corrected++;
					}
				} else {
					if (temp > DYNAMIC_CNT_LOWER - 2) {
						pr_info("%s %d uncorrect ",
							__func__, __LINE__);
		pr_info("ecccnt:%d limit:%d pg:%d blk:%d C%d rrcnt:%d\n",
			aml_chip->ecc_cnt_cur,
			aml_chip->ecc_cnt_limit,
			page_addr,
			page_addr >> pages_per_blk_shift,
			i, temp);
						mtd->ecc_stats.corrected++;
					}
				}
			} else if (aml_chip->new_nand_info.type) {
				if (readretry_failed_cnt > (retry_cnt-2)) {
					pr_info("%s %d uncorrect ",
						__func__, __LINE__);
		pr_info("ecccnt:%d limit:%d page:%d blk:%d C%d rrcnt:%d\n",
			aml_chip->ecc_cnt_cur, aml_chip->ecc_cnt_limit,
			page_addr,
			page_addr >> pages_per_blk_shift,
			i, readretry_failed_cnt);
					mtd->ecc_stats.corrected++;
				}

			}
		}
			mtd->ecc_stats.corrected += stat;
		}
	if (aml_chip->support_new_nand == 1) {
		if (readretry_failed_cnt) {
			if ((aml_chip->new_nand_info.type == SANDISK_19NM)
				&& (nand_dynamic_read->dynamic_read_exit))
				nand_dynamic_read->dynamic_read_exit(mtd, i);
			else if ((aml_chip->new_nand_info.type)
				&& (nand_read_retry->read_retry_exit))
				nand_read_retry->read_retry_exit(mtd, i);
		}
	}
		oob_buffer += user_byte_num;
		nand_read_size -= dma_once_size;
	} else {
		error = -ENODEV;
		goto exit;
	} /*plane*/
			} /*for (j = 0; j < internal_chipnr; j++) { */
		} /*if (aml_chip->valid_chip[i]) {*/
	} /*for (i = 0; i < read_chip_num; i++) {*/
exit:
	return nand_read_size;
}

int aml_nand_write_oob(struct mtd_info *mtd, struct nand_chip *chip, int page)
{
	pr_info("our host controller`s structure couldn`t support oob write\n");
	/*WARN_ON();*/
	return 0;
}

int aml_nand_block_bad(struct mtd_info *mtd, loff_t ofs, int getchip)
{
	struct nand_chip *chip = mtd->priv;
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct aml_nand_platform *plat = aml_chip->platform;
	struct mtd_oob_ops aml_oob_ops;
	int32_t read_cnt, page, mtd_erase_shift, blk_addr, pages_per_blk;
	loff_t addr;
	int32_t ret = 0;

	if ((!strncmp((char *)plat->name,
		NAND_BOOT_NAME, strlen((const char *)NAND_BOOT_NAME))))
		return 0;

	mtd_erase_shift = fls(mtd->erasesize) - 1;
	blk_addr = (int)(ofs >> mtd_erase_shift);

	if (aml_chip->block_status != NULL) {
		if (aml_chip->block_status[blk_addr] == NAND_BLOCK_BAD) {
			pr_info(" NAND bbt detect Bad block at %llx\n",
				(uint64_t)ofs);
			return -EFAULT;
		}
		if (aml_chip->block_status[blk_addr] == NAND_FACTORY_BAD) {
			pr_info(" NAND bbt detect factory Bad block at %llx\n",
				(uint64_t)ofs);
			return FACTORY_BAD_BLOCK_ERROR;  /* 159  EFAULT */
		} else if (aml_chip->block_status[blk_addr] == NAND_BLOCK_GOOD)
			return 0;
	}
	chip->pagebuf = -1;
	pages_per_blk = (1 << (chip->phys_erase_shift - chip->page_shift));
	if (getchip) {
		aml_oob_ops.mode = MTD_OPS_AUTO_OOB;
		aml_oob_ops.len = mtd->writesize;
		aml_oob_ops.ooblen = mtd->oobavail;
		aml_oob_ops.ooboffs = chip->ecc.layout->oobfree[0].offset;
		aml_oob_ops.datbuf = chip->buffers->databuf;
		aml_oob_ops.oobbuf = chip->oob_poi;

		for (read_cnt = 0; read_cnt < 2; read_cnt++) {
			addr =
			ofs + (pages_per_blk - 1) * read_cnt * mtd->writesize;
			ret = mtd->_read_oob(mtd, addr, &aml_oob_ops);
			if (ret == -EUCLEAN)
				ret = 0;
			if (ret < 0) {
				pr_info("1NAND detect Bad block:%llx\n",
					(uint64_t)addr);
				return -EFAULT;
			}
			if (aml_oob_ops.oobbuf[chip->badblockpos] == 0xFF)
				continue;
			if (aml_oob_ops.oobbuf[chip->badblockpos] == 0) {
				memset(aml_chip->aml_nand_data_buf,
					0, aml_oob_ops.ooblen);
				if (!memcmp(aml_chip->aml_nand_data_buf,
				aml_oob_ops.oobbuf, aml_oob_ops.ooblen)) {
					pr_info("2NAND detect Bad block:%llx\n",
						(uint64_t)addr);
					return -EFAULT;
				}
			}
		}
	} else {
		for (read_cnt = 0; read_cnt < 2; read_cnt++) {
			addr =
			ofs + (pages_per_blk - 1) * read_cnt * mtd->writesize;
			page = (int)(addr >> chip->page_shift);
			ret = chip->ecc.read_oob(mtd, chip, page);
			if (ret == -EUCLEAN)
				ret = 0;
			if (ret < 0)
				return -EFAULT;
			if (chip->oob_poi[chip->badblockpos] == 0xFF)
				return 0;

			if (chip->oob_poi[chip->badblockpos] == 0) {
				memset(aml_chip->aml_nand_data_buf,
					0, (mtd->writesize + mtd->oobsize));
		if (!memcmp(aml_chip->aml_nand_data_buf + mtd->writesize,
			chip->oob_poi, mtd->oobavail)) {
			pr_info("3NAND detect Bad blk:%llx\n", (uint64_t)addr);
			return -EFAULT;
		}
			}
		}
	}

	return 0;
}

int aml_nand_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
	struct nand_chip *chip = mtd->priv;
	struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd);
	struct mtd_oob_ops aml_oob_ops;
	int blk_addr, mtd_erase_shift;
	int8_t *buf = NULL;

	mtd_erase_shift = fls(mtd->erasesize) - 1;
	blk_addr = (int)(ofs >> mtd_erase_shift);
	if (aml_chip->block_status != NULL) {
		if ((aml_chip->block_status[blk_addr] == NAND_BLOCK_BAD)
		|| (aml_chip->block_status[blk_addr] == NAND_FACTORY_BAD))
			/* return 0; */
			goto mark_bad;
		else if (aml_chip->block_status[blk_addr] == NAND_BLOCK_GOOD) {
			aml_chip->block_status[blk_addr] = NAND_BLOCK_BAD;
			buf = aml_chip->block_status;
			aml_nand_save_bbt(mtd, (u_char *)buf);
		}
	}
mark_bad:
	/*no erase here, fixit*/
	aml_oob_ops.mode = MTD_OPS_AUTO_OOB;
	aml_oob_ops.len = mtd->writesize;
	aml_oob_ops.ooblen = mtd->oobavail;
	aml_oob_ops.ooboffs = chip->ecc.layout->oobfree[0].offset;
	aml_oob_ops.datbuf = chip->buffers->databuf;
	aml_oob_ops.oobbuf = chip->oob_poi;
	chip->pagebuf = -1;

	memset((unsigned char *)aml_oob_ops.datbuf, 0x0, mtd->writesize);
	memset((unsigned char *)aml_oob_ops.oobbuf, 0x0, aml_oob_ops.ooblen);

	return mtd->_write_oob(mtd, ofs, &aml_oob_ops);
}

static uint8_t aml_platform_read_byte(struct mtd_info *mtd)
{
	/* struct nand_chip *chip = mtd->priv; */
	/* struct aml_nand_chip *aml_chip = mtd_to_nand_chip(mtd); */
	uint8_t status;

	NFC_SEND_CMD_DRD(controller, controller->chip_selected, 0);
	NFC_SEND_CMD_IDLE(controller, NAND_TWB_TIME_CYCLE);

	NFC_SEND_CMD_IDLE(controller, 0);
	NFC_SEND_CMD_IDLE(controller, 0);

	while (NFC_CMDFIFO_SIZE(controller) > 0)
		;
	status = amlnf_read_reg32(controller->reg_base + P_NAND_BUF);
	/* pr_info("rd:%x\n", status); */
	return status;
}

void aml_platform_write_byte(struct aml_nand_chip *aml_chip, uint8_t data)
{
	NFC_SEND_CMD_IDLE(controller, NAND_TWB_TIME_CYCLE);
	NFC_SEND_CMD_DWR(controller, controller->chip_selected, data);
	NFC_SEND_CMD_IDLE(controller, NAND_TWB_TIME_CYCLE);

	NFC_SEND_CMD_IDLE(controller, 0);
	NFC_SEND_CMD_IDLE(controller, 0);

	while (NFC_CMDFIFO_SIZE(controller) > 0)
		;
}

int aml_nand_init(struct aml_nand_chip *aml_chip)
{
	struct aml_nand_platform *plat = aml_chip->platform;
	struct nand_chip *chip = &aml_chip->chip;
	struct mtd_info *mtd = &aml_chip->mtd;
	int err = 0, i = 0, tmp_value;
	int oobmul;
	unsigned valid_chip_num = 0;
	struct nand_oobfree *oobfree = NULL;
	u32 elsize;
	struct new_tech_nand_t *new_nand_info = &aml_chip->new_nand_info;
	struct aml_nand_read_retry *nand_read_retry;
	struct aml_nand_slc_program *slc_program_info;

	nand_read_retry = &new_nand_info->read_rety_info;
	slc_program_info = &new_nand_info->slc_program_info;

	chip->IO_ADDR_R = chip->IO_ADDR_W =
		devm_ioremap_nocache(controller->device,
			NAND_BASE_APB + P_NAND_BUF,
			sizeof(int));

	chip->options |= NAND_SKIP_BBTSCAN;
	chip->options |= NAND_NO_SUBPAGE_WRITE;

	if (chip->ecc.mode != NAND_ECC_SOFT) {
		if (aml_chip->user_byte_mode == 2)
			chip->ecc.layout = &aml_nand_oob_64;
	}

	chip->select_chip = aml_nand_select_chip;
	chip->cmd_ctrl = aml_nand_cmd_ctrl;
	chip->read_byte = aml_platform_read_byte;

	controller->chip_num = plat->platform_nand_data.chip.nr_chips;
	if (controller->chip_num > MAX_CHIP_NUM) {
		err = -ENXIO;
		goto exit_error;
	}

	controller->chip_num = 1;
	for (i = 0; i < controller->chip_num; i++)
		aml_chip->valid_chip[i] = 1;

	/*use NO RB mode to detect nand chip num*/
	aml_chip->ops_mode |= AML_CHIP_NONE_RB;
	chip->chip_delay = 100;

	aml_chip->aml_nand_hw_init(aml_chip);
	aml_chip->toggle_mode = 0;
	if (nand_scan(mtd, controller->chip_num) == 0) {
		chip->options = 0;
		chip->options |=  NAND_SKIP_BBTSCAN;
		chip->options |= NAND_NO_SUBPAGE_WRITE;
		if (aml_nand_scan(mtd, controller->chip_num)) {
			err = -ENXIO;
			goto exit_error;
		}
	} else {
		pr_info("pre nand scan failed\n");
		err = -ENXIO;
		goto exit_error;
	}
	valid_chip_num = 0;
	for (i = 0; i < controller->chip_num; i++) {
		if (aml_chip->valid_chip[i])
			valid_chip_num++;
	}

	chip->scan_bbt = aml_nand_scan_bbt;
	if (aml_chip->aml_nand_adjust_timing)
		aml_chip->aml_nand_adjust_timing(aml_chip);

	if (aml_chip->aml_nand_options_confirm(aml_chip)) {
		err = -ENXIO;
		goto exit_error;
	}
	if (plat->platform_nand_data.chip.ecclayout)
		chip->ecc.layout = plat->platform_nand_data.chip.ecclayout;
	else {
		oobmul = mtd->oobsize / aml_chip->oob_size;
		if (!chip->ecc.layout)
			chip->ecc.layout =
			kzalloc(sizeof(struct nand_ecclayout), GFP_KERNEL);
		if (!chip->ecc.layout) {
			err = -ENOMEM;
			goto exit_error;
		}
		if (!strncmp((char *)plat->name, NAND_BOOT_NAME,
			strlen((const char *)NAND_BOOT_NAME)))
			memcpy(chip->ecc.layout,
			&aml_nand_uboot_oob, sizeof(struct nand_ecclayout));
		else if (chip->ecc.mode != NAND_ECC_SOFT) {
			elsize = sizeof(struct nand_ecclayout);
			switch (aml_chip->oob_size) {
			case 64:
			memcpy(chip->ecc.layout, &aml_nand_oob_64, elsize);
			break;
			case 128:
			memcpy(chip->ecc.layout, &aml_nand_oob_128, elsize);
			break;
			case 218:
			memcpy(chip->ecc.layout, &aml_nand_oob_218, elsize);
			break;
			case 224:
			memcpy(chip->ecc.layout, &aml_nand_oob_224, elsize);
			break;
			case 256:
			memcpy(chip->ecc.layout, &aml_nand_oob_256, elsize);
			break;
			case 376:
			memcpy(chip->ecc.layout, &aml_nand_oob_376, elsize);
			break;
			case 436:
			memcpy(chip->ecc.layout, &aml_nand_oob_436, elsize);
			break;
			case 448:
			memcpy(chip->ecc.layout, &aml_nand_oob_448, elsize);
			break;
			case 640:
			memcpy(chip->ecc.layout, &aml_nand_oob_640, elsize);
			break;
			case 744:
			memcpy(chip->ecc.layout, &aml_nand_oob_744, elsize);
			break;
			case 1280:
			memcpy(chip->ecc.layout, &aml_nand_oob_1280, elsize);
			break;
			case 1664:
			memcpy(chip->ecc.layout, &aml_nand_oob_1664, elsize);
			break;
			default:
			pr_info("default,use nand base oob layout %d\n",
				mtd->oobsize);
			chip->ecc.layout->oobfree[0].length =
		(mtd->writesize / chip->ecc.size) * aml_chip->user_byte_mode;
			break;
			}

			chip->ecc.layout->oobfree[0].length *= oobmul;
			chip->ecc.layout->eccbytes *= oobmul;
			pr_info("%s :oobmul=%d,oobfree.length=%d,oob_size=%d\n",
				__func__,
				oobmul,
				chip->ecc.layout->oobfree[0].length,
				aml_chip->oob_size);
		}
	}

	/*
	 * The number of bytes available for a client to place data into
	 * the out of band area
	 */
	chip->ecc.layout->oobavail = 0;
	oobfree = chip->ecc.layout->oobfree;
	tmp_value = ARRAY_SIZE(chip->ecc.layout->oobfree);
	for (i = 0; oobfree[i].length && i < tmp_value; i++)
		chip->ecc.layout->oobavail += oobfree[i].length;
	pr_info("oob avail size %d\n", chip->ecc.layout->oobavail);

	mtd->oobavail = chip->ecc.layout->oobavail;
	mtd->ecclayout = chip->ecc.layout;

	aml_chip->virtual_page_size = mtd->writesize;
	aml_chip->virtual_block_size = mtd->erasesize;
#ifndef AML_NAND_UBOOT
#if (AML_CFG_CONHERENT_BUFFER)
	aml_chip->aml_nand_data_buf = dma_alloc_coherent(aml_chip->device,
		(mtd->writesize + mtd->oobsize),
		&aml_chip->data_dma_addr, GFP_KERNEL);
#else
	aml_chip->aml_nand_data_buf = kmalloc(mtd->writesize + mtd->oobsize,
		GFP_KERNEL);
#endif
	if (aml_chip->aml_nand_data_buf == NULL) {
		pr_info("no memory for flash data buf\n");
		err = -ENOMEM;
		goto exit_error;
	}
	aml_chip->user_info_buf = dma_alloc_coherent(aml_chip->device,
		(mtd->writesize / chip->ecc.size) * PER_INFO_BYTE,
		&aml_chip->info_dma_addr, GFP_KERNEL);
	if (aml_chip->user_info_buf == NULL) {
		pr_info("no memory for flash info buf\n");
		err = -ENOMEM;
		goto exit_error;
	}
#else
	aml_chip->aml_nand_data_buf =
		kzalloc((mtd->writesize + mtd->oobsize), GFP_KERNEL);
	if (aml_chip->aml_nand_data_buf == NULL) {
		pr_info("no memory for flash data buf\n");
		err = -ENOMEM;
		goto exit_error;
	}
	aml_chip->user_info_buf =
		kzalloc((mtd->writesize / chip->ecc.size) * PER_INFO_BYTE,
		GFP_KERNEL);
	if (aml_chip->user_info_buf == NULL) {
		pr_info("no memory for flash info buf\n");
		err = -ENOMEM;
		goto exit_error;
	}
#endif
	/*
	*if (chip->buffers)
		kfree(chip->buffers);
	if (mtd->oobsize >= NAND_MAX_OOBSIZE)
		chip->buffers =
		kzalloc((mtd->writesize + 3 * mtd->oobsize), GFP_KERNEL);
	else
		chip->buffers =
		kzalloc((mtd->writesize + 3 * NAND_MAX_OOBSIZE), GFP_KERNEL);
	*/

	if (chip->buffers == NULL) {
		pr_info("no memory for flash data buf\n");
		err = -ENOMEM;
		goto exit_error;
	}

	chip->oob_poi = chip->buffers->databuf + mtd->writesize;
	chip->options |= NAND_OWN_BUFFERS;

	if (aml_chip->support_new_nand == 1) {
		if ((new_nand_info->type) && (new_nand_info->type < 10)) {
			if (slc_program_info->get_default_value)
				slc_program_info->get_default_value(mtd);
		}
	}
	pr_info("%s %d: plat-name:%s\n",
		__func__, __LINE__, (char *)plat->name);
	if (strncmp((char *)plat->name,
		NAND_BOOT_NAME, strlen((const char *)NAND_BOOT_NAME))) {
		if (aml_chip->support_new_nand == 1) {
			if ((new_nand_info->type)
				&& (new_nand_info->type < 10)) {
				if (nand_read_retry->get_default_value)
					nand_read_retry->get_default_value(mtd);
			}
		if ((new_nand_info->type)
		&& ((new_nand_info->type == SANDISK_19NM)
		|| (new_nand_info->type == SANDISK_24NM)))
			new_nand_info->dynamic_read_info.dynamic_read_init(mtd);
		}
	aml_nand_rsv_info_init(mtd);
	err = aml_nand_bbt_check(mtd);
	if (err) {
		pr_info("invalid nand bbt\n");
		goto exit_error;
	}
	aml_nand_env_check(mtd);
	aml_nand_key_check(mtd);
	aml_nand_dtb_check(mtd);

	if (aml_chip->support_new_nand == 1) {
		if ((new_nand_info->type)
		&& (new_nand_info->type < 10)
		&& (new_nand_info->read_rety_info.default_flag == 0))
			new_nand_info->read_rety_info.save_default_value(mtd);
	}
	}
	if (aml_nand_add_partition(aml_chip) != 0) {
		err = -ENXIO;
		goto exit_error;
	}

	pr_info("%s initialized ok\n", mtd->name);
	return 0;

exit_error:
	if (aml_chip->user_info_buf) {
#ifndef AML_NAND_UBOOT
		dma_free_coherent(NULL,
			(mtd->writesize / chip->ecc.size) * PER_INFO_BYTE,
			aml_chip->user_info_buf,
			aml_chip->info_dma_addr);
#else
		kfree(aml_chip->user_info_buf);
#endif
		aml_chip->user_info_buf = NULL;
	}
	kfree(chip->buffers);
	chip->buffers = NULL;

	kfree(chip->ecc.layout);
	chip->ecc.layout = NULL;

	if (aml_chip->aml_nand_data_buf) {
#ifndef AML_NAND_UBOOT

#if (AML_CFG_CONHERENT_BUFFER)
		dma_free_coherent(NULL, (mtd->writesize + mtd->oobsize),
			aml_chip->aml_nand_data_buf,
			aml_chip->data_dma_addr);
#else
		kfree(aml_chip->aml_nand_data_buf);
#endif

#else
		kfree(aml_chip->aml_nand_data_buf);
#endif
		aml_chip->aml_nand_data_buf = NULL;
	}
	kfree(aml_chip->block_status);
	aml_chip->block_status = NULL;
	return err;
}
