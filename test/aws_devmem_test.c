#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>
#include <sys/errno.h>

int main() {
	/*
	 * 34:00.0 Memory controller: Amazon.com, Inc. Device f010
        Subsystem: Device fedc:1d51
        Physical Slot: 3-1
        Flags: fast devsel, NUMA node 0
        Memory at 5004c000000 (64-bit, prefetchable) [size=64M]
        Memory at 50048100000 (64-bit, prefetchable) [size=64K]
        Memory at 52000000000 (64-bit, prefetchable) [size=128G]
        Capabilities: <access denied>
	*/
	int fd = open("/dev/mem", O_RDWR | O_SYNC);
	int sz = 128;
	printf("APP_PF_BAR0\n");
	unsigned long long addr = 0x5004c000000ULL;
	void *mapped_region = mmap(NULL, sz,  // 64MB for Region 0
			PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_FILE,
			fd,
			addr);
	if(mapped_region == MAP_FAILED) {
		printf("%s\n", strerror(errno));
		exit(0);
	}

	volatile uint32_t* ptr = (uint32_t*)mapped_region;
	for (int i = 0; i < 16; ++i) {
		//*ptr = i;
		printf("[%d] = %x\n", i, *ptr);
		ptr++;
	}

	munmap(mapped_region, sz);
	

	printf("APP_PF_BAR4\n");
	addr = 0x52000000000ULL;
	mapped_region = mmap(NULL, sz,  // 64MB for Region 0
			PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_FILE,
			fd,
			addr);
	if(mapped_region == MAP_FAILED) {
		printf("%s\n", strerror(errno));
		exit(0);
	}

	ptr = (uint32_t*)mapped_region;
	for (int i = 0; i < 16; ++i) {
		*ptr = i;
		ptr++;
	}
	ptr = (uint32_t*)mapped_region;
	for (int i = 0; i < 16; ++i) {
		printf("[%d] = %x\n", i, *ptr);
		ptr++;
	}
	munmap(mapped_region, sz);

	printf("APP_PF_BAR2\n");
	addr = 0x50048100000ULL;
	mapped_region = mmap(NULL, sz,  // 64MB for Region 0
			PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_FILE,
			fd,
			addr);
	if(mapped_region == MAP_FAILED) {
		printf("%s\n", strerror(errno));
		exit(0);
	}

	ptr = (uint32_t*)mapped_region;
	for (int i = 0; i < 16; ++i) {
		//*ptr = i;
		printf("[%d] = %x\n", i, *ptr);
		ptr++;
	}
	munmap(mapped_region, sz);
	close(fd);
}
