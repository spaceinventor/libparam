#include <string.h>
#include <stdio.h>
#include <vmem/vmem_mmap.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

static void ensure_init(vmem_mmap_driver_t *drv, uint64_t *size)
{
	if (0 == drv->physaddr)
	{
		int fd;
		if ((fd = open(drv->filename, O_CREAT|O_RDWR, 0600)) == -1)
		{
			perror("open");
			return;
		}
		uint32_t _size = (uint32_t)(*size);
		long cur_size = lseek(fd, 0, SEEK_END);
		if (cur_size < _size) {
			/* Grow the file if needed */
			if (ftruncate(fd, _size) != 0) {
				close(fd);
				perror("ftruncate");
				return;
			}
		} else {
			/* File size is >= requested size, don't destroy/truncate data but adjust the driver size instead */
			*size = (uint64_t)cur_size;
		}
		drv->physaddr = mmap(0, *size, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
		close(fd);
		if (drv->physaddr == MAP_FAILED)
		{
			perror("mmap");
			return;
		}
	}
}

void vmem_mmap_read(vmem_t *vmem, uint64_t addr, void *dataout, uint32_t len)
{
	vmem_mmap_driver_t *drv = (vmem_mmap_driver_t *)vmem->driver;
	ensure_init(drv, &vmem->size);
	memcpy(dataout, ((vmem_mmap_driver_t *)vmem->driver)->physaddr + addr, len);
}

void vmem_mmap_write(vmem_t *vmem, uint64_t addr, const void *datain, uint32_t len)
{
	vmem_mmap_driver_t *drv = (vmem_mmap_driver_t *)vmem->driver;
	ensure_init(drv, &vmem->size);
	if ((addr + len) >  vmem->size) {
		// Grow the file...
		if (msync(drv->physaddr, vmem->size, MS_SYNC|MS_INVALIDATE) == -1) {
			perror("Could not sync the file to disk");
		}
		vmem->size = (addr + len);
		drv->physaddr = 0;
		ensure_init(drv, &vmem->size);
	}
	memcpy(drv->physaddr + addr, datain, len);
}
