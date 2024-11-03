//
// Created by zwz on 2024/11/3.
//
#include <sys/mman.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>

#include "rk_pcie_dma.h"

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long   u64;
typedef u64 phys_addr_t;
#define BIT(n)  (1 << n)

#define debug

#define PCI_CFG_SPACE_SIZE  256
#define PCI_CFG_SPACE_EXP_SIZE  4096

#define PCIE_ELBI_CFG_BASE       0xe00

#define PCI_EXT_CAP_ID(header)      (header & 0x0000ffff)
#define PCI_EXT_CAP_VER(header)     ((header >> 16) & 0xf)
#define PCI_EXT_CAP_NEXT(header)    ((header >> 20) & 0xffc)

#define PCI_EXT_CAP_ID_REBAR    0x15    /* Resizable BAR */

#define PCIE_ELBI_LOCAL_BASE    0x200e00
#define PCIE_ELBI_REG_NUM       0x2

#define PCIE_ATU_OFFSET         0x300000
#define PCIE_LOCAL_ATU_OFFSET   PCIE_ATU_OFFSET

#define PCIE_DMA_OFFSET         0x380000
#define PCIE_LOCAL_DMA_OFFSET   PCIE_DMA_OFFSET

#define PCIE_DMA_CTRL_OFF       0x8
#define PCIE_DMA_WR_ENB         0xc
#define PCIE_DMA_WR_CTRL_LO     0x200
#define PCIE_DMA_WR_CTRL_HI     0x204
#define PCIE_DMA_WR_XFERSIZE        0x208
#define PCIE_DMA_WR_SAR_PTR_LO      0x20c
#define PCIE_DMA_WR_SAR_PTR_HI      0x210
#define PCIE_DMA_WR_DAR_PTR_LO      0x214
#define PCIE_DMA_WR_DAR_PTR_HI      0x218
#define PCIE_DMA_WR_WEILO       0x18
#define PCIE_DMA_WR_WEIHI       0x1c
#define PCIE_DMA_WR_DOORBELL        0x10
#define PCIE_DMA_WR_INT_STATUS      0x4c
#define PCIE_DMA_WR_INT_MASK        0x54
#define PCIE_DMA_WR_INT_CLEAR       0x58
#define PCIE_DMA_WR_ERR_STATUS      0x5c

#define PCIE_DMA_RD_ENB         0x2c
#define PCIE_DMA_RD_CTRL_LO     0x300
#define PCIE_DMA_RD_CTRL_HI     0x304
#define PCIE_DMA_RD_XFERSIZE        0x308
#define PCIE_DMA_RD_SAR_PTR_LO      0x30c
#define PCIE_DMA_RD_SAR_PTR_HI      0x310
#define PCIE_DMA_RD_DAR_PTR_LO      0x314
#define PCIE_DMA_RD_DAR_PTR_HI      0x318
#define PCIE_DMA_RD_WEILO       0x38
#define PCIE_DMA_RD_WEIHI       0x3c
#define PCIE_DMA_RD_DOORBELL        0x30
#define PCIE_DMA_RD_INT_STATUS      0xa0
#define PCIE_DMA_RD_INT_MASK        0xa8
#define PCIE_DMA_RD_INT_CLEAR       0xac
#define PCIE_DMA_RD_ERR_STATUS_LOW    0xb8
#define PCIE_DMA_RD_ERR_STATUS_HIGH   0xbc

#define PCIE_DMA_CHANEL_MAX_NUM     2

#define RK_RMD_USER_MEM_SIZE        0x4000000

/* Only support EP wired atu register */
#define PCIE_WIRED_DMA_OFFSET    0x0
#define PCIE_WIRED_ATU_OFFSET    0x2000

#define PCIE_EP_OBJ_BAR2_INBOUND_INDEX 1
#define PCIE_EP_OBJ_ELBI_USERDATA_NUM  8

/*
 * SIGPOLL (or any other signal without signal specific si_codes) si_codes
 */
#define NSIGPOLL        6

struct dw_pcie
{
    unsigned char* bar4_mapped_base;
    unsigned char* dbi_base; /* local dbi */
    unsigned char* dbi_uniform_base; /* rc-bar4_mapped_base, ep-dbi_base */
    unsigned char* dma_base;
    unsigned char* atu_base;
    bool support_rc_dma;
};

struct rockchip_pcie
{
    struct dw_pcie* pci;
    int fd;
};

enum dma_dir
{
    DMA_FROM_BUS,
    DMA_TO_BUS,
};

/**
 * The Channel Control Register for read and write.
 */
union chan_ctrl_lo
{
    struct
    {
        u32 cb      : 1; // 0
        u32 tcb     : 1; // 1
        u32 llp     : 1; // 2
        u32 lie     : 1; // 3
        u32 rie     : 1; // 4
        u32 cs      : 2; // 5:6
        u32 rsvd1       : 1; // 7
        u32 ccs     : 1; // 8
        u32 llen        : 1; // 9
        u32 b_64s       : 1; // 10
        u32 b_64d       : 1; // 11
        u32 pf      : 5; // 12:16
        u32 rsvd2       : 7; // 17:23
        u32 sn      : 1; // 24
        u32 ro      : 1; // 25
        u32 td      : 1; // 26
        u32 tc      : 3; // 27:29
        u32 at      : 2; // 30:31
    };
    u32 asdword;
};

/**
 * The Channel Control Register high part for read and write.
 */
union chan_ctrl_hi
{
    struct
    {
        u32 vfenb       : 1; // 0
        u32 vfunc       : 8; // 1-8
        u32 rsvd0       : 23;   // 9-31
    };
    u32 asdword;
};

/**
 * The Channel Weight Register.
 */
union weight
{
    struct
    {
        u32 weight0     : 5; // 0:4
        u32 weight1     : 5; // 5:9
        u32 weight2     : 5; // 10:14
        u32 weight3     : 5; // 15:19
        u32 rsvd        : 12;   // 20:31
    };
    u32 asdword;
};

/**
 * The Doorbell Register for read and write.
 */
union db
{
    struct
    {
        u32 chnl        : 3; // 0
        u32 reserved0   : 28;   // 3:30
        u32 stop        : 1; // 31
    };
    u32 asdword;
};

/**
 * The Context Registers for read and write.
 */
struct ctx_regs
{
    union chan_ctrl_lo      ctrllo;
    union chan_ctrl_hi      ctrlhi;
    u32             xfersize;
    u32             sarptrlo;
    u32             sarptrhi;
    u32             darptrlo;
    u32             darptrhi;
};

/**
 * The Enable Register for read and write.
 */
union enb
{
    struct
    {
        u32 enb     : 1; // 0
        u32 reserved0   : 31;   // 1:31
    };
    u32 asdword;
};

/**
 * The Interrupt Status Register for read and write.
 */
union int_status
{
    struct
    {
        u32 donesta     : 8;
        u32 rsvd0       : 8;
        u32 abortsta    : 8;
        u32 rsvd1       : 8;
    };
    u32 asdword;
};

/**
 * The Interrupt Clear Register for read and write.
 */
union int_clear
{
    struct
    {
        u32 doneclr     : 8;
        u32 rsvd0       : 8;
        u32 abortclr    : 8;
        u32 rsvd1       : 8;
    };
    u32 asdword;
};

struct dma_table
{
    int             chn;
    u32             dir;
    u32             type;
    union enb           enb;
    struct ctx_regs         ctx_reg;
    union weight            weilo;
    union weight            weihi;
    union db            start;
    phys_addr_t         local;
    phys_addr_t         bus;
    size_t              buf_size;
};

