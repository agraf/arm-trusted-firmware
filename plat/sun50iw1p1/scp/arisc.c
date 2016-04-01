/*
 *  drivers/arisc/arisc.c
 *
 * Copyright (c) 2012 Allwinner.
 * 2012-10-01 Written by superm (superm@allwinnertech.com).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "arisc_i.h"

/* local functions */
static int arisc_wait_ready(unsigned int timeout);

struct dts_cfg dts_cfg = {
	.dram_para = {
		.dram_clk = 0,
		.dram_type = 0,
		.dram_zq = 0,
		.dram_odt_en = 0,
		.dram_para1 = 0,
		.dram_para2 = 0,
		.dram_mr0 = 0,
		.dram_mr1 = 0,
		.dram_mr2 = 0,
		.dram_mr3 = 0,
		.dram_tpr0 = 0,
		.dram_tpr1 = 0,
		.dram_tpr2 = 0,
		.dram_tpr3 = 0,
		.dram_tpr4 = 0,
		.dram_tpr5 = 0,
		.dram_tpr6 = 0,
		.dram_tpr7 = 0,
		.dram_tpr8 = 0,
		.dram_tpr9 = 0,
		.dram_tpr10 = 0,
		.dram_tpr11 = 0,
		.dram_tpr12 = 0,
		.dram_tpr13 = 0,
	},
	.vf = {
		{
			.freq = 0x44aa2000,
			.voltage = 0x514,
		}, {
			.freq = 0x41cdb400,
			.voltage = 0x4ec,
		}, {
			.freq = 0x3ef14800,
			.voltage = 0x4d8,
		}, {
			.freq = 0x3c14dc00,
			.voltage = 0x4b0,
		}, {
			.freq = 0x39387000,
			.voltage = 0x488,
		}, {
			.freq = 0x365c0400,
			.voltage = 0x460,
		}, {
			.freq = 0x30a32c00,
			.voltage = 0x438,
		}, {
			.freq = 0x269fb200,
			.voltage = 0x410,
		}
	},
	.space = {
		.sram_dst = 0x40000,
		.sram_offset = 0x0,
		.sram_size = 0x14000,
		.dram_dst = 0x40100000,
		.dram_offset = 0x18000,
		.dram_size = 0x4000,
		.para_dst = 0x40104000,
		.para_offset = 0x0,
		.para_size = 0x1000,
		.msgpool_dst = 0x40105000,
		.msgpool_offset = 0x0,
		.msgpool_size = 0x1000,
		.standby_dst = 0x41020000,
		.standby_offset = 0x0,
		.standby_size = 0x800,
	},
	.image = {
		.base = 0x40000,
		.size = 0x19a00,
	},
	.prcm = {
		.base = 0x1f01400,
		.size = 0x400,
	},
	.cpuscfg = {
		.base = 0x1f01c00,
		.size = 0x400,
	},
	.msgbox = {
		.base = 0x1c17000,
		.size = 0x1000,
		.irq = 0,
		.status = 0x1,
	},
	.hwspinlock = {
		.base = 0x1c18000,
		.size = 0x1000,
		.irq = 0,
		.status = 0x1,
	},
	.s_uart = {
		.base = 0x1f02800,
		.size = 0x400,
		.irq = 0,
		.status = 0x1,
	},
	.s_rsb = {
		.base = 0x1f03400,
		.size = 0x400,
		.irq = 0,
		.status = 0x1,
	},
	.s_jtag = {
		.base = 0,
		.size = 0,
		.irq = 0,
		.status = 0,
	},
	.s_cir = {
		.base = 0,
		.size = 0,
		.irq = 0,
		.power_key_code = 0x4d,
		.addr_code = 0x4040,
		.status = 0,
	},
	.pmu = {
		.pmu_bat_shutdown_ltf = 0xc80,
		.pmu_bat_shutdown_htf = 0xed,
		.pmu_pwroff_vol = 0xce4,
		.power_start = 0x0,
	},
	.power = {
		.powchk_used = 0,
		.power_reg = 0x2309621,
		.system_power = 0x32,
	},
};
unsigned int arisc_debug_dram_crc_en = 0;
unsigned int arisc_debug_dram_crc_srcaddr = 0x40000000;
unsigned int arisc_debug_dram_crc_len = (1024 * 1024);
unsigned int arisc_debug_dram_crc_error = 0;
unsigned int arisc_debug_dram_crc_total_count = 0;
unsigned int arisc_debug_dram_crc_error_count = 0;
volatile const unsigned int arisc_debug_level = 2;
static unsigned char arisc_version[40] = "arisc defualt version";

