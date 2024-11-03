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

#include "pcie_common.h"
#define PCIE_ENABLE          "enable"
#define PCIE_RESOURCE0       "resource0"
#define PCIE_RESOURCE2       "resource2"
#define PCIE_RESOURCE4       "resource4"
#define PCIE_CONFIG          "config"
#define RK_OBJNAMELEN   1024
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
static inline char* rk_sysfs_name(struct pci_access *a)
{
    return pci_get_param(a, "sysfs.path");
}

static int rk_device_name(struct pci_dev *d, char* buf)
{
    int n = snprintf(buf, RK_BUS_DEVICELEN, "%04x:%02x:%02x.%d",
                     d->domain, d->bus, d->dev, d->func);
    if (n < 0 || n >= RK_OBJNAMELEN)
    {
        RK_PCIE_LOGE("File name too long");
        return RK_PCIE_ERR_OUTOF_RANGE;
    }

    return RK_PCIE_OK;
}


static int rk_sysfs_obj_name(struct pci_dev *d, char* object, char* buf)
{
    int n = snprintf(buf, RK_OBJNAMELEN, "%s/devices/%04x:%02x:%02x.%d/%s",
                     rk_sysfs_name(d->access), d->domain, d->bus, d->dev, d->func, object);
    if (n < 0 || n >= RK_OBJNAMELEN)
    {
        RK_PCIE_LOGE("File name too long");
        return RK_PCIE_ERR_OUTOF_RANGE;
    }

    return RK_PCIE_OK;
}

int pcie_get_resource(struct pci_dev *dev, char* resource_name, int* resource_fd)
{
    int ret;
    char resource_path[1024];
    int fd;
    ret = rk_sysfs_obj_name(dev, resource_name, resource_path);
    if (ret != RK_PCIE_OK)
    {
        RK_PCIE_LOGE("Can't get dirname ");
        goto err;
    }
    RK_PCIE_LOGV("resource path = %s", resource_path);

    if ((fd = open(resource_path, O_RDWR | O_SYNC)) == -1)
    {
        RK_PCIE_LOGE("resource fd failed: %s", strerror(errno));
        ret = RK_PCIE_ERR_OPEN_FILE;
        goto err;
    }

    *resource_fd = fd;

    return RK_PCIE_OK;

err:
    return ret;
}

void pcie_close_resource(int resource_fd, unsigned long size, void* ptr_addr)
{
    if (ptr_addr > 0)
    {
        munmap(ptr_addr, size);
    }

    if (resource_fd > 0)
    {
        close(resource_fd);
    }
}

// 移除PCI设备
int pcie_remove_device(char* pci_bus_address)
{
    char path[256];
    int fd;

    sprintf(path, "%s/%s/remove", PCI_REMOVE_DEVICE, pci_bus_address);

    fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        RK_PCIE_LOGE("Failed to open device for removal, path: %s. %s", path, strerror(errno));
        return RK_PCIE_ERR_OPEN_FILE;
    }

    // 向设备的remove文件写入值1
    if (write(fd, "1", 1) != 1)
    {
        RK_PCIE_LOGE("Failed to write to device remove file. %s", strerror(errno));
        return RK_PCIE_ERR_VALUE;
    }

    close(fd);

    return RK_PCIE_OK;
}

// 重新扫描PCI设备
int pcie_rescan_devices(void)
{
    int fd = open(PCI_RESCAN_DEVICE, O_WRONLY);
    if (fd == -1)
    {
        RK_PCIE_LOGE("Failed to open PCI rescan file. %s", strerror(errno));
        return RK_PCIE_ERR_OPEN_FILE;
    }

    // 向rescan文件写入值1
    if (write(fd, "1", 1) != 1)
    {
        RK_PCIE_LOGE("Failed to write to PCI rescan file. %s", strerror(errno));
        return RK_PCIE_ERR_VALUE;
    }

    close(fd);

    return RK_PCIE_OK;
}