struct dma_trx_user_obj
{
    struct rockchip_pcie* dev;
    void (*start_dma_func)(struct dma_trx_user_obj *obj, struct dma_table *table);
    void (*config_dma_func)(struct dma_table *table);
    int (*get_dma_status)(struct dma_trx_user_obj *obj, u8 chn, enum dma_dir dir);
    int (*cb)(struct dma_trx_user_obj *obj, u32 chn, enum dma_dir dir);
    bool irq;
    void* user_mem;

    struct pcie_ep_obj_info* obj_info;  /* Bar0 resource */
    void* user_buffer;          /* Bar2 resource */
    phys_addr_t user_buffer_cpu_addr;   /* Bar2 cpu address */
    phys_addr_t user_buffer_bus_addr;   /* Bar2 bus address */
    u32 bar_size[PCIE_BAR_MAX_NUM];

    sem_t rd_done[PCIE_DMA_CHANEL_MAX_NUM];
    sem_t wr_done[PCIE_DMA_CHANEL_MAX_NUM];

    pthread_mutex_t mutex_dma_wr;
    pthread_mutex_t mutex_dma_rd;
    pthread_mutex_t mutex_elbi;
};

static inline void writel(u32 val, void* addr)
{
    *(volatile u32*)addr = val;
}

static inline u32 readl(void* addr)
{
    return *(volatile u32*)addr;
}

static inline void writel_reg(void* addr, u32 reg, u32 val)
{
    *(volatile u32*)(addr + reg) = val;
}

static inline u32 readl_reg(void* addr, u32 reg)
{
    return *(volatile u32*)(addr + reg);
}

static inline void __attribute__((unused)) dw_pcie_writel_dbi(struct dw_pcie *pci, u32 reg, u32 val)
{
    writel(val, pci->dbi_base + reg);
}

static inline u32 __attribute__((unused)) dw_pcie_readl_dbi(struct dw_pcie *pci, u32 reg)
{
    return readl(pci->dbi_base + reg);
}

static inline void __attribute__((unused)) dw_pcie_writel_uniform_dbi(struct dw_pcie *pci, u32 reg, u32 val)
{
    writel(val, pci->dbi_uniform_base + reg);
}

static inline u32 __attribute__((unused)) dw_pcie_readl_uniform_dbi(struct dw_pcie *pci, u32 reg)
{
    return readl(pci->dbi_uniform_base + reg);
}

static inline void __attribute__((unused)) rk_pci_write_config_dword(struct rockchip_pcie *rockchip, u32 reg, u32 val)
{
    lseek(rockchip->fd, reg, SEEK_SET);
    write(rockchip->fd, &val, sizeof(u32));
    fsync(rockchip->fd);
}

static inline u32 __attribute__((unused)) rk_pci_read_config_dword(struct rockchip_pcie *rockchip, u32 reg)
{
    u32 val;

    lseek(rockchip->fd, reg, SEEK_SET);
    read(rockchip->fd, &val, sizeof(u32));

    return val;
}

static inline void rk_pci_write_type0head_dword(struct rockchip_pcie *rockchip, u32 reg, u32 val)
{
#ifdef PCIE_RC
    lseek(rockchip->fd, reg, SEEK_SET);
    write(rockchip->fd, &val, sizeof(u32));
    fsync(rockchip->fd);
#else
    dw_pcie_writel_dbi(rockchip->pci, reg, val);
#endif
}

static inline u32 rk_pci_read_type0head_dword(struct rockchip_pcie *rockchip, u32 reg)
{
    u32 val;

#ifdef PCIE_RC
    lseek(rockchip->fd, reg, SEEK_SET);
    read(rockchip->fd, &val, sizeof(u32));
#else
    val = dw_pcie_readl_dbi(rockchip->pci, reg);
#endif

    return val;
}

static u64 rk_pci_read_inbound_atu_cpu_addr(struct rockchip_pcie *rockchip, u32 index)
{
    u64 val;
    u32 off;

    off = 0x200 * index + 0x100;
    val = readl_reg(rockchip->pci->atu_base, off + 0x14);
    val |= (u64)readl_reg(rockchip->pci->atu_base, off + 0x18) << 32;

    return val;
}

static u64 __attribute__((unused)) rk_pci_prog_inbound_atu_cpu_addr(struct rockchip_pcie *rockchip, u32 index, u64 val, u32 bar_size_limit)
{
    u32 off;
    u32 addr_low = val & ~(bar_size_limit - 1);
    u32 addr_hi = (val >> 32) & 0xffffffff;
    u64 addr = ((u64)addr_hi << 32) | (u64)addr_low;

    off = 0x200 * index + 0x100;
    writel_reg(rockchip->pci->atu_base, off + 0x14, addr_low);
    writel_reg(rockchip->pci->atu_base, off + 0x18, addr_hi);

    return addr;
}

static int rk_pci_find_resbar_capability(struct rockchip_pcie *rockchip)
{
    u32 header;
    int ttl;
    int start = 0;
    int pos = PCI_CFG_SPACE_SIZE;
    int cap = PCI_EXT_CAP_ID_REBAR;

    /* minimum 8 bytes per capability */
    ttl = (PCI_CFG_SPACE_EXP_SIZE - PCI_CFG_SPACE_SIZE) / 8;

    header = rk_pci_read_type0head_dword(rockchip, pos);

    /*
     * If we have no capabilities, this is indicated by cap ID,
     * cap version and next pointer all being 0.
     */
    if (header == 0)
    {
        return 0;
    }

    while (ttl-- > 0)
    {
        if (PCI_EXT_CAP_ID(header) == cap && pos != start)
        {
            return pos;
        }

        pos = PCI_EXT_CAP_NEXT(header);
        if (pos < PCI_CFG_SPACE_SIZE)
        {
            break;
        }

        header = rk_pci_read_type0head_dword(rockchip, pos);
        if (!header)
        {
            break;
        }
    }

    return 0;
}

static bool rk_pcie_check_active(struct dma_trx_user_obj *obj)
{
    int ret = true;

    ret = obj->obj_info->magic == RKEP_BOOT_MAGIC;
    if (!ret)
    {
        LOG_INFO("check fail, %x", obj->obj_info->magic);
        ret = RK_PCIE_ERR_BAD;
    }
    else
    {
        ret = RK_PCIE_OK;
    }
    return ret;
}

int rk_pcie_rc_check_support_local_dma(struct ep_dev_st *ep_dev)
{
    return ep_dev->obj->dev->pci->support_rc_dma;
}

int rk_pci_get_bar_size(struct ep_dev_st *ep_dev, unsigned char bar)
{
    int resbar_base;
    u32 size_shift, reg;
    struct rockchip_pcie rockchip = {0};

#ifdef PCIE_RC
    rockchip.fd = ep_dev->pcie_ep_fd;
#else
    struct dw_pcie pci = {0};

    rockchip.pci = &pci;
    rockchip.pci->dbi_base = ep_dev->bar4_vir_addr;
#endif
    resbar_base = rk_pci_find_resbar_capability(&rockchip);

    reg = rk_pci_read_type0head_dword(&rockchip, resbar_base + 0x4 + bar * 0x8);
    if ((bar == 0 && reg != 0x40) || (bar == 2 && reg != 0x400) || (bar == 4 && reg != 0x10))
    {
        RK_PCIE_LOGE("Bar cap, bar[%d]=%x", bar, reg);
    }

    size_shift = rk_pci_read_type0head_dword(&rockchip, resbar_base + 0x8 + bar * 0x8) >> 8;

    return (1 << (size_shift + 20));
}

static void rk_pcie_elbi_write(struct ep_dev_st *ep_dev, int index, int value)
{
    int i = 0;
    struct rockchip_pcie *rockchip = ep_dev->obj->dev;
    u32 elbi_off;

#ifdef PCIE_RC
    elbi_off = PCIE_ELBI_CFG_BASE;
#else
    elbi_off = PCIE_ELBI_LOCAL_BASE;
#endif

    if (index <= 2)
    {
        i = index + 4;
    }
    else
    {
        i = index + 8;
    }

    rk_pci_write_type0head_dword(rockchip, elbi_off + i * 4, value);
}

