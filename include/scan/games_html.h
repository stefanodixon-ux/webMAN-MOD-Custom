#define SUFIX(a)	((a==1)? "_1" :(a==2)? "_2" :(a==3)? "_3" :(a==4)?"_4":"")
#define SUFIX2(a)	((a==1)?" (1)":(a==2)?" (2)":(a==3)?" (3)":(a==4)?" (4)":"")
#define SUFIX3(a)	((a==1)?" (1).ntfs[":(a==2)?" (2).ntfs[":(a==3)?" (3).ntfs[":(a==4)?" (4).ntfs[":"")

// paths: 0="GAMES", 1="GAMEZ", 2="PS3ISO", 3="BDISO", 4="DVDISO", 5="PS2ISO", 6="PSXISO", 7="PSXGAMES", 8="PSPISO", 9="ISO", 10="video", 11="GAMEI", 12="ROMS"

enum paths_ids
{
	id_GAMES    = 0,
	id_GAMEZ    = 1,
	id_PS3ISO   = 2,
	id_BDISO    = 3,
	id_DVDISO   = 4,
	id_PS2ISO   = 5,
	id_PSXISO   = 6,
	id_PSXGAMES = 7,
	id_PSPISO   = 8,
	id_ISO      = 9,
	id_VIDEO    = 10,
	id_GAMEI    = 11,
	id_ROMS     = 12,
	id_NPDRM    = 99,
};

#define IS_ISO_FOLDER   ((f1>=id_PS3ISO) && (f1!=id_GAMEI) && (f1!=id_VIDEO))
#define IS_PS3_TYPE     ((f1<=id_PS3ISO) || (f1==id_VIDEO || (f1==id_GAMEI)))
#define IS_BLU_TYPE     ((f1<=id_BDISO)  || (f1==id_VIDEO || (f1==id_GAMEI)))

#define IS_JB_FOLDER    ((f1<=id_GAMEZ) || (f1==id_VIDEO) || (f1==id_GAMEI))
#define IS_PS3ISO        (f1==id_PS3ISO)
#define IS_BDISO         (f1==id_BDISO)
#define IS_DVDISO        (f1==id_DVDISO)
#define IS_PS2ISO        (f1==id_PS2ISO)
#define IS_PSXISO       ((f1==id_PSXISO) || (f1==id_PSXGAMES))
#define IS_PSPISO       ((f1==id_PSPISO) || (f1==id_ISO))
#define IS_ISO_DIR       (f1==id_ISO)
#define IS_VIDEO_FOLDER  (f1==id_VIDEO)
#define IS_GAMEI_FOLDER  (f1==id_GAMEI)
#define IS_ROMS_FOLDER   (f1==id_ROMS)

#define IS_HDD0          (f0 == 0)
#define IS_NTFS          (f0 == NTFS)
#define IS_NET           (f0 >= NET && f0 < NTFS)

#define INFO_PATH		0x00
#define INFO_PATH_ID	0x01
#define INFO_ID			0x02
#define INFO_NONE		0x03
#define INFO_TAGS		0x10
#define INFO_TAGS_ONLY	0x13
#define INFO_VER		0x20

#define TYPE_ALL 0
#define TYPE_PS1 1
#define TYPE_PS2 2
#define TYPE_PS3 3
#define TYPE_PSP 4
#define TYPE_VID 5
#define TYPE_ROM 6
#define TYPE_MAX 7

#define GAME_DIV_PREFIX  "<div class=\"gc\"><div class=\"ic\"><a href=\"/mount.ps3"
#define GAME_DIV_SUFIX   "</a></div></div>"

#define GAME_DIV_SIZE    72 // strlen(GAME_DIV_PREFIX + GAME_DIV_SUFIX) = (56 + 16)

#define HTML_KEY_LEN  6

#define HTML_KEY    -1

typedef struct
{
	char key[HTML_KEY_LEN];
	char data[];
} t_new_entry;

#include "games_launchpad.h"
#include "games_slaunch.h"
#include "games_covers.h"

#if defined(MOUNT_GAMEI) || defined(MOUNT_ROMS)
 static u8 f1_len = 13;       // VIDEO + GAMEI + ROMS
#else
 static const u8 f1_len = 11; // VIDEO
#endif

static u8 loading_games = 0;

static void add_title_id(char *title, char *title_id)
{
	// invalid character breaks XML / HTML
	replace_char(title, '<', ' ');

	remove_brackets(title);

	if(webman_config->tid && HAS_TITLE_ID)
	{
		int tlen = strlen(title); if((tlen < 50) && !strstr(title, " [")) sprintf(title + tlen, " [%s]", title_id);
	}
}

#ifdef NET_SUPPORT
//static bool is_iso_file(char *entry_name, int flen, u8 f1, u8 f0);
static int add_net_game(int ns, netiso_read_dir_result_data *data, int v3_entry, char *neth, char *param, char *title, char *tempstr, char *enc_dir_name, char *icon, char *title_id, char *app_ver, u8 f1, u8 is_html)
{
	*app_ver = NULL;

	if((data[v3_entry].name[0] == '.') || strchr(data[v3_entry].name, '<')) return FAILED;

	if(data[v3_entry].is_directory == false)
	{
		const char *ext = get_ext(data[v3_entry].name);
		#ifdef MOUNT_ROMS
		if(IS_ROMS_FOLDER)
		{
			if(ROMS_EXTENSIONS && !strcasestr(ROMS_EXTENSIONS + SKIP_CUE, ext)) return FAILED;
		}
		else
		#endif
		if(IS_PSPISO && strstr(data[v3_entry].name, ".EBOOT.")) return FAILED;
		else
			if(!strcasestr(ISO_EXTENSIONS + 8, ext)) return FAILED;
	}

	*icon = *title_id = NULL;

	char *local_path = title;
	char *remote_path = enc_dir_name;

	// skip duplicated games in /dev_hdd0
	sprintf(local_path, "%s%s/%s", drives[0], param, data[v3_entry].name);
	if(file_exists(local_path)) return FAILED;

	// get name
	if(IS_PS3_TYPE) //PS3 games only (0="GAMES", 1="GAMEZ", 2="PS3ISO", 10="video", 11="GAMEI")
	{
		if(data[v3_entry].is_directory)
			concat_path2(local_path, WMTMP, data[v3_entry].name, ".SFO");
		else
			{get_name(local_path, data[v3_entry].name, GET_WMTMP); strcat(local_path, ".SFO");}

		if(not_exists(local_path))
		{
			if(data[v3_entry].is_directory)
			{
				if(IS_GAMEI_FOLDER)
					concat_path2(remote_path, param, data[v3_entry].name, "/PARAM.SFO");
				else
					concat_path2(remote_path, param, data[v3_entry].name, "/PS3_GAME/PARAM.SFO");

				copy_net_file(local_path, remote_path, ns);

				strcpy(local_path + strlen(local_path) - 4, ".png");
				strcpy(remote_path + strlen(remote_path) - 9, "ICON0.PNG");
			}
			else
			{
				get_name(tempstr, data[v3_entry].name, NO_EXT);
				concat_path2(remote_path, param, tempstr, ".SFO");
			}
			copy_net_file(local_path, remote_path, ns);
		}

		if(webman_config->info & INFO_VER) getTitleID(local_path, app_ver, GET_VERSION);

		get_title_and_id_from_sfo(title, title_id, data[v3_entry].name, icon, tempstr, 0);

		get_local_app_ver(app_ver, title_id, tempstr);
	}
	else if(is_html)
		{get_name(enc_dir_name, data[v3_entry].name, NO_EXT); htmlenc(title, enc_dir_name, 1);}
	else
		{get_name(title, data[v3_entry].name, NO_EXT);}

	// check for /title/title.iso
	if(data[v3_entry].is_directory && BETWEEN(id_PS3ISO, f1, id_PSPISO))
	{
		for(u8 e = 0; e < 11; e++)
		{
			if(e >= 10) return FAILED;

			sprintf(tempstr, "%s/%s/%s%s", param, data[v3_entry].name, data[v3_entry].name, iso_ext[e]);
			if(remote_file_exists(ns, tempstr)) break;
		}

		u8 index = 4;

		// cover: folder/filename.jpg
		for(u8 e = 0; e < 4; e++)
		{
			sprintf(remote_path, "%s/%s/%s%s", param, data[v3_entry].name, data[v3_entry].name, ext[ex[e]]);
			if(remote_file_exists(ns, remote_path)) {index = ex[e]; swap_ex(e); break;}
		}

		if(index < 4)
		{
			get_name(icon, data[v3_entry].name, GET_WMTMP); strcat(icon, ext[index]);
			copy_net_file(icon, remote_path, ns);

			if(not_exists(icon)) *icon = NULL;
		}

		strcopy(data[v3_entry].name, tempstr + strlen(param) + 1);
	}

	add_title_id(title, title_id);
	urlenc(enc_dir_name, data[v3_entry].name);

	get_default_icon(icon, param, data[v3_entry].name, data[v3_entry].is_directory, title_id, ns, ((neth[4] & 0x0F) + NET), f1);

	if(SHOW_COVERS_OR_ICON0 && (NO_ICON || (webman_config->nocov == SHOW_ICON0))) {get_name(tempstr, data[v3_entry].name, GET_WMTMP); strcat(tempstr, ".PNG"); if(file_exists(tempstr)) strcpy(icon, tempstr);}

	return CELL_OK;
}
#endif // #ifdef NET_SUPPORT

static void add_query_html(char *buffer, const char *param)
{
	char html[64];
	sprintf(html,"[<a href=\"/index.ps3?%s\">%s</a>] ",
														param,
														to_upper(strfmt("%s", param))); // label
	concat(buffer, html);
}

static int check_drive(u8 f0)
{
	if(f0 >= MAX_DRIVES) return FAILED;

	if(!webman_config->usb0 && (f0 == 1)) return FAILED;
	if(!webman_config->usb1 && (f0 == 2)) return FAILED;
	if(!webman_config->usb2 && (f0 == 3)) return FAILED;
	if(!webman_config->usb3 && (f0 == 4)) return FAILED;
	if(!webman_config->usb6 && (f0 == 5)) return FAILED;
	if(!webman_config->usb7 && (f0 == 6)) return FAILED;

	// f0 -> 7 to 11 (net), 12 ntfs/ext

	if(!webman_config->dev_sd && (f0 == 13)) return FAILED;
	if(!webman_config->dev_ms && (f0 == 14)) return FAILED;
	if(!webman_config->dev_cf && (f0 == 15)) return FAILED;

	#ifdef COBRA_ONLY
	if( (IS_NTFS) && (!webman_config->ntfs) && (!webman_config->usb0 && !webman_config->usb1 && !webman_config->usb2 &&
												!webman_config->usb3 && !webman_config->usb6 && !webman_config->usb7)) return FAILED;
	#else
	if(IS_NTFS) return FAILED; // is_ntfs (nonCobra)
	#endif

	// is_net
	#ifdef NET_SUPPORT
	if(IS_NET && !is_netsrv_enabled(f0 - NET)) return FAILED; //net
	#else
	if(IS_NET) return FAILED; // is_net (LITE_EDITION)
	#endif

	return CELL_OK;
}

static int check_content_type(u8 f1)
{
	if( (webman_config->cmask & PS3) && IS_PS3_TYPE) return FAILED; // 0="GAMES", 1="GAMEZ", 2="PS3ISO", 10="video", 11="GAMEI"
	if( (webman_config->cmask & BLU) && IS_BDISO   ) return FAILED;
	if( (webman_config->cmask & DVD) && IS_DVDISO  ) return FAILED;
	if( (webman_config->cmask & PS2) && IS_PS2ISO  ) return FAILED;
	if( (webman_config->cmask & PS1) && IS_PSXISO  ) return FAILED;
	if( (webman_config->cmask & PSP) && IS_PSPISO  ) return FAILED;
	#ifdef MOUNT_ROMS
	if((!webman_config->roms)        && IS_ROMS_FOLDER) return FAILED;
	#endif
	return CELL_OK;
}

static char *is_multi_cd(const char *name)
{
	char   *p = strstr(name, "CD");
	if(!p) {p = strcasestr(name, "Vol");
	if(!p)  p = strcasestr(name, "Disc");}
	return p;
}

static void set_sort_key(char *skey, char *title, int key, u8 subfolder, u8 f1)
{
	bool is_html = (key <= HTML_KEY);

	if(title[1] == '/')
		strcpy(title, title + 2); // skip single letter folder
	else
	{
		char *p = strchr(title, '/');
		if(p)
		{
			*p = NULL;
			if(islike(title, p + 1))
				strcpy(skey, p + 1);
			else
				sprintf(skey, "[%s] %s", title, p + 1);
			strcpy(title, skey);
		}
	}

	u16 tlen = strlen(title);
	if(tlen < HTML_KEY_LEN) strcat(title, "      "); // HTML_KEY_LEN = 6

	u8 c = 0, s = 5;
	if(title[0] == '[')
	{
		if(title[4] == ']') {c = (title[5] != ' ') ? 5 : 6;} // ignore tag prefixes. e.g. [PS3] [PS2] [PSX] [PSP] [DVD] [BDV] [ISO] etc.
		if(title[3] == ']') {c = (title[4] != ' ') ? 4 : 5;} // ignore tag prefixes. e.g. [CD]
		if(title[2] == ']') {c = (title[3] != ' ') ? 3 : 4;} // ignore tag prefixes. e.g. [C]
		if(c) strcat(title, "      ");
	}

	if(is_html)
	{
	#ifdef LAUNCHPAD
		if(webman_config->launchpad_grp || (key ==  HTML_KEY))
			strncopy(skey, HTML_KEY_LEN + 1, title);
		else
		{
			char group = IS_BDISO ? id_VIDEO : get_default_icon_by_type(f1);
			snprintf(skey, HTML_KEY_LEN + 1, "%c%s", group, title);
		}
	#else
		strncopy(skey, HTML_KEY_LEN + 1, title);
	#endif
	}
	else
		sprintf(skey, "!%.6s%04i", title + c, key);

	if(c == 0 && title[0] == '[') {char *s = strstr(title + 3, "] "); if(s) {skey[4]=s[2],skey[5]=s[3],skey[6]=s[4];}}
	//else if(subfolder) {char *s = strchr(title + 3, '/'); if(s) {skey[4]=s[1],skey[5]=s[2],skey[6]=s[3];}}

	title[tlen] = '\0';

	char *p = NULL, *nm = title + 5;
	if(f1 > id_PS3ISO)
	{
		p = is_multi_cd(nm);
		if( p) {while(*p && !ISDIGIT(*p)) ++p;}
	}
	if( p)
	{
		skey[6] = '0';
		for(; nm < p; nm++) if(ISDIGIT(*nm)) {skey[6] += (*nm - '0')<<4; break;}
		if(ISDIGIT(p[0])) skey[6] += (p[0] - '0'); if(ISDIGIT(p[1])) skey[6] += (p[1] - '0'); // sort by CD#
	}
	else
	{
		char w2 = 0;
		if(tlen > 64) tlen = 64;
		for(u16 i = s; i < tlen; i++)
		{
			if(title[i+1]=='[') {tlen = i; break;}
			if(title[i] == ' ') {s = i + 1; w2 = skey[6] = title[s]; break;} // sort by 2nd word
		}
		for(u16 i = tlen; i > s; i--)
		{
			if(ISDIGIT(title[i])) {if(w2) skey[5] = w2; skey[6] = title[i]; break;} // sort by game number (if possible)
		}
	}

	to_upper(skey + (is_html ? 0 : 1));
}

static bool is_dupe(u8 f0, u8 f1, const char *d_name, char *hdd_path)
{
	if(IS_HDD0 || IS_ROMS_FOLDER) return false;

	sprintf(hdd_path, "%s/%s/%s", drives[0], paths[f1], d_name);

#ifdef COBRA_ONLY
	if(IS_NTFS)
	{
		char *ext = strstr(hdd_path, ".ntfs["); if(!ext) return true;
		for(u8 e = 0; e < 10; e++)
			{strcpy(ext, iso_ext[e]); if(file_exists(hdd_path)) return true;}
		*ext = NULL;
	}
#endif

	return file_exists(hdd_path);
}

static bool is_iso_file(char *entry_name, int flen, u8 f1, u8 f0)
{
#if defined(COBRA_ONLY) || defined(MOUNT_ROMS)
	const char *ext = get_ext(entry_name + MAX(flen - 13, 0));
#endif
#ifdef MOUNT_ROMS
	if(IS_ROMS_FOLDER)
	{
		return ROMS_EXTENSIONS && strcasestr(ROMS_EXTENSIONS + SKIP_CUE, ext);
	}
	else
#endif
#ifdef COBRA_ONLY
	if(IS_NTFS)
		return islike(ext, ".ntfs[");
	else
	{
		if(IS_ISO_FOLDER && (_IS(ext, ".zip") || _IS(ext, ".7z"))) return true;

		return ((IS_ISO_FOLDER || IS_VIDEO_FOLDER)  && (
				(_IS(ext, ".iso") || _IS(entry_name + MAX(flen - 6, 0), ".iso.0"))  ||
				((IS_PS2ISO || IS_PSXISO || IS_DVDISO || IS_BDISO) && strcasestr(ISO_EXTENSIONS + 14, ext))
				));
	}
#else
	return (IS_PS2ISO && IS(entry_name + MAX(flen - 8, 0), ".BIN.ENC"));
#endif
}

static void set_scan_path(u8 li, u8 f0, u8 f1, u8 is_net, u8 uprofile, char *param)
{
	#ifdef NET_SUPPORT
	if(is_net)
	{
		char ll[4]; if(li) sprintf(ll, "/%c", '@'+li); else *ll = NULL;

		if(li == LANG_CUSTOM) 
			sprintf(param, "/%s%s",   paths[f1], AUTOPLAY_TAG);
		else
			sprintf(param, "/%s%s%s", paths[f1], SUFIX(uprofile), ll);
	}
	else
	#endif
	{
		if(IS_NTFS) //ntfs
			sprintf(param, WMTMP);
		else
		{
			if(li == LANG_CUSTOM)
				concat_path2(param, drives[f0], paths[f1], AUTOPLAY_TAG);
			else
				concat_path2(param, drives[f0], paths[f1], SUFIX(uprofile));

			// drives: 1="/dev_usb000", 2="/dev_usb001", 3="/dev_usb002", 4="/dev_usb003", 5="/dev_usb006", 6="/dev_usb007", 13="/dev_sd", 14="/dev_ms", 15="/dev_cf"
			if(f0 && isDir(param) == false)
				sprintf(param, "%s/PS3/%s%s", drives[f0], paths[f1], SUFIX(uprofile));
		}
	}

	show_progress(param, OV_SCAN);
}

static int qcompare_html(const void *a, const void *b)
{
	return memcmp((const char*)a, (const char*)b, HTML_KEY_LEN);
}

static bool game_listing(char *buffer, char *templn, char *param, char *tempstr, u8 mode, bool auto_mount)
{
	u16 retry = 0;
	u32 buf_len = strlen(buffer);

	struct CellFsStat buf;
	int fd;

	u8 _f1_;
	u8 mobile_mode = mode, launchpad_mode = (mode == LAUNCHPAD_MODE);

	#ifdef LAUNCHPAD
	if(launchpad_mode) cellFsUnlink(LAUNCHPAD_FILE_XML); else
	#endif
	if((!mobile_mode) && strstr(param, "/index.ps3"))
	{
		char *pbuffer = buffer + buf_len; concat(pbuffer, "<font style=\"font-size:13px\">");

	#ifdef COBRA_ONLY
		if(!(webman_config->cmask & PS3)) { add_query_html(pbuffer, "ps3");
											add_query_html(pbuffer, "games");
											add_query_html(pbuffer, "PS3ISO");
				if( webman_config->npdrm )  add_query_html(pbuffer, "npdrm");}

		if(!(webman_config->cmask & PS2))   add_query_html(pbuffer, "PS2ISO");
		if(!(webman_config->cmask & PSP))   add_query_html(pbuffer, "PSPISO");
		if(!(webman_config->cmask & PS1))   add_query_html(pbuffer, "PSXISO");
		if(!(webman_config->cmask & BLU))   add_query_html(pbuffer, "BDISO" );
		if(!(webman_config->cmask & DVD))   add_query_html(pbuffer, "DVDISO");

		#ifdef MOUNT_GAMEI
		if(webman_config->gamei) {add_query_html(pbuffer, "GAMEI");}
		#endif
		#ifdef MOUNT_ROMS
		if(webman_config->roms)  {add_query_html(pbuffer, "ROMS");}
		#endif
		#ifdef NET_SUPPORT
		if(webman_config->netd[0] || webman_config->netd[1] || webman_config->netd[2] || webman_config->netd[3] || webman_config->netd[4]) add_query_html(pbuffer, "net");
		#endif
		add_query_html(pbuffer, "hdd");
		add_query_html(pbuffer, "usb");
		add_query_html(pbuffer, "ntfs");
	#else
		if(!(webman_config->cmask & PS3)) add_query_html(pbuffer, "games");
		if(!(webman_config->cmask & PS2)) add_query_html(pbuffer, "PS2ISO");

		add_query_html(pbuffer, "hdd");
		add_query_html(pbuffer, "usb");
	#endif //#ifdef COBRA_ONLY

		// cookies script & icon size slider
		add_html(dat_SEL_ICON_SIZE, 0, pbuffer, templn);

		if(webman_config->sman) concat(pbuffer, "<p></font>");

		buf_len += strlen(pbuffer);
	}
	else
		buf_len += concat(buffer + buf_len, " <br>");

	// --- wait until 120 seconds if server is busy loading games ---
	retry = 0; while(loading_games && working && (++retry < 600)) sys_ppu_thread_usleep(200000);

	if(retry >= 600 || !working) return false;
	// ---

	const u32 BUFFER_MAXSIZE = (BUFFER_SIZE_ALL - _12KB_);

	// use cached page
	retry = loading_games = 1;

	if(launchpad_mode) buf_len = 0; else
	if(mobile_mode) {cellFsUnlink(GAMELIST_JS); buf_len = 0;}
	else
	{
		if(islike(param, "/index.ps3?") || islike(param, "/sman.ps3?")) cellFsUnlink(WMTMP "/games.html");

		if(cellFsStat(WMTMP "/games.html", &buf) == CELL_FS_SUCCEEDED && (buf.st_size > 10 && buf.st_size < BUFFER_MAXSIZE))
		{
			if(read_file(WMTMP "/games.html", (char*)(buffer + buf_len), buf.st_size, 0))
			{
				loading_games = 0; // return
				if( islike(param, "/sman.ps3") &&  islike(buffer + buf_len, "<div id=\"wmsg\">")) loading_games = 1;
				if(!islike(param, "/sman.ps3") && !islike(buffer + buf_len, "<div id=\"wmsg\">")) loading_games = 1;
			}
		}
	}

	if(loading_games)
	{
		u8 is_net = 0;

		u16 idx = 0;
		u32 tlen = buf_len; buffer[tlen] = '\0';
		char *sysmem_html = buffer + (webman_config->sman ? _12KB_ : _8KB_);
		char *title = templn, *param_sfo = templn, *entry_name = templn;
		t_new_entry *new_entry = (t_new_entry *)tempstr; char *new_entry_ = (char*)new_entry;

		typedef struct
		{
			char path[MAX_LINE_LEN];
		} t_line_entries;

		t_line_entries *line_entry = (t_line_entries *)sysmem_html;
		u16 max_entries = (BUFFER_MAXSIZE / MAX_LINE_LEN); tlen = 0;

		check_cover_folders(templn);

		char onerror_prefix[24]=" onerror=\"this.src='", onerror_suffix[8]="';\"";  // wm_icons[default_icon]
		#ifndef ENGLISH_ONLY
		if(!use_custom_icon_path) *onerror_prefix = *onerror_suffix = NULL;
		#endif
		char icon[STD_PATH_LEN], subpath[STD_PATH_LEN], enc_dir_name[STD_PATH_LEN * 3];

		// filter html content
		char filter_name[STD_PATH_LEN]; *filter_name = NULL;
		u8 filter0, filter1, b0, b1; filter0 = filter1 = b0 = b1 = 0;
		u8 div_size = mobile_mode ? 0 : GAME_DIV_SIZE;

		#ifdef COBRA_ONLY
		u8 clear_ntfs = 0;
		#endif

		#if defined(MOUNT_GAMEI) || defined(MOUNT_ROMS)
		f1_len = (webman_config->roms ? id_ROMS : webman_config->gamei ? id_GAMEI : id_VIDEO) + 1;
		#endif

		#ifdef LAUNCHPAD
		if(launchpad_mode)
		{
			max_entries = LAUNCHPAD_MAX_ITEMS, tlen = 0;
		}
		else
		#endif
		{
		#ifdef COBRA_ONLY
			if(strstr(param, "ntfs")) {filter0 = NTFS, b0 = 1; clear_ntfs = (strstr(param, "ntfs(0)") != NULL);} else
		#endif
			for(u8 f0 = 0; f0 < MAX_DRIVES; f0++) if(strstr(param, drives[f0])) {filter0 = f0, b0 = 1; break;}
			for(u8 f1 = 0; f1 < f1_len; f1++) if(strstr(param, paths [f1])) {filter1 = f1, b1 = 1; break;}
			if(!b0 && strstr(param, "hdd" ))  {filter0 = 0, b0 = 1;}
			if(!b0 && strstr(param, "usb" ))  {filter0 = 1, b0 = 2;}
			if(!b1 && strstr(param, "games")) {filter1 = 0, b1 = 2;}
			if(!b1 && strstr(param, "?ps3"))  {filter1 = 0, b1 = 3;}
			if(!b1 && strstr(param, "npdrm")) {filter1 = 0, b1 = id_NPDRM;}
		#ifdef NET_SUPPORT
			if(!b0 && strstr(param, "net" ))  {filter0 = NET, b0 = 3;}
		#endif
			if(strchr(param, '?') && ((!b0 && !b1) || (strrchr(param, '?') > strchr(param, '?'))) && !strstr(param, "?html") && !strstr(param, "mobile")) strcpy(filter_name, strrchr(param, '?') + 1);
		}

		int ns = NONE; u8 uprofile, all_profiles = (profile >= 5); enum icon_type default_icon;

		#ifdef NET_SUPPORT
		int abort_connection = 0;
		#ifdef USE_INTERNAL_NET_PLUGIN
		if(g_socket >= 0 && open_remote_dir(g_socket, "/", &abort_connection, false) < 0) do_umount(false);
		#endif
		#endif

		#ifdef SLAUNCH_FILE
		int fdsl = 0;
		if(cellFsOpen(SLAUNCH_FILE, CELL_FS_O_RDONLY, &fd, NULL, 0) == CELL_FS_SUCCEEDED)
		{
			_slaunch slaunch; u64 read_e;

			int flen, slen;

			while(cellFsRead(fd, &slaunch, sizeof(_slaunch), &read_e) == CELL_FS_SUCCEEDED && read_e > 0)
			{
				if(idx >= max_entries || tlen >= BUFFER_MAXSIZE) break;

				char *title = slaunch.name;
				const char *path = slaunch.name + slaunch.path_pos;
				const char *param = path + 10; // remove /mount_ps3
				char *icon = slaunch.name + slaunch.icon_pos;

				if(*filter_name >= ' '  && !strcasestr(title, filter_name)
										&& !strcasestr(param, filter_name)) continue;

				u8 f1 = (slaunch.type == TYPE_PS1) ? id_PSXISO :
						(slaunch.type == TYPE_PS2) ? id_PS2ISO :
						(slaunch.type == TYPE_PS3) ? id_PS3ISO :
						(slaunch.type == TYPE_PSP) ? id_PSPISO :
						(slaunch.type == TYPE_VID) ? id_BDISO  : id_ROMS;

				if(f1 == id_PS3ISO)
				{
					if(filter1 == f1 && islike(param, HDD0_GAME_DIR)) continue;
					char *pos = strstr(param, "/GAME"); if(pos) f1 = (pos[5] == 'I') ? id_GAMEI : id_GAMES;
				}

				#ifdef COBRA_ONLY
				if(filter0 == NTFS) {if(!islike(param, WMTMP)) continue;} else
				if(filter0 == NET)  {if(!islike(param, "/net")) continue;} else
				#endif
				if(filter0 == 1) {if(!islike(param, "/dev_usb")) continue;} else
				if(b0 == 1) {if(!islike(param, drives[filter0])) continue;} else
				if(b0 == 2) {if(!islike(param, "/dev_usb")) continue;}
				if(b1)
				{
					if((b1 == id_NPDRM) && !islike(param, HDD0_GAME_DIR)) continue;
					if((b1 >= 2) && ((f1 < b1) || IS_JB_FOLDER) && (filter1 < 3)); else if(filter1 != f1) continue;
				}
				else
					if(check_content_type(f1)) continue;

				default_icon =  get_default_icon_by_type(f1);

				if(custom_icon && *param == '/')
				{
					char *pos = get_filename(param); *pos = *icon = NULL;
					get_default_icon(icon, param, (pos + 1), isDir(param), slaunch.id, 0, 0, f1);
					*pos = '/';
				}

				set_sort_key(new_entry->key, title, HTML_KEY - launchpad_mode, 0, f1); // sort key

				#ifdef LAUNCHPAD
				if(launchpad_mode)
				{
					flen = sprintf(new_entry_, "http://%s%s", local_ip, path);
					if(flen >= MAX_LINE_LEN) continue; //ignore lines too long
					strcpy(line_entry[idx].path, new_entry_);
					flen = add_launchpad_entry(new_entry->data, title, line_entry[idx].path, slaunch.id, icon, false);
				}
				else
				#endif
				if(mobile_mode)
				{
					if(!icon || strchr(icon, '"')) continue; // ignore names with quotes: cause syntax error in javascript: gamelist.js

					replace_invalid_chars(title);

					int w = 260, h = 300; if(strstr(icon, "ICON0.PNG")) {w = 320, h = 176;} else if(strstr(icon, "icon_wm_")) {w = 280, h = 280;}

					flen = sprintf(new_entry->data, "{img:\"%s\",width:%i,height:%i,desc:\"%s\",url:\"%s\"},",
													icon, w, h, title, param);
				}
				else
				{
					slen = slaunch.path_pos - 1;
					do
					{
						flen = sprintf(new_entry->data, "%s\"><img id=\"im%i\" src=\"%s\"%s%s%s class=\"gi\"></a></div><div class=\"gn\"><a href=\"%s\">%s",
														param, idx, icon, onerror_prefix, (*onerror_prefix && default_icon) ? wm_icons[default_icon] : "", onerror_suffix, param, title);

						slen -= 4; if(slen < 32) break;
						title[slen] = '\0';
					}
					while(flen > MAX_LINE_LEN);
				}

				if(flen > MAX_LINE_LEN) continue; //ignore lines too long
				strcpy(line_entry[idx].path, new_entry_); idx++;
				tlen += (flen + div_size);
			}
			cellFsClose(fd);
			filter0 = MAX_DRIVES;
		}
		else if(!b0 && !b1 && !filter_name[0]) fdsl = create_slaunch_file();
		#endif // #ifdef SLAUNCH_FILE

#ifdef USE_NTFS
list_games:
#endif

		for(u8 f0 = filter0; f0 < MAX_DRIVES; f0++)  // drives: 0="/dev_hdd0", 1="/dev_usb000", 2="/dev_usb001", 3="/dev_usb002", 4="/dev_usb003", 5="/dev_usb006", 6="/dev_usb007", 7="/net0", 8="/net1", 9="/net2", 10="/net3", 11="/net4", 12="/ext", 13="/dev_sd", 14="/dev_ms", 15="/dev_cf"
		{
			if(!loading_games) break;

			if(check_drive(f0)) continue;

			is_net = IS_NET;

			if(!(is_net || IS_NTFS) && (isDir(drives[f0]) == false)) continue;
//
			#ifdef NET_SUPPORT
			if(is_net && (ns >= 0) && (ns!=g_socket)) sclose(&ns);
			#endif
			ns = NONE;
			for(u8 f1 = filter1; f1 < f1_len; f1++) // paths: 0="GAMES", 1="GAMEZ", 2="PS3ISO", 3="BDISO", 4="DVDISO", 5="PS2ISO", 6="PSXISO", 7="PSXGAMES", 8="PSPISO", 9="ISO", 10="video", 11="GAMEI", 12="ROMS"
			{
				if(!loading_games) break;

				_f1_ = f1; // _f1_ changes to id_PS2ISO for CD/DVD folders (9="CD" / 10="DVD")
				
				if(idx >= max_entries || tlen >= BUFFER_MAXSIZE) break;
				#ifndef COBRA_ONLY
				if(IS_ISO_FOLDER && !(IS_PS2ISO)) continue; // 0="GAMES", 1="GAMEZ", 5="PS2ISO", 10="video"
				#endif

				//if(IS_PS2ISO && f0>0)  continue; // PS2ISO is supported only from /dev_hdd0
				if(IS_GAMEI_FOLDER) {if((!webman_config->gamei) || (IS_HDD0) || (IS_NTFS)) continue;}
				if(IS_ISO_DIR     ) {if(is_net) continue; else {concat_path(param, drives[f0], CUSTOM_PATH1); if(isDir(param)) _f1_ = id_PS2ISO; strcpy(paths[id_ISO], (_f1_ == id_PS2ISO) ? CUSTOM_PATH1 : "ISO");}}
				if(IS_VIDEO_FOLDER) {if(is_net) continue; else if(IS_HDD0) strcpy(paths[id_VIDEO], "video"); else {concat_path(param, drives[f0], CUSTOM_PATH2); if(isDir(param)) _f1_ = id_PS2ISO; strcpy(paths[id_VIDEO], (_f1_ == id_PS2ISO) ? CUSTOM_PATH2 : "GAMES_DUP");}}
				if(IS_NTFS) {if(f1 >= id_ISO) break; else if(IS_JB_FOLDER || (f1 == id_PSXGAMES)) continue;} // 0="GAMES", 1="GAMEZ", 7="PSXGAMES", 9="ISO", 10="video", 11="GAMEI", 12="ROMS"

#ifdef NET_SUPPORT
				if(is_net)
				{
					if(f1 >= id_ISO) f1 = id_GAMEI; // ignore 9="ISO", 10="video"
				}
#endif
				if(b0) {if((b0 == 2) && (f0 < NET)); else if((b0 == 3) && (!IS_NTFS)); else if(filter0 != f0) continue;}
				if(b1) {if((b1 >= 2) && ((f1 < b1) || IS_JB_FOLDER) && (filter1 < 3)); else if(filter1 != f1) continue;}
				else
					if(check_content_type(_f1_)) continue;

				#ifdef NET_SUPPORT
				#ifdef USE_INTERNAL_NET_PLUGIN
				if(is_net && (netiso_svrid == (f0-NET)) && (g_socket != NONE)) ns = g_socket; /* reuse current server connection */ else
				#endif
				if(is_net && (ns<0)) ns = connect_to_remote_server(f0-NET);
				#endif
				if(is_net && (ns<0)) break;
//
				bool ls; u8 li, subfolder; li = subfolder = 0; ls = false; // single letter folder

		subfolder_letter_html:
				subfolder = 0; if(all_profiles) uprofile = 1; else uprofile = profile;
		read_folder_html:
//
				if(!loading_games) break;
				set_scan_path(li, f0, f1, is_net, uprofile, param);

				#ifdef NET_SUPPORT
				if(is_net && open_remote_dir(ns, param, &abort_connection, !IS_JB_FOLDER) < 0) goto continue_reading_folder_html; //continue;
				#endif

				CellFsDirectoryEntry entry; u32 read_e;
				int fd2 = 0, flen, slen;
				char title_id[12], app_ver[8]; *app_ver = NULL;
				u8 is_iso = 0;

				#ifdef NET_SUPPORT
				sys_addr_t data2 = NULL;
				int v3_entries, v3_entry; v3_entries = v3_entry = 0;
				netiso_read_dir_result_data *data = NULL; char neth[8];
				if(is_net)
				{
					v3_entries = read_remote_dir(ns, &data2, &abort_connection);
					if(!data2) goto continue_reading_folder_html; //continue;
					data = (netiso_read_dir_result_data*)data2; sprintf(neth, "/net%i", (f0-NET));
				}
				#endif
				if(!is_net && cellFsOpendir(param, &fd) != CELL_FS_SUCCEEDED) goto continue_reading_folder_html; //continue;

				default_icon =  get_default_icon_by_type(f1);

				while((!is_net && (!cellFsGetDirectoryEntries(fd, &entry, sizeof(entry), &read_e) && read_e > 0))
					#ifdef NET_SUPPORT
					|| (is_net && (v3_entry < v3_entries))
					#endif
					)
				{
					if(idx >= max_entries || tlen >= BUFFER_MAXSIZE) break;

					#ifdef NET_SUPPORT
					if(is_net)
					{
						bool is_slf = BETWEEN(id_PS3ISO, f1, id_PSPISO) && (data[v3_entry].is_directory) && (data[v3_entry].name[1] == '\0');

						if(is_slf) {ls = true; v3_entry++; continue;} // single letter folder was found

						if(add_net_game(ns, data, v3_entry, neth, param, title, tempstr, enc_dir_name, icon, title_id, app_ver, f1, 1) == FAILED) {v3_entry++; continue;}

						#ifdef SLAUNCH_FILE
						if(fdsl && (idx < MAX_SLAUNCH_ITEMS)) add_slaunch_entry(fdsl, neth, param, data[v3_entry].name, icon, title, title_id, f1);
						#endif

						if(*filter_name >= ' '  && !strcasestr(title, filter_name)
												&& !strcasestr(param, filter_name)
												&& !strcasestr(data[v3_entry].name, filter_name)) {v3_entry++; continue;}

						if(urlenc(tempstr, icon)) strncopy(icon, STD_PATH_LEN, tempstr);

						set_sort_key(new_entry->key, title, HTML_KEY - launchpad_mode, 0, f1); // sort key

						#ifdef LAUNCHPAD
						if(launchpad_mode)
						{
							flen = sprintf(new_entry_, "http://%s/mount_ps3%s%s/%s", local_ip, neth, param, enc_dir_name);
							if(flen >= MAX_LINE_LEN) continue; //ignore lines too long
							strcpy(line_entry[idx].path, new_entry_);
							flen = add_launchpad_entry(new_entry->data, title, line_entry[idx].path, title_id, icon, false);
						}
						else
						#endif
						if(mobile_mode)
						{
							if(!icon ||  strchr(enc_dir_name, '"') || strchr(icon, '"')) continue; // ignore: cause syntax error in javascript: gamelist.js

							replace_invalid_chars(title);

							int w = 260, h = 300; if(strstr(icon, "ICON0.PNG")) {w = 320; h = 176;} else if(strstr(icon, "icon_wm_")) {w = 320; h = 280;}

							flen = sprintf(new_entry->data, "{img:\"%s\",width:%i,height:%i,desc:\"%s\",url:\"%s%s/%s\"},",
											*icon ? icon : wm_icons[default_icon], w, h, title, neth, param, enc_dir_name);
						}
						else
							flen = sprintf(new_entry->data, "%s%s/%s\"><img id=\"im%i\" src=\"%s\"%s%s%s class=\"gi\"></a></div><div class=\"gn\"><a href=\"%s%s/%s\">%s",
											neth, param, enc_dir_name, idx,
											icon, onerror_prefix, (*onerror_prefix  && default_icon) ? wm_icons[default_icon] : "", onerror_suffix,
											neth, param, enc_dir_name, title);

						v3_entry++;
						if((flen + HTML_KEY_LEN) > MAX_LINE_LEN) continue; //ignore lines too long
						strcpy(line_entry[idx].path, new_entry_); idx++;
						tlen += (flen + div_size);
					}
					else
					#endif // #ifdef NET_SUPPORT
					{
						if(entry.entry_name.d_name[0] == '.') continue;

						// skip duplicated game in /dev_hdd0
						if(is_dupe(f0, f1, entry.entry_name.d_name, templn)) continue;

						//////////////////////////////
						subfolder = 0;
						if(BETWEEN(id_PS3ISO, f1, id_ISO) || (f1 == id_ROMS))
						{
							snprintf(subpath, STD_PATH_LEN, "%s/%s", param, entry.entry_name.d_name); cellFsClosedir(fd2);
							if(cellFsOpendir(subpath, &fd2) == CELL_FS_SUCCEEDED)
							{
								strcpy(subpath, entry.entry_name.d_name); subfolder = 1;
next_html_entry:
								cellFsGetDirectoryEntries(fd2, &entry, sizeof(entry), &read_e);
								if(read_e < 1) {cellFsClosedir(fd2); fd2 = 0; continue;}
								if(entry.entry_name.d_name[0] == '.') goto next_html_entry;

								strcpy(entry_name, entry.entry_name.d_name);
								entry.entry_name.d_namlen = snprintf(entry.entry_name.d_name, D_NAME_LEN, "%s/%s", subpath, entry_name);
							}
						}
						//////////////////////////////

						if(idx >= max_entries || tlen >= BUFFER_MAXSIZE) break;

						if(IS_JB_FOLDER)
						{
							#ifdef MOUNT_GAMEI
							if(IS_GAMEI_FOLDER)
							{
								if(!webman_config->gamei) continue;

								if(!is_app_dir(param, entry.entry_name.d_name)) continue;
								concat_path2(param_sfo, param, entry.entry_name.d_name, "/PARAM.SFO");
							}
							else
							#endif
							{
								concat_path2(param_sfo, param, entry.entry_name.d_name, "/PS3_GAME/PARAM.SFO");
								check_ps3_game(param_sfo);
							}

							is_iso = false;
						}
						else
						{
							flen = entry.entry_name.d_namlen;
							is_iso = is_iso_file(entry.entry_name.d_name, flen, _f1_, f0); if(!is_iso) goto continue_loop; //continue;
						}

						if(is_iso || (IS_JB_FOLDER && file_exists(param_sfo)))
						{
							*icon = *title_id = NULL;

							if(!is_iso) // IS_JB_FOLDER
							{
								get_title_and_id_from_sfo(title, title_id, entry.entry_name.d_name, icon, tempstr, 0);
							}
							else // is_iso
							{
								if(IS_HDD0 && IS(entry.entry_name.d_name, "~tmp.iso")) goto continue_loop; //continue;

								#ifdef COBRA_ONLY
								if(get_name_iso_or_sfo(title, title_id, icon, param, entry.entry_name.d_name, f0, f1, uprofile, flen, tempstr) == FAILED) goto continue_loop; //continue;
								#else
								get_name(title, entry.entry_name.d_name, NO_EXT);
								#endif
							}

							if(*filter_name >= ' '  && !strcasestr(title, filter_name)
													&& !strcasestr(param, filter_name)
													&& !strcasestr(entry.entry_name.d_name, filter_name)) goto continue_loop; //continue;

							get_default_icon(icon, param, entry.entry_name.d_name, !is_iso, title_id, ns, f0, f1);

							#ifdef SLAUNCH_FILE
							if(fdsl && (idx < MAX_SLAUNCH_ITEMS)) add_slaunch_entry(fdsl, "", param, entry.entry_name.d_name, icon, title, title_id, f1);
							#endif
							add_title_id(title, title_id);
							urlenc(enc_dir_name, entry.entry_name.d_name);

							title[80] = '\0'; // truncate title name

							if(urlenc(tempstr, icon)) strncopy(icon, STD_PATH_LEN, tempstr);

							set_sort_key(new_entry->key, title, HTML_KEY - launchpad_mode, subfolder, f1); // sort key

							#ifdef LAUNCHPAD
							if(launchpad_mode)
							{
								flen = sprintf(new_entry_, "http://%s/mount_ps3%s/%s", local_ip, param, enc_dir_name);
								if(flen >= MAX_LINE_LEN)  goto continue_loop; //continue; //ignore lines too long
								strcpy(line_entry[idx].path, new_entry_);
								flen = add_launchpad_entry(new_entry->data, title, line_entry[idx].path, title_id, icon, false);
							}
							else
							#endif
							if(mobile_mode)
							{
								if(!icon || strchr(enc_dir_name, '"') || strchr(icon, '"'))  goto continue_loop; // ignore names with quotes: cause syntax error in javascript: gamelist.js

								replace_invalid_chars(title);

								int w = 260, h = 300; if(strstr(icon, "ICON0.PNG")) {w = 320, h = 176;} else if(strstr(icon, "icon_wm_")) {w = 280, h = 280;}

								flen = sprintf(new_entry->data, "{img:\"%s\",width:%i,height:%i,desc:\"%s\",url:\"%s/%s\"},",
												icon, w, h, title, param, enc_dir_name);
							}
							else
							{
								slen = strlen(title);
								do
								{
									flen = sprintf(new_entry->data, "%s%s/%s\"><img id=\"im%i\" src=\"%s\"%s%s%s class=\"gi\"></a></div><div class=\"gn\"><a href=\"%s%s/%s\">%s",
													param, "", enc_dir_name, idx, icon, onerror_prefix, (*onerror_prefix && default_icon) ? wm_icons[default_icon] : "", onerror_suffix, param, "", enc_dir_name, title);

									slen -= 4; if(slen < 32) break;
									title[slen] = '\0';
								}
								while(flen > MAX_LINE_LEN);
							}

							if(flen > MAX_LINE_LEN)  goto continue_loop; //ignore lines too long
							strcpy(line_entry[idx].path, new_entry_); idx++;
							tlen += (flen + div_size);
						}
						//////////////////////////////
					continue_loop:
						if(subfolder) goto next_html_entry;
						//////////////////////////////
					}
				} //while

				if(!is_net) cellFsClosedir(fd); cellFsClosedir(fd2);

				#ifdef NET_SUPPORT
				if(data2) {sys_memory_free(data2); data2 = NULL;}
				#endif
//
			continue_reading_folder_html:
				if(IS_ROMS_FOLDER || (f1 < id_ISO && !IS_NTFS))
				{
					if(uprofile > 0) {subfolder = 0; if(all_profiles && (uprofile < 4)) ++uprofile; else uprofile = 0; goto read_folder_html;}
					if(is_net && BETWEEN(id_PS3ISO, f1, id_PSPISO))
					{
						if(ls && (li < 27)) {li++; goto subfolder_letter_html;} else if(li < LANG_CUSTOM) {li = LANG_CUSTOM; goto subfolder_letter_html;}
					}
				}
//
			} //f1

			#ifdef NET_SUPPORT
			if(is_net && (ns >= 0) && (ns!=g_socket)) sclose(&ns);
			#endif
		} //f0

		#ifdef SLAUNCH_FILE
		close_slaunch_file(fdsl);
		#endif

		if(idx)
		{   // sort html game items
			qsort(line_entry, idx, sizeof(t_line_entries), qcompare_html);
		}

		#ifdef USE_NTFS
		else if(retry && (filter0 == NTFS)) {prepNTFS(clear_ntfs); --retry; goto list_games;}
		#endif

		#ifndef LITE_EDITION
		bool sortable = false;
		#endif

		t_string sout; _set(&sout, buffer, buf_len); char *html = templn;

		#ifdef LAUNCHPAD
		if(launchpad_mode)
		{
			del("/dev_hdd0/tmp/explore/nsx/", RECURSIVE_DELETE); // Clear LaunchPad Cache
			add_launchpad_header();
			add_launchpad_extras(tempstr, templn);
		}
		else
		#endif
		if(mobile_mode)
			_concat(&sout, "slides = [");
		else if(islike(param, "/sman.ps3") || webman_config->sman)
		{
			sprintf(html, "<script>document.getElementById('ngames').innerHTML='%s: %'i';</script>", (strstr(param, "DI")!=NULL) ? STR_FILES : STR_GAMES, idx); _concat(&sout, html);
		}
		else
		{
			sprintf(html,	// wait dialog div
							"<div id=\"wmsg\"><H1>. . .</H1></div>"
							// show games count + find icon
							"<a href=\"javascript:_find();\"> &nbsp; %s: %'i &#x1F50D;</a></font>"
							// separator
							"<HR><span style=\"white-space:normal;\">", strstr(param, "DI") ? STR_FILES : STR_GAMES, idx); _concat(&sout, html);

			#ifndef LITE_EDITION
			sortable = file_exists(JQUERY_LIB_JS) && file_exists(JQUERY_UI_LIB_JS);
			if(sortable)
			{	// add external jquery libraries
				_concat(&sout, strfmt(SCRIPT_SRC_FMT, JQUERY_LIB_JS));
				_concat(&sout, strfmt(SCRIPT_SRC_FMT, JQUERY_UI_LIB_JS));
				_concat(&sout, "<script>$(function(){$(\"#mg\").sortable();});</script>");
			}
			#endif
		}

		if(!mobile_mode)
		{
			if(file_exists(GAMES_SCRIPT_JS))
			{
				_concat(&sout, strfmt(SCRIPT_SRC_FMT, GAMES_SCRIPT_JS));
			}
			add_html(dat_GET_ICON_SIZE, 0, sout.str, html);
		}

		tlen = buf_len;

		#ifdef LAUNCHPAD
		if(launchpad_mode)
		{
			int fd, size;
			if(cellFsOpen(LAUNCHPAD_FILE_XML, CELL_FS_O_WRONLY | CELL_FS_O_APPEND | CELL_FS_O_CREAT, &fd, NULL, 0) == CELL_FS_SUCCEEDED)
			{
				char *xml_item = tempstr;
				for(u16 m = 0; m < idx; m++)
				{
					size = sprintf(xml_item, "<mtrl id=\"%lu\" until=\"2100-12-31T23:59:00.000Z\">\n"
											 "<desc>%s"
											 "<cntry agelmt=\"0\">all</cntry>\n"
											 "<lang>all</lang></mtrl>\n\n", (1080000000UL + m), (line_entry[m].path) + HTML_KEY_LEN);
					cellFsWrite(fd, xml_item, size, NULL);
				}
				cellFsClose(fd);
			}
			add_launchpad_footer(tempstr);
			loading_games = 0;
			return true;
		}
		else
		#endif
		if(mobile_mode)
			for(u16 m = 0; m < idx; m++)
			{
				_concat(&sout, (line_entry[m].path) + HTML_KEY_LEN);
			}
		else
		{
			syscalls_removed = CFW_SYSCALLS_REMOVED(TOC);
			if(syscalls_removed)
			{
				sprintf(html, "<h1>%s</h1>%s", STR_CFWSYSALRD, HTML_RED_SEPARATOR); _concat(&sout, html);
			}
			#ifdef COBRA_ONLY
			else if(!cobra_version)
			{
				sprintf(html, "<h1>%s %s</h1>%s", "Cobra", STR_DISABLED, HTML_RED_SEPARATOR); _concat(&sout, html);
			}
			#endif

			for(u16 m = 0; m < idx; m++)
			{
				_concat3(&sout, GAME_DIV_PREFIX, (line_entry[m].path + HTML_KEY_LEN), GAME_DIV_SUFIX);
			}
		}

		#ifndef LITE_EDITION
		if(sortable) _concat(&sout, "</div>");
		#endif
		if(auto_mount && idx == 1)
		{
			char *mount_path = line_entry[0].path + HTML_KEY_LEN;
			get_flag(mount_path, "?random=");
			get_flag(mount_path, "\">");
			concat2(buffer, "/mount.ps3", mount_path);
		}
		else if(mobile_mode)
		{
			_concat(&sout, "];");
			save_file(GAMELIST_JS, sout.str, sout.size);
		}
		else
		{
			save_file(WMTMP "/games.html", (sout.str + buf_len), sout.size - buf_len);
		}

		loading_games = 0;
	}
	disable_progress();
	return true;
}
