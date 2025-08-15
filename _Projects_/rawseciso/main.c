#include <sdk_version.h>
#include <cellstatus.h>
#include <cell/cell_fs.h>

#include <sys/prx.h>
#include <sys/ppu_thread.h>
#include <sys/event.h>
#include <sys/syscall.h>
#include <sys/memory.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "types.h"
#include "common.h"
#include "storage.h"
#include "syscall8.h"
#include "scsi.h"

#define SC_COBRA_SYSCALL8				(8)
#define SC_GET_PRX_MODULE_BY_ADDRESS	(461)
#define SC_STOP_PRX_MODULE				(482)
#define SC_PPU_THREAD_EXIT				(41)

SYS_MODULE_INFO(RAWSECISO, 0, 1, 0);
SYS_MODULE_START(rawseciso_start);
SYS_MODULE_STOP(rawseciso_stop);

#ifdef DEBUG
#define THREAD_NAME "rawseciso_thread"
#define STOP_THREAD_NAME "rawseciso_stop_thread"
#else
#define THREAD_NAME ""
#define STOP_THREAD_NAME ""
#endif

#define MIN(a, b)		((a) <= (b) ? (a) : (b))
#define ABS(a)			(((a) < 0) ? -(a) : (a))

#define CD_CACHE_SIZE			(48) // sectors 48*2448 = 115KB (up to 52 sectors fit in 128KB)

#define DISC_TYPE_NONE			0

#define MAX_TRACKS	98

enum EMU_TYPE
{
	EMU_OFF = 0,
	EMU_PS3,
	EMU_PS2_DVD,
	EMU_PS2_CD,
	EMU_PSX,
	EMU_BD,
	EMU_DVD,
	EMU_MAX,
	EMU_PSP, // not a DiscEmu
};

#define EMU_PSX_MULTI (EMU_PSX + 16)

#define	_8KB_		0x02000UL
#define	_40KB_		0x0A000UL
#define	_64KB_		0x10000UL
#define	_128KB_		0x20000UL
#define	_256KB_		0x40000UL

#define OK	  0
#define FAILED -1

#define LAST_SECTOR				1
#define READ_SECTOR				0xFFFFFFFF

#define NONE -1
#define SYS_PPU_THREAD_NONE		(sys_ppu_thread_t)NONE
#define SYS_EVENT_QUEUE_NONE	   (sys_event_queue_t)NONE
#define SYS_DEVICE_HANDLE_NONE	 (sys_device_handle_t)NONE

enum STORAGE_COMMAND
{
	CMD_READ_ISO,
	CMD_READ_DISC,
	CMD_READ_CD_ISO_2352,
	CMD_FAKE_STORAGE_EVENT,
	CMD_GET_PSX_VIDEO_MODE
};

typedef struct
{
	u64 device;
	u32 emu_mode;
	u32 num_sections;
	u32 num_tracks;
	// sections after
	// sizes after
	// tracks after
} __attribute__((packed)) rawseciso_args;

/* new for PSX with multiple disc support (derivated from Estwald's PSX payload):

in 0x80000000000000D0, u64 (8 bytes) containing ISOs sector size (for alls)

tracks index is from lv2peek(0x8000000000000050)

max 8 disc, 4*u32 (16 bytes) containing

0 -> u32 lv2 address for next entry (if 0 indicates no track info: you must generate it!)
1 -> u32 to 0 (reserved)
2 -> u32 lv2 address for track table info
3 -> u32 with size of track table info

table info: (from 0x80000000007E0000)
u8 datas

0, 1 -> u16 with size - 2 of track info block
2 -> to 1
3 -> to 1
// first track
4 -> 0
5 -> mode 0x14 for data, 0x10 for audio
6 -> track number (from 1)
7 -> 0
8, 9, 10, 11 -> u32 with the LBA

....
....

// last track datas (skiped in rawseciso)

0,
0x14,
0xAA,
0,

Ejection / insertion test (to change the disc):

if psxseciso_args->device == 0
test if virtual directory "/psx_cdrom0" is present

virtual disc file names (use this name when psxseciso_args->discs_desc[x][0]==0):

"/psx_d0"
"/psx_d1"
"/psx_d2"
"/psx_d3"
"/psx_d4"
"/psx_d5"
"/psx_d6"
"/psx_d7"

when psxseciso_args->discs_desc[x][0]!=0 it describe a list of sectors/numbers sectors arrays

discs_desc[x][0] = (parts << 16) | offset: parts number of entries, offset: in u32 units from the end of this packet
discs_desc[x][1] = toc_filesize (in LBA!. Used for files/sectors arrays isos to know the toc)

sector array sample:

u32 *datas = &psxseciso_args->discs_desc[8][0]; // datas starts here

u32 *sectors_array1 = datas + offset;
u32 *nsectors_array1 = datas + offset + parts;

offset += parts * 2; // to the next disc sector array datas...

*/

typedef struct
{
	u64 device;
	u32 emu_mode;		 //   16 bits	16 bits	32 bits
	u32 discs_desc[8][2]; //	parts  |   offset | toc_filesize   -> if parts == 0  use files. Offset is in u32 units from the end of this packet. For 8 discs
	// sector array 1
	// numbers sector array 1
	//...
	// sector array 8
	// numbers sector array 8
} __attribute__((packed)) psxseciso_args;

volatile int eject_running = 1;