static void rk_pcie_elbi_read(struct ep_dev_st *ep_dev, int index, int* value)
{
    int i = 0;
    struct rockchip_pcie *rockchip = ep_dev->obj->dev;
    u32 elbi_off;

#ifdef PCIE_RC
    elbi_off = PCIE_ELBI_CFG_BASE;
#else
    elbi_off = PCIE_ELBI_LOCAL_BASE;
#endif

    if (index <= 2)
    {
        i = index + 4;
    }
    else
    {
        i = index + 8;
    }

    *value = rk_pci_read_type0head_dword(rockchip, elbi_off + i * 4);
}

static void rockchip_pcie_start_dma_rd(struct dma_trx_user_obj *obj, struct dma_table *cur, int channel)
{
    struct rockchip_pcie *rockchip = obj->dev;
    struct dw_pcie *pci = rockchip->pci;
    unsigned int ctr_off = channel * 0x200;

    writel_reg(pci->dma_base, PCIE_DMA_RD_ENB, cur->enb.asdword);
    writel_reg(pci->dma_base, ctr_off + PCIE_DMA_RD_CTRL_LO, cur->ctx_reg.ctrllo.asdword);
    writel_reg(pci->dma_base, ctr_off + PCIE_DMA_RD_CTRL_HI, cur->ctx_reg.ctrlhi.asdword);
    writel_reg(pci->dma_base, ctr_off + PCIE_DMA_RD_XFERSIZE, cur->ctx_reg.xfersize);
    writel_reg(pci->dma_base, ctr_off + PCIE_DMA_RD_SAR_PTR_LO, cur->ctx_reg.sarptrlo);
    writel_reg(pci->dma_base, ctr_off + PCIE_DMA_RD_SAR_PTR_HI, cur->ctx_reg.sarptrhi);
    writel_reg(pci->dma_base, ctr_off + PCIE_DMA_RD_DAR_PTR_LO, cur->ctx_reg.darptrlo);
    writel_reg(pci->dma_base, ctr_off + PCIE_DMA_RD_DAR_PTR_HI, cur->ctx_reg.darptrhi);
    writel_reg(pci->dma_base, PCIE_DMA_RD_DOORBELL, cur->start.asdword);
}

static void rockchip_pcie_start_dma_wr(struct dma_trx_user_obj *obj, struct dma_table *cur, int channel)
{
    struct rockchip_pcie *rockchip = obj->dev;
    struct dw_pcie *pci = rockchip->pci;
    unsigned int ctr_off = channel * 0x200;

    writel_reg(pci->dma_base, PCIE_DMA_WR_ENB, cur->enb.asdword);
    writel_reg(pci->dma_base, ctr_off + PCIE_DMA_WR_CTRL_LO, cur->ctx_reg.ctrllo.asdword);
    writel_reg(pci->dma_base, ctr_off + PCIE_DMA_WR_CTRL_HI, cur->ctx_reg.ctrlhi.asdword);
    writel_reg(pci->dma_base, ctr_off + PCIE_DMA_WR_XFERSIZE, cur->ctx_reg.xfersize);
    writel_reg(pci->dma_base, ctr_off + PCIE_DMA_WR_SAR_PTR_LO, cur->ctx_reg.sarptrlo);
    writel_reg(pci->dma_base, ctr_off + PCIE_DMA_WR_SAR_PTR_HI, cur->ctx_reg.sarptrhi);
    writel_reg(pci->dma_base, ctr_off + PCIE_DMA_WR_DAR_PTR_LO, cur->ctx_reg.darptrlo);
    writel_reg(pci->dma_base, ctr_off + PCIE_DMA_WR_DAR_PTR_HI, cur->ctx_reg.darptrhi);
    writel_reg(pci->dma_base, ctr_off + PCIE_DMA_WR_WEILO, cur->weilo.asdword);
    writel_reg(pci->dma_base, PCIE_DMA_WR_DOORBELL, cur->start.asdword);
}

static void rockchip_pcie_start_dma_dwc(struct dma_trx_user_obj *obj, struct dma_table *table)
{
    int dir = table->dir;
    int chn = table->chn;

    if (dir == DMA_FROM_BUS)
    {
        rockchip_pcie_start_dma_rd(obj, table, chn);
    }
    else if (dir == DMA_TO_BUS)
    {
        rockchip_pcie_start_dma_wr(obj, table, chn);
    }
}

static void rockchip_pcie_config_dma_dwc(struct dma_table *table)
{
    table->enb.enb = 0x1;
    table->ctx_reg.ctrllo.lie = 0x1;
    table->ctx_reg.ctrllo.rie = 0x1;
    table->ctx_reg.ctrllo.td = 0x1;
    table->ctx_reg.ctrlhi.asdword = 0x0;
    table->ctx_reg.xfersize = table->buf_size;
    if (table->dir == DMA_FROM_BUS)
    {
        table->ctx_reg.sarptrlo = (u32)(table->bus & 0xffffffff);
        table->ctx_reg.sarptrhi = (u32)(table->bus >> 32);
        table->ctx_reg.darptrlo = (u32)(table->local & 0xffffffff);
        table->ctx_reg.darptrhi = (u32)(table->local >> 32);
    }
    else if (table->dir == DMA_TO_BUS)
    {
        table->ctx_reg.sarptrlo = (u32)(table->local & 0xffffffff);
        table->ctx_reg.sarptrhi = (u32)(table->local >> 32);
        table->ctx_reg.darptrlo = (u32)(table->bus & 0xffffffff);
        table->ctx_reg.darptrhi = (u32)(table->bus >> 32);
    }
    table->weilo.weight0 = 0x0;
    table->start.stop = 0x0;
    table->start.chnl = table->chn;
}

