// #include <sys/return_code.h>
#include <sys/timer.h>
#include <sys/memory.h>

#include <cell/cell_fs.h>
// #include <cell/usbd.h>

// #include <stdio.h>
// #include <string.h>
// #include <cell/hash/libmd5.h>

#define COBRA_ONLY
#define _COBRA_C

#include "cobra.h"
#include "storage_ext.h"
#include "psp.h"
// #include "syscall8.h"
// #include "scsi.h"
// #include "ufi.h"
// #include "base_mds.h"

#define TYPE_HOST2DEV (USB_REQTYPE_DIR_TO_DEVICE|USB_REQTYPE_TYPE_VENDOR)
#define TYPE_DEV2HOST (USB_REQTYPE_DIR_TO_HOST|USB_REQTYPE_TYPE_VENDOR)

#define FAILED		-1

#define SWAP32(x) ((((x) & 0xff) << 24) | (((x) & 0xff00) << 8) | (((x) & 0xff0000) >> 8) | (((x) >> 24) & 0xff))
#define RANGE(a, b, c)	((a) <= (b) ? (b) : (a) >= (c) ? (c) : (a))

#define KB			   1024
#define  _16KB_		  16384UL
#define _128KB_		 131072UL

#define PSPL_PATH1		"/dev_hdd0//game/PSPM66820"
#define PSPL_ICON1		PSPL_PATH1 "/ICON0.PNG"
#define PSPL_PATH2		"/dev_hdd0//game/PSPC66820"
#define PSPL_ICON2		PSPL_PATH2 "/ICON0.PNG"

//#define PSPL_LAMBDA		PSPL_PATH1 "/USRDIR/CONTENT/lambda.db"
//#define PSPL_LAMBDA_NONCE 	0x0ab40b3bbd1f1a7bULL

#define DPRINTF(...)

enum
{
	// Reads the flash rawly. Used by PC flasher.
	CMD_SPI_FLASH_READ = 0x10,
	// Reads flash and decrypts with keys 2. Deprecated since version 3.0.
	CMD_SPI_FLASH_READ_AND_DECRYPT,
	// Programs a page. Used by PC flasher.
	CMD_SPI_FLASH_PAGE_PROGRAM,
	// Decrypts buffer with key 0 and programs to flash. Used by PC flasher.
	CMD_SPI_FLASH_DECRYPT_AND_PAGE_PROGRAM,
	// Erase a sector. Used by PC flasher
	CMD_SPI_FLASH_ERASE_SECTOR,
	// Erases the chip.
	CMD_SPI_FLASH_CHIP_ERASE,
	// Read the scp flashrom. Used by PC flasher, but its value is not used currenttly.
	CMD_SCP_FLASHROM_READ,
	// Sets buffer for crypt operation. There is one special mode which should be forbidden if not in PS3 mode.
	CMD_SCP_SET_BUFFER,
	// Starts a decryption/encryption operation. Encryption operations are forbidden on preprogrammed keys. Used by PC flasher (key 1 and 3 decryption).
	CMD_SCP_CRYPT,
	// Starts a handshake operation.
	CMD_SCP_HANDSHAKE,
	// Sets user key for encryption/decryption
	CMD_SCP_SET_USER_KEY,
	// Unused scp jtag opcodes
	CMD_SCP_SET_JTAG, /* UNUSED */
	CMD_SCP_READ_TDO, /* UNUSED */
	// Decrypts with key 2 and writes to eeprom. Used by PC flasher.
	CMD_MCU_EEPROM_DECRYPT_AND_WRITE,
	// Reboots MCU. Used by PC flasher, but no current firmware has used this opcode.
	CMD_MCU_REBOOT,
	// Starts bootloader. Used by PC flasher.
	CMD_MCU_START_BOOTLOADER,
	// Reads flash and decrypts with keys 1. Scurity panic if used and not ps3 mode.
	CMD_SPI_FLASH_READ_AND_DECRYPT2,
	// LEDs control. Added in firmware 3.0
	CMD_LED_CONTROL,
	// PS3 security. Added in firmware 3.0. IT MUST ONLY BE USED by PS3.
	CMD_PS3_SECURITY_IN,
	CMD_PS3_SECURITY_OUT,
	// Sets ps3 mode
	CMD_PS3_SET,
	// Validates a PS3 encoded psid
	CMD_PS3_VALIDATE,
	// Hashes flash
	CMD_SPI_FLASH_HASH,
	// Set Hash size
	CMD_SPI_FLASH_SET_HASH_SIZE
};
/*
typedef struct
{
	char key[64];
	int value;
} KeyValue;

typedef struct
{
	char server[0x40];
	char path[0x420];
	u32 emu_mode;
	u32 numtracks;
	u16 port;
	u8 pad[6];
	ScsiTrackDescriptor tracks[32];
} __attribute__((packed)) netiso_args;
*/

void _memset(void *m, size_t n);

size_t read_file(const char *file, char *data, const size_t size, s32 offset);
int save_file(const char *file, const char *mem, s64 size);

int file_copy(const char *file1, char *file2);
int wait_for(const char *path, u8 timeout);

#define not_exists(path)	(!file_exists(path))

static u8 file_exists(const char *path)
{
	CellFsStat stat;
	return (cellFsStat(path, &stat) == CELL_FS_SUCCEEDED);
}