u32 real_disctype;
ScsiTrackDescriptor tracks[MAX_TRACKS];
u32 emu_mode, num_tracks;
sys_event_port_t result_port;

// mode_file: 0 = ISO raw sectors, 1 = ISO file(s), 2 (>1) = fake ISO (ISO raw sector[0] + file sectors)
static int mode_file = 0, cd_sector_size_param = 0;

#define IS_RAW_MODE		(mode_file == 0)
#define IS_FILEMODE		(mode_file == 1)
#define IS_FAKE_ISO		(mode_file  > 1)
#define NOT_FILEMODE	(mode_file != 1)

#define CD_SECTOR_SIZE_2048			2048

static u64 sec_size = 512;
static u32 CD_SECTOR_SIZE_2352 = 2352;

static sys_device_info_t disc_info;

u64 usb_device = 0;

static sys_ppu_thread_t thread_id = SYS_PPU_THREAD_NONE, thread_id2 = SYS_PPU_THREAD_NONE;
static int ntfs_running = 1;

static volatile int do_run = 0;

sys_device_handle_t handle = SYS_DEVICE_HANDLE_NONE;
sys_event_queue_t command_queue = SYS_EVENT_QUEUE_NONE;

static u32 *sections, *sections_size;
static u32 num_sections;

static u64 discsize = 0;
static u8 *cd_cache = 0;
static int is_cd2352 = 0; // 0 = BD/DVD, 1 = PSX CD, 2 = PSX Multi CD


int rawseciso_start(u64 arg);
int rawseciso_stop(void);

static inline u64 lv2peek(u64 lv2addr)
{
	system_call_1(6, lv2addr);
	return (u64)p1;
}

static inline int sysUsleep(u32 useconds)
{
	system_call_1(141, useconds);
	return (int)p1;
}

static inline int sys_shutdown(void)
{
	system_call_4(379, 0x1100, 0, 0, 0);
	return (int)p1;
}

static inline void _sys_ppu_thread_exit(u64 val)
{
	system_call_1(41, val);
}

static inline sys_prx_id_t prx_get_module_id_by_address(void *addr)
{
	system_call_1(461, (u64)(u32)addr);
	return (int)p1;
}

static int fake_eject_event(void)
{
	sys_storage_ext_fake_storage_event(4, 0, BDVD_DRIVE);
	return sys_storage_ext_fake_storage_event(8, 0, BDVD_DRIVE);
}

static int fake_insert_event(u64 disctype)
{
	u64 param = (u64)(disctype) << 32ULL;
	sys_storage_ext_fake_storage_event(7, 0, BDVD_DRIVE);
	return sys_storage_ext_fake_storage_event(3, param, BDVD_DRIVE);
}

static int sys_set_leds(u64 color, u64 state)
{
	system_call_2(386,  (u64) color, (u64) state);
	return (int)p1;
}

static u32 default_cd_sector_size(size_t discsize)
{
	if(!(discsize % 0x930)) return 2352;
	if(!(discsize % 0x920)) return 2336;
	if(!(discsize % 0x940)) return 2368; // not supported by Cobra
	if(!(discsize % 0x990)) return 2448;
	if(!(discsize % 0x800)) return 2048;

	return 2352;
}

#define PLAYSTATION      "PLAYSTATION "

static u32 detect_cd_sector_size(char *buffer)
{
	u16 sec_size[7] = {2352, 2048, 2336, 2448, 2328, 2340, 2368};
	for(u8 n = 0; n < 7; n++)
	{
		if( (!strncmp(buffer + ((sec_size[n]<<4) + 0x20), PLAYSTATION, 0xC)) ||
			(!strncmp(buffer + ((sec_size[n]<<4) + 0x19), "CD001", 5) && buffer[(sec_size[n]<<4) + 0x18] == 0x01) ) return sec_size[n];
	}

	return 2352;
}

static void memcpy64(void *dst, void *src, int n)
{
	uint8_t p = n & 7;

	n >>= 3;
	uint64_t *d = (uint64_t *) dst;
	uint64_t *s = (uint64_t *) src;
	while (n--) *d++ = *s++;

	if(p)
	{
		char *m = (char *) d;
		char *c = (char *) s;
		while (p--) *m++ = *c++;
	}
}

static void get_next_read(u64 discoffset, u64 bufsize, u64 *offset, u64 *readsize, int *idx, u64 sec_size)
{
	u64 base = 0;
	*idx = NONE;
	*readsize = bufsize;
	*offset = 0;

	for(u32 i = 0; i < num_sections; i++)
	{
		u64 last;
		u64 sz;

		if((i == 0) && (IS_FAKE_ISO))
			sz = (((u64)sections_size[i]) * CD_SECTOR_SIZE_2048);
		else
			sz = (((u64)sections_size[i]) * sec_size);

		last = base + sz;

		if(discoffset >= base && discoffset < last)
		{
			u64 maxfileread = last-discoffset;

			if(bufsize > maxfileread)
				*readsize = maxfileread;
			else
				*readsize = bufsize;

			*idx = i;
			*offset = discoffset-base;
			return;
		}

		base += sz;
	}

	// We can be here on video blu-ray
	DPRINTF("Offset or size out of range  %lx%08lx   %lx!!!!!!!!\n", discoffset>>32, discoffset, bufsize);
}

u8 last_sect_buf[4096] __attribute__((aligned(16)));
u32 last_sect = READ_SECTOR;

