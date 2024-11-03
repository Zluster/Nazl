//
// Created by zwz on 2024/11/3.
//

#ifndef NAZL_HAL_PCIE_RK_PCIE_DMA_H_
#define NAZL_HAL_PCIE_RK_PCIE_DMA_H_
#include "pcie_common.h"

#define PCIE_DBI_BASE_SIZE          0x400000

enum pcie_dma_mode
{
    LOCAL_DMA_MODE = 0x0,
    WIRED_DMA_MODE = 0x1,
    WIRED_DMA_KERNEL_MODE = 0x2,
};

int rk_pcie_dma_init(struct ep_dev_st *ep_dev);
void rk_pcie_dma_deinit(struct ep_dev_st *ep_dev);

int rk_pcie_start_dma_wr(struct ep_dev_st *ep_dev, size_t remote_addr, size_t local_addr,
                         size_t size, unsigned int mode);
int rk_pcie_start_dma_rd(struct ep_dev_st *ep_dev, size_t local_addr, size_t remote_addr,
                         size_t size, unsigned int mode);
int rk_pcie_rc_check_support_local_dma(struct ep_dev_st *ep_dev);

int rk_pcie_rc_set_data_elbi_irq_by_virtual_id(struct ep_dev_st *ep_dev, int virtual_id);
int rk_pcie_ep_find_first_elbi_irq_by_virtual_id(struct ep_dev_st *ep_dev);
int rk_pcie_ep_clear_elbi_irq_by_virtual_id(struct ep_dev_st *ep_dev, int virtual_id);

int rk_pcie_raise_irq_user(struct ep_dev_st *ep_dev, int index);
int rk_pcie_poll_irq_user(struct ep_dev_st *ep_dev, struct pcie_ep_obj_poll_virtual_id_cfg *cfg);

int rk_pci_get_bar_size(struct ep_dev_st *ep_dev, unsigned char bar);

#endif //NAZL_HAL_PCIE_RK_PCIE_DMA_H_