/*
#define N_TITLE_IDS	102
#define N_TITLE_NAMES	24

static KeyValue emu_by_title_ids[N_TITLE_IDS] =
{
	// First, games really fixed //
	// Lunar silver star
	{ "NPJH-90073", EMU_400 },
	{ "NPUH-90053", EMU_400 },
	{ "ULUS-10482", EMU_400 },
	{ "NPEH-00076", EMU_400 },
	{ "ULJM-05535", EMU_400 },
	{ "ULUS-10514", EMU_400 },
	// Tekken 6
	{ "ULES-01376", EMU_400 },
	{ "NPJH-50184", EMU_400 },
	{ "ULJS-00224", EMU_400 },
	{ "ULUS-10466", EMU_400 },
	{ "ULAS-42214", EMU_400 },
	{ "ULJS-19054", EMU_400 },
	// BlazBlue - Continuum Shift II
	{ "ULJM-05850", EMU_400 },
	{ "ULUS-10579", EMU_400 },
	// Exit
	{ "ULES-00285", EMU_400 },
	{ "ULUS-10074", EMU_400 },
	{ "ULJM-05062", EMU_400 },
	{ "ULJM-05271", EMU_400 },
	// DJ max portable 3
	{ "ULKS-46236", EMU_400 },
	{ "ULUS-10538", EMU_400 },
	{ "ULJM-05836", EMU_400 },
	{ "CF00-20046", EMU_400 },
	// Fate/Extra
	{ "ULUS-10576", EMU_400 },
	{ "NPJH-50247", EMU_400 },
	{ "ULJS-00254", EMU_400 },
	{ "ULJS-00253", EMU_400 },
	{ "ULUS-10588", EMU_400 },
	// 3rd birthday
	{ "ULES-01513", EMU_400 },
	{ "ULJM-05798", EMU_400 },
	{ "ULUS-10567", EMU_400 },
	// Lego pirates of the caribbean
	{ "ULES-01528", EMU_400 },
	{ "ULES-01529", EMU_400 },
	{ "ULUS-10575", EMU_400 },
	// Persona 2: innocent sin
	{ "ULUS-10584", EMU_400 },
	{ "ULJM-05759", EMU_400 },
	{ "UCAS-40338", EMU_400 },
	{ "NPJH-50329", EMU_400 },
	// Resistance retribution
	{ "NPEG-90013", EMU_400 },
	{ "NPJG-90037", EMU_400 },
	{ "NPUG-22850", EMU_400 },
	{ "UCES-01184", EMU_400 },
	{ "UCUS-98668", EMU_400 },
	{ "UCJS-10090", EMU_400 },
	// Soul calibur broken destiny
	{ "ULES-01298", EMU_400 },
	{ "ULJS-00202", EMU_400 },
	{ "ULUS-10457", EMU_400 },
	{ "ULJS-19055", EMU_400 },
	// Star ocean first departure
	{ "ULJM-05290", EMU_400 },
	{ "ULJM-05298", EMU_400 },
	{ "ULES-01154", EMU_400 },
	{ "ULUS-10374", EMU_400 },
	{ "ULJM-05590", EMU_400 },
	// Valhalla Knights
	{ "ULJS-00075", EMU_400 },
	{ "ULUS-10230", EMU_400 },
	{ "ULJM-00075", EMU_400 },
	{ "ULKS-46133", EMU_400 },
	{ "ULJS-19010", EMU_400 },
	{ "ULES-00657", EMU_400 },
	// White Knight Chronicles: origins
	{ "UCJS-10115", EMU_400 },
	{ "UCES-01511", EMU_400 },
	// Z.H.P. : Unlosing Ranger Vs Darkdeath
	{ "ULUS-10559", EMU_400 },
	{ "ULJS-00262", EMU_400 },
	{ "ULJS-00261", EMU_400 },

	// Games that still don't work, but show VISIBLE progress //
	// Ben 10 Ultimate Alien: Cosmic Destruction -> Warning, we don't have yet a proper title_name for this one, and we aer missing title id's
	{ "ULUS-10542", EMU_400 },
	// Final Fantasy Dissidia 012
	{ "ULES-01505", EMU_400 },
	{ "ULJM-05814", EMU_400 },
	{ "ULUS-10566", EMU_400 },
	// Final Fantasy IV Complete Collection
	{ "ULJM-05855", EMU_400 },
	{ "ULUS-10560", EMU_400 },
	{ "NPJH-50414", EMU_400 },
	// Gods Eater Burst -> Warning, we don't have yet a proper title name
	{ "ULES-01519", EMU_400 },
	{ "NPJH-50352", EMU_400 },
	{ "ULUS-10563", EMU_400 },
	{ "ULJS-00351", EMU_400 },
	{ "ULJS-00350", EMU_400 },
	{ "ULJS-19056", EMU_400 },
	// Need for Speed: Most Wanted
	{ "ULAS-42031", EMU_400 },
	{ "ULES-00196", EMU_400 },
	{ "ULJM-05073", EMU_400 },
	{ "ULKS-46044", EMU_400 },
	{ "ULUS-10036", EMU_400 },
	{ "ULJM-05183", EMU_400 },
	// Persona 3
	{ "ULES-01523", EMU_400 },
	{ "NPJH-50040", EMU_400 },
	{ "ULUS-10512", EMU_400 },
	{ "UCAS-40288", EMU_400 },
	{ "ULJM-05489", EMU_400 },
	{ "ULJM-08044", EMU_400 },
	// Ridge Racer 1
	{ "ULJS-00001", EMU_400 },
	{ "UCES-00002", EMU_400 },
	{ "UCAS-40015", EMU_400 },
	{ "ULUS-10001", EMU_400 },
	{ "UCKS-45002", EMU_400 },
	{ "ULJS-19002", EMU_400 },
	{ "UCKS-45053", EMU_400 },
	// Tactics ogre
	{ "ULJM-05753", EMU_400 },
	{ "ULUS-10565", EMU_400 },
	{ "ULES-01500", EMU_400 },
	// Warhammer 40000 Squad Team
	{ "NPEH-90001", EMU_400 },
	{ "ULUD-90004", EMU_400 },
	{ "ULES-00873", EMU_400 },
	{ "ULUS-10313", EMU_400 },
};

static KeyValue emu_by_title_name[N_TITLE_NAMES] =
{
	// First, games really fixed //
	{ { 0x42, 0x4C, 0x41, 0x5A, 0x42, 0x4C, 0x55, 0x45, 0x20, 0x43, 0x4F, 0x4E, 0x54, 0x49,
	    0x4E, 0x55, 0x55, 0x4D, 0x20, 0x53, 0x48, 0x49, 0x46, 0x54, 0x20, 0xE2, 0x85, 0xA1, 0x00 }, EMU_400 }, // BLAZBLUE CONTINUUM SHIFT II
	{ "DJMAX PORTABLE 3", EMU_400 },
	{ "EXIT", EMU_400 },
	{ "Fate/EXTRA", EMU_400 },
	{ { 0x4C, 0x45, 0x47, 0x4F, 0xC2, 0xAE, 0x20, 0x50, 0x69, 0x72, 0x61, 0x74, 0x65, 0x73, 0x20, 0x6F,
	    0x66, 0x20, 0x74, 0x68, 0x65, 0x20, 0x43, 0x61, 0x72, 0x69, 0x62, 0x62, 0x65, 0x61, 0x6E, 0x20,
	    0x54, 0x68, 0x65, 0x20, 0x56, 0x69, 0x64, 0x65, 0x6F, 0x20, 0x47, 0x61, 0x6D, 0x65, 0x20, 0x00 }, EMU_400 }, //LEGOÂ® Pirates of the Caribbean The Video Game
	{ "LUNAR: SILVER STAR HARMONY", EMU_400 },
	{ { 0x52, 0x45, 0x53, 0x49, 0x53, 0x54, 0x41, 0x4E, 0x43, 0x45, 0xC2, 0xAE, 0x20, 0xEF, 0xBD, 0x9E,
	    0xE5, 0xA0, 0xB1, 0xE5, 0xBE, 0xA9, 0xE3, 0x81, 0xAE, 0xE5, 0x88, 0xBB, 0xEF, 0xBD, 0x9E, 0x00 }, EMU_400 }, // Resistance retribution (JP)
	{ { 0x52, 0x65, 0x73, 0x69, 0x73, 0x74, 0x61, 0x6E, 0x63, 0x65, 0x3A, 0x20, 0x52, 0x65, 0x74, 0x72,
	    0x69, 0x62, 0x75, 0x74, 0x69, 0x6F, 0x6E, 0xE2, 0x84, 0xA2, 0x00 }, EMU_400 }, // Resistance: Retributionâ„¢
	{ "SMT: Persona 2: Innocent Sin", EMU_400 },
	{ { 0x50, 0x45, 0x52, 0x53, 0x4F, 0x4E, 0x41, 0x32, 0x20, 0xE7, 0xBD, 0xAA, 0x00 }, EMU_400 }, // Persona 2 jap
	{ "SOULCALIBUR: Broken Destiny", EMU_400 },
	{ "STAR OCEAN: First Departure", EMU_400 },
	{ "TEKKEN 6", EMU_400 },
	{ "The 3rd Birthday", EMU_400 },
	{ "VALHALLAKNIGHTS", EMU_400 },
	{ { 0x57, 0x68, 0x69, 0x74, 0x65, 0x20, 0x4B, 0x6E, 0x69, 0x67, 0x68, 0x74, 0x20, 0x43, 0x68, 0x72,
	    0x6F, 0x6E, 0x69, 0x63, 0x6C, 0x65, 0x73, 0xE2, 0x84, 0xA2, 0x3A, 0x20, 0x4F, 0x72, 0x69, 0x67,
	    0x69, 0x6E, 0x73, 0x00 }, EMU_400 }, // White Knight Chroniclesâ„¢: Origins
	{ "Z.H.P.: Unlosing Ranger VS Darkdeath Evilman", EMU_400 },



	// Games that don't work yet, but show progress //
	{ "DISSIDIA duodecim FINAL FANTASY", EMU_400 },
	{ "FINAL FANTASY IV Complete Collection", EMU_400 },
	{ { 0x4E, 0x45, 0x45, 0x44, 0x20, 0x46, 0x4F, 0x52, 0x20, 0x53, 0x50, 0x45, 0x45, 0x44, 0xE2, 0x84,
	    0xA2, 0x20, 0x4D, 0x4F, 0x53, 0x54, 0x20, 0x57, 0x41, 0x4E, 0x54, 0x45, 0x44, 0x20, 0x35, 0x2D,
	    0x31, 0x2D, 0x30, 0x00 }, EMU_400 }, // NEED FOR SPEEDâ„¢ MOST WANTED 5-1-0
	{ "Persona3 PORTABLE", EMU_400 },
	{ "RIDGE RACER", EMU_400 },
	{ "TACTICS OGRE Let Us Cling Together", EMU_400 },
	{ "Warhammer 40,000: Squad Command", EMU_400 },
};

static u8 lambda_md5[16] =
{
	0xE1, 0x99, 0xCA, 0x7D, 0x48, 0x3B, 0xC0, 0x7B, 0x4D, 0xC6, 0xE7, 0x4A, 0xE5, 0x53, 0x76, 0xCE
};
*/

static int translate_type(unsigned int type)
{
	if(type == 0)					return DISC_TYPE_NONE;
	if(type == DEVICE_TYPE_PS3_BD)	return DISC_TYPE_PS3_BD;
	if(type == DEVICE_TYPE_PS3_DVD)	return DISC_TYPE_PS3_DVD;
	if(type == DEVICE_TYPE_PS2_DVD)	return DISC_TYPE_PS2_DVD;
	if(type == DEVICE_TYPE_PS2_CD)	return DISC_TYPE_PS2_CD;
	if(type == DEVICE_TYPE_PSX_CD)	return DISC_TYPE_PSX_CD;
	if(type == DEVICE_TYPE_BDROM)	return DISC_TYPE_BDROM;
	if(type == DEVICE_TYPE_BDMR_SR)	return DISC_TYPE_BDMR_SR;
	if(type == DEVICE_TYPE_BDMR_RR)	return DISC_TYPE_BDMR_RR;
	if(type == DEVICE_TYPE_BDMRE)	return DISC_TYPE_BDMRE;
	if(type == DEVICE_TYPE_DVD)		return DISC_TYPE_DVD;
	if(type == DEVICE_TYPE_CD)		return DISC_TYPE_CD;

	return DISC_TYPE_UNKNOWN;
}

/*
static char *get_blank_iso_path(void)
{
	char *s = malloc(32);

	strcpy(s, "/dev_hdd0/vsh/task.dat\0");

	return s;
}
*/

static void build_iso_record(u8 *buf, u32 offset)
{
	buf[offset] = 0x28;
	buf[offset + 2] = buf[offset + 9] = 0x18;

	buf[offset + 0x0B] = buf[offset + 0x10] = 8;
	buf[offset + 0x12] = 0x6F;
	buf[offset + 0x13] = 7;
	buf[offset + 0x14] = 0x16;
	buf[offset + 0x15] = 2;
	buf[offset + 0x16] = 0x2B;
	buf[offset + 0x17] = buf[offset + 0x19] = 2;
	buf[offset + 0x1C] = buf[offset + 0x1F] = buf[offset + 0x20] = 1;
}