static void rockchip_pcie_dma_table_debug(struct dma_trx_user_obj *obj, struct dma_table *table)
{
    struct rockchip_pcie *rockchip = obj->dev;
    struct dw_pcie *pci = rockchip->pci;
    unsigned int ctr_off = table->chn * 0x200;

#ifdef PCIE_RC
    RK_PCIE_LOGE("using rc dma");
#else
    RK_PCIE_LOGE("using ep dma");
#endif
    RK_PCIE_LOGE("chnl=%x", table->start.chnl);
    RK_PCIE_LOGE("%s", table->dir == DMA_FROM_BUS ? "dma read" : "dma write");
    RK_PCIE_LOGE("src=0x%x %x", table->ctx_reg.sarptrhi, table->ctx_reg.sarptrlo);
    RK_PCIE_LOGE("dst=0x%x %x", table->ctx_reg.darptrhi, table->ctx_reg.darptrlo);
    RK_PCIE_LOGE("xfersize=%x", table->ctx_reg.xfersize);

    if (table->dir == DMA_FROM_BUS)
    {
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_RD_INT_MASK = %x", PCIE_DMA_RD_INT_MASK, readl_reg(pci->dma_base, PCIE_DMA_RD_INT_MASK));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_RD_ENB = %x", PCIE_DMA_RD_ENB, readl_reg(pci->dma_base, PCIE_DMA_RD_ENB));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_RD_CTRL_LO = %x", ctr_off + PCIE_DMA_RD_CTRL_LO, readl_reg(pci->dma_base, ctr_off + PCIE_DMA_RD_CTRL_LO));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_RD_CTRL_HI = %x", ctr_off + PCIE_DMA_RD_CTRL_HI,  readl_reg(pci->dma_base, ctr_off + PCIE_DMA_RD_CTRL_HI));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_RD_XFERSIZE = %x", ctr_off + PCIE_DMA_RD_XFERSIZE,  readl_reg(pci->dma_base, ctr_off + PCIE_DMA_RD_XFERSIZE));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_RD_SAR_PTR_LO = %x", ctr_off + PCIE_DMA_RD_SAR_PTR_LO,  readl_reg(pci->dma_base, ctr_off + PCIE_DMA_RD_SAR_PTR_LO));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_RD_SAR_PTR_HI = %x", ctr_off + PCIE_DMA_RD_SAR_PTR_HI,  readl_reg(pci->dma_base, ctr_off + PCIE_DMA_RD_SAR_PTR_HI));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_RD_DAR_PTR_LO = %x", ctr_off + PCIE_DMA_RD_DAR_PTR_LO,  readl_reg(pci->dma_base, ctr_off + PCIE_DMA_RD_DAR_PTR_LO));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_RD_DAR_PTR_HI = %x", ctr_off + PCIE_DMA_RD_DAR_PTR_HI,  readl_reg(pci->dma_base, ctr_off + PCIE_DMA_RD_DAR_PTR_HI));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_RD_DOORBELL = %x", PCIE_DMA_RD_DOORBELL, readl_reg(pci->dma_base, PCIE_DMA_RD_DOORBELL));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_RD_INT_STATUS = %x", PCIE_DMA_RD_INT_STATUS, readl_reg(pci->dma_base, PCIE_DMA_RD_INT_STATUS));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_RD_ERR_STATUS_LOW = %x", PCIE_DMA_RD_ERR_STATUS_LOW, readl_reg(pci->dma_base, PCIE_DMA_RD_ERR_STATUS_LOW));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_RD_ERR_STATUS_HIGH = %x", PCIE_DMA_RD_ERR_STATUS_HIGH, readl_reg(pci->dma_base, PCIE_DMA_RD_ERR_STATUS_HIGH));
    }
    else
    {
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_WR_INT_MASK = %x", PCIE_DMA_WR_INT_MASK, readl_reg(pci->dma_base, PCIE_DMA_WR_INT_MASK));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_WR_ENB = %x", PCIE_DMA_WR_ENB, readl_reg(pci->dma_base, PCIE_DMA_WR_ENB));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_WR_CTRL_LO = %x", ctr_off + PCIE_DMA_WR_CTRL_LO, readl_reg(pci->dma_base, ctr_off + PCIE_DMA_WR_CTRL_LO));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_WR_CTRL_HI = %x", ctr_off + PCIE_DMA_WR_CTRL_HI,  readl_reg(pci->dma_base, ctr_off + PCIE_DMA_WR_CTRL_HI));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_WR_XFERSIZE = %x", ctr_off + PCIE_DMA_WR_XFERSIZE,  readl_reg(pci->dma_base, ctr_off + PCIE_DMA_WR_XFERSIZE));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_WR_SAR_PTR_LO = %x", ctr_off + PCIE_DMA_WR_SAR_PTR_LO,  readl_reg(pci->dma_base, ctr_off + PCIE_DMA_WR_SAR_PTR_LO));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_WR_SAR_PTR_HI = %x", ctr_off + PCIE_DMA_WR_SAR_PTR_HI,  readl_reg(pci->dma_base, ctr_off + PCIE_DMA_WR_SAR_PTR_HI));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_WR_DAR_PTR_LO = %x", ctr_off + PCIE_DMA_WR_DAR_PTR_LO,  readl_reg(pci->dma_base, ctr_off + PCIE_DMA_WR_DAR_PTR_LO));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_WR_DAR_PTR_HI = %x", ctr_off + PCIE_DMA_WR_DAR_PTR_HI,  readl_reg(pci->dma_base, ctr_off + PCIE_DMA_WR_DAR_PTR_HI));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_WR_DOORBELL = %x", PCIE_DMA_WR_DOORBELL, readl_reg(pci->dma_base, PCIE_DMA_WR_DOORBELL));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_WR_INT_STATUS = %x", PCIE_DMA_WR_INT_STATUS, readl_reg(pci->dma_base, PCIE_DMA_WR_INT_STATUS));
        RK_PCIE_LOGE("reg[0x%x] PCIE_DMA_WR_ERR_STATUS = %x", PCIE_DMA_WR_ERR_STATUS, readl_reg(pci->dma_base, PCIE_DMA_WR_ERR_STATUS));
    }
}

static int rockchip_pcie_get_dma_status(struct dma_trx_user_obj *obj, u8 chn, enum dma_dir dir)
{
    struct rockchip_pcie *rockchip = obj->dev;
    struct dw_pcie *pci = rockchip->pci;
    union int_status status;
    union int_clear clears;
    int ret = 0;

    //RK_PCIE_LOGV("%s %x %x\n", __func__, readl_reg(pci->dma_base, dma_offset + PCIE_DMA_WR_INT_STATUS),
    //      readl_reg(pci->dma_base, dma_offset + PCIE_DMA_RD_INT_STATUS));

    if (dir == DMA_TO_BUS)
    {
        status.asdword = readl_reg(pci->dma_base, PCIE_DMA_WR_INT_STATUS);
        if (status.donesta & BIT(chn))
        {
            clears.doneclr = 0x1 << chn;
            writel_reg(pci->dma_base, PCIE_DMA_WR_INT_CLEAR, clears.asdword);
            ret = 1;
        }

        if (status.abortsta & BIT(chn))
        {
            RK_PCIE_LOGE("write abort");
            clears.abortclr = 0x1 << chn;
            writel_reg(pci->dma_base, PCIE_DMA_WR_INT_CLEAR, clears.asdword);
            ret = -1;
        }
    }
    else
    {
        status.asdword = readl_reg(pci->dma_base, PCIE_DMA_RD_INT_STATUS);

        if (status.donesta & BIT(chn))
        {
            clears.doneclr = 0x1 << chn;
            writel_reg(pci->dma_base, PCIE_DMA_RD_INT_CLEAR, clears.asdword);
            ret = 1;
        }

        if (status.abortsta & BIT(chn))
        {
            RK_PCIE_LOGE("read abort %x", status.asdword);
            clears.abortclr = 0x1 << chn;
            writel_reg(pci->dma_base, PCIE_DMA_RD_INT_CLEAR, clears.asdword);
            ret = -1;
        }
    }

    return ret;
}

static int rockchip_pcie_dma_interrupt_handler_call_back(struct dma_trx_user_obj *obj, u32 chn, enum dma_dir dir)
{
    if (chn >= PCIE_DMA_CHANEL_MAX_NUM)
    {
        return RK_PCIE_ERR_VALUE;
    }

    if (dir == DMA_FROM_BUS)
    {
        sem_post(&obj->rd_done[chn]);
    }
    else
    {
        sem_post(&obj->wr_done[chn]);
    }

    return RK_PCIE_OK;
}

static void __attribute__((unused)) rk_pcie_dma_sig_handler(struct ep_dev_st *ep_dev)
{
    union int_status wr_status, rd_status;
    u32 chn;
    union int_clear clears;
    u32 *reg_wr, * reg_rd;

#ifdef PCIE_RC
    reg_wr = &(ep_dev->obj->obj_info->dma_status_rc.wr);
    reg_rd = &(ep_dev->obj->obj_info->dma_status_rc.rd);
#else
    reg_wr = &(ep_dev->obj->obj_info->dma_status_ep.wr);
    reg_rd = &(ep_dev->obj->obj_info->dma_status_ep.rd);
#endif

    /* DMA helper */
    wr_status.asdword = *reg_wr;
    rd_status.asdword = *reg_rd;

    for (chn = 0; chn < PCIE_DMA_CHANEL_MAX_NUM; chn++)
    {
        if (wr_status.donesta & BIT(chn))
        {
            if (ep_dev->obj->cb)
            {
                ep_dev->obj->cb(ep_dev->obj, chn, DMA_TO_BUS);
            }
            clears.doneclr = 0x1 << chn;
            *reg_wr = (wr_status.asdword & (~clears.doneclr));
        }

        if (wr_status.abortsta & BIT(chn))
        {
            RK_PCIE_LOGE("wr abort");
            clears.abortclr = 0x1 << chn;
            *reg_wr = (wr_status.asdword & (~clears.abortclr));
        }
    }

    for (chn = 0; chn < PCIE_DMA_CHANEL_MAX_NUM; chn++)
    {
        if (rd_status.donesta & BIT(chn))
        {
            if (ep_dev->obj->cb)
            {
                ep_dev->obj->cb(ep_dev->obj, chn, DMA_FROM_BUS);
            }
            clears.doneclr = 0x1 << chn;
            *reg_rd = (rd_status.asdword & (~clears.doneclr));
        }

        if (rd_status.abortsta & BIT(chn))
        {
            RK_PCIE_LOGE("rd abort");
            clears.abortclr = 0x1 << chn;
            *reg_rd = (rd_status.asdword & (~clears.abortclr));
        }
    }
}