static int arisc_wait_ready(unsigned int timeout)
{
	/* wait arisc startup ready */
	while (1) {
		/*
		 * linux cpu interrupt is disable now,
		 * we should query message by hand.
		 */
		struct arisc_message *pmessage = arisc_hwmsgbox_query_message();
		if (pmessage == NULL) {
			/* try to query again */
			continue;
		}
		/* query valid message */
		if (pmessage->type == ARISC_STARTUP_NOTIFY) {
			/* check arisc software and driver version match or not */
			if (pmessage->paras[0] != ARISC_VERSIONS) {
				ARISC_ERR("arisc firmware:%d and driver version:%u not matched\n", pmessage->paras[0], ARISC_VERSIONS);
				return -EINVAL;
			} else {
				/* printf the main and sub version string */
				memcpy((void *)arisc_version, (const void*)(&(pmessage->paras[1])), 40);
				ARISC_LOG("arisc version: [%s]\n", arisc_version);
			}

			/* received arisc startup ready message */
			ARISC_INF("arisc startup ready\n");
			if ((pmessage->attr & ARISC_MESSAGE_ATTR_SOFTSYN) ||
				(pmessage->attr & ARISC_MESSAGE_ATTR_HARDSYN)) {
				/* synchronous message, just feedback it */
				ARISC_INF("arisc startup notify message feedback\n");
				pmessage->paras[0] = (uint32_t)dts_cfg.image.base;
				arisc_hwmsgbox_feedback_message(pmessage, ARISC_SEND_MSG_TIMEOUT);
			} else {
				/* asyn message, free message directly */
				ARISC_INF("arisc startup notify message free directly\n");
				arisc_message_free(pmessage);
			}
			break;
		}
		/*
		 * invalid message detected, ignore it.
		 * by superm at 2012-7-6 18:34:38.
		 */
		ARISC_WRN("arisc startup waiting ignore message\n");
		if ((pmessage->attr & ARISC_MESSAGE_ATTR_SOFTSYN) ||
			(pmessage->attr & ARISC_MESSAGE_ATTR_HARDSYN)) {
			/* synchronous message, just feedback it */
			arisc_hwmsgbox_send_message(pmessage, ARISC_SEND_MSG_TIMEOUT);
		} else {
			/* asyn message, free message directly */
			arisc_message_free(pmessage);
		}
		/* we need waiting continue */
	}

	return 0;
}

int sunxi_deassert_arisc(void)
{
	ARISC_INF("set arisc reset to de-assert state\n");
	{
		volatile unsigned long value;
		value = readl(dts_cfg.cpuscfg.base + 0x0);
		value &= ~1;
		writel(value, dts_cfg.cpuscfg.base + 0x0);
		value = readl(dts_cfg.cpuscfg.base + 0x0);
		value |= 1;
		writel(value, dts_cfg.cpuscfg.base + 0x0);
	}

	return 0;
}