static void build_blank_iso(const char *title_id)
{
	sys_addr_t sysmem = NULL;
	if(sys_memory_allocate(_128KB_, SYS_MEMORY_PAGE_SIZE_64K, &sysmem) /*!= CELL_OK*/) return;
	u8 *buf = (u8*)sysmem;

/*
	// build task.dat from external template file
	read_file("/dev_hdd0/tmp/wm_res/task.dat", (char*)buf, _128KB_, 0);

	memcpy(buf + 0x810, title_id, 4);
	buf[0x814] = '-';
	memcpy(buf + 0x815, title_id+4, 5);

	save_file("/dev_hdd0/vsh/task.dat", (char*)buf, _128KB_);

	if(sysmem) sys_memory_free(sysmem);
	return;
*/

	_memset(buf, _128KB_);

	buf[3] = 2;
	buf[0x17] = 0x3F;
	strcpy((char *)buf + 0x800, "PlayStation3");
	memcpy(buf + 0x810, title_id, 4);
	buf[0x814] = '-';
	memcpy(buf + 0x815, title_id + 4, 5);
	memset(buf + 0x81A, ' ', 0x16);
	buf[0x8000] = 1;
	strcpy((char *)buf + 0x8001, "CD001");
	buf[0x8006] = 1;
	memset(buf + 0x8008, ' ', 0x40);
	memcpy(buf + 0x8028, "PS3VOLUME", 9);
	buf[0x8050] = buf[0x8057] = 0x40;
	buf[0x8078] = buf[0x807B] = buf[0x807C] = buf[0x807F] = 1;
	buf[0x8081] = buf[0x8082] = 8;
	buf[0x8084] = buf[0x808B] = 0xA;
	buf[0x808C] = 0x14;
	buf[0x8097] = 0x15;

	build_iso_record(buf, 0x809C);
	buf[0x809C] = 0x22;

	memcpy(buf + 0x80BE, "PS3VOLUME", 9);
	memset(buf + 0x80C7, ' ', 0x266);
	strcpy((char *)buf + 0x832D, "20131111050513");
	memset(buf + 0x833B, '0', 0x35);
	buf[0x833D] = buf[0x834E] = buf[0x835F] = 0;
	buf[0x8371] = 1;

	buf[0x8800] = 2;
	strcpy((char *)buf + 0x8801, "CD001");
	buf[0x8806] = 1;
	buf[0x8829] = 'P';
	buf[0x882B] = 'S';
	buf[0x882D] = '3';
	buf[0x882F] = 'V';
	buf[0x8831] = 'O';
	buf[0x8833] = 'L';
	buf[0x8835] = 'U';
	buf[0x8837] = 'M';
	buf[0x8839] = 'E';
	buf[0x8850] = buf[0x8857] = buf[0x885A] = 0x40;
	buf[0x8858] = 0x25;
	buf[0x8859] = 0x2F;
	buf[0x8878] = buf[0x887B] = buf[0x887C] = buf[0x887F] = 1;
	buf[0x8881] = buf[0x8882] = 8;
	buf[0x8884] = buf[0x888B] = 0xA;
	buf[0x888C] = 0x16;
	buf[0x8897] = 0x17;

	build_iso_record(buf, 0x889C);
	buf[0x889C] = 0x22;
	buf[0x889E] = buf[0x88A5] = 0x19;

	memcpy(buf + 0x88BF, buf + 0x8829, 17); // 'P S 3 V O L U M E'
	memcpy((char *)buf + 0x8B2D, (char *)buf + 0x832D, 0x43);
	buf[0x8B71] = 1;
	buf[0x9000] = 0xFF;
	strcpy((char *)buf + 0x9001, "CD001");

	buf[0xA000] = buf[0xA006] = buf[0xA800] = buf[0xA807] = 1;
	buf[0xA002] = buf[0xA805] = 0x18;
	buf[0xB000] = buf[0xB006] = buf[0xB800] = buf[0xB807] = 1;
	buf[0xB002] = buf[0xB805] = 0x19;

	build_iso_record(buf, 0xC000);
	build_iso_record(buf, 0xC028);
	buf[0xC049] = 1;

	memcpy(buf + 0xC800, buf + 0xC000, 0x50);

	save_file("/dev_hdd0/vsh/task.dat", (char*)buf, _128KB_);

	if(sysmem) sys_memory_free(sysmem);
	return;
}
/*
static int copy_file(char *src, char *dst)
{

	int ret;
	int fd_s, fd_d;

	ret = cellFsOpen(src, CELL_FS_O_RDONLY, &fd_s, NULL, 0);
	if(ret == 0)
	{
		ret = cellFsOpen(dst, CELL_FS_O_CREAT | CELL_FS_O_WRONLY | CELL_FS_O_TRUNC, &fd_d, NULL, 0);
		if(ret == 0)
		{
			const u32 buf_size = _16KB_;
			u8 *buf = (u8 *)malloc(buf_size);
			while(1)
			{
				u64 nread, nwritten;

				ret = cellFsRead(fd_s, buf, buf_size, &nread);
				if (ret != CELL_FS_SUCCEEDED || nread == 0) break;

				ret = cellFsWrite(fd_d, buf, nread, &nwritten);
				if (ret != CELL_FS_SUCCEEDED) break;

				if (nwritten != nread)
				{
					ret = -1;
					break;
				}
			}

			cellFsClose(fd_d);
			free(buf);
		}

		cellFsClose(fd_s);

		if(ret)
		{
			cellFsUnlink(dst);
		}
		else
		{
			cellFsChmod(dst, CELL_FS_S_IFMT | 0777);
		}
	}

	//DPRINTF("Copy file returning: %x\n", ret);

	free(buf);
	return ret;
}
*/
static int sys_get_hw_config(u8 *ret, u8 *config)
{
	system_call_2(393, (u64)(u32)ret, (u64)(u32)config);
	return (int)p1;
}
/*
static int sys_get_version(u32 *version)
{
	system_call_2(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_GET_VERSION, (u64)(u32)version);
	return (int)p1;
}

static int sys_get_version2(u16 *version)
{
	system_call_2(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_GET_VERSION2, (u64)(u32)version);
	return (int)p1;
}
*/
static int sys_read_cobra_config(CobraConfig *cfg)
{
	system_call_2(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_READ_COBRA_CONFIG, (u64)(u32)cfg);
	return (int)p1;
}

static int sys_write_cobra_config(CobraConfig *cfg)
{
	system_call_2(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_WRITE_COBRA_CONFIG, (u64)(u32)cfg);
	return (int)p1;
}

/*
static char *trim(char *str)
{
	int len = strlen(str);
	u8 *temp = (u8 *)strdup(str);
	u8 *p = temp;
	int i;

	for (i = 0; i < len; i++)
	{
		if (temp[i] <= ' ')
			p++;
		else
			break;
	}

	len = strlen((char *)p);

	for (i = len-1; i >= 0; i--)
	{
		if (p[i] <= ' ')
			p[i] = 0;
		else
			break;
	}

	strcpy(str, (char *)p);
	free(temp);
	return str;
}


static int parse_param_sfo(char *file, const char *field, char *title_name)
{
	FILE *fp;

	fp = fopen(file, "rb");
	if (fp) {
		unsigned len, pos, str;
		unsigned char *mem = NULL;

		fseek(fp, 0, SEEK_END);
		len = ftell(fp);

		mem = (unsigned char *) malloc(len + 16);
		if (!mem) {
			fclose(fp);
			return -2;
		}

		_memset(mem, len + 16);

		fseek(fp, 0, SEEK_SET);
		fread((void *) mem, len, 1, fp);

		fclose(fp);

		str = (mem[8] + (mem[9] << 8));
		pos = (mem[0xc] + (mem[0xd] << 8));

		int indx = 0;

		while (str < len) {
			if (mem[str] == 0)
				break;

			if (!strcmp((char *) &mem[str], field)) {
				strncpy(title_name, (char *) &mem[pos], 63);
				free(mem);
				return 0;
			}
			while (mem[str])
				str++;
			str++;
			pos += (mem[0x1c + indx] + (mem[0x1d + indx] << 8));
			indx += 16;
		}
		//if (mem) free(mem);
	}

	return -1;
}
*/
/*
static int sys_permissions_get_access(void)
{
	system_call_1(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_GET_ACCESS);
	return (int)p1;
}

static int sys_permissions_remove_access(void)
{
	system_call_1(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_REMOVE_ACCESS);
	return (int)p1;
}

static int cobra_usb_command(u8 command, int requestType, u32 addr, void *buf, u16 size)
{
	system_call_6(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_COBRA_USB_COMMAND, command, requestType, addr, (u64)(u32)buf, size);
	return (int)p1;
}

int cobra_lib_init(void)
{
	return sys_permissions_get_access();
}

int cobra_lib_finalize(void)
{
	return sys_permissions_remove_access();
}
*/
int cobra_get_disc_type(unsigned int *real_disctype, unsigned int *effective_disctype, unsigned int *iso_disctype)
{
	unsigned int rdt, edt;
	int ret;

	ret = sys_storage_ext_get_disc_type(&rdt, &edt, NULL);
	if (ret)
		return ret;

	if (real_disctype)
	{
		*real_disctype = translate_type(rdt);
	}

	if (effective_disctype)
	{
		*effective_disctype = translate_type(edt);
	}

	if (iso_disctype)
	{
		*iso_disctype = DISC_TYPE_NONE;

		sys_emu_state_t emu_state;

		emu_state.size = sizeof(sys_emu_state_t);
		ret = sys_storage_ext_get_emu_state(&emu_state);

		if (ret == 0)
		{
			int disc_emulation = emu_state.disc_emulation;

			if (disc_emulation != EMU_OFF)
			{
				switch (disc_emulation)
				{
					case EMU_PS3:
						*iso_disctype = DISC_TYPE_PS3_BD;
					break;

					case EMU_PS2_DVD:
						*iso_disctype = DISC_TYPE_PS2_DVD;
					break;

					case EMU_PS2_CD:
						*iso_disctype = DISC_TYPE_PS2_CD;
					break;

					case EMU_PSX:
						*iso_disctype = DISC_TYPE_PSX_CD;
					break;

					case EMU_BD:
						if (edt != DISC_TYPE_NONE)
							*iso_disctype = edt;
						else
							*iso_disctype = DISC_TYPE_BDMR_SR;
					break;

					case EMU_DVD:
						*iso_disctype = DISC_TYPE_DVD;
					break;

					default:
						*iso_disctype = DISC_TYPE_UNKNOWN;
				}
			}
		}
	}

	return 0;
}