static int rk_pcie_dma_wait_for_finised(struct dma_trx_user_obj *obj, struct dw_pcie *pci, struct dma_table *table)
{
    int ret = RK_PCIE_OK, timeout_us, i;

    if (!pci)
    {
        RK_PCIE_LOGE("pci is null");
        return RK_PCIE_ERR_NULL_PTR;
    }

    timeout_us = table->buf_size / 100 + 100; /* 100MB/s for redundant calculate */

    for (i = 0; i < timeout_us; i++)
    {
        ret = obj->get_dma_status(obj, table->chn, table->dir);
        if (ret == 1)
        {
            ret = RK_PCIE_OK;
            break;
        }
        else if (ret < 0)
        {
            ret = RK_PCIE_ERR_UNKNOWN;
            break;
        }
        usleep(1);
    }

    if (i >= timeout_us || ret)
    {
        RK_PCIE_LOGE("dma timeout");
        rockchip_pcie_dma_table_debug(obj, table);
        return RK_PCIE_ERR_TIMEOUT;
    }

    return ret;
}

static int __rk_pcie_dma_frombus(struct dma_trx_user_obj *obj, u32 chn, u64 local_paddr, u64 bus_paddr, u64 size)
{
    struct dma_table table = {0};
    struct dw_pcie *pci = obj->dev->pci;
    int ret = 0;

    if (chn >= PCIE_DMA_CHANEL_MAX_NUM)
    {
        return RK_PCIE_ERR_VALUE;
    }

    if (obj->irq)
    {
        sem_init(&obj->rd_done[chn], 0, 0);
    }

    table.buf_size = size;
    table.bus = bus_paddr;
    table.local = local_paddr;
    table.chn = chn;
    table.dir = DMA_FROM_BUS;

    obj->config_dma_func(&table);
    obj->start_dma_func(obj, &table);

    if (obj->irq)
    {
        sem_wait(&obj->rd_done[chn]);
    }
    else
    {
        ret = rk_pcie_dma_wait_for_finised(obj, pci, &table);
    }

    return ret;
}

static int __rk_pcie_dma_tobus(struct dma_trx_user_obj *obj, u32 chn, u64 bus_paddr, u64 local_paddr, u64 size)
{
    struct dma_table table = {0};
    struct dw_pcie *pci = obj->dev->pci;
    int ret = 0;

    if (chn >= PCIE_DMA_CHANEL_MAX_NUM)
    {
        return RK_PCIE_ERR_VALUE;
    }

    if (obj->irq)
    {
        sem_init(&obj->wr_done[chn], 0, 0);
    }

    table.buf_size = size;
    table.bus = bus_paddr;
    table.local = local_paddr;
    table.chn = chn;
    table.dir = DMA_TO_BUS;

    obj->config_dma_func(&table);
    obj->start_dma_func(obj, &table);

    if (obj->irq)
    {
        sem_wait(&obj->wr_done[chn]);
    }
    else
    {
        ret = rk_pcie_dma_wait_for_finised(obj, pci, &table);
    }

    return ret;
}

static int __rk_pcie_wired_dma_frombus(struct dma_trx_user_obj *obj, u32 chn, u64 local_paddr, u64 bus_paddr, u64 size)
{
    return __rk_pcie_dma_tobus(obj, chn, local_paddr, bus_paddr, size);
}

static int __rk_pcie_wired_dma_tobus(struct dma_trx_user_obj *obj, u32 chn, u64 bus_paddr, u64 local_paddr, u64 size)
{
    return __rk_pcie_dma_frombus(obj, chn, bus_paddr, local_paddr, size);
}

static int __rk_pcie_dma_frombus_by_kernel_api(struct dma_trx_user_obj *obj, int fd, u32 chn, u64 local_paddr, u64 bus_paddr, u32 size)
{
    struct pcie_ep_dma_block_req dma;
    int ret;

    if (chn >= PCIE_DMA_CHANEL_MAX_NUM)
    {
        return RK_PCIE_ERR_VALUE;
    }

    memset(&dma, 0, sizeof(dma));

    dma.vir_id = 0;
    dma.chn = chn;
    dma.wr = 0;
    dma.block.bus_paddr = bus_paddr;
    dma.block.local_paddr = local_paddr;
    dma.block.size = size;

    ret = ioctl(fd, PCIE_EP_DMA_XFER_BLOCK, &dma);

    return ret;
}

static int __rk_pcie_dma_tobus_by_kernel_api(struct dma_trx_user_obj *obj, int fd, u32 chn, u64 bus_paddr, u64 local_paddr, u32 size)
{
    struct pcie_ep_dma_block_req dma;
    int ret;

    if (chn >= PCIE_DMA_CHANEL_MAX_NUM)
    {
        return RK_PCIE_ERR_VALUE;
    }

    memset(&dma, 0, sizeof(dma));

    dma.vir_id = 0;
    dma.chn = chn;
    dma.wr = 1;
    dma.block.bus_paddr = bus_paddr;
    dma.block.local_paddr = local_paddr;
    dma.block.size = size;

    ret = ioctl(fd, PCIE_EP_DMA_XFER_BLOCK, &dma);

    return ret;
}

static void rk_pcie_dma_set_dma_base(struct dma_trx_user_obj *obj, enum pcie_dma_mode mode)
{
    if (mode == LOCAL_DMA_MODE)
    {
        obj->dev->pci->dma_base = obj->dev->pci->dbi_base + PCIE_LOCAL_DMA_OFFSET;
    }
    else
    {
        obj->dev->pci->dma_base = obj->dev->pci->bar4_mapped_base + PCIE_WIRED_DMA_OFFSET;
    }
}