static int sunxi_arisc_para_init(struct arisc_para *para)
{
	/* init para */
	memset(para, 0, sizeof(struct arisc_para));
	para->message_pool_phys = (uint32_t)dts_cfg.space.msgpool_dst;
	para->message_pool_size = (uint32_t)dts_cfg.space.msgpool_size;
	para->standby_base = (uint32_t)dts_cfg.space.standby_dst;
	para->standby_size = (uint32_t)dts_cfg.space.standby_size;
	memcpy((void *)&para->vf, (void *)dts_cfg.vf, sizeof(para->vf));
	memcpy((void *)&para->dram_para, (void *)&dts_cfg.dram_para, sizeof(para->dram_para));
	para->power_key_code = dts_cfg.s_cir.power_key_code;
	para->addr_code = dts_cfg.s_cir.addr_code;
	para->suart_status = dts_cfg.s_uart.status;
	para->pmu_bat_shutdown_ltf = dts_cfg.pmu.pmu_bat_shutdown_ltf;
	para->pmu_bat_shutdown_htf = dts_cfg.pmu.pmu_bat_shutdown_htf;
	para->pmu_pwroff_vol = dts_cfg.pmu.pmu_pwroff_vol;
	para->power_start = dts_cfg.pmu.power_start;
	para->powchk_used = dts_cfg.power.powchk_used;
	para->power_reg = dts_cfg.power.power_reg;
	para->system_power = dts_cfg.power.system_power;

	ARISC_LOG("arisc_para size:%llx\n", sizeof(struct arisc_para));
	ARISC_INF("msgpool base:%x, size:%u\n", para->message_pool_phys,
		para->message_pool_size);

	return 0;
}

uint32_t sunxi_copy_arisc_para(void *para, size_t para_size)
{
	void *dst;
	void *src;
	size_t size;

	/* para space */
	dst = (void *)dts_cfg.space.para_dst;
	src = para;
	size = dts_cfg.space.para_size;
	memset(dst, 0, size);
	memcpy(dst, src, size);
	ARISC_INF("setup arisc para finish\n");
	//dcsw_op_all(DCCISW);
	flush_dcache_range((uint64_t)dst, (uint64_t)size);
	isb();

	return 0;
}

int sunxi_arisc_probe(void *cfg)
{
	struct arisc_para para;

	ARISC_LOG("sunxi-arisc driver begin startup %d\n", arisc_debug_level);
	//memcpy((void *)&dts_cfg, (const void *)cfg, sizeof(struct dts_cfg));

	/* init arisc parameter */
	sunxi_arisc_para_init(&para);

	/* copy arisc para */
	sunxi_copy_arisc_para((void *)(&para), sizeof(struct arisc_para));

	/* initialize hwspinlock */
	ARISC_INF("hwspinlock initialize\n");
	arisc_hwspinlock_init();

	/* initialize hwmsgbox */
	ARISC_INF("hwmsgbox initialize\n");
	arisc_hwmsgbox_init();

	/* initialize message manager */
	ARISC_INF("message manager initialize start:0x%llx, size:0x%llx\n", dts_cfg.space.msgpool_dst, dts_cfg.space.msgpool_size);
	arisc_message_manager_init((void *)dts_cfg.space.msgpool_dst, dts_cfg.space.msgpool_size);

	/* wait arisc ready */
	ARISC_INF("wait arisc ready....\n");
	if (arisc_wait_ready(10000)) {
		ARISC_LOG("arisc startup failed\n");
	}

	arisc_set_paras();

	/* enable arisc asyn tx interrupt */
	//arisc_hwmsgbox_enable_receiver_int(ARISC_HWMSGBOX_ARISC_ASYN_TX_CH, AW_HWMSG_QUEUE_USER_AC327);

	/* enable arisc syn tx interrupt */
	//arisc_hwmsgbox_enable_receiver_int(ARISC_HWMSGBOX_ARISC_SYN_TX_CH, AW_HWMSG_QUEUE_USER_AC327);

	/* arisc initialize succeeded */
	ARISC_LOG("sunxi-arisc driver v%s is starting\n", DRV_VERSION);

	return 0;
}

int sunxi_arisc_wait_ready(void)
{
	ARISC_INF("wait arisc ready....\n");
	if (arisc_wait_ready(10000)) {
		ARISC_LOG("arisc startup failed\n");
	}
	arisc_set_paras();
	ARISC_LOG("sunxi-arisc driver v%s startup ok\n", DRV_VERSION);
	return 0;
}