int get_pci_bus_addresses_for_device(int vendor_id, int device_id, struct pci_bus_st *pci_bus, int support_device_max_num)
{
    struct pci_access *pacc;
    struct pci_dev *dev;
    int dev_max_num = 0;
    int ret;

    pacc = pci_alloc(); /* Get the pci_access structure */
    /* Set all options you want -- here we stick with the defaults */
    pci_init(pacc);                      /* Initialize the PCI library */
    pci_scan_bus(pacc);                  /* We want to get the list of devices */

    for (dev = pacc->devices; dev; dev = dev->next)   /* Iterate over all devices */
    {
        pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS); /* Fill in header info we need */
        if (dev->device_id == device_id && dev->vendor_id == vendor_id)
        {

            if (dev_max_num >= support_device_max_num)
            {
                RK_PCIE_LOGE("The maximum number of supported devices is %d, Current number of devices : %d", support_device_max_num, dev_max_num);
                break;
            }

            ret = rk_device_name(dev, pci_bus[dev_max_num].address);
            if (ret != RK_PCIE_OK)
            {
                RK_PCIE_LOGE("get bus address error, ret = %d, device number = %d", ret, dev_max_num);
                continue;
            }

            dev_max_num++;
        }
    }

    pci_cleanup(pacc);

    return dev_max_num;
}

int rk_pcie_dev_init(struct pcie_ep_objs_st *pcie_dev_objs)
{
    struct pci_access *pacc;
    struct pci_dev *dev;
    char enable = '1';
    //int dev_id = 0;
    int ret;
    int i = 0;
    char bus_address[RK_BUS_DEVICELEN];
    struct ep_dev_st *ep_dev;
    enum pcie_ep_mmap_resource mmap_res;

    ret = RK_PCIE_OK;

    pacc = pci_alloc(); /* Get the pci_access structure */
    /* Set all options you want -- here we stick with the defaults */
    pci_init(pacc);                      /* Initialize the PCI library */
    pci_scan_bus(pacc);                  /* We want to get the list of devices */

    ep_dev = &pcie_dev_objs->ep_dev;

    for (dev = pacc->devices; dev; dev = dev->next)   /* Iterate over all devices */
    {
        pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS); /* Fill in header info we need */
        if (dev->device_id == pcie_dev_objs->device_id && dev->vendor_id == pcie_dev_objs->vendor_id)
        {

            ret = rk_device_name(dev, bus_address);
            if (ret != RK_PCIE_OK)
            {
                RK_PCIE_LOGE("get device name error ");
                continue;
            }

            if (strncmp((char*)ep_dev->pci_bus_address, bus_address, RK_BUS_DEVICELEN) != 0)
            {
                continue;
            }

            ep_dev->dev = dev;

            /* enable pcie */
            ret = pcie_get_resource(dev, PCIE_ENABLE, &ep_dev->enable_fd);
            if (ret != RK_PCIE_OK)
            {
                RK_PCIE_LOGW("Can't get dirname: %s ", PCIE_ENABLE);
            }

            ret = write(ep_dev->enable_fd, &enable, sizeof(enable));
            if (ret != sizeof(enable))
            {
                //RK_PCIE_LOGW("write error, ret = %d, %s\n", ret, strerror(errno));
            }

            for (i = 0; i < PCIE_BAR_MAX_NUM; i++)
            {
                ret = rk_pci_get_bar_size(ep_dev, i);
                if (ret > 0)
                {
                    RK_PCIE_LOGD("bar%d size=0x%x", i, ret);
                    ep_dev->bar_size[i] = ret;
                }
            }

            mmap_res = PCIE_EP_MMAP_RESOURCE_BAR0;
            ioctl(ep_dev->pcie_ep_fd, PCIE_EP_SET_MMAP_RESOURCE, &mmap_res);
            ep_dev->bar0_vir_addr = mmap(NULL, ep_dev->bar_size[BAR0_INDEX], PROT_READ | PROT_WRITE,
                                         MAP_SHARED, ep_dev->pcie_ep_fd, 0);
            if (ep_dev->bar0_vir_addr == MAP_FAILED)
            {
                RK_PCIE_LOGE("mmap bar0 failed: %s", strerror(errno));
                ep_dev->bar0_vir_addr = NULL;
                ret = RK_PCIE_ERR_MMAP;
                goto err;
            }

            mmap_res = PCIE_EP_MMAP_RESOURCE_BAR2;
            ioctl(ep_dev->pcie_ep_fd, PCIE_EP_SET_MMAP_RESOURCE, &mmap_res);
            ep_dev->bar2_vir_addr = mmap(NULL, ep_dev->bar_size[BAR2_INDEX], PROT_READ | PROT_WRITE,
                                         MAP_SHARED, ep_dev->pcie_ep_fd, 0);
            if (ep_dev->bar2_vir_addr == MAP_FAILED)
            {
                RK_PCIE_LOGW("mmap bar2 failed: %s", strerror(errno));
                ep_dev->bar2_vir_addr = NULL;
                ret = RK_PCIE_ERR_MMAP;
                goto err;
            }

            mmap_res = PCIE_EP_MMAP_RESOURCE_BAR4;
            ioctl(ep_dev->pcie_ep_fd, PCIE_EP_SET_MMAP_RESOURCE, &mmap_res);
            ep_dev->bar4_vir_addr = mmap(NULL, ep_dev->bar_size[BAR4_INDEX], PROT_READ | PROT_WRITE,
                                         MAP_SHARED, ep_dev->pcie_ep_fd, 0);
            if (ep_dev->bar4_vir_addr == MAP_FAILED)
            {
                RK_PCIE_LOGW("mmap bar4 failed: %s", strerror(errno));
                ep_dev->bar4_vir_addr = NULL;
                ret = RK_PCIE_ERR_MMAP;
                goto err;
            }

            ret = rk_pcie_dma_init(ep_dev);
            if (ret != RK_PCIE_OK)
            {
                RK_PCIE_LOGW("pcie dma init fail, ret = %d", ret);
                goto err;
            }

            ret = RK_PCIE_OK;

            break;
err:
            if (ep_dev->bar0_vir_addr > 0)
            {
                pcie_close_resource(ep_dev->bar0_fd, ep_dev->bar_size[BAR0_INDEX], ep_dev->bar0_vir_addr);
            }

            if (ep_dev->bar2_vir_addr > 0)
            {
                pcie_close_resource(ep_dev->bar2_fd, ep_dev->bar_size[BAR2_INDEX], ep_dev->bar2_vir_addr);
            }

            if (ep_dev->bar4_vir_addr > 0)
            {
                pcie_close_resource(ep_dev->bar4_fd, ep_dev->bar_size[BAR4_INDEX], ep_dev->bar4_vir_addr);
            }

            if (ep_dev->enable_fd > 0)
            {
                close(ep_dev->enable_fd);
            }

            break;
        }
    }

    if (dev == NULL)
    {
        ret = RK_PCIE_ERR_HW_UNSUPPORT;
    }

    pci_cleanup(pacc);

    return ret;
}