static int process_read_iso_cmd(u8 *buf, u64 offset, u64 size)
{
	u64 remaining;
	int x;

	//DPRINTF("read iso: %p %lx %lx\n", buf, offset, size);
	remaining = size;

	while(remaining)
	{
		u64 pos, readsize;
		int idx;
		int ret;
		u32 sector;
		u32 r;

		if(!ntfs_running) return FAILED;

		get_next_read(offset, remaining, &pos, &readsize, &idx, sec_size);

		if((idx == NONE) || (sections[idx] == 0xFFFFFFFF))
		{
			memset(buf, 0, readsize);
			buf += readsize;
			offset += readsize;
			remaining -= readsize;
			continue;
		}

		if(pos % sec_size)
		{
			sector = sections[idx] + (pos / sec_size);
			for(x = 0; x < 16; x++)
			{
				r = 0;
				if(last_sect == sector)
				{
					ret = OK; r = LAST_SECTOR;
				}
				else
					ret = sys_storage_read(handle, 0, sector, 1, last_sect_buf, &r, 0);

				if((ret == OK) && (r == LAST_SECTOR))
					last_sect = sector;
				else
					last_sect = READ_SECTOR;

				if((ret != OK) || (r != LAST_SECTOR))
				{
					if(emu_mode == EMU_PSX_MULTI) return (int) 0x8001000A; // EBUSY

					if((ret == (int) 0x80010002) || (ret == (int) 0x8001002D))
					{
						if(handle != SYS_DEVICE_HANDLE_NONE) sys_storage_close(handle); handle = SYS_DEVICE_HANDLE_NONE;

						while(ntfs_running)
						{
							if(sys_storage_get_device_info(usb_device, &disc_info) == OK)
							{
								ret = sys_storage_open(usb_device, 0, &handle, 0);
								if(ret == OK) break;

								handle = SYS_DEVICE_HANDLE_NONE; sysUsleep(500000);
							}
							else sysUsleep(7000000);
						}
						x= -1; continue;
					}


					if(x == 15 || !ntfs_running)
					{
						DPRINTF("sys_storage_read failed: %x 1 -> %x\n", sector, ret);
						return FAILED;
					}
					else sysUsleep(100000);
				}
				else break;
			}

			u64 csize = sec_size - (pos % sec_size);

			if(csize > readsize) csize = readsize;

			memcpy64(buf, last_sect_buf + (pos % sec_size), csize);
			buf += csize;
			offset += csize;
			pos += csize;
			remaining -= csize;
			readsize -= csize;
		}

		if(readsize)
		{
			u32 n = readsize / sec_size;

			if(n)
			{
				sector = sections[idx] + (pos / sec_size);
				for(x = 0; x < 16; x++)
				{
					r = 0;
					ret = sys_storage_read(handle, 0, sector, n, buf, &r, 0);

					if((ret == OK) && (r == n))
						last_sect = sector + n - 1;
					else
						last_sect = READ_SECTOR;

					if((ret != OK) || (r != n))
					{
						if(emu_mode == EMU_PSX_MULTI) return (int) 0x8001000A; // EBUSY

						if((ret == (int) 0x80010002) || (ret == (int) 0x8001002D))
						{
							if(handle != SYS_DEVICE_HANDLE_NONE) sys_storage_close(handle); handle = SYS_DEVICE_HANDLE_NONE;

							while(ntfs_running)
							{
								if(sys_storage_get_device_info(usb_device, &disc_info) == OK)
								{
									ret = sys_storage_open(usb_device, 0, &handle, 0);
									if(ret == OK) break;

									handle = SYS_DEVICE_HANDLE_NONE; sysUsleep(500000);
								}
								else sysUsleep(7000000);
							}
							x= -1; continue;
						}

						if(x == 15 || !ntfs_running)
						{
							DPRINTF("sys_storage_read failed: %x %x -> %x\n", sector, n, ret);
							return FAILED;
						}
						else sysUsleep(100000);
					}
					else break;
				}

				u64 s = n * sec_size;

				buf += s;
				offset += s;
				pos += s;
				remaining -= s;
				readsize -= s;
			}

			if(readsize)
			{
				sector = sections[idx] + pos / sec_size;
				for(x = 0; x < 16; x++)
				{
					r = 0;
					if(last_sect == sector)
					{
						ret = OK; r = LAST_SECTOR;
					}
					else
						ret = sys_storage_read(handle, 0, sector, 1, last_sect_buf, &r, 0);

					if((ret == OK) && (r == LAST_SECTOR))
						last_sect = sector;
					else
						last_sect = READ_SECTOR;

					if((ret != OK) || (r != LAST_SECTOR))
					{
						if(emu_mode == EMU_PSX_MULTI) return (int) 0x8001000A; // EBUSY

						if((ret == (int) 0x80010002) || (ret == (int) 0x8001002D))
						{
							if(handle != SYS_DEVICE_HANDLE_NONE) sys_storage_close(handle); handle = SYS_DEVICE_HANDLE_NONE;

							while(ntfs_running)
							{
								if(sys_storage_get_device_info(usb_device, &disc_info) == OK)
								{
									ret = sys_storage_open(usb_device, 0, &handle, 0);
									if(ret == OK) break;

									handle = SYS_DEVICE_HANDLE_NONE; sysUsleep(500000);
								}
								else sysUsleep(3000000);
							}
							x= -1; continue;
						}

						if(x == 15 || !ntfs_running)
						{
							DPRINTF("sys_storage_read failed: %x 1 -> %x\n", sector, ret);
							return FAILED;
						}
						else sysUsleep(100000);
					}
					else break;
				}

				memcpy64(buf, last_sect_buf, readsize);
				buf += readsize;
				offset += readsize;
				remaining -= readsize;
			}
		}
	}

	return OK;
}