#ifdef PCIE_RC
int rk_pcie_dma_init(struct ep_dev_st *ep_dev)
{
    u32 val, ret = 0;
    enum pcie_ep_mmap_resource mmap_res;
    RKChipInfo* chipinfo;

    if (ep_dev == NULL)
    {
        return RK_PCIE_ERR_NULL_PTR;
    }

    ep_dev->obj = malloc(sizeof(struct dma_trx_user_obj));
    if (!ep_dev->obj)
    {
        RK_PCIE_LOGE("dma_trx_user_obj malloc failed");
        return RK_PCIE_ERR_MALLOC;
    }
    ep_dev->obj->config_dma_func = rockchip_pcie_config_dma_dwc;
    ep_dev->obj->start_dma_func = rockchip_pcie_start_dma_dwc;
    ep_dev->obj->get_dma_status = rockchip_pcie_get_dma_status;
    ep_dev->obj->cb = rockchip_pcie_dma_interrupt_handler_call_back;

    ep_dev->obj->dev = malloc(sizeof(struct rockchip_pcie));
    if (!ep_dev->obj->dev)
    {
        RK_PCIE_LOGE("rockchip_pcie malloc failed");
        ret =  RK_PCIE_ERR_MALLOC;
        goto err;
    }
    memset(ep_dev->obj->dev, 0, sizeof(struct rockchip_pcie));

    ep_dev->obj->dev->pci = malloc(sizeof(struct dw_pcie));
    if (!ep_dev->obj->dev->pci)
    {
        RK_PCIE_LOGE("dw_pcie malloc failed");
        ret = RK_PCIE_ERR_MALLOC;
        goto err;
    }
    memset(ep_dev->obj->dev->pci, 0, sizeof(struct dw_pcie));

    /* Configure space */
    ep_dev->obj->dev->fd = ep_dev->pcie_ep_fd;
    RK_PCIE_LOGI(" - id=%x", rk_pci_read_config_dword(ep_dev->obj->dev, 0));

    if (ep_dev->bar0_vir_addr == NULL)
    {
        RK_PCIE_LOGE("bar0 memory failed");
        ret = RK_PCIE_ERR_NULL_PTR;
        goto err;
    }
    else
    {
        ep_dev->obj->obj_info  = ep_dev->bar0_vir_addr;
    }
    RK_PCIE_LOGI(" - magic=%x, ver=%x", ep_dev->obj->obj_info->magic, ep_dev->obj->obj_info->version);

    if (ep_dev->bar4_vir_addr == NULL)
    {
        RK_PCIE_LOGE("bar4 addr failed");
        ret =  RK_PCIE_ERR_NULL_PTR;
        goto err;
    }
    else
    {
        ep_dev->obj->dev->pci->bar4_mapped_base = ep_dev->bar4_vir_addr;
        ep_dev->obj->dev->pci->atu_base = ep_dev->obj->dev->pci->bar4_mapped_base + PCIE_WIRED_ATU_OFFSET;
    }

    /* dev/mem mapped dbi register */
    if (ep_dev->bar2_vir_addr == NULL)
    {
        RK_PCIE_LOGE("bar2 addr failed");
        ret = RK_PCIE_ERR_NULL_PTR;
        goto err;
    }
    else
    {
        ep_dev->obj->user_buffer = ep_dev->bar2_vir_addr;
        ep_dev->obj->user_buffer_bus_addr = ((u64)rk_pci_read_config_dword(ep_dev->obj->dev, 0x1c) << 32) |
                                            (rk_pci_read_config_dword(ep_dev->obj->dev, 0x18) & ~0xf);
        ep_dev->obj->user_buffer_cpu_addr = rk_pci_read_inbound_atu_cpu_addr(ep_dev->obj->dev, PCIE_EP_OBJ_BAR2_INBOUND_INDEX);
        RK_PCIE_LOGI(" - bar2 bus_addr=0x%llx, cpu_addr=0x%llx", ep_dev->obj->user_buffer_bus_addr,
                     ep_dev->obj->user_buffer_cpu_addr);
    }

    chipinfo = getChipInfo();
    if (chipinfo != NULL)
    {
        RK_PCIE_LOGI("chip info: %s ", chipinfo->name);
        switch (chipinfo->type)
        {
        case RK_CHIP_3588:
            mmap_res = PCIE_EP_MMAP_RESOURCE_RK3588_RC_DBI;
            break;
        case RK_CHIP_3568:
        case RK_CHIP_3566:
            mmap_res = PCIE_EP_MMAP_RESOURCE_RK3568_RC_DBI;
            break;
        default:
            mmap_res = PCIE_EP_MMAP_RESOURCE_MAX;
            break;
        }
        if (mmap_res != PCIE_EP_MMAP_RESOURCE_MAX)
        {
            ioctl(ep_dev->pcie_ep_fd, PCIE_EP_SET_MMAP_RESOURCE, &mmap_res);
            ep_dev->obj->dev->pci->dbi_base = mmap(NULL, PCIE_DBI_BASE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                                                   ep_dev->obj->dev->fd, 0);
            if (ep_dev->obj->dev->pci->dbi_base == MAP_FAILED)
            {
                ep_dev->obj->dev->pci->dbi_base = NULL;
                RK_PCIE_LOGE("mmap dbi failed: %s", strerror(errno));
            }
        }
        else
        {
            ep_dev->obj->dev->pci->dbi_base = NULL;
            LOG_INFO("map dbi memory failed");
        }
    }
    else
    {
        ep_dev->obj->dev->pci->dbi_base = NULL;
        LOG_INFO("get chip info failed.");
    }

    /* Get DMA status with cpu polling, configure it */
    ep_dev->obj->irq = false;

    if (!ep_dev->obj->irq && ep_dev->obj->dev->pci->dbi_base != NULL)
    {
        /* RC local dma only support polling mode */
        rk_pcie_dma_set_dma_base(ep_dev->obj, LOCAL_DMA_MODE);
        val = readl_reg(ep_dev->obj->dev->pci->dbi_base, 0);
        if ((val & 0xffff) == 0x1d87)
        {
            ep_dev->obj->dev->pci->support_rc_dma = 1;
            writel_reg(ep_dev->obj->dev->pci->dma_base, PCIE_DMA_WR_INT_MASK, 0xffffffff);
            writel_reg(ep_dev->obj->dev->pci->dma_base, PCIE_DMA_RD_INT_MASK, 0xffffffff);
        }
        else
        {
            LOG_INFO("map /dev/mem dbi memory success, buf it's not RK RC");
        }
    }


    if (!ep_dev->obj->irq)
    {
        /* Unmask DMA IRQ and poll for status */
        rk_pcie_dma_set_dma_base(ep_dev->obj, WIRED_DMA_MODE);
        writel_reg(ep_dev->obj->dev->pci->dma_base, PCIE_DMA_WR_INT_MASK, 0xffffffff);
        writel_reg(ep_dev->obj->dev->pci->dma_base, PCIE_DMA_RD_INT_MASK, 0xffffffff);
    }

    pthread_mutex_init(&ep_dev->obj->mutex_dma_wr, NULL);
    pthread_mutex_init(&ep_dev->obj->mutex_dma_rd, NULL);
    pthread_mutex_init(&ep_dev->obj->mutex_elbi, NULL);

    return RK_PCIE_OK;

err:

    if (ep_dev->obj)
    {
        if (ep_dev->obj->dev)
        {
            if (ep_dev->obj->dev->pci)
            {
                free(ep_dev->obj->dev->pci);
            }
            free(ep_dev->obj->dev);
        }
        free(ep_dev->obj);
        ep_dev->obj = NULL;
    }

    return ret;
}