void rk_pcie_dev_deinit(struct pcie_ep_objs_st *pcie_dev_objs)
{
    struct ep_dev_st *ep_dev = &pcie_dev_objs->ep_dev;

    rk_pcie_dma_deinit(ep_dev);

    if (ep_dev->bar0_vir_addr > 0)
    {
        pcie_close_resource(ep_dev->bar0_fd, ep_dev->bar_size[BAR0_INDEX], ep_dev->bar0_vir_addr);
    }

    if (ep_dev->bar2_vir_addr > 0)
    {
        pcie_close_resource(ep_dev->bar2_fd, ep_dev->bar_size[BAR2_INDEX], ep_dev->bar2_vir_addr);
    }

    if (ep_dev->bar4_vir_addr > 0)
    {
        pcie_close_resource(ep_dev->bar4_fd, ep_dev->bar_size[BAR4_INDEX], ep_dev->bar4_vir_addr);
    }

    if (ep_dev->enable_fd > 0)
    {
        close(ep_dev->enable_fd);
    }
}

int rk_pcie_request_virtual_id(struct pcie_ep_objs_st *pcie_dev_objs)
{
    int id = -1, fd, ret;

    fd = pcie_dev_objs->ep_dev.pcie_ep_fd;

    ret = ioctl(fd, PCIE_EP_REQUEST_VIRTUAL_ID, &id);
    if (ret < 0)
    {
        RK_PCIE_LOGE("error %s", strerror(errno));
    }

    return id;
}

int rk_pcie_release_virtual_id(struct pcie_ep_objs_st *pcie_dev_objs, int id)
{
    int fd = -1, ret;

    fd = pcie_dev_objs->ep_dev.pcie_ep_fd;

    ret = ioctl(fd, PCIE_EP_RELEASE_VIRTUAL_ID, &id);
    if (ret < 0)
    {
        RK_PCIE_LOGE("error %s", strerror(errno));
    }

    return ret;
}