static int last_index = NONE;
static int discfd = NONE;

static int process_read_file_cmd(u8 *buf, u64 offset, u64 size)
{
	u64 remaining;

	char *path_name = (char *) sections;

	if(IS_FAKE_ISO)
	{
		last_index = NONE;
		path_name-= 0x200;
	}

	remaining = size;

	while(remaining)
	{
		u64 pos, p, readsize;
		int idx;
		int ret;

		get_next_read(offset, remaining, &pos, &readsize, &idx, 2048);

		if((idx == NONE) || (path_name[0x200 * idx] == 0))
		{
			memset(buf, 0, readsize);
			buf += readsize;
			offset += readsize;
			remaining -= readsize;
			continue;
		}
		else
		{
			if(idx != last_index)
			{
				if(discfd != NONE) cellFsClose(discfd);

				while(ntfs_running)
				{
					ret = cellFsOpen(&path_name[0x200 * idx], CELL_FS_O_RDONLY, &discfd, NULL, 0);
					if(ret == OK) break;

					discfd = NONE;
					sysUsleep(5000000);
				}

				last_index = idx;
			}

			p = 0;
			ret = cellFsReadWithOffset(discfd, pos, buf, readsize, &p);
			if(ret)
			{
				last_index = NONE;

				continue;
			}
			else if(readsize != p)
			{
				if((offset + readsize) < discsize) return FAILED;
			}

			buf += readsize;
			offset += readsize;
			remaining -= readsize;
		}
	}

	return OK;
}

int psx_indx = 0;

u32 *psx_isos_desc;

char psx_filename[8][8]= {
	"/psx_d0",
	"/psx_d1",
	"/psx_d2",
	"/psx_d3",
	"/psx_d4",
	"/psx_d5",
	"/psx_d6",
	"/psx_d7"
};

static int process_read_psx_cmd(u8 *buf, u64 offset, u64 size, u32 ssector)
{
	u64 remaining, sect_size;
	int ret, rel;

	u32 lba = offset + 150, lba2;

	sect_size = lv2peek(0x80000000000000D0ULL);
	offset *= sect_size;
	//size   *= sector;

	remaining = size;

	if(!psx_isos_desc[psx_indx * 2])
	{
		if(discfd == NONE)
		{
			while(ntfs_running)
			{
				ret = cellFsOpen(&psx_filename[psx_indx][0], CELL_FS_O_RDONLY, &discfd, NULL, 0);
				if(ret == OK) break;

				discfd = NONE;
				sysUsleep(5000000);
			}
		}
	}

	while(remaining)
	{
		u64 pos, p, readsize = (u64)CD_SECTOR_SIZE_2352;


			p = 0;
			rel = 0;

			pos = offset;
			if(ssector == CD_SECTOR_SIZE_2048) { if(sect_size >= CD_SECTOR_SIZE_2352) pos += 24ULL; readsize = CD_SECTOR_SIZE_2048;}
			else
			{
				if(sect_size == CD_SECTOR_SIZE_2048)
				{
					rel = 24; readsize = CD_SECTOR_SIZE_2048;
					memset(&buf[0], 0x0, 24);
					memset(&buf[1], 0xff, 10);
					buf[0x12] = buf[0x16]= 8;
					buf[0xf] = 2;

					buf[0xe] = lba % 75;

					lba2 = lba / 75;
					buf[0xd] = lba2 % 60;
					lba2 /= 60;
					buf[0xc] = lba2;
				}
			}

			if(!psx_isos_desc[psx_indx * 2])
			{
				ret = cellFsReadWithOffset(discfd, pos, buf + rel, readsize, &p);
				if(ret)
				{
					if(ret == (int) 0x8001002B) return (int) 0x8001000A; // EBUSY

					return OK;
				}
				else if(readsize != p)
				{
					if((offset + readsize) < discsize) return OK;
				}
			}
			else
			{
				ret = process_read_iso_cmd(buf + rel, pos, readsize);
				if(ret) return ret;
			}


			buf += ssector;
			offset += sect_size;
			remaining --;
			lba++;
	}

	return OK;
}

static inline void lv2_memcpy64(u64 *dst, u64 src, int size)
{
	for(int n = 0, bytes = 0; bytes < size; bytes += 8, n++) dst[n] = lv2peek(src + bytes);
}

static u32 cached_cd_sector = 0x80000000;

static int process_read_cd_2048_cmd(u8 *buf, u32 start_sector, u32 sector_count)
{
	u32 capacity = sector_count * CD_SECTOR_SIZE_2048;
	u32 fit = capacity / CD_SECTOR_SIZE_2352;
	u32 rem = (sector_count-fit);
	u32 i;
	u8 *in = buf;
	u8 *out = buf;

	if(fit)
	{
		process_read_iso_cmd(buf, start_sector * CD_SECTOR_SIZE_2352, fit * CD_SECTOR_SIZE_2352);

		for(i = 0; i < fit; i++)
		{
			memcpy64(out, in + 24, CD_SECTOR_SIZE_2048);
			in += CD_SECTOR_SIZE_2352;
			out += CD_SECTOR_SIZE_2048;
			start_sector++;
		}
	}

	for(i = 0; i < rem; i++)
	{
		process_read_iso_cmd(out, (start_sector * CD_SECTOR_SIZE_2352) + 24, CD_SECTOR_SIZE_2048);
		out += CD_SECTOR_SIZE_2048;
		start_sector++;
	}

	return OK;
}