void rk_pcie_dma_deinit(struct ep_dev_st *ep_dev)
{
    if (ep_dev == NULL)
    {
        return;
    }

    if (ep_dev->obj)
    {
        pthread_mutex_destroy(&ep_dev->obj->mutex_elbi);
        pthread_mutex_destroy(&ep_dev->obj->mutex_dma_wr);
        pthread_mutex_destroy(&ep_dev->obj->mutex_dma_rd);
        if (ep_dev->obj->dev)
        {
            if (ep_dev->obj->dev->pci)
            {
                if (ep_dev->obj->dev->pci->dbi_base != NULL)
                {
                    munmap(ep_dev->obj->dev->pci->dbi_base, PCIE_DBI_BASE_SIZE);
                }
                free(ep_dev->obj->dev->pci);
            }
            free(ep_dev->obj->dev);
        }
        free(ep_dev->obj);
    }

    ep_dev->obj = NULL;
}
#else
int rk_pcie_dma_init(struct ep_dev_st *ep_dev)
{
    int ret;

    if (ep_dev == NULL)
    {
        return RK_PCIE_ERR_NULL_PTR;
    }

    ep_dev->obj = malloc(sizeof(struct dma_trx_user_obj));
    if (!ep_dev->obj)
    {
        RK_PCIE_LOGE("dma_trx_user_obj malloc failed");
        return RK_PCIE_ERR_MALLOC;
    }
    ep_dev->obj->config_dma_func = rockchip_pcie_config_dma_dwc;
    ep_dev->obj->start_dma_func = rockchip_pcie_start_dma_dwc;
    ep_dev->obj->get_dma_status = rockchip_pcie_get_dma_status;
    ep_dev->obj->cb = rockchip_pcie_dma_interrupt_handler_call_back;

    ep_dev->obj->dev = malloc(sizeof(struct rockchip_pcie));
    if (!ep_dev->obj->dev)
    {
        RK_PCIE_LOGE("rockchip_pcie malloc failed");
        ret = RK_PCIE_ERR_MALLOC;
        goto err;
    }

    ep_dev->obj->dev->pci = malloc(sizeof(struct dw_pcie));
    if (!ep_dev->obj->dev->pci)
    {
        RK_PCIE_LOGE("dw_pcie malloc failed");
        ret = RK_PCIE_ERR_MALLOC;
        goto err;
    }

    if (ep_dev->bar4_vir_addr == NULL)
    {
        RK_PCIE_LOGE("bar4 addr failed");
        ret = RK_PCIE_ERR_NULL_PTR;
        goto err;
    }
    else
    {
        ep_dev->obj->dev->pci->dbi_base = ep_dev->bar4_vir_addr;
        ep_dev->obj->dev->pci->atu_base = ep_dev->obj->dev->pci->dbi_base + PCIE_LOCAL_ATU_OFFSET;
    }
    RK_PCIE_LOGI(" - id=%x", dw_pcie_readl_dbi(ep_dev->obj->dev->pci, 0));

    if (ep_dev->bar0_vir_addr == NULL)
    {
        RK_PCIE_LOGE("bar0 addr failed");
        ret = RK_PCIE_ERR_NULL_PTR;
        goto err;
    }
    else
    {
        ep_dev->obj->obj_info  = ep_dev->bar0_vir_addr;
    }
    RK_PCIE_LOGI(" - magic=%x, ver=%x", ep_dev->obj->obj_info->magic, ep_dev->obj->obj_info->version);

    if (ep_dev->bar2_vir_addr == NULL)
    {
        RK_PCIE_LOGE("bar2 addr failed");
        ret = RK_PCIE_ERR_NULL_PTR;
        goto err;
    }
    else
    {
        ep_dev->obj->user_buffer = ep_dev->bar2_vir_addr;
        ep_dev->obj->user_buffer_cpu_addr = rk_pci_read_inbound_atu_cpu_addr(ep_dev->obj->dev, PCIE_EP_OBJ_BAR2_INBOUND_INDEX);
        ep_dev->bar2_phys_addr = ep_dev->obj->user_buffer_cpu_addr;
        RK_PCIE_LOGI(" - bar2 cpu_addr=0x%llx", ep_dev->obj->user_buffer_cpu_addr);
    }

    /* Get DMA status with cpu polling */
    ep_dev->obj->irq = false;
    if (!ep_dev->obj->irq)
    {
        /* Unmask DMA IRQ and poll for status */
        ioctl(ep_dev->pcie_ep_fd, PCIE_DMA_IRQ_MASK_ALL, NULL);
    }

    pthread_mutex_init(&ep_dev->obj->mutex_dma_wr, NULL);
    pthread_mutex_init(&ep_dev->obj->mutex_dma_rd, NULL);
    pthread_mutex_init(&ep_dev->obj->mutex_elbi, NULL);

    return RK_PCIE_OK;

err:
    if (ep_dev->obj)
    {
        if (ep_dev->obj->dev)
        {
            if (ep_dev->obj->dev->pci)
            {
                free(ep_dev->obj->dev->pci);
            }
            free(ep_dev->obj->dev);
        }
        free(ep_dev->obj);
    }

    return ret;
}

void rk_pcie_dma_deinit(struct ep_dev_st *ep_dev)
{
    if (ep_dev == NULL)
    {
        return;
    }

    if (ep_dev->obj)
    {
        pthread_mutex_destroy(&ep_dev->obj->mutex_elbi);
        pthread_mutex_destroy(&ep_dev->obj->mutex_dma_wr);
        pthread_mutex_destroy(&ep_dev->obj->mutex_dma_rd);

        if (ep_dev->obj->dev)
        {
            if (ep_dev->obj->dev->pci)
            {
                free(ep_dev->obj->dev->pci);
            }
            free(ep_dev->obj->dev);
        }
        free(ep_dev->obj);
    }
}
#endif

int rk_pcie_start_dma_wr(struct ep_dev_st *ep_dev, size_t remote_addr, size_t local_addr,
                         size_t size, unsigned int mode)
{
    int chn = 0;
    int ret = 0;

    if (rk_pcie_check_active(ep_dev->obj) != RK_PCIE_OK)
    {
        return RK_PCIE_ERR_BAD;
    }

    /* RC端DMA需要使用本地BAR映射的地址才能访问到EP端内存。
       默认remote_addr是EP端的内存地址，这里需要和RC端地址做个映射*/
#ifdef PCIE_RC
    if (mode != WIRED_DMA_KERNEL_MODE || ep_dev->rc_dma.using_rc_dma == 1)
    {
        pthread_mutex_lock(&ep_dev->obj->mutex_dma_wr);
    }

    chn = 1; //使用EP DMA情况下，RC端DMA使用通道1，EP端DMA使用通道0
    if (ep_dev->rc_dma.using_rc_dma == 1)
    {
        if (mode == LOCAL_DMA_MODE)
        {
            /* Dynamic adjustment of inbound mapping */
            if (remote_addr < ep_dev->obj->user_buffer_cpu_addr || remote_addr >= ep_dev->obj->user_buffer_cpu_addr + ep_dev->bar_size[BAR2_INDEX])
            {
                ep_dev->obj->user_buffer_cpu_addr = rk_pci_prog_inbound_atu_cpu_addr(ep_dev->obj->dev, PCIE_EP_OBJ_BAR2_INBOUND_INDEX, remote_addr, ep_dev->bar_size[BAR2_INDEX]);
            }
            /* RC -> pcie addr + offset -> inbound cpu addr + offset */
            if (remote_addr >= ep_dev->obj->user_buffer_cpu_addr &&
                    (remote_addr + size) <= (ep_dev->obj->user_buffer_cpu_addr + ep_dev->bar_size[BAR2_INDEX]))
            {
                phys_addr_t offset = remote_addr - ep_dev->obj->user_buffer_cpu_addr;

                remote_addr = ep_dev->obj->user_buffer_bus_addr + offset;
                chn = ep_dev->rc_dma.rc_dma_chn;
            }
            else
            {
                mode = WIRED_DMA_KERNEL_MODE;
                LOG_INFO("change from rc dam to ep dma, %lx %lx", remote_addr, size);
            }
        }
    }
#else
    if (mode != WIRED_DMA_KERNEL_MODE)
    {
        pthread_mutex_lock(&ep_dev->obj->mutex_dma_wr);
    }
#endif
    // RK_PCIE_LOGV("tobus fd=%x chn=%x bus=%lx local=%lx size=%lx\n", ep_dev->pcie_ep_fd, chn, remote_addr, local_addr, size);

    rk_pcie_dma_set_dma_base(ep_dev->obj, mode);
    if (mode == LOCAL_DMA_MODE)
    {
        ret = __rk_pcie_dma_tobus(ep_dev->obj, chn, remote_addr, local_addr, size);
    }
    else if (mode == WIRED_DMA_KERNEL_MODE)
    {
        ret = __rk_pcie_dma_tobus_by_kernel_api(ep_dev->obj, ep_dev->pcie_ep_fd, chn, remote_addr, local_addr, size);
    }
    else if (mode == WIRED_DMA_MODE)
    {
        ret = __rk_pcie_wired_dma_tobus(ep_dev->obj, chn, remote_addr, local_addr, size);
    }
    else
    {
        ret = RK_PCIE_ERR_UNKNOWN;
    }
#ifdef PCIE_RC
    if (mode != WIRED_DMA_KERNEL_MODE || ep_dev->rc_dma.using_rc_dma == 1)
    {
        pthread_mutex_unlock(&ep_dev->obj->mutex_dma_wr);
    }
#else
    if (mode != WIRED_DMA_KERNEL_MODE)
    {
        pthread_mutex_unlock(&ep_dev->obj->mutex_dma_wr);
    }
#endif
    return ret;
}