int cobra_disc_auth(void)
{
	unsigned int real_disctype;
	int ret;

	ret = sys_storage_ext_get_disc_type(&real_disctype, NULL, NULL);
	if (ret)
		return ret;

	if (real_disctype == 0)
		return ENODEV;

	if (real_disctype == DEVICE_TYPE_PS3_BD || real_disctype == DEVICE_TYPE_PS3_DVD)
	{
		static u8 buf[1024]; _memset(buf, sizeof(buf));

		sys_ss_disc_auth(0x5007, (u64)(u32)buf);
	}
	else
	{
		sys_ss_disc_auth(0x5004, 0x29);
	}

	return 0;
}

static unsigned int ejected_realdisc;

int cobra_send_fake_disc_eject_event(void)
{
	sys_storage_ext_get_disc_type(&ejected_realdisc, NULL, NULL);

	sys_storage_ext_fake_storage_event(4, 0, BDVD_DRIVE);
	return sys_storage_ext_fake_storage_event(8, 0, BDVD_DRIVE);
}

int cobra_send_fake_disc_insert_event(void)
{
	u64 param;
	unsigned int real_disctype, effective_disctype, iso_disctype;

	cobra_get_disc_type(&real_disctype, &effective_disctype, &iso_disctype);

	if (ejected_realdisc == 0 && real_disctype == 0 && effective_disctype == 0 && iso_disctype == 0)
	{
		//printf("All disc types 0, aborting\n");
		return EABORT;
	}

	param = (u64)(ejected_realdisc) << 32ULL;
	sys_storage_ext_get_disc_type(&ejected_realdisc, NULL, NULL);
	sys_storage_ext_fake_storage_event(7, 0, BDVD_DRIVE);
	return sys_storage_ext_fake_storage_event(3, param, BDVD_DRIVE);
}

int cobra_mount_ps3_disc_image(char *files[], unsigned int num)
{
	if(!files) return EINVAL;

	return sys_storage_ext_mount_ps3_discfile(num, files);
}

int cobra_mount_dvd_disc_image(char *files[], unsigned int num)
{
	if(!files) return EINVAL;

	return sys_storage_ext_mount_dvd_discfile(num, files);
}

int cobra_mount_bd_disc_image(char *files[], unsigned int num)
{
	if (!files) return EINVAL;

	return sys_storage_ext_mount_bd_discfile(num, files);
}

static void init_tracks(int num_tracks, TrackDef *tracks, ScsiTrackDescriptor *scsi_tracks)
{
	_memset(scsi_tracks, num_tracks * sizeof(ScsiTrackDescriptor));

	for (int i = 0; i < num_tracks; i++)
	{
		scsi_tracks[i].adr_control = (tracks[i].is_audio) ? 0x10 : 0x14;
		scsi_tracks[i].track_number = i + 1;
		scsi_tracks[i].track_start_addr = tracks[i].lba;
	}
}

int cobra_mount_psx_disc_image(char *file, TrackDef *tracks, unsigned int num_tracks)
{
	if (!file || !tracks) return EINVAL;

	num_tracks = RANGE(num_tracks, 1, MAX_TRACKS);

	ScsiTrackDescriptor scsi_tracks[num_tracks];
	init_tracks(num_tracks, tracks, scsi_tracks);

	return sys_storage_ext_mount_psx_discfile(file, num_tracks, scsi_tracks);
}

int cobra_mount_ps2_disc_image(char *files[], int num, TrackDef *tracks, unsigned int num_tracks)
{
	if (!files || !tracks) return EINVAL;

	num_tracks = RANGE(num_tracks, 1, MAX_TRACKS);

	ScsiTrackDescriptor scsi_tracks[num_tracks];
	init_tracks(num_tracks, tracks, scsi_tracks);

	num = RANGE(num, 1, 32);

	return sys_storage_ext_mount_ps2_discfile(num, files, num_tracks, scsi_tracks);
}

int cobra_umount_disc_image(void)
{
	int ret = sys_storage_ext_umount_discfile();
	if (ret == FAILED)
		ret = ENODEV;

	return ret;
}