static int process_read_cd_2352_cmd(u8 *buf, u32 sector, u32 remaining)
{
	if(remaining <= CD_CACHE_SIZE)
	{
		int dif = (int)cached_cd_sector - sector;

		if(ABS(dif) < CD_CACHE_SIZE)
		{
			u8 *copy_ptr = NULL;
			u32 copy_offset = 0;
			u32 copy_size = 0;

			if(dif > 0)
			{
				if(dif < (int)remaining)
				{
					copy_ptr = cd_cache;
					copy_offset = dif;
					copy_size = remaining - dif;
				}
			}
			else
			{

				copy_ptr = cd_cache + ((-dif) * CD_SECTOR_SIZE_2352);
				copy_size = MIN((int)remaining, CD_CACHE_SIZE + dif);
			}

			if(copy_ptr)
			{
				memcpy64(buf + (copy_offset * CD_SECTOR_SIZE_2352), copy_ptr, copy_size * CD_SECTOR_SIZE_2352);

				if(remaining == copy_size)
				{
					return OK;
				}

				remaining -= copy_size;

				if(dif <= 0)
				{
					u32 newsector = cached_cd_sector + CD_CACHE_SIZE;
					buf += ((newsector - sector) * CD_SECTOR_SIZE_2352);
					sector = newsector;
				}
			}
		}
	}
	else
	{
		return process_read_iso_cmd(buf, sector * CD_SECTOR_SIZE_2352, remaining * CD_SECTOR_SIZE_2352);
	}

	if(!cd_cache)
	{
		sys_addr_t addr;

		int ret = sys_memory_allocate(_128KB_, SYS_MEMORY_PAGE_SIZE_64K, &addr);
		if(ret)
		{
			DPRINTF("sys_memory_allocate failed: %x\n", ret);
			return ret;
		}

		cd_cache = (u8 *)addr;
	}

	if(process_read_iso_cmd(cd_cache, sector * CD_SECTOR_SIZE_2352, CD_CACHE_SIZE * CD_SECTOR_SIZE_2352) /* != CELL_OK*/)
		return FAILED;

	memcpy64(buf, cd_cache, remaining * CD_SECTOR_SIZE_2352);
	cached_cd_sector = sector;
	return OK;
}

static void get_psx_track_data(void)
{
	u32 track_data[4];
	u8 buff[CD_SECTOR_SIZE_2048];
	u64 lv2_addr = 0x8000000000000050ULL;
	lv2_addr += psx_indx * 0x10;

	lv2_memcpy64((u64*)(u32)track_data, lv2_addr, 0x10);

	int k = 4;
	num_tracks = 0;

	if(!track_data[0])
	{
		tracks[num_tracks].adr_control = 0x14;
		tracks[num_tracks].track_number = 1;
		tracks[num_tracks].track_start_addr = 0;
		num_tracks++;
	}
	else
	{
		lv2_addr = 0x8000000000000000ULL | (u64) track_data[2];
		lv2_memcpy64((u64*)buff, lv2_addr, track_data[3]);

		while(k < (int) track_data[3])
		{
			tracks[num_tracks].adr_control = (buff[k + 1] != 0x14) ? 0x10 : 0x14;
			tracks[num_tracks].track_number = num_tracks + 1;
			tracks[num_tracks].track_start_addr = ((u32) buff[k + 4] << 24) |
												  ((u32) buff[k + 5] << 16) |
												  ((u32) buff[k + 6] << 8)  |
												  ((u32) buff[k + 7]);
			num_tracks++;
			k += 8;
		}

		if(num_tracks > 1) num_tracks--;
	}

	discsize = ((u64) psx_isos_desc[psx_indx * 2 + 1]) * (u64)CD_SECTOR_SIZE_2352;

	num_sections = (psx_isos_desc[psx_indx * 2] >> 16) & 0xffff;
	sections = &psx_isos_desc[16] + (psx_isos_desc[psx_indx * 2] & 0xffff);
	sections_size = sections + num_sections;
}

static int ejected_disc(void)
{
	static int ejected = 0;
	static int counter = 0;
	int fd = NONE;

	if(usb_device)
	{
		int r = sys_storage_get_device_info(usb_device, &disc_info);
		if(r == OK)
		{
			counter++;

			if(ejected && counter > 100)
			{
				if(handle != SYS_DEVICE_HANDLE_NONE) sys_storage_close(handle); handle = SYS_DEVICE_HANDLE_NONE;

				if(sys_storage_open(usb_device, 0, &handle, 0) == OK)
				{
					sys_set_leds(1 , 1);
					ejected = 0;
					counter = 0;
					return 1;

				}
				else
				{
					handle = SYS_DEVICE_HANDLE_NONE;
					return FAILED;
				}
			}
			else
				return 2;
		}
		else if(r == (int) 0x80010002)
		{
			sys_set_leds(1 , 2);

			if(!ejected)
			{
				counter = 0;

				ejected = 1;
				return OK;
			}
			else
				return FAILED;
		}
		else
		{
			ejected = 1;
			return -128;
		}
	}


	if(cellFsOpendir("/psx_cdrom0", &fd)==0)
	{
		cellFsClosedir(fd);
		if(ejected)
		{
			sys_set_leds(1 , 1);
			ejected = 0;
			return 1;
		}
		else
			return 2;

	}
	else
	{
		sys_set_leds(1 , 2);

		if(!ejected)
		{
			ejected = 1;
			return OK;
		}
		else
			return FAILED;
	}

	return -2;
}

