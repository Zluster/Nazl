//
// Created by zwz on 2024/11/3.
//

#ifndef NAZL_HAL_PCIE_PCEI_COMMON_H_
#define NAZL_HAL_PCIE_PCEI_COMMON_H_

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
class pcei_common
{

};

#endif //NAZL_HAL_PCIE_PCEI_COMMON_H_
