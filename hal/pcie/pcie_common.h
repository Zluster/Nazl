//
// Created by zwz on 2024/11/3.
//

#ifndef NAZL_HAL_PCIE_PCEI_COMMON_H_
#define NAZL_HAL_PCIE_PCEI_COMMON_H_

define RKEP_BAR0_CMD_OFFSET        0x400
#define RKEP_BOOT_MAGIC             0x524b4550 /* RKEP */
#define RKEP_CMD_LOADER_RUN         0x524b4501

#define PCIE_BASE   'P'
#define PCIE_DMA_CACHE_INVALIDE     _IOW(PCIE_BASE, 1, struct pcie_ep_dma_cache_cfg)
#define PCIE_DMA_CACHE_FLUSH        _IOW(PCIE_BASE, 2, struct pcie_ep_dma_cache_cfg)
#define PCIE_DMA_IRQ_MASK_ALL       _IOW(PCIE_BASE, 3, int)
#define PCIE_EP_RAISE_MSI           _IOW(PCIE_BASE, 4, int)
#define PCIE_EP_SET_MMAP_RESOURCE   _IOW(PCIE_BASE, 6, int)
#define PCIE_EP_RAISE_ELBI          _IOW(PCIE_BASE, 7, int)
#define PCIE_EP_REQUEST_VIRTUAL_ID  _IOR(PCIE_BASE, 16, int)
#define PCIE_EP_RELEASE_VIRTUAL_ID  _IOW(PCIE_BASE, 17, int)
#define PCIE_EP_RAISE_IRQ_USER      _IOW(PCIE_BASE, 18, int)
#define PCIE_EP_POLL_IRQ_USER       _IOW(PCIE_BASE, 19, struct pcie_ep_obj_poll_virtual_id_cfg)
#define PCIE_EP_DMA_XFER_BLOCK      _IOW(PCIE_BASE, 32, struct pcie_ep_dma_block_req)

#define PCIE_BAR_MAX_NUM        6
#define BAR0_INDEX              0
#define BAR2_INDEX              2
#define BAR4_INDEX              4

#define RK_BUS_DEVICELEN        16

#define PCI_ERROR_DEVICE        -1
#define PCI_NO_DEVICE           -2
#define PCI_SUCESS              0

#define PCI_REMOVE_DEVICE       "/sys/bus/pci/devices"
#define PCI_RESCAN_DEVICE       "/sys/bus/pci/rescan"

typedef enum _RK_PCIE_RET
{
    RK_PCIE_OK                      = 0,

    RK_PCIE_ERR_BAD                 = -1,
    RK_PCIE_ERR_UNKNOWN             = -2,
    RK_PCIE_ERR_NULL_PTR            = -3,
    RK_PCIE_ERR_MALLOC              = -4,
    RK_PCIE_ERR_OPEN_FILE           = -5,
    RK_PCIE_ERR_VALUE               = -6,
    RK_PCIE_ERR_READ_BIT            = -7,
    RK_PCIE_ERR_TIMEOUT             = -8,
    RK_PCIE_ERR_UNIMPLIMENTED       = -9,
    RK_PCIE_ERR_UNSUPPORT           = -10,
    RK_PCIE_ERR_SUSPEND             = -11,
    RK_PCIE_ERR_NO_BUFFER           = -12,
    RK_PCIE_ERR_HW_UNSUPPORT        = -13,
    RK_PCIE_ERR_RETRY               = -14,
    RK_PCIE_ERR_MMAP                = -15,

    RK_PCIE_ERR_BASE                = -64,

    RK_PCIE_ERR_INIT                = RK_PCIE_ERR_BASE - 1,
    RK_PCIE_ERR_NOINIT              = RK_PCIE_ERR_BASE - 2,
    RK_PCIE_ERR_FATAL_THREAD        = RK_PCIE_ERR_BASE - 3,
    RK_PCIE_ERR_NOMEM               = RK_PCIE_ERR_BASE - 4,
    RK_PCIE_ERR_OUTOF_RANGE         = RK_PCIE_ERR_BASE - 5,
    RK_PCIE_ERR_END_OF_STREAM       = RK_PCIE_ERR_BASE - 6,

    /* The error in list */
    RK_PCIE_ERR_LIST_BASE           = -128,
    RK_PCIE_ERR_LIST_EMPTY          = RK_PCIE_ERR_LIST_BASE - 1,
    RK_PCIE_ERR_LIST_FULL           = RK_PCIE_ERR_LIST_BASE - 2,
    RK_PCIE_ERR_LIST_OUTOF_RANGE    = RK_PCIE_ERR_LIST_BASE - 3,
} RK_PCIE_RET;

struct mblk
{
    unsigned char* vir_addr; //虚拟地址
    size_t phy_addr; //物理地址
    size_t size;
    int handle;
    int fd;
};
struct mem_dev_st
{
    size_t in_buff_size;
    int in_max_buffer_num;
    struct mblk* in_mb;

    size_t out_buff_size;
    int out_max_buffer_num;
    struct mblk* out_mb;

    int using_rc_dma; //是否使用rc的dma
    unsigned char* bar2_vir_addr; //虚拟地址
    unsigned long long bar2_phys_addr; //物理地址

    unsigned char* base_vir_addr; //虚拟地址
    size_t base_phy_addr; //物理地址
};
struct rkpcie_cmd
{
    unsigned int cmd;
    unsigned int size;
    unsigned int data[6];
};

