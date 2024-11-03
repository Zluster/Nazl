//
// Created by zwz on 2024/11/3.
//
#include <sys/mman.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include "pcei_common.h"
int rk_mem_init(void** mem)
{
    return RK_PCIE_OK;
}

int rk_mem_deinit(void* mem)
{
    return RK_PCIE_OK;
}

int rk_mem_create(void* mem, struct mem_dev_st *mem_dev)
{
    static in_total_size = 0;
    size_t max_size =
        mem_dev->in_max_buffer_num * mem_dev->in_buffer_size + mem_dev->out_max_buffer_num * mem_dev->out_buffer_size;
    int ret;
    LOG_INFO("create mem size %d\n", max_size);
    if (mem_dev->base_vir_addr == NULL)
    {
        LOG_INFO("rkep vir size %d fial", (int) max_size);
        return RK_PCIE_ERR_NULL_PTR;
    }
    if (mem_dev->base_phy_addr == 0)
    {
        LOG_INFO("rkep phy size %d fial", (int) max_size);
        return RK_PCIE_ERR_NULL_PTR;
    }
    if (mem_dev->in_max_buffer_num)
    {
        mem_dev->in_mb = (struct mblk*) malloc(sizeof(struct mblk) * mem_dev->in_max_buffer_num);
        if (mem_dev->in_mb == NULL)
        {
            LOG_INFO("rkep in mb malloc fail");
            return RK_PCIE_ERR_MALLOC;
            goto release;
        }
        for (int i = 0; i < mem_dev->in_max_buffer_num; i++)
        {
            mem_dev->in_mb[i].vir_addr = mem_dev->base_vir_addr + mem_dev->in_buff_size * i;
            mem_dev->in_mb[i].phy_addr = mem_dev->base_phy_addr + mem_dev->in_buff_size * i;
            mem_dev->in_mb[i].size = mem_dev->in_buff_size;
            mem_dev->in_mb[i].fd = -1;
            in_total_size += mem_dev->in_buff_size;
            LOG_INFO("in buff %d phy %p vir %p size %d\n",
                     i,
                     mem_dev->in_mb[i].phy_addr,
                     mem_dev->in_mb[i].vir_addr,
                     mem_dev->in_buff_size);
        }
    }
    if (mem_dev->out_max_buffer_num)
    {
        mem_dev->out_mb = (struct mblk*) malloc(sizeof(struct mblk) * mem_dev->out_max_buffer_num);
        if (mem_dev->out_mb == NULL)
        {
            LOG_INFO("rkep out mb malloc fail");
            return RK_PCIE_ERR_MALLOC;
            goto release;
        }
        for (int i = 0; i < mem_dev->out_max_buffer_num; i++)
        {
            mem_dev->out_mb[i].vir_addr = mem_dev->base_vir_addr + mem_dev->in_buff_size * mem_dev->in_max_buffer_num
                                          + mem_dev->out_buff_size * i;
            mem_dev->out_mb[i].phy_addr = mem_dev->base_phy_addr + mem_dev->in_buff_size * mem_dev->in_max_buffer_num
                                          + mem_dev->out_buff_size * i;
            mem_dev->out_mb[i].size = mem_dev->out_buff_size;
            mem_dev->out_mb[i].fd = -1;
            LOG_INFO("out buff %d phy %p vir %p size %d\n",
                     i,
                     mem_dev->out_mb[i].phy_addr,
                     mem_dev->out_mb[i].vir_addr,
                     mem_dev->out_buff_size);
        }
    }
    return RK_PCIE_OK;
release:
    rk_mem_destroy(mem, mem_dev);
    return ret;
}

void rk_mem_destory(void* mem, struct mem_dev_st *mem_dev)
{
    if (mem_dev->in_mb)
    {
        free(mem_dev->in_mb);
        mem_dev->in_mb = NULL;
    }

    if (mem_dev->out_mb)
    {
        free(mem_dev->out_mb);
        mem_dev->out_mb = NULL;
    }
}