/*
int cobra_read_ps3_disc(void *buf, u64 sector, u32 count)
{
	return sys_storage_ext_read_ps3_disc(buf, sector, count);
}

int cobra_get_disc_phys_info(u32 handle, u8 layer, DiscPhysInfo *info)
{
	ScsiReadDiscStructureFormat0Response *response;
	u8 scsi_cmd[56];
	static u8 output[64] __attribute__((aligned(64)));
	int ret;

	if (!info)
		return EINVAL;

	ScsiCmdReadDiscStructure *cmd = (ScsiCmdReadDiscStructure *)scsi_cmd;
	StorageCmdScsiData *data = (StorageCmdScsiData *)(scsi_cmd+32);

	_memset(scsi_cmd, sizeof(scsi_cmd));
	cmd->opcode = SCSI_CMD_READ_DISC_STRUCTURE;
	cmd->alloc_length = sizeof(ScsiReadDiscStructureFormat0Response);
	cmd->layer_num = layer;
	data->inlen = 12;
	data->unk1 = data->unk2 = data->unk3 = 1;
	data->outlen = sizeof(ScsiReadDiscStructureFormat0Response);

	response = (ScsiReadDiscStructureFormat0Response *)output;
	ret = sys_storage_send_device_command(handle, STORAGE_COMMAND_NATIVE, scsi_cmd, sizeof(scsi_cmd), response, sizeof(ScsiReadDiscStructureFormat0Response));

	if (ret)
		return ret;

	memcpy(info, output+4, 17);
	return 0;
}


int cobra_get_cd_td(u32 handle, TrackDef *td, unsigned int max_tracks, unsigned int *num_tracks, u32 *lba_end)
{
	u32 cd_num_tracks, cd_total_tracks;
	u8 scsi_cmd[56];
	int ret;
	int i, j;

	if (!td || !num_tracks)
		return EINVAL;

	ScsiCmdReadTocPmaAtip *cmd = (ScsiCmdReadTocPmaAtip *)scsi_cmd;
	StorageCmdScsiData *data = (StorageCmdScsiData *)(scsi_cmd+32);
	ScsiTocResponse toc_info;

	_memset(scsi_cmd, sizeof(scsi_cmd));
	cmd->opcode = SCSI_CMD_READ_TOC_PMA_ATIP;
	cmd->alloc_length = sizeof(toc_info);
	data->inlen = 12;
	data->unk1 = data->unk2 = data->unk3 = 1;
	data->outlen = cmd->alloc_length;

	ret = sys_storage_send_device_command(handle, STORAGE_COMMAND_NATIVE, scsi_cmd, sizeof(scsi_cmd), &toc_info, sizeof(toc_info));
	if (ret)
		return ret;

	cd_num_tracks = toc_info.last_track;
	u8 *response = (u8 *) malloc(toc_info.toc_length+2);
	cmd->alloc_length = toc_info.toc_length+2;
	data->outlen = cmd->alloc_length;

	ret = sys_storage_send_device_command(handle, STORAGE_COMMAND_NATIVE, scsi_cmd, sizeof(scsi_cmd), response, cmd->alloc_length);
	if (ret)
	{
		free(response);
		return ret;
	}

	if (toc_info.first_track != 1)
	{
		return ENOTSUP;
	}
	else if ((unsigned int)(toc_info.toc_length-2) < (unsigned int)toc_info.last_track*sizeof(ScsiTrackDescriptor))
	{
		return ENOTSUP;
	}

	ScsiTrackDescriptor *tracks = (ScsiTrackDescriptor *)(response+sizeof(ScsiTocResponse));
	cd_total_tracks = (toc_info.toc_length-2) / sizeof(ScsiTrackDescriptor);

	for (i = 1; i <= cd_num_tracks; i++)
	{
		int found = 0;

		for (j = 0; j < cd_total_tracks; j++)
		{
			if (tracks[j].track_number == i)
			{
				int track_type = tracks[j].adr_control & 0xD;
				int is_audio = (track_type == 0 || track_type == 1 || track_type == 8 || track_type == 9);

				if (i  <= max_tracks)
				{
					td[i-1].lba = tracks[j].track_start_addr;
					td[i-1].is_audio = is_audio;
				}

				found = 1;
				break;
			}
		}

		if (!found)
		{
			ret = ENOTSUP;
			break;
		}
	}

	if (ret)
	{
		free(response);
		return ret;
	}

	for (i = 0; i < cd_total_tracks; i++)
	{
		if (tracks[i].track_number == 0xAA)
		{
			if (lba_end)
			{
				*lba_end = tracks[i].track_start_addr;
			}
			break;
		}
	}

	if (i == cd_total_tracks)
		return ENOTSUP;

	free(response);

	if (cd_num_tracks > max_tracks)
		*num_tracks = max_tracks;
	else
		*num_tracks = cd_num_tracks;

	DPRINTF("Track dump\n");
	for (i = 0; i < cd_num_tracks; i++)
	{
		DPRINTF("LBA:%08X, is_audio=%d\n", td[i].lba, td[i].is_audio);
	}
	DPRINTF("Last lba: %d\n", (lba_end) ? -7 : *lba_end);

	return 0;
}


int cobra_cd_read(u32 handle, void *buf, u32 sector, u32 count, int is_audio, int *num_errors)
{
	u8 scsi_cmd[56];
	ScsiCmdReadCd *cmd = (ScsiCmdReadCd *)scsi_cmd;
	StorageCmdScsiData *data = (StorageCmdScsiData *)(scsi_cmd+32);
	int nerrors = 0;

	if (!buf)
		return EINVAL;

	u8 *read_buf = (u8 *)buf;

	DPRINTF("Function begin, sector=%08X, count=%d, audio=%d\n", sector, count, is_audio);

	for (u32 i = 0; i < count; i++)
	{
		_memset(scsi_cmd, sizeof(scsi_cmd));
		cmd->opcode = SCSI_CMD_READ_CD;
		cmd->lba = sector;
		cmd->length[0] = 0;
		cmd->length[1] = 0;
		cmd->length[2] = 1;
		cmd->misc = (is_audio) ? 0x10 : 0xF8;
		data->inlen = 12;
		data->unk1 = data->unk2 = data->unk3 = 1;
		data->outlen = 2352;

		DPRINTF("Reading sector %08X, try=%d\n", sector, i+1);

		for (int retry = 0; retry < 10; retry++)
		{
			int ret = sys_storage_send_device_command(handle, STORAGE_COMMAND_NATIVE, scsi_cmd, sizeof(scsi_cmd), read_buf, 2352);
			if (ret == 0)
			{
				break;
			}
			else if (retry == 9)
			{
				unsigned int disctype;

				_memset(read_buf, 2352);
				nerrors++;
				DPRINTF("Read sector: %lx all retries failed (%x)\n", (long unsigned int)sector, ret);

				sys_storage_ext_get_disc_type(NULL, &disctype, NULL);
				if (disctype == 0)
				{
					DPRINTF("Disc was removed!!\n");
					return ENODEV;
				}
			}
		}

		sector++;
		read_buf += 2352;
	}

	if (num_errors)
		*num_errors = nerrors;

	DPRINTF("Returning, number_of_errors=%d\n", nerrors);

	return (nerrors > 0) ? EIO : 0;
}

int cobra_parse_cue(void *cue, u32 size, TrackDef *tracks, unsigned int max_tracks, unsigned int *num_tracks, char *filename, unsigned int fn_size)
{
#define skip_spaces() \
	while (*p <= ' ' && *p >= 0) \
	{ \
		p++; \
		\
		if (*(p-1) == '\n') \
			goto continue_loop; \
		\
		else if (*(p-1) == 0) \
			goto exit_loop; \
	}

	char *cue_buf = cue;
	char *p;
	char *bin_file;
	int read_index;
	u32 ntracks;
	u32 tracks_lba[max_tracks];
	int ret = EABORT;

	if (!cue)
		return EINVAL;

	p = cue_buf;
	ntracks = 0;
	bin_file = NULL;
	read_index = 0;

	while (ntracks < max_tracks)
	{
continue_loop:
		skip_spaces();

		if (p[4] <= ' ' && p[4] > 0 && strncasecmp(p, "FILE", 4) == 0)
		{
			if (bin_file)
			{
				// We don't support more than one file, leave here
				break;
			}

			p = p+4;

			skip_spaces();

			if (*p == '"')
			{
				p++;
				bin_file = p;

				while (*p != '"')
				{
					p++;

					if (*(p-1) == '\n')
						goto continue_loop;

					else if (*(p-1) == 0)
					{
						ntracks = 0;
						ret = EABORT;
						break;
					}

				}

				*p = 0;
				p++;

				char *last_trail1 = strrchr(bin_file, '/');
				char *last_trail2 = strrchr(bin_file, '\\');

				char *last_trail = (last_trail1 > last_trail2) ? last_trail1 : last_trail2;

				if (last_trail)
				{
					bin_file = last_trail+1;
				}

				strncpy(filename, bin_file, fn_size);
			}
		}
		else if (p[5] <= ' ' && p[5] > 0 && strncasecmp(p, "TRACK", 5) == 0)
		{
			if (read_index)
			{
				//DPRINTF("Found other track before INDEX 01\n");
				ret = ENOTSUP;
				break;
			}

			if (!bin_file)
			{
				//DPRINTF("TRACK before file!!!\n");
				ret = ENOTSUP;
				break;
			}

			p = p+5;

			skip_spaces();

			if (val(p) != (int)(ntracks+1))
				break;

			skip_spaces();

			if (ntracks == 0)
			{
				if (!(p[10] <= ' ' && p[10] >= 0) || strncasecmp(p, "MODE2/2352", 9) != 0)
				{
					//DPRINTF("First track is not MODE2/2352!!!\n");
					ret = ENOTSUP;
					break;
				}
			}
			else if (!(p[5] <= ' ' && p[5] >= 0) || strncasecmp(p, "AUDIO", 5) != 0)
			{
				//DPRINTF("Found a track which is not AUDIO!!!\n");
				ret = ENOTSUP;
				break;
			}

			read_index = 1;
		}
		else if (p[5] <= ' ' && p[5] > 0 && strncasecmp(p, "INDEX", 5) == 0)
		{
			if (read_index)
			{
				p += 5;
				skip_spaces();

				if (val(p) == 1)
				{
					skip_spaces();

					int minutes, seconds, frames;

					minutes = val(p);
//					if (minutes >= 256 || minutes < 0)
//					{
//						//DPRINTF("Bad minutes format.\n");
//					}

//					if (*p != ':')
//					{
//						//DPRINTF("Bad index formatting (1)\n");
//					}

					seconds = val(p+1);
//					if (seconds >= 256 || seconds < 0)
//					{
//						//DPRINTF("Bad seconds format\n");
//					}

//					if (*p != ':')
//					{
//						//DPRINTF("Bad index formatting(2)\n");
//					}

					frames = val(p+1);
//					if (frames >= 256 || frames < 0)
//					{
//						//DPRINTF("Bad frames format\n");
//					}

					u32 lba = msf_to_lba(minutes, seconds, frames);

					if (ntracks == 0)
					{
						if (lba)
						{
							//DPRINTF("First track must be at 00:00:00, but it is at %02d:%02d:%02d\n", minutes, seconds, frames);
							ret = ENOTSUP;
							break;
						}
					}
					//else if ((lba*2352) > bin_size)
					//{
						//DPRINTF("Track outside file scope.\n");
					//	break;
					//}

					tracks_lba[ntracks++] = lba;
					read_index = 0;
				}
			}
			else
			{
				//DPRINTF("Warning: INDEX before track.\n");
			}
		}

		while (*p != '\n' && *p != 0)
			p++;

		if (*p == 0)
			break;
	}

exit_loop:

	if (ntracks == 0)
	{
		//DPRINTF("No tracks or error.\n");
		return ret;
	}

	for (unsigned int i = 0; i < ntracks; i++)
	{
		if (i < max_tracks)
		{
			tracks[i].lba = tracks_lba[i];
			tracks[i].is_audio = (i != 0);
		}
	}

	if (num_tracks)
	{
		if (ntracks > max_tracks)
			*num_tracks = max_tracks;
		else
			*num_tracks = ntracks;
	}

	return 0;
}

int cobra_create_cue(char *path, char *filename, TrackDef *tracks, unsigned int num_tracks)
{
	if (!path || !tracks || !num_tracks)
		return EINVAL;

	FILE *cue = fopen(path, "w");
	if (!cue)
		return EIO;

	fprintf(cue, "FILE \"%s\" BINARY\r\n", filename);

	for (int i = 0; i < num_tracks; i++)
	{
		u32 lba;
		u8 m, s, f;

		lba = tracks[i].lba-tracks[0].lba;

		lba_to_msf(lba, &m, &s, &f);
		fprintf(cue, "  TRACK %02d %s\r\n", i+1, (!tracks[i].is_audio) ? "MODE2/2352" : "AUDIO");
		fprintf(cue, "    INDEX 01 %02d:%02d:%02d\r\n", m, s, f);
	}

	fclose(cue);
	return 0;
}

int cobra_create_mds(char *path, u64 size_in_sectors, DiscPhysInfo *layer0, DiscPhysInfo *layer1)
{
	u32 size = size_in_sectors;

	if (!path || !layer0 || !layer1)
		return EINVAL;

	FILE *mds = fopen(path, "wb");
	if (mds)
	{
		*(u32 *)&base_mds[0x5C] = *(u32 *)&base_mds[0x7C] = SWAP32(size);
		memcpy(base_mds+0x8C4, layer0, 17);
		memcpy(base_mds+0x18c8, layer1, 17);

		fwrite(base_mds, 1, sizeof(base_mds), mds);
		fclose(mds);
	}
	else
	{
		return EIO;
	}

	return 0;
}
*/

int cobra_map_game(const char *path, const char *title_id, int use_app_home)
{
/*
	if (!path)
	{
		int ret = sys_map_path("/dev_bdvd", NULL);
		if (ret)
			return ret;

		sys_map_path("/app_home", "/dev_usb000");
		return 0;
	}

	if (!title_id || strlen(title_id) != 9)
		return EINVAL;
*/
	build_blank_iso(title_id);

	int ret = sys_map_path("/dev_bdvd", path);
	if (ret) return ret;

	sys_map_path("//dev_bdvd", path);

	if(use_app_home)
	{
		map_app_home(path);
	}

	unsigned int real_disctype;

	sys_storage_ext_get_disc_type(&real_disctype, NULL, NULL);

	if (real_disctype == DISC_TYPE_NONE)
	{
		cobra_send_fake_disc_eject_event();
		sys_timer_usleep(20000);

		char *files[1];
		files[0] = (char*)"/dev_hdd0/vsh/task.dat";

		ret = sys_storage_ext_mount_ps3_discfile(1, files);

		cobra_send_fake_disc_insert_event();
		sys_timer_usleep(20000);
	}

	return 0;
}

/*
int cobra_map_paths(char *paths[], char *new_paths[], unsigned int num)
{
	return sys_map_paths(paths, new_paths, num);
}
*/