struct rkpcie_boot
{
    /* magic: "RKEP" */
    unsigned int magic;
    unsigned int version;
    struct
    {
        unsigned short mode;
        unsigned short submode;
    } devmode;
    /* Size of ATAGS for cap */
    unsigned int cap_size;
    struct
    {
        unsigned char cmd;
        unsigned char status;
        /* Error code for current CMD */
        unsigned short opcode;
    } cmd_status;
};

/*
 * rockchip driver cache ioctrl input param
 */
struct pcie_ep_dma_cache_cfg
{
    unsigned long long addr;
    unsigned int size;
};
struct pcie_ep_dma_block
{
    unsigned long long bus_paddr;
    unsigned long long local_paddr;
    unsigned int size;
};

struct pcie_ep_dma_block_req
{
    unsigned short vir_id;  /* Default 0 */
    unsigned char chn;
    unsigned char wr;
    unsigned int flag;
#define PCIE_EP_DMA_BLOCK_FLAG_COHERENT BIT(0)      /* Cache coherent, 1-need, 0-None */
    struct pcie_ep_dma_block block;
};

enum pcie_ep_obj_irq_type
{
    OBJ_IRQ_UNKNOWN,
    OBJ_IRQ_DMA,
    OBJ_IRQ_USER,
    OBJ_IRQ_ELBI,
};

struct pcie_ep_obj_irq_dma_status
{
    unsigned int wr;
    unsigned int rd;
};

/*
 * rockchip driver ep_obj poll ioctrl input param
 */
struct pcie_ep_obj_poll_virtual_id_cfg
{
    unsigned int timeout_ms;
    unsigned int sync;
    unsigned int virtual_id;
    unsigned int poll_status;
};

enum pcie_ep_mmap_resource
{
    PCIE_EP_MMAP_RESOURCE_DBI,
    PCIE_EP_MMAP_RESOURCE_BAR0,
    PCIE_EP_MMAP_RESOURCE_BAR2,
    PCIE_EP_MMAP_RESOURCE_BAR4,
    PCIE_EP_MMAP_RESOURCE_USER_MEM,
    PCIE_EP_MMAP_RESOURCE_RK3568_RC_DBI,
    PCIE_EP_MMAP_RESOURCE_RK3588_RC_DBI,
    PCIE_EP_MMAP_RESOURCE_MAX,
};

#define PCIE_EP_OBJ_INFO_MSI_DATA_NUM   0x8
#define RKEP_EP_VIRTUAL_ID_MAX          (PCIE_EP_OBJ_INFO_MSI_DATA_NUM * 32) /* 256 virtual_id */

/*
 * rockchip ep device information which is store in BAR0
 */
struct pcie_ep_obj_info
{
    unsigned int magic;
    unsigned int version;
    struct
    {
        unsigned short mode;
        unsigned short submode;
    } devmode;
    unsigned int msi_data[PCIE_EP_OBJ_INFO_MSI_DATA_NUM];
    unsigned char reserved[0x1D0];

    unsigned int irq_type_rc;                               /* Generate in ep isr, valid only for rc, clear in rc */
    struct pcie_ep_obj_irq_dma_status dma_status_rc;        /* Generate in ep isr, valid only for rc, clear in rc */
    unsigned int irq_type_ep;                               /* Generate in ep isr, valid only for ep, clear in ep */
    struct pcie_ep_obj_irq_dma_status dma_status_ep;        /* Generate in ep isr, valid only for ep, clear in ep */
    unsigned intirq_user_data_rc;                           /* Generate in ep, valid only for rc, No need to clear */
    unsigned irq_user_data_ep;                              /* Generate in rc, valid only for ep, No need to clear */
};

struct pcie_rc_dma_st
{
    unsigned char using_rc_dma;
    unsigned char rc_dma_chn;
};

struct ep_dev_st
{
    unsigned long id;
    int enable_fd;
    int bar0_fd;
    int bar2_fd;
    int bar4_fd;
    int pcie_ep_fd;
    void* bar0_vir_addr;
    void* bar2_vir_addr;
    void* bar4_vir_addr;
    struct dma_trx_user_obj* obj;
    struct pci_dev* dev;
    unsigned char pci_bus_address[RK_BUS_DEVICELEN];
    unsigned int bar_size[PCIE_BAR_MAX_NUM];
    struct pcie_rc_dma_st rc_dma;
};

struct pcie_ep_objs_st
{
    unsigned long vendor_id;
    unsigned long device_id;
    struct ep_dev_st ep_dev;
};

struct pci_bus_st
{
    char address[RK_BUS_DEVICELEN];
};

int get_pci_bus_addresses_for_device(int vendor_id, int device_id, struct pci_bus_st *pci_bus, int support_device_max_num);
int rk_pcie_dev_init(struct pcie_ep_objs_st *pcie_dev_objs);
void rk_pcie_dev_deinit(struct pcie_ep_objs_st *pcie_dev_objs);
int pcie_remove_device(char* pci_bus_address);
int pcie_rescan_devices(void);
int rk_pcie_request_virtual_id(struct pcie_ep_objs_st *pcie_dev_objs);
int rk_pcie_release_virtual_id(struct pcie_ep_objs_st *pcie_dev_objs, int id);

#endif //NAZL_HAL_PCIE_PCEI_COMMON_H_