int rk_pcie_start_dma_rd(struct ep_dev_st *ep_dev, size_t local_addr, size_t remote_addr,
                         size_t size, unsigned int mode)
{
    int ret = 0;
    int chn = 0;

    if (rk_pcie_check_active(ep_dev->obj) != RK_PCIE_OK)
    {
        return RK_PCIE_ERR_BAD;
    }

#ifdef PCIE_RC
    chn = 1;
    if (mode != WIRED_DMA_KERNEL_MODE || ep_dev->rc_dma.using_rc_dma == 1)
    {
        pthread_mutex_lock(&ep_dev->obj->mutex_dma_rd);
    }

    if (ep_dev->rc_dma.using_rc_dma == 1)
    {
        if (mode == LOCAL_DMA_MODE)
        {
            /* Dynamic adjustment of inbound mapping */
            if (remote_addr < ep_dev->obj->user_buffer_cpu_addr || remote_addr >= ep_dev->obj->user_buffer_cpu_addr + ep_dev->bar_size[BAR2_INDEX])
            {
                ep_dev->obj->user_buffer_cpu_addr = rk_pci_prog_inbound_atu_cpu_addr(ep_dev->obj->dev, PCIE_EP_OBJ_BAR2_INBOUND_INDEX, remote_addr, ep_dev->bar_size[BAR2_INDEX]);
            }
            /* RC -> pcie addr + offset -> inbound cpu addr + offset */
            if (remote_addr >= ep_dev->obj->user_buffer_cpu_addr &&
                    (remote_addr + size) <= (ep_dev->obj->user_buffer_cpu_addr + ep_dev->bar_size[BAR2_INDEX]))
            {
                phys_addr_t offset = remote_addr - ep_dev->obj->user_buffer_cpu_addr;

                remote_addr = ep_dev->obj->user_buffer_bus_addr + offset;
                chn = ep_dev->rc_dma.rc_dma_chn;
            }
            else
            {
                mode = WIRED_DMA_KERNEL_MODE;
                LOG_INFO("change from rc dam to ep dma, %lx %lx", remote_addr, size);
            }
        }
    }
#else
    if (mode != WIRED_DMA_KERNEL_MODE)
    {
        pthread_mutex_lock(&ep_dev->obj->mutex_dma_rd);
    }
#endif
    // RK_PCIE_LOGV("frombus fd=%x chn=%x local=%lx bus=%lx size=%lx\n", ep_dev->pcie_ep_fd, 1, local_addr, remote_addr, size);

    rk_pcie_dma_set_dma_base(ep_dev->obj, mode);
    if (mode == LOCAL_DMA_MODE)
    {
        ret = __rk_pcie_dma_frombus(ep_dev->obj, chn, local_addr, remote_addr, size);
    }
    else if (mode == WIRED_DMA_KERNEL_MODE)
    {
        ret = __rk_pcie_dma_frombus_by_kernel_api(ep_dev->obj, ep_dev->pcie_ep_fd, chn, local_addr, remote_addr, size);
    }
    else if (mode == WIRED_DMA_MODE)
    {
        ret = __rk_pcie_wired_dma_frombus(ep_dev->obj, chn, local_addr, remote_addr, size);
    }
    else
    {
        ret = RK_PCIE_ERR_UNKNOWN;
    }

#ifdef PCIE_RC
    if (mode != WIRED_DMA_KERNEL_MODE || ep_dev->rc_dma.using_rc_dma == 1)
    {
        pthread_mutex_unlock(&ep_dev->obj->mutex_dma_rd);
    }
#else
    if (mode != WIRED_DMA_KERNEL_MODE)
    {
        pthread_mutex_unlock(&ep_dev->obj->mutex_dma_rd);
    }
#endif
    return ret;
}

int rk_pcie_rc_set_data_elbi_irq_by_virtual_id(struct ep_dev_st *ep_dev, int virtual_id)
{
    int val = 0xffffffff, index, bit_mask;
    int timeout_us = 1000;

    pthread_mutex_lock(&ep_dev->obj->mutex_elbi);
    index = virtual_id / 32;
    bit_mask = 1 << (virtual_id % 32);

    while ((val & bit_mask) && timeout_us--)
    {
        rk_pcie_elbi_read(ep_dev, index, &val);
        usleep(1);
    }

    if (timeout_us <= 0)
    {
        rk_pcie_ep_clear_elbi_irq_by_virtual_id(ep_dev, virtual_id);
        RK_PCIE_LOGE(" id=%d, timeout, val=%x\n", virtual_id, val);
    }

    val |= bit_mask;
    rk_pcie_elbi_write(ep_dev, index, val);
    pthread_mutex_unlock(&ep_dev->obj->mutex_elbi);

    return RK_PCIE_OK;
}

int rk_pcie_ep_find_first_elbi_irq_by_virtual_id(struct ep_dev_st *ep_dev)
{
    int val, index, bit;

    for (index = 0; index < PCIE_EP_OBJ_ELBI_USERDATA_NUM; index++)
    {
        rk_pcie_elbi_read(ep_dev, index, &val);
        for (bit = 0; bit < 32; bit++)
        {
            if (val & (1 << bit))
            {
                return index * 32 + bit;
            }
        }
    }

    return RK_PCIE_ERR_VALUE;
}

int rk_pcie_ep_clear_elbi_irq_by_virtual_id(struct ep_dev_st *ep_dev, int virtual_id)
{
    int val, index, bit_mask;

    if (virtual_id < 0 || virtual_id >= (PCIE_EP_OBJ_ELBI_USERDATA_NUM * 32))
    {
        return RK_PCIE_ERR_VALUE;
    }

    index = virtual_id / 32;
    bit_mask = 1 << (virtual_id % 32);

    rk_pcie_elbi_read(ep_dev, index, &val);
    val &= (~bit_mask);
    rk_pcie_elbi_write(ep_dev, index, val);

    return RK_PCIE_OK;
}

int rk_pcie_raise_irq_user(struct ep_dev_st *ep_dev, int index)
{
    int ret;

    if (index < 0 || index >= (PCIE_EP_OBJ_INFO_MSI_DATA_NUM))
    {
        return RK_PCIE_ERR_VALUE;
    }

    ret = ioctl(ep_dev->pcie_ep_fd, PCIE_EP_RAISE_IRQ_USER, &index);
    if (ret < 0)
    {
        return RK_PCIE_ERR_TIMEOUT;
    }
    else
    {
        return RK_PCIE_OK;
    }
}

int rk_pcie_poll_irq_user(struct ep_dev_st *ep_dev, struct pcie_ep_obj_poll_virtual_id_cfg *cfg)
{
    if (!cfg || (cfg->virtual_id < 0 || cfg->virtual_id >= (PCIE_EP_OBJ_INFO_MSI_DATA_NUM)))
    {
        return RK_PCIE_ERR_VALUE;
    }

    ioctl(ep_dev->pcie_ep_fd, PCIE_EP_POLL_IRQ_USER, cfg);
    if (cfg->poll_status == POLL_IN)
    {
        return RK_PCIE_OK;
    }
    else if (cfg->poll_status == NSIGPOLL)
    {
        return RK_PCIE_ERR_TIMEOUT;
    }
    else
    {
        return RK_PCIE_ERR_UNKNOWN;
    }
}