static void restore_bak(const char *filename)
{
	char bak[64];
	sprintf(bak, "/%s.bak", filename);

	if(file_exists(bak))
	{
		cellFsUnlink(filename);
		file_copy(bak, (char*)filename); // restore original edat from bak
	}
	else // create bak if not exists
	{
		file_copy(filename, bak);
	}
}

int cobra_unset_psp_umd(void)
{
	restore_bak(PSPL_ICON1);
	restore_bak(PSPL_ICON2);

	cellFsUnlink(PSPL_PATH1 "/PIC1.PNG");
	cellFsUnlink(PSPL_PATH2 "/PIC1.PNG");

	cellFsUnlink(PSPL_PATH1 "/PIC0.PNG");
	cellFsUnlink(PSPL_PATH2 "/PIC0.PNG");

	cellFsUnlink(PSPL_PATH1 "/ICON1.PAM");
	cellFsUnlink(PSPL_PATH2 "/ICON1.PAM");

	cellFsUnlink(PSPL_PATH1 "/SND0.AT3");
	cellFsUnlink(PSPL_PATH2 "/SND0.AT3");

	restore_bak(PSPL_PATH1 "/USRDIR/MINIS.EDAT");
	restore_bak(PSPL_PATH2 "/USRDIR/MINIS.EDAT");
	restore_bak(PSPL_PATH2 "/USRDIR/MINIS2.EDAT");

	sys_psp_set_umdfile(NULL, NULL, 0);
	//sys_psp_change_emu_path(NULL);
	//sys_storage_ext_mount_encrypted_image(NULL, (char*)"/dev_moo", NULL, 0);

	return 0;
}

int cobra_set_psp_umd(char *path, char *umd_root, char *icon_save_path)
{
	if (!path || !icon_save_path)
		return EINVAL;

	if( not_exists(PSPL_ICON1) && not_exists(PSPL_ICON2) )
	{
		return EABORT;
	}

	sys_psp_set_umdfile(NULL, NULL, 0);

	//char sector[1024];
	//read_file(path, sector, sizeof(sector), 0x8000);
	//if (sector[0] != 1 || memcmp(vol_descriptor, "CD001", 5) != 0) return EIO;

	char vol_descriptor[8];
	read_file(path, vol_descriptor, 6, 0x8000);
	if (*vol_descriptor != 1 || memcmp(vol_descriptor + 1, "CD001", 5) != 0) return EIO;

	unsigned int real_disctype, effective_disctype, iso_disctype;

	char title_id[11];
	read_file(path, title_id, 10, 0x8373); title_id[10] = '\0';

	//memcpy(title_id, sector + 0x373, 10); title_id[10] = '\0';

	u8 is_dir = 1;
	u8 do_mount = 0;
	u8 do_eject = 0;
	u8 prometheus = 0;
	u8 has_header = 0;

	u8 pspl1 = file_exists(PSPL_PATH1);
	u8 pspl2 = file_exists(PSPL_PATH2);

	char umd_file[256];
	u32 header[0xD4/4];

	char *root = umd_root;

	if (!root)
	{
		cobra_get_disc_type(&real_disctype, &effective_disctype, &iso_disctype);

		if (iso_disctype != DISC_TYPE_NONE)
			return EBUSY;

		if (effective_disctype != DISC_TYPE_NONE)
		{
			cobra_send_fake_disc_eject_event();
		}

		int ret;
		char *files[1];

		files[0] = (char *)path;
		ret = sys_storage_ext_mount_dvd_discfile(1, files);
		if (ret)
		{
			if (real_disctype != DISC_TYPE_NONE)
				cobra_send_fake_disc_insert_event();
			return ret;
		}

		cobra_send_fake_disc_insert_event();

		// Wait 3 seconds for automounter to mount iso
		if (wait_for("/dev_bdvd", 3) == FAILED)
		{
			cobra_send_fake_disc_eject_event();
			sys_storage_ext_umount_discfile();

			if (real_disctype != DISC_TYPE_NONE)
				cobra_send_fake_disc_insert_event();

			return EIO;
		}

		root = (char*)"/dev_bdvd/PSP_GAME"; do_eject = 1;
	}
	else
	{
		real_disctype = DISC_TYPE_NONE;

		int ext = strlen(root) - 4; if(ext < 0) ext = 0;

		u8 is_iso = strstr(root + ext, ".iso") ||
					strstr(root + ext, ".ISO");

		if(is_iso)
		{
			// check EBOOT exists before copy images
			sprintf(umd_file, "%s.EBOOT.OLD", root);
			if(file_exists(umd_file))
			{
				do_mount = prometheus = 1;
			}
			else
			{
				sprintf(umd_file, "%s.EBOOT.BIN", root);
				do_mount = (file_exists(umd_file));
			}

			if(!do_mount)
			{
				const u32 buf_size = 4096;
				char *buf = (char *)malloc(buf_size);
				if(buf)
				{
					if (read_file(root, (char*)buf, buf_size, 0xC000) == buf_size)
					{
						u32 pos;
						for(pos = 0x80; pos < buf_size; pos++)
							if(!strncmp(&buf[pos], "EBOOT.OLD", 9)) break; // find offset of directory entry
						if(pos >= buf_size)
							for(pos = 0x80; pos < buf_size; pos++)
								if(!strncmp(&buf[pos], "EBOOT.BIN", 9)) break; // find alternate file
						if(pos < buf_size)
						{
							pos -= 0x1B; // get lba offset
							u32 lba = (buf[pos] << 24) + (buf[pos + 1] << 16) + (buf[pos + 2] << 8) + buf[pos + 3];
							pos = (2048UL * lba); // sector size * lba
							do_mount = has_header = (read_file(root, (char*)&header, sizeof(header), pos) == sizeof(header));
						}
					}
					free(buf);
				}
			}
			// copy images to psp launcher
			if(do_mount)
			{
				char *icon = umd_file;
				sprintf(icon, "%s.PNG", root);  // game.iso.PNG
				if(not_exists(icon))
				{
					sprintf(icon, "%s.png", root);  // game.iso.png
					if(not_exists(icon))
						sprintf(icon + ext, ".png"); // game.png
					if(not_exists(icon))
						sprintf(icon + ext, ".PNG"); // game.PNG
					if(not_exists(icon))
						sprintf(icon + ext, ".ICON0.PNG"); // game.ICON0.PNG
				}

				if(file_exists(icon))
				{
					if(pspl1) sys_map_path(PSPL_ICON1, icon);
					if(pspl2) sys_map_path(PSPL_ICON2, icon);
				}

				// get EBOOT.OLD or EBOOT.BIN to find decryption keys
				if(prometheus)
					sprintf(umd_file, "%s.EBOOT.OLD", root);
				else
					sprintf(umd_file, "%s.EBOOT.BIN", root);
			}

			is_dir = 0; // Using cached files in /dev_hdd0/tmp/wmtmp
		}
	}

	if(is_dir)
	{
		sprintf(umd_file, "%s/ICON0.PNG", root);
		do_mount = (file_copy(umd_file, icon_save_path) >= CELL_FS_SUCCEEDED);
	}

	if(do_mount)
	{
		u8 decrypt_patch = 1;

		u32 tag  = 0;
		u8 *keys = NULL;
		u8  code = 0;

		if(is_dir)
		{
			if(pspl1) sys_map_path(PSPL_ICON1, icon_save_path);
			if(pspl2) sys_map_path(PSPL_ICON2, icon_save_path);

			sprintf(umd_file, "%s/SYSDIR/EBOOT.OLD", root);
			if (file_exists(umd_file))
			{
				prometheus = 1;
			}
			else
			{
				sprintf(umd_file, "%s/SYSDIR/EBOOT.BIN", root);
			}
		}

		if (has_header || (read_file(umd_file, (char*)&header, sizeof(header), 0) == sizeof(header)))
		{
			int len;
			if(is_dir)
				len = sprintf(umd_file, "%s/", root);
			else
				len = sprintf(umd_file, "%s.", root);

			sprintf(umd_file + len, "PIC1.PNG");
			if(pspl1) file_copy(umd_file, (char*)PSPL_PATH1 "/PIC1.PNG");
			if(pspl2) file_copy(umd_file, (char*)PSPL_PATH2 "/PIC1.PNG");

			sprintf(umd_file + len, "PIC0.PNG");
			if(pspl1) file_copy(umd_file, (char*)PSPL_PATH1 "/PIC0.PNG");
			if(pspl2) file_copy(umd_file, (char*)PSPL_PATH2 "/PIC0.PNG");

			sprintf(umd_file + len, "SND0.AT3");
			if(pspl1) file_copy(umd_file, (char*)PSPL_PATH1 "/SND0.AT3");
			if(pspl2) file_copy(umd_file, (char*)PSPL_PATH2 "/SND0.AT3");

			sprintf(umd_file + len, "ICON1.PAM");
			if(pspl1) file_copy(umd_file, (char*)PSPL_PATH1 "/ICON1.PAM");
			if(pspl2) file_copy(umd_file, (char*)PSPL_PATH2 "/ICON1.PAM");

			if (header[0] == 0x7E505350) // "~PSP"
			{
				unsigned int i;

				decrypt_patch = 0;
				for (i = 0; i < NUM_SUPPORTED_TAGS; i++)
				{
					if (emulator_supported_tags355[i] == header[0xD0/4])
						break;
				}

				if (i == NUM_SUPPORTED_TAGS)
				{
					for (i = 0; i < NUM_EXTRA_KEYS; i++)
					{
						if (psp_extra_keys[i].tag == header[0xD0/4])
						{
							tag  = psp_extra_keys[i].tag;
							code = 0x5D; //psp_extra_keys[i].code;
							keys = psp_extra_keys[i].keys;
							break;
						}
					}
				}
			}

			if (do_eject)
			{
				cobra_send_fake_disc_eject_event();
				sys_storage_ext_umount_discfile();

				if (real_disctype != DISC_TYPE_NONE)
					cobra_send_fake_disc_insert_event();
			}

			sys_psp_set_umdfile(path, title_id, prometheus);
			sys_psp_set_decrypt_options(decrypt_patch, tag, keys, code, 0, NULL, 0);

			return CELL_FS_SUCCEEDED;
		}
	}

	return EIO;
}