static void eject_thread(u64 arg)
{
	while(eject_running)
	{
		int ret;

		if(emu_mode == EMU_PSX_MULTI)
		{
			ret = ejected_disc();

			if(ret == OK)
			{
				sys_storage_ext_get_disc_type(&real_disctype, NULL, NULL);
				fake_eject_event();
				sys_storage_ext_umount_discfile();

				if(real_disctype != DISC_TYPE_NONE)
				{
					fake_insert_event(real_disctype);
				}

				if(discfd != NONE) cellFsClose(discfd); discfd = NONE;
				if(handle != SYS_DEVICE_HANDLE_NONE) sys_storage_close(handle); handle = SYS_DEVICE_HANDLE_NONE;

				if(command_queue != SYS_EVENT_QUEUE_NONE)
				{
					eject_running = 2;
					sys_event_queue_t command_queue2 = command_queue;
					command_queue = SYS_EVENT_QUEUE_NONE;

					if(sys_event_queue_destroy(command_queue2, SYS_EVENT_QUEUE_DESTROY_FORCE) /*!= CELL_OK*/)
					{
						DPRINTF("Failed in destroying command_queue\n");
					}
				}



				while(do_run) sysUsleep(100000);

				psx_indx = (psx_indx + 1) & 7;

			}
			else if(ret == 1)
			{
				get_psx_track_data();

				sys_storage_ext_get_disc_type(&real_disctype, NULL, NULL);

				if(real_disctype != DISC_TYPE_NONE)
				{
					fake_eject_event();
				}

				sys_event_queue_attribute_t queue_attr;
				sys_event_queue_attribute_initialize(queue_attr);
				ret = sys_event_queue_create(&command_queue, &queue_attr, 0, 1);
				if(ret == OK) {eject_running = 1; ret = sys_storage_ext_mount_discfile_proxy(result_port, command_queue, emu_mode & 0xF, discsize, 256*1024, (num_tracks | cd_sector_size_param), tracks);}

				if(ret) psx_indx = (psx_indx - 1) & 7;
				else {fake_insert_event(real_disctype);}
			}
		}


		sysUsleep(70000);
	}

	DPRINTF("Exiting eject thread!\n");
	sys_ppu_thread_exit(0);
}

