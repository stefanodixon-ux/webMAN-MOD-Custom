#define SYSCALL8_OPCODE_ENABLE_PS2NETEMU	0x1ee9	/* Cobra 7.50 */
#define PS2NETEMU_GET_STATUS				2

static bool is_BIN_ENC(const char *filename)
{
	return !extcmp(filename, ".BIN.ENC", 8);
}

#ifdef COBRA_ONLY
static int get_cobra_ps2netemu_status(void)
{
	system_call_2(SC_COBRA_SYSCALL8, (u64) SYSCALL8_OPCODE_ENABLE_PS2NETEMU, (u64) PS2NETEMU_GET_STATUS);
	return (int)p1;
}

static void enable_ps2netemu_cobra(int emu)
{
#ifdef SPOOF_CONSOLEID
	if ((eid0_idps[0] & 0x00000000000000FF) > 0x04) return; // 0x01 = CECH-A*, 0x02 = CECH-B, 0x03 = CECH-C, 0x04 = CECH-E
#endif
	int status = get_cobra_ps2netemu_status(); // 0 = default ps2 emulator, 1 = ps2_netemu

	if(status < 0 || status == emu) return;

	system_call_2(SC_COBRA_SYSCALL8, (u64) SYSCALL8_OPCODE_ENABLE_PS2NETEMU, (u64) emu);

	if(pad_data.len > 0)
	{
		show_msg2(emu ? "ps2_netemu.self" : "ps2_emu.self", STR_ENABLED);
	}
}
#endif

// called from mount_misc.h
static void copy_ps2icon(char *imgfile, const char *_path)
{
	char pic[64]; sprintf(pic, PS2_CLASSIC_ISO_ICON);
	concat2(imgfile, PS2_CLASSIC_ISO_ICON, ".bak");

	if(not_exists(imgfile))
		force_copy(pic, imgfile);

	int len = concat2(imgfile, _path, ".png"); len -= 12;
	if(not_exists(imgfile)) concat2(imgfile, _path, ".PNG");
	if(not_exists(imgfile) && (len > 0)) strcopy(imgfile + len, ".png"); // remove .BIN.ENC
	if(not_exists(imgfile) && (len > 0)) strcopy(imgfile + len, ".PNG");

	if(file_exists(imgfile))
		cellFsUnlink(pic);
	else
		concat2(imgfile, PS2_CLASSIC_ISO_ICON, ".bak");

	force_copy(imgfile, pic);

	for(u8 i = 0; i <= 2; i++)
	{
		// backup original picture
		sprintf(imgfile, "%s/PIC%i.PNG.bak", PS2_CLASSIC_LAUCHER_DIR, i);
		if(not_exists(imgfile))
		{
			sprintf(pic, "%s/PIC%i.PNG", PS2_CLASSIC_LAUCHER_DIR, i);
			force_copy(pic, imgfile);
		}

		// get game picture from /PS2ISO
		sprintf(imgfile, "%s.PIC%i.PNG", _path, i);
		if(not_exists(imgfile) && (len > 0)) sprintf(imgfile + len, ".PIC%i.PNG", i); // remove .BIN.ENC

		// replace picture in PS2 Classic Launcher
		sprintf(pic, "%s/PIC%i.PNG", PS2_CLASSIC_LAUCHER_DIR, i);
		cellFsUnlink(pic);
		if(file_exists(imgfile))
			force_copy(imgfile, pic);
		else
		{
			sprintf(imgfile, "%s/PIC%i.PNG.bak", PS2_CLASSIC_LAUCHER_DIR, i); // restore original
			force_copy(imgfile, pic);
		}
	}
}

static void get_ps_titleid_from_path(char *title_id, const char *_path)
{
	// returns title_id as "SCES00000"

	if(!title_id) return;

	if(!_path) {*title_id = NULL; return;}

	const char *path = _path; if(*_path == '/') get_filename(_path);
	const char *game_id = strstr(_path, " [S"); // title id enclosed in square brackets

	if(game_id)
		path = game_id + 2;
	else
	{
		game_id = strstr(_path, "[S"); // title id enclosed in square brackets
		if(game_id)
			path = game_id + 1;
		else
		{
			game_id = strstr(_path, "(S"); // title id enclosed in round brackets
			if(game_id) path = game_id + 1;
		}
	}

	// SLES 5052 games		// SCES 5003 games
	// SLUS 2325 games		// SCUS  604 games
	// SLPS 2119 games		// SCPS  735 games
	// SLPM 3488 games		// SCPM  ??? games
	// SLKA  481 games		// SCKA  151 games
	// SLAJ  175 games		// SCAJ  397 games

	// TCPS  178 games

		game_id = strstr(path, "SL"); // 13640 games SLES/SLUS/SLPM/SLPS/SLAJ/SLKA
	if(!game_id)
		game_id = strstr(path, "SC"); // 6890 games SCES/SCUS/SCPS/SCAJ/SCKA
	if(!game_id)
		game_id = strstr(path, "TC"); // 178 games TCPS/ ??? TCES
	if(!game_id)
		game_id = strstr(path, "TL"); // ??? games TLES
	if(!game_id)
		game_id = strstr(path, "SRPM"); // ??? games SRPM

	u16 len = 0; if(game_id) while(game_id[len] && ++len < 12);

	if(!game_id || len < 9 || game_id[5] < '0' || game_id[5] > '9' || !strchr("EUPKA", game_id[2]))
		*title_id = NULL;
	else if(game_id[4] == '_' && len >= 11)
		sprintf(title_id, "%.4s%.3s%.2s", game_id, game_id + 5, game_id + 9); // SLxS_000.00
	else if(game_id[4] == '-' && len >= 10)
		sprintf(title_id, "%.4s%.5s", game_id, game_id + 5); // SLxS-00000
	else
		sprintf(title_id, "%.9s", game_id); // SLxS00000
}