/*
int cobra_set_psp_umd(char *path, char *umd_root, char *icon_save_path)
{
	CellFsStat stat;
	char umd_file[256];
	char title_id[11];
	char *root;
	unsigned int real_disctype, effective_disctype, iso_disctype;
	int ret;

	int decrypt_patch = 1;
	u32 tag = 0;
	u8 *keys = NULL;
	u8 code = 0;

	if (!path || !icon_save_path)
		return EINVAL;

	if (cellFsStat(PSPL_ICON1, &stat) != CELL_FS_SUCCEEDED)
	{
		return EABORT;
	}

	int fd; u8 sector[2048]; _memset(sector, sizeof(sector));

	if (cellFsOpen(path, CELL_FS_O_RDONLY, &fd, NULL, 0) == CELL_FS_SUCCEEDED)
	{
		u64 pos;

		if (cellFsLseek(fd, 0x8000, CELL_FS_SEEK_SET, &pos) == CELL_FS_SUCCEEDED)
		{
			cellFsRead(fd, sector, sizeof(sector), NULL);
		}
		cellFsClose(fd);
	}

	if (sector[0] != 1 || memcmp(sector+1, "CD001", 5) != CELL_FS_SUCCEEDED)
		return EIO;

	_memset(title_id, sizeof(title_id));
	memcpy(title_id, sector+0x373, 10);

	root = umd_root;

	if (!root)
	{
		char *files[1];
		int i;

		cobra_get_disc_type(&real_disctype, &effective_disctype, &iso_disctype);

		if (iso_disctype != DISC_TYPE_NONE)
			return EBUSY;

		if (effective_disctype != DISC_TYPE_NONE)
		{
			cobra_send_fake_disc_eject_event();
		}

		files[0] = path;
		ret = sys_storage_ext_mount_dvd_discfile(1, files);
		if (ret)
		{
			if (real_disctype != DISC_TYPE_NONE)
				cobra_send_fake_disc_insert_event();

			return ret;
		}

		cobra_send_fake_disc_insert_event();

		// Wait 0.5 seconds for automonter to mount iso
		for (i = 0; i < 25; i++)
		{
			CellFsStat stat;

			if (cellFsStat("/dev_bdvd", &stat) == CELL_FS_SUCCEEDED)
			{
				break;
			}

			sys_timer_usleep(20000);
		}

		if (i == 25)
		{
			cobra_send_fake_disc_eject_event();
			sys_storage_ext_umount_discfile();

			if (real_disctype != DISC_TYPE_NONE)
				cobra_send_fake_disc_insert_event();

			return EIO;
		}

		root = "/dev_bdvd";
	}
	else
	{
		real_disctype = DISC_TYPE_NONE;
	}

	snprintf(umd_file, sizeof(umd_file), "%s/PSP_GAME/ICON0.PNG", root);

	if (copy_file(umd_file, icon_save_path) == 0)
	{
		int fd;

		sys_map_path(PSPL_ICON1, icon_save_path);
		snprintf(umd_file, sizeof(umd_file), "%s/PSP_GAME/SYSDIR/prometheus.prx", root);

		if (cellFsStat(umd_file, &stat) != CELL_FS_SUCCEEDED)
		{
			snprintf(umd_file, sizeof(umd_file), "%s/PSP_GAME/SYSDIR/EBOOT.BIN", root);

			if (cellFsOpen(umd_file, CELL_FS_O_RDONLY, &fd, NULL, 0) == CELL_FS_SUCCEEDED)
			{
				u32 header[0xD4/4];
				u64 read;

				cellFsRead(fd, header, sizeof(header), &read);
				if (read == sizeof(header))
				{
					if (header[0] == 0x7E505350) // "~PSP"
					{
						unsigned int i;

						decrypt_patch = 0;
						for (i = 0; i < NUM_SUPPORTED_TAGS; i++)
						{
							if (emulator_supported_tags355[i] == header[0xD0/4])
								break;
						}

						if (i == NUM_SUPPORTED_TAGS)
						{
							unsigned int j;

							//DPRINTF("Tag not supported natively.\n");

							for (j = 0; j < NUM_EXTRA_KEYS; j++)
							{
								if (psp_extra_keys[j].tag == header[0xD0/4])
								{
									tag = psp_extra_keys[j].tag;
									code = psp_extra_keys[j].code;
									keys = psp_extra_keys[j].keys;
									//DPRINTF("Tag %08X found\n", psp_extra_keys[j].tag);
									break;
								}
							}

							if (j == NUM_EXTRA_KEYS)
							{
								//DPRINTF("No tag found. Game will crash.\n");
							}
						}
						else
						{
							//DPRINTF("Tag supported natively.\n");
						}
					}
				}
				cellFsClose(fd);
			}
			ret = 0;
		}
		else
		{
			ret = ENOTSUP;
		}
	}
	else
	{
		ret = EIO;
	}

	if (!umd_root)
	{
		cobra_send_fake_disc_eject_event();
		sys_storage_ext_umount_discfile();

		if (real_disctype != DISC_TYPE_NONE)
			cobra_send_fake_disc_insert_event();
	}

	if (ret == 0)
	{
		sys_psp_set_umdfile(path, title_id, 0);
		sys_psp_set_decrypt_options(decrypt_patch, tag, keys, code, 0, NULL, 0);
	}

	return ret;
}*/

