#include <string.h>
#include <stdio.h>
#include <vmem/vmem_mmap.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

static void ensure_init(vmem_mmap_driver_t *drv, uint32_t size)
{
	if (0 == drv->physaddr)
	{
		int fd;
		if ((fd = open(drv->filename, O_CREAT|O_RDWR, 0600)) == -1)
		{
			perror("open");
			return;
		}
		ftruncate(fd, size);
		drv->physaddr = mmap(0, size, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
		if (drv->physaddr == MAP_FAILED)
		{
			perror("mmap");
			return;
		}
	}
}

void vmem_mmap_read(vmem_t *vmem, uint32_t addr, void *dataout, uint32_t len)
{
	vmem_mmap_driver_t *drv = (vmem_mmap_driver_t *)vmem->driver;
	ensure_init(drv, vmem->size);
	memcpy(dataout, ((vmem_mmap_driver_t *)vmem->driver)->physaddr + addr, len);
}

void vmem_mmap_write(vmem_t *vmem, uint32_t addr, const void *datain, uint32_t len)
{
	vmem_mmap_driver_t *drv = (vmem_mmap_driver_t *)vmem->driver;
	ensure_init(drv, vmem->size);
	memcpy(drv->physaddr + addr, datain, len);
 	if (msync(drv->physaddr, vmem->size, MS_SYNC|MS_INVALIDATE) == -1) {
        perror("Could not sync the file to disk");
    }
}