static void rawseciso_thread(u64 arg)
{
	rawseciso_args *args;
	psxseciso_args *psx_args;

	sys_event_queue_attribute_t queue_attr;

	int ret; cd_sector_size_param = 0;

	args = (rawseciso_args *)(u32)arg;
	psx_args = (psxseciso_args *)(u32)arg;

	DPRINTF("Hello VSH\n");

	num_sections = 0;
	CD_SECTOR_SIZE_2352 = 2352;

	emu_mode = args->emu_mode & 0x3FF;

	if(emu_mode != EMU_PSX_MULTI)
	{
		num_sections = args->num_sections;
		sections = (u32 *)(args + 1);

		mode_file = (args->emu_mode & 3072) >> 10;

		if(IS_RAW_MODE)
			sections_size = sections + num_sections;
		else if(IS_FILEMODE)
			sections_size = sections + num_sections * 0x200/4;
		else // if(IS_FAKE_ISO)
		{
			sections += 0x200/4;
			sections_size = sections + num_sections;
		}

		discsize = 0;

		for(u32 i = (IS_FAKE_ISO); i < num_sections; i++)
		{
			discsize += sections_size[i];
		}
	}
	else
		mode_file = 0; // RAW_MODE

	if(NOT_FILEMODE)
	{
		sec_size = 512ULL;
		if(args->device)
		{
			for(int retry = 0; retry < 16; retry++)
			{
				if(sys_storage_get_device_info(args->device, &disc_info) == OK)
				{
					sec_size  = (u32) disc_info.sector_size;
					break;
				}

				sysUsleep(500000);
			}
		}

	}
	else
		sec_size = CD_SECTOR_SIZE_2048;

	if(NOT_FILEMODE)
	{
		usb_device = args->device;

		if(usb_device)
		{
			ret = sys_storage_open(usb_device, 0, &handle, 0);
			if(ret)
			{
				DPRINTF("sys_storage_open : %x\n", ret);
				sys_memory_free((sys_addr_t)args);
				sys_ppu_thread_exit(ret);
			}
		}
	}

	if(emu_mode == EMU_PSX_MULTI)
	{
		psx_isos_desc = &psx_args->discs_desc[0][0];

		is_cd2352 = 2;
	}
	else if(emu_mode == EMU_PSX)
	{   // old psx support
		num_tracks = args->num_tracks;

		if(num_tracks > 0xFF)
		{
			CD_SECTOR_SIZE_2352 = (num_tracks & 0xffff00)>>4;
		}

		num_tracks &= 0xff; if(num_tracks > MAX_TRACKS) num_tracks = MAX_TRACKS;

		if(num_tracks)
			memcpy64((void *) tracks, (void *) ((ScsiTrackDescriptor *)(sections_size + num_sections)), num_tracks * sizeof(ScsiTrackDescriptor));
		else
		{
			tracks[num_tracks].adr_control = 0x14;
			tracks[num_tracks].track_number = 1;
			tracks[num_tracks].track_start_addr = 0;
			num_tracks++;
		}

		is_cd2352 = 1;

		discsize *= sec_size;
		CD_SECTOR_SIZE_2352 = default_cd_sector_size(discsize);

		sys_addr_t addr_cache;

		if(sys_memory_allocate(_64KB_, SYS_MEMORY_PAGE_SIZE_64K, &addr_cache) == CELL_OK)
		{
			cd_cache = (u8 *)addr_cache;

			if(process_read_iso_cmd(cd_cache, 0, _40KB_) == CELL_OK)
			{
				CD_SECTOR_SIZE_2352 = detect_cd_sector_size((char*)cd_cache);
			}

			sys_memory_free(addr_cache); cd_cache = 0;
		}

		if(discsize % CD_SECTOR_SIZE_2352)
		{
			discsize -= (discsize % CD_SECTOR_SIZE_2352);
		}

		//if(CD_SECTOR_SIZE_2352 != 2368 && CD_SECTOR_SIZE_2352 != 2048 && CD_SECTOR_SIZE_2352 != 2336 && CD_SECTOR_SIZE_2352 != 2448) CD_SECTOR_SIZE_2352 = 2352;
		if(CD_SECTOR_SIZE_2352 & 0xf)
			cd_sector_size_param = CD_SECTOR_SIZE_2352<<8;
		else if(CD_SECTOR_SIZE_2352 != 2352)
			cd_sector_size_param = CD_SECTOR_SIZE_2352<<4;
	}
	else
	{
		num_tracks = 0;
		discsize *= sec_size;
		if(IS_FAKE_ISO) discsize += (u64) (CD_SECTOR_SIZE_2048 * sections[0]);

		is_cd2352 = 0;
	}


	DPRINTF("discsize = %lx%08lx\n", discsize>>32, discsize);

	ret = sys_event_port_create(&result_port, 1, SYS_EVENT_PORT_NO_NAME);
	if(ret)
	{
		DPRINTF("sys_event_port_create failed: %x\n", ret);
		if(handle != SYS_DEVICE_HANDLE_NONE) sys_storage_close(handle);
		sys_memory_free((sys_addr_t)args);
		sys_ppu_thread_exit(ret);
	}

	sys_event_queue_attribute_initialize(queue_attr);
	ret = sys_event_queue_create(&command_queue, &queue_attr, 0, 1);
	if(ret)
	{
		DPRINTF("sys_event_queue_create failed: %x\n", ret);
		sys_event_port_destroy(result_port);
		if(handle != SYS_DEVICE_HANDLE_NONE) sys_storage_close(handle);
		sys_memory_free((sys_addr_t)args);
		sys_ppu_thread_exit(ret);
	}

	sys_storage_ext_get_disc_type(&real_disctype, NULL, NULL);

	if(real_disctype != DISC_TYPE_NONE)
	{
		fake_eject_event();
	}

	if(emu_mode == EMU_PSX_MULTI)
	{
		get_psx_track_data();

		sys_ppu_thread_create(&thread_id2, eject_thread, 0, 3000, _8KB_, SYS_PPU_THREAD_CREATE_JOINABLE, THREAD_NAME"_eject");
	}

	ret = sys_storage_ext_mount_discfile_proxy(result_port, command_queue, emu_mode & 0xF, discsize, _256KB_, (num_tracks | cd_sector_size_param), tracks);
	DPRINTF("mount = %x\n", ret);

	fake_insert_event(real_disctype);

	if(ret)
	{
		sys_event_port_disconnect(result_port);
		// Queue destroyed in stop thread sys_event_queue_destroy(command_queue);
		sys_event_port_destroy(result_port);
		if(handle != SYS_DEVICE_HANDLE_NONE) sys_storage_close(handle);
		sys_memory_free((sys_addr_t)args);
		sys_ppu_thread_exit(0);
	}

	while(ntfs_running)
	{
		sys_event_t event;

		do_run = 0;

		if(command_queue == SYS_EVENT_QUEUE_NONE)
		{
			if(!ntfs_running) break;

			sysUsleep(100000);

			continue;
		}

		ret = sys_event_queue_receive(command_queue, &event, 0);
		if(ret)
		{
			if((command_queue == SYS_EVENT_QUEUE_NONE || eject_running == 2) && (ret != OK))
			{
				if(!ntfs_running) break;

				sysUsleep(100000);

				continue;
			}
			if(ret != (int) 0x80010013) sys_shutdown();
			//DPRINTF("sys_event_queue_receive failed: %x\n", ret);
			break;
		}

		if(!ntfs_running) break;

		do_run = 1;

		void *buf = (void *)(u32)(event.data3>>32ULL);
		u64 offset = event.data2;
		u32 size = event.data3&0xFFFFFFFF;

		switch (event.data1)
		{
			case CMD_READ_ISO:
			{
				if(is_cd2352)
				{
					if(is_cd2352 == 1)
						ret = process_read_cd_2048_cmd(buf, offset / CD_SECTOR_SIZE_2048, size / CD_SECTOR_SIZE_2048);
					else
						ret = process_read_psx_cmd(buf, offset / CD_SECTOR_SIZE_2048, size / CD_SECTOR_SIZE_2048, CD_SECTOR_SIZE_2048);
				}
				else
				{
					if(IS_RAW_MODE)
						ret = process_read_iso_cmd(buf, offset, size);
					else if(IS_FILEMODE)
						ret = process_read_file_cmd(buf, offset, size);
					else // if(IS_FAKE_ISO)
					{
						if(offset < (((u64)sections_size[0]) * CD_SECTOR_SIZE_2048))
						{
							u32 rd = sections_size[0] * CD_SECTOR_SIZE_2048;

							if(rd > size) rd = size;
							ret = process_read_file_cmd(buf, offset, rd);
							if(ret == OK)
							{
								size-= rd;
								offset+= rd;
								buf = ((char *) buf) + rd;

								if(size)
									ret = process_read_iso_cmd(buf, offset, size);
							}
						}
						else
							ret = process_read_iso_cmd(buf, offset, size);

						if(discfd != NONE) cellFsClose(discfd); discfd = NONE;
					}
				}
			}
			break;

			case CMD_READ_CD_ISO_2352:
			{
				if(is_cd2352 == 1)
					ret = process_read_cd_2352_cmd(buf, offset/CD_SECTOR_SIZE_2352, size/CD_SECTOR_SIZE_2352);
				else
					ret = process_read_psx_cmd(buf, offset/CD_SECTOR_SIZE_2352, size/CD_SECTOR_SIZE_2352, CD_SECTOR_SIZE_2352);
			}
			break;
		}

		while(ntfs_running)
		{
			ret = sys_event_port_send(result_port, ret, 0, 0);
			if(ret == OK) break;

			if(ret == (int) 0x8001000A)
			{	   // EBUSY
					sysUsleep(100000);
					continue;
			}

			DPRINTF("sys_event_port_send failed: %x\n", ret);
			break;
		}

		if(ret) break;
	}

	do_run = 0;

	eject_running = 0;

	sys_memory_free((sys_addr_t)args);

	sys_storage_ext_get_disc_type(&real_disctype, NULL, NULL);
	fake_eject_event();
	sys_storage_ext_umount_discfile();

	if(real_disctype != DISC_TYPE_NONE)
	{
		fake_insert_event(real_disctype);
	}

	if(cd_cache)
	{
		sys_memory_free((sys_addr_t)cd_cache);
	}

	if(handle != SYS_DEVICE_HANDLE_NONE) sys_storage_close(handle);

	if(discfd != NONE) cellFsClose(discfd);

	sys_event_port_disconnect(result_port);
	if(sys_event_port_destroy(result_port) != CELL_OK)
	{
		DPRINTF("Error destroyng result_port\n");
	}

	// queue destroyed in stop thread

	DPRINTF("Exiting main thread!\n");
	sys_ppu_thread_exit(0);
}