/*
static int get_emu(char *title_id, char *title_name)
{
	for (int i = 0; i < N_TITLE_IDS; i++)
	{
		if (strcmp(title_id, emu_by_title_ids[i].key) == 0)
			return emu_by_title_ids[i].value;
	}

	if (title_name[0])
	{
		for (int i = 0; i < N_TITLE_NAMES; i++)
		{
			if (strcmp(title_name, emu_by_title_name[i].key) == 0)
				return emu_by_title_name[i].value;
		}
	}

	return EMU_355;
}

static int check_lambda(void)
{
	u8 *buf = malloc(512*KB);
	int fd;
	int ret;

	ret = cellFsOpen(PSPL_LAMBDA, CELL_FS_O_RDONLY, &fd, NULL, 0);
	if (ret == 0)
	{
		CellMd5WorkArea workarea;
		u8 md5[16];

		cellMd5BlockInit(&workarea);

		while (1)
		{
			u64 nread;

			cellFsRead(fd, buf, 512*KB, &nread);

			if (nread == 0)
				break;

			cellMd5BlockUpdate(&workarea, buf, nread);
		}

		cellFsClose(fd);
		cellMd5BlockResult(&workarea, md5);

		if (memcmp(md5, lambda_md5, 16) != 0)
		{
			ret = -1;
		}
	}

	free(buf);
	return ret;
}

int cobra_set_psp_umd2(char *path, char *umd_root, char *icon_save_path, u64 options)
{
	int ret;

	if (!path || !icon_save_path)
		return EINVAL;

	CellFsStat stat;

	if (cellFsStat(PSPL_ICON1, &stat) != CELL_FS_SUCCEEDED)
	{
		return EABORT;
	}

	if (cellFsStat(PSPL_LAMBDA, &stat) != CELL_FS_SUCCEEDED)
	{
		return ESYSVER;
	}

	//u8 sector[2048]; _memset(sector, sizeof(sector));
	u8 *sector = (u8*)malloc(1024);

	int fd;
	if (cellFsOpen(path, CELL_FS_O_RDONLY, &fd, NULL, 0) == CELL_FS_SUCCEEDED)
	{
		u64 pos;

		if (cellFsLseek(fd, 0x8000, CELL_FS_SEEK_SET, &pos) == CELL_FS_SUCCEEDED)
		{
			cellFsRead(fd, sector, sizeof(sector), NULL);
		}
		cellFsClose(fd);
	}

	if (sector[0] != 1 || memcmp(sector + 1, "CD001", 5) != 0) {free(sector); return EIO;}

	unsigned int real_disctype, effective_disctype, iso_disctype;

	char title_id[11];
	_memset(title_id, sizeof(title_id));
	memcpy(title_id, sector+0x373, 10);
	free(sector);

	char *root;

	root = umd_root;

	if (!root)
	{
		cobra_get_disc_type(&real_disctype, &effective_disctype, &iso_disctype);

		if (iso_disctype != DISC_TYPE_NONE)
			return EBUSY;

		if (effective_disctype != DISC_TYPE_NONE)
		{
			cobra_send_fake_disc_eject_event();
		}

		char *files[1];

		files[0] = path;
		ret = sys_storage_ext_mount_dvd_discfile(1, files);
		if (ret)
		{
			if (real_disctype != DISC_TYPE_NONE)
				cobra_send_fake_disc_insert_event();

			return ret;
		}

		cobra_send_fake_disc_insert_event();

		// Wait 0.5 seconds for automounter to mount iso
		if (wait_for("/dev_bdvd", 1) == FAILED)
		{
			cobra_send_fake_disc_eject_event();
			sys_storage_ext_umount_discfile();

			if (real_disctype != DISC_TYPE_NONE)
				cobra_send_fake_disc_insert_event();

			return EIO;
		}

		root = (char*)"/dev_bdvd";
	}
	else
	{
		real_disctype = DISC_TYPE_NONE;
	}

	//int emu = options & 0xF;

	int prometheus = 0;
	int decrypt_patch = 1;

	u32 tag = 0;
	u8 *keys = NULL;
	u8 code = 0;

	char umd_file[256];

	snprintf(umd_file, sizeof(umd_file), "%s/PSP_GAME/ICON0.PNG", root);

	if(file_copy(umd_file, icon_save_path, 0) >= CELL_FS_SUCCEEDED)
	{
		int fd;

		sys_map_path(PSPL_ICON1, icon_save_path);
		snprintf(umd_file, sizeof(umd_file), "%s/PSP_GAME/SYSDIR/EBOOT.OLD", root);

		if (cellFsStat(umd_file, &stat) != CELL_FS_SUCCEEDED)
		{
			snprintf(umd_file, sizeof(umd_file), "%s/PSP_GAME/SYSDIR/EBOOT.BIN", root);
		}
		else
		{
			prometheus = 1;
		}

		if (cellFsOpen(umd_file, CELL_FS_O_RDONLY, &fd, NULL, 0) == CELL_FS_SUCCEEDED)
		{
			u32 header[0xD4/4];
			u64 read;

			cellFsRead(fd, header, sizeof(header), &read);
			if (read == sizeof(header))
			{
				if (header[0] == 0x7E505350) // "~PSP"
				{
					unsigned int i;

					decrypt_patch = 0;
					for (i = 0; i < NUM_SUPPORTED_TAGS; i++)
					{
						if (emulator_supported_tags355[i] == header[0xD0/4])
							break;
					}

					if (i == NUM_SUPPORTED_TAGS)
					{
						//DPRINTF("Tag not supported natively.\n");

						for (i = 0; i < NUM_EXTRA_KEYS; i++)
						{
							if (psp_extra_keys[i].tag == header[0xD0/4])
							{
								tag = psp_extra_keys[i].tag;
								code = psp_extra_keys[i].code;
								keys = psp_extra_keys[i].keys;
								//DPRINTF("Tag %08X found\n", psp_extra_keys[i].tag);
								break;
							}
						}
						if (i == NUM_EXTRA_KEYS)
						{
							//DPRINTF("No tag found. Game will crash.\n");
						}

					}
					else
					{
						//DPRINTF("Tag supported natively.\n");
					}
				}
			}
			cellFsClose(fd);
		}

		char title_name[256];

		if (emu == EMU_AUTO)
		{
			snprintf(umd_file, sizeof(umd_file), "%s/PSP_GAME/PARAM.SFO", root);
			if (parse_param_sfo(umd_file, "TITLE", title_name))
			{
				title_name[0] = 0;
			}

			emu = get_emu(title_id, title_name);
		}

		ret = 0;
	}
	else
	{
		ret = EIO;
	}

	if (!umd_root)
	{
		cobra_send_fake_disc_eject_event();
		sys_storage_ext_umount_discfile();

		if (real_disctype != DISC_TYPE_NONE)
			cobra_send_fake_disc_insert_event();
	}

	if (ret == 0)
	{
		if (emu == EMU_400)
		{
			if (check_lambda() < 0)
				return ECANCELED;

			sys_storage_ext_mount_encrypted_image((char*)PSPL_LAMBDA, (char*)"/dev_moo", (char*)"CELL_FS_FAT", PSPL_LAMBDA_NONCE);
			sys_psp_change_emu_path("/dev_moo/pspemu");
		}

		sys_psp_set_umdfile(path, title_id, prometheus);
		sys_psp_set_decrypt_options(decrypt_patch, tag, keys, code, 0, NULL, 0);
	}

	return ret;
}

int cobra_get_usb_device_name(char *mount_point, char *dev_name)
{
	int ret;
	u64 i, size, device;
	CellFsMountInfo *info;
	sys_device_handle_t handle;

	ret = cellFsUtilGetMountInfoSize(&size);
	if (ret)
		return ret;

	info = malloc(size*sizeof(CellFsMountInfo));
	ret = cellFsUtilGetMountInfo(info, size, &size);
	if (ret)
	{
		free(info);
		return ret;
	}

	for (i = 0; i < size; i++)
	{
		if (strcmp(info[i].mount_point, mount_point) == 0)
			break;
	}

	if (i == size)
	{
		free(info);
		return -1;
	}

	device = get_device(info[i].block_dev);
	free(info);

	if (device == 0)
	{
		//DPRINTF("Cannot translate device name: %s\n", info[i].block_dev);
		return -1;
	}

	ret = sys_storage_open(device, 0, &handle, 0);
	if (ret == 0)
	{
		u8 cmd[64];
		_memset(cmd, sizeof(cmd));

		UfiCmdInquiry *inquiry_cmd = (UfiCmdInquiry *)(cmd+4);

		inquiry_cmd->opcode = UFI_CMD_INQUIRY;
		inquiry_cmd->alloc_length = sizeof(UfiInquiryResponse);
		cmd[23] = 1;

		ret = sys_storage_send_device_command(handle, STORAGE_COMMAND_NATIVE, cmd, 28, cmd, 64);
		if (ret == 0)
		{
			UfiInquiryResponse *response = (UfiInquiryResponse *)(cmd+28);

			memcpy(dev_name, response->product, 16);
			dev_name[17] = 0;
			trim(dev_name);
		}
		else
		{
			//DPRINTF("sys_storage_send_device_command failed: %x\n", ret);
		}

		sys_storage_close(handle);
	}
	else
	{
		//DPRINTF("sys_storge_open failed: %x\n", ret);
	}

	return ret;
}

int cobra_get_version(u16 *cobra_version, u16 *ps3_version)
{
	u32 version1;
	u16 version2;
	int ret;

	ret = sys_get_version(&version1);
	if (ret)
		return ret;

	if (cobra_version && sys_get_version2(&version2) == 0)
	{
		*cobra_version = version2;
	}
	else if (cobra_version)
	{
		switch (version1&0xFF)
		{
			case 1:
				*cobra_version = 0x0102;
			break;

			case 2:
				*cobra_version = 0x0200;
			break;

			case 3:
				*cobra_version = 0x0300;
			break;

			case 4:
				*cobra_version = 0x0310;
			break;

			case 5:
				*cobra_version = 0x0320;
			break;

			case 6:
				*cobra_version = 0x0330;
			break;

			case 7:
				*cobra_version = 0x0400;
			break;

			default:
				*cobra_version = 0x0410;
			break;
		}
	}

	if (ps3_version)
	{
		*ps3_version = ((version1>>8)&0xFFFF);

		if (*ps3_version == 0x0000)
		{
			*ps3_version = 0x0341;
		}
	}

	return 0;
}

int cobra_led_control(unsigned int led)
{
	return cobra_usb_command(CMD_LED_CONTROL, TYPE_HOST2DEV, led, NULL, 0);
}

int cobra_build_netiso_params(void *param_buf, char *server, u16 port, char *remote_path, int emu_mode, int num_tracks, TrackDef *tracks)
{
	netiso_args *args = (netiso_args *)param_buf;
	ScsiTrackDescriptor *scsi_tracks = (ScsiTrackDescriptor *)&args->tracks[0];

	_memset(param_buf, 65536);

	if (emu_mode < EMU_PS3 || emu_mode >= EMU_MAX || num_tracks >= MAX_TRACKS)
		return EINVAL;


	if (strlen(server) >= 0x40 || strlen(remote_path) >= 0x420)
		return EINVAL;

	strcpy(args->server, server);
	strcpy(args->path, remote_path);
	args->port = port;
	args->emu_mode = emu_mode;
	args->numtracks = num_tracks;

	if (tracks)
	{
		for (int i = 0; i < num_tracks; i++)
		{
			scsi_tracks[i].adr_control = (tracks[i].is_audio) ? 0x10 : 0x14;
			scsi_tracks[i].track_number = i+1;
			scsi_tracks[i].track_start_addr = tracks[i].lba;
		}
	}

	return 0;
}
*/

int cobra_get_ps2_emu_type(void)
{
	int ret;
	u8 hw_config[8], ret2;

	ret = sys_get_hw_config(&ret2, hw_config);
	if (ret)
	{
		return ret;
	}

	if (hw_config[6] & 1)
	{
		ret = PS2_EMU_HW;
	}
	else if (hw_config[0] & 0x20)
	{
		ret = PS2_EMU_GX;
	}
	else
	{
		ret = PS2_EMU_SW;
	}

	return ret;
}

int cobra_read_config(CobraConfig *cfg)
{
	if(!cfg) return EINVAL;

	_memset((u8*)cfg, sizeof(CobraConfig));

	u16 cobra_version;
	sys_get_version2(&cobra_version);

	if(cobra_version <= 0x0820)
		cfg->size = sizeof(CobraConfig_Old);
	else
		cfg->size = sizeof(CobraConfig);

	return sys_read_cobra_config(cfg);
}

int cobra_write_config(CobraConfig *cfg)
{
	if(!cfg) return EINVAL;

	u16 cobra_version;
	sys_get_version2(&cobra_version);

	if(cobra_version <= 0x0820)
		cfg->size = sizeof(CobraConfig_Old);
	else
		cfg->size = sizeof(CobraConfig);

	return sys_write_cobra_config(cfg);
}

int sys_get_version2(u16 *version)
{
	system_call_2(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_GET_VERSION2, (u64)(u32)version);
	return (int)p1;
}

int cobra_load_vsh_plugin(unsigned int slot, const char *path, void *arg, u32 arg_size)
{
	system_call_5(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_LOAD_VSH_PLUGIN, slot, (u64)(u32)path, (u64)(u32)arg, arg_size);
	return (int)p1;
}

int cobra_unload_vsh_plugin(unsigned int slot)
{
	system_call_2(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_UNLOAD_VSH_PLUGIN, slot);
	return (int)p1;
}
