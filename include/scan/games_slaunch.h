#ifdef SLAUNCH_FILE

#define MAX_SLAUNCH_ITEMS	5000

static bool custom_icon = false; // override default cover mode using index.ps3?cover=<cover type> (icon0, mm, disc, online)

typedef struct // 1MB for 2000+1 titles
{
	u8  type;
	char id[10];
	u8  path_pos; // start position of path
	u16 icon_pos; // start position of icon
	u16 padd;
	char name[508]; // name + path + icon
} __attribute__((packed)) _slaunch;

#ifdef WM_REQUEST
static bool photo_gui = true;
static bool usb0_picture_exists = false;
#endif

static int create_slaunch_file(void)
{
	#ifdef WM_REQUEST
	usb0_picture_exists = photo_gui && isDir("/dev_usb000/PICTURE");
	#endif
	int fd; custom_icon = false;
	if(cellFsOpen(SLAUNCH_FILE, CELL_FS_O_CREAT | CELL_FS_O_WRONLY | CELL_FS_O_TRUNC, &fd, NULL, 0) == CELL_FS_SUCCEEDED)
		return fd;
	else
		return 0;
}

static void add_slaunch_entry(int fd, const char *neth, const char *path, const char *filename, const char *icon, const char *name, const char *id, u8 f1)
{
	if(!fd) return;

	_slaunch slaunch; _memset(&slaunch, sizeof(_slaunch));

	char c, enc_filename[MAX_PATH_LEN];

	slaunch.type =  IS_ROMS_FOLDER ? TYPE_ROM :
					IS_PS3_TYPE    ? TYPE_PS3 :
					IS_PSXISO      ? TYPE_PS1 :
					IS_PS2ISO      ? TYPE_PS2 :
					IS_PSPISO      ? TYPE_PSP : TYPE_VID;

	#ifdef WM_REQUEST
	if(usb0_picture_exists)
	{
		u8 f1 = (slaunch.type == TYPE_PS1) ? id_PSXISO :
				(slaunch.type == TYPE_PS2) ? id_PS2ISO :
				(slaunch.type == TYPE_PS3) ? id_PS3ISO :
				(slaunch.type == TYPE_PSP) ? id_PSPISO :
				(slaunch.type == TYPE_VID) ? id_BDISO  : id_ROMS;

		concat_path(enc_filename, "/dev_usb000/PICTURE", paths[f1]); cellFsMkdir(enc_filename, DMODE);
		strcat(enc_filename, filename);
		strcat(enc_filename, strcasestr(icon, ".png") ? ".PNG" : ".JPG");

		if(not_exists(enc_filename)) file_copy(icon, enc_filename);
	}
	#endif

	strncopy(slaunch.id, sizeof(slaunch.id), id); urlenc_ex(enc_filename, filename, false);

	slaunch.path_pos = strncopy(slaunch.name, 128, name) + 1;

	if(slaunch.type == TYPE_PS2 && strcasestr(filename, ".BIN.ENC")) c = '.'; else c = '_';

	slaunch.icon_pos = snprintf(slaunch.name + slaunch.path_pos, 454 - slaunch.path_pos, "/mount%cps3%s%s/%s", c, neth, path, enc_filename) + slaunch.path_pos + 1;
					   strncopy(slaunch.name + slaunch.icon_pos, 507 - slaunch.icon_pos, icon);

	cellFsWrite(fd, (void *)&slaunch, sizeof(_slaunch), NULL);
}

static void close_slaunch_file(int fd)
{
	if(!fd) return;

	cellFsClose(fd);
	cellFsChmod(SLAUNCH_FILE, MODE);
}

static int find_slaunch_game(char *filename, u8 offset)
{
	int fsl = 0, ret = 0;
	if(cellFsOpen(SLAUNCH_FILE, CELL_FS_O_RDONLY, &fsl, NULL, 0) == CELL_FS_SUCCEEDED)
	{
		_slaunch slaunch; u64 read_e;
		char *name = slaunch.name;
		char filename2[MAX_PATH_LEN]; urlenc_ex(filename2, filename, false);
		while(cellFsRead(fsl, &slaunch, sizeof(_slaunch), &read_e) == CELL_FS_SUCCEEDED && read_e > 0)
		{
			char *path = slaunch.name + slaunch.path_pos;
			if((strcasestr(path, filename2) == NULL) && (strcasestr(name, filename) == NULL)) continue;
			ret = strcopy(filename, path + offset); break;
		}
		cellFsClose(fsl);
	}
	return ret;
}
#endif // #ifdef SLAUNCH_FILE