int rawseciso_start(u64 arg)
{
	if(arg)
	{
		u64 *argp = (u64*)(u32)arg;
		sys_addr_t sysmem;

		if(sys_memory_allocate(_64KB_, SYS_MEMORY_PAGE_SIZE_64K, &sysmem) == CELL_OK)
		{
			memcpy64((void *)sysmem, argp, _64KB_);
			sys_ppu_thread_create(&thread_id, rawseciso_thread, (u64)sysmem, -0x1d8, _8KB_, SYS_PPU_THREAD_CREATE_JOINABLE, THREAD_NAME);
		}
	}
	// Exit thread using directly the syscall and not the user mode library or we will crash
	_sys_ppu_thread_exit(0);
	return SYS_PRX_RESIDENT;
}

static void rawseciso_stop_thread(u64 arg)
{
	u64 exit_code;

	DPRINTF("rawseciso_stop_thread\n");

	ntfs_running = do_run = 0;

	if(command_queue != SYS_EVENT_QUEUE_NONE)
	{
		if(sys_event_queue_destroy(command_queue, SYS_EVENT_QUEUE_DESTROY_FORCE) != CELL_OK)
		{
			DPRINTF("Failed in destroying command_queue\n");
		}
	}

	if(thread_id != SYS_PPU_THREAD_NONE)
	{
		sys_ppu_thread_join(thread_id, &exit_code);
	}

	eject_running = 0;

	if(thread_id2 != SYS_PPU_THREAD_NONE)
	{
		sys_ppu_thread_join(thread_id2, &exit_code);
	}

	DPRINTF("Exiting stop thread!\n");
	sys_ppu_thread_exit(0);
}

static void finalize_module(void)
{
	u64 meminfo[5];

	sys_prx_id_t prx = prx_get_module_id_by_address(finalize_module);

	meminfo[0] = 0x28;
	meminfo[1] = 2;
	meminfo[3] = 0;

	system_call_3(SC_STOP_PRX_MODULE, prx, 0, (u64)(u32)meminfo);
}

int rawseciso_stop(void)
{
	sys_ppu_thread_t t;
	u64 exit_code;

	sys_ppu_thread_create(&t, rawseciso_stop_thread, 0, 0, _8KB_, SYS_PPU_THREAD_CREATE_JOINABLE, STOP_THREAD_NAME);
	sys_ppu_thread_join(t, &exit_code);

	finalize_module();
	_sys_ppu_thread_exit(0);
	return SYS_PRX_STOP_OK;
}