#ifdef COBRA_NON_LITE
static bool copy_ps2config_iso(char *entry_name, char *_path)
{
	const char *tempID = to_upper(entry_name);
	if (
		(tempID[1] == 'L' || tempID[1] == 'C') &&
		(tempID[2] == 'U' || tempID[2] == 'E' || tempID[2] == 'P' || tempID[2] == 'A' || tempID[2] == 'H' || tempID[2] == 'J' || tempID[2] == 'K') &&
		(tempID[3] == 'S' || tempID[3] == 'M' || tempID[3] == 'J' || tempID[3] == 'A') &&
		(tempID[4] == '_' && tempID[8] == '.') &&
		ISDIGIT(tempID[5]) &&
		ISDIGIT(tempID[6]) &&
		ISDIGIT(tempID[7]) &&
		ISDIGIT(tempID[9])
	   )
	{
		char temp[STD_PATH_LEN];
		concat_path2(temp, PS2CONFIG_PATH, tempID, ".CONFIG");
		if(file_exists(temp))
			force_copy(temp, _path);
		else
		{
			const char *config_path[4] = {"CUSTOM", "NET", "GX", "SOFT"};
			for(u8 i = 0; i < 4; i++)
			{
				sprintf(temp, "%s/CONFIG/%s/%s.CONFIG", PS2CONFIG_PATH, config_path[i], tempID);
				if(file_exists(temp)) {force_copy(temp, _path); return true;}
				sprintf(temp, "%s/sys/CONFIG/%s/%s.CONFIG", MANAGUNZ, config_path[i], tempID);
				if(file_exists(temp)) {force_copy(temp, _path); return true;}
			}
		}
		return true;
	}
	return false;
}
#endif

static void copy_ps2config(char *config, const char *_path)
{
	char config_path[STD_PATH_LEN];
	size_t len = concat2(config, _path, ".CONFIG"); // <name>.BIN.ENC.CONFIG
	strcpy(config_path, config);

	if(not_exists(config) && len > 15) strcopy(config + len - 15, ".CONFIG"); // remove .BIN.ENC
	if(not_exists(config))
	{
		char title_id[TITLE_ID_LEN + 1];
		get_ps_titleid_from_path(title_id, _path);

		if(strlen(title_id) == TITLE_ID_LEN)
		{
			sprintf(config, "%s%s/%.4s_%.3s.%.2s.ENC", PS2CONFIG_PATH, "/CONFIG/ENC",
							title_id,      // SLES, SLUS, SLPM, SLPS, SCES, SCUS, SCPS
							title_id + 4,  // _000.00
							title_id + 7); // SLxS00000
		}
	}

	cellFsUnlink(PS2_CLASSIC_ISO_CONFIG);
	force_copy(config, (char*)PS2_CLASSIC_ISO_CONFIG);

	if(!webman_config->ps2config && not_exists(config_path)) force_copy(config, config_path);
}

static void copy_ps2savedata(char *vme, const char *_path)
{
	char savedata_vme[64], savedata_bak[64];

	for(u8 len, i = 0; i < 2; i++)
	{
		len = sprintf(vme, "%s.SCEVMC%i.VME", _path, i); // <name>.BIN.ENC.SCEVMC0.VME
		if(not_exists(vme) && (len > 20)) sprintf(vme + len - 20, ".SCEVMC%i.VME", i); // remove .BIN.ENC

		for(u8 v = 0; v < 2; v++)
		{
			if(v)
				sprintf(savedata_vme, "%s/SCEVMC%i.VME", "/dev_hdd0/savedata/vmc", i);
			else
				sprintf(savedata_vme, "%s/SAVEDATA/SCEVMC%i.VME", PS2_CLASSIC_PLACEHOLDER, i);

			concat2(savedata_bak, savedata_vme, ".bak");
			if(file_exists(vme))
			{
				cellFsRename(savedata_vme, savedata_bak); // backup default vme
				force_copy(vme, savedata_vme);
			}
			else if(file_exists(savedata_bak))
			{
				cellFsUnlink(savedata_vme);
				cellFsRename(savedata_bak, savedata_vme); // restore backup vme
			}

			len = strcopy(vme, savedata_vme); vme[len - 5] = '1' - i;

			if(not_exists(vme))
				force_copy(savedata_vme, vme);
		}
	}
}

#ifndef LITE_EDITION

static void enable_classic_ps2_mode(void)
{
	create_file(PS2_CLASSIC_TOGGLER);
}

static void disable_classic_ps2_mode(void)
{
	cellFsUnlink(PS2_CLASSIC_TOGGLER);
}

#endif