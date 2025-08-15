#define SYSCALLS_UNAVAILABLE    0xFFFFFFFF80010003ULL

#define MAX_BLOCKED_URL         64

static u64 blocked_url[MAX_BLOCKED_URL][2]; u8 url_count = 0;

#ifdef REMOVE_SYSCALLS

#ifndef COBRA_ONLY
#define disable_signin_dialog() {}
#define enable_signin_dialog() {}
#endif

#ifdef DISABLE_SIGNIN_DIALOG
static void disable_signin_dialog(void)
{
	#ifdef COBRA_ONLY
	if(file_exists(NPSIGNIN_PLUGIN_OFF))
	{
		sys_map_path(NPSIGNIN_PLUGIN_RCO, NPSIGNIN_PLUGIN_OFF);
	}
	#endif
}

static void enable_signin_dialog(void)
{
	#ifdef COBRA_ONLY
	sys_map_path(NPSIGNIN_PLUGIN_RCO, NPSIGNIN_PLUGIN_ON);
	#endif
}
#endif

#ifdef PS3MAPI

static bool notify_restore = true;
static u64 sc_backup[CFW_SYSCALLS];

static void backup_cfw_syscalls(void)
{
	for(u8 sc = 0; sc < CFW_SYSCALLS; sc++)
		sc_backup[sc] = peekq( SYSCALL_PTR(sc_disable[sc]) );
}

static void restore_cfw_syscalls(void)
{
	#ifdef COBRA_ONLY
	{ PS3MAPI_ENABLE_ACCESS_SYSCALL8 }

	{ system_call_2(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_DISABLE_COBRA, 0);}

	{ system_call_3(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_PDISABLE_SYSCALL8, 0); }

	for(u8 sc = 0; sc < CFW_SYSCALLS; sc++)
		lv2_poke_ps3mapi( SYSCALL_PTR(sc_disable[sc]), sc_backup[sc] );

	syscalls_removed = (lv2_peek_hen(TOC) == SYSCALLS_UNAVAILABLE);

	//ps3mapi_key = 0;
	{ PS3MAPI_DISABLE_ACCESS_SYSCALL8 }
	#else
	for(u8 sc = 0; sc < CFW_SYSCALLS; sc++)
		pokeq( SYSCALL_PTR(sc_disable[sc]), sc_backup[sc] );

	syscalls_removed = (lv2_peek_hen(TOC) == SYSCALLS_UNAVAILABLE);
	#endif

	#ifdef DISABLE_SIGNIN_DIALOG
	if(!syscalls_removed) disable_signin_dialog();
	#endif

#ifndef ENGLISH_ONLY
	char STR_RSTCFWSYS[80];//	= "CFW Syscalls restored!";
	char STR_RSTCFWSYSF[80];//	= "Failed to restore CFW Syscalls";

	language("STR_RSTCFWSYS", STR_RSTCFWSYS, "CFW Syscalls restored!");
	language("STR_RSTCFWSYSF", STR_RSTCFWSYSF, "Failed to restore CFW Syscalls");

	close_language();
#endif

	if(syscalls_removed)
	{
		if(!webman_config->nobeep) { BEEP2 }
		show_msg_with_icon(ICON_ERROR, STR_RSTCFWSYSF);
	}
	else
	{
		disable_signin_dialog();
		if(notify_restore)
		{
			if(!webman_config->nobeep) play_rco_sound("snd_trophy");
			show_msg_with_icon(ICON_CHECK, STR_RSTCFWSYS);
			notify_restore = false;
		}
	}

	if(payload_ps3hen)
	{
		peekq = lv2_peek_hen;
		pokeq = lv2_poke_hen;
		lv2_poke_fan = lv2_poke_fan_hen;
	}
	else
	{
		peekq = lv2_peek_cfw;
		pokeq = lv2_poke_cfw;
		lv2_poke_fan =  lv2_poke_cfw;
	}

	peek_lv1 = lv1_peek_cfw;
	poke_lv1 = lv1_poke_cfw;
}

#endif // #ifdef PS3MAPI

static void restore_blocked_urls(bool notify)
{
	enable_signin_dialog();

	if(!url_count) return;

	if(notify) show_msg_with_icon(ICON_CHECK, "PSN servers restored");

	// restore blocked servers
	{for(u8 u = 0; u < url_count; u++) poke_lv1(blocked_url[u][0], blocked_url[u][1]); url_count = 0;}
}

static void remove_cfw_syscall8(void)
{
	if(!SYSCALL_TABLE) return;

	#ifdef COBRA_ONLY
	{ PS3MAPI_ENABLE_ACCESS_SYSCALL8 }
	#endif

	restore_blocked_urls(true);

	u64 sc_null = lv2_peek_hen(SYSCALL_TABLE), toc = lv2_peek_hen(TOC);

	// disable syscall 8 only if others cfw syscalls were disabled
	if(syscalls_removed || toc == SYSCALLS_UNAVAILABLE || toc == sc_null)
	{
		#ifdef COBRA_ONLY
		{ system_call_3(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_PDISABLE_SYSCALL8, 0); }
		{ system_call_3(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_DISABLE_SYSCALL, SC_COBRA_SYSCALL8); }

		pokeq(SYSCALL_PTR( SC_COBRA_SYSCALL8 ), sc_null);
		#else
		pokeq(SYSCALL_PTR( 8 ), sc_null);
		#endif
	}
}

static void remove_cfw_syscalls(bool keep_ccapi)
{
	detect_firmware();

	syscalls_removed = CFW_SYSCALLS_REMOVED(TOC);

	if(!SYSCALL_TABLE || syscalls_removed) return;

	u64 sc_null = peekq(SYSCALL_TABLE);

	u32 initial_sc = keep_ccapi ? 5 : 0;

	#ifdef COBRA_ONLY
	for(u8 sc = initial_sc; sc < CFW_SYSCALLS; sc++)
	{ system_call_3(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_DISABLE_SYSCALL, (u64)sc_disable[sc]); }
	{ system_call_3(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_PDISABLE_SYSCALL8, webman_config->sc8mode); } // default: Partial disable syscall8 (Keep cobra/mamba+ps3mapi features only)

	if(webman_config->sc8mode == PS3MAPI_DISABLED) remove_cfw_syscall8();
	#endif

	for(u8 sc = initial_sc; sc < CFW_SYSCALLS; sc++)
		pokeq(SYSCALL_PTR( sc_disable[sc] ), sc_null);

	syscalls_removed = CFW_SYSCALLS_REMOVED(TOC);

	#ifdef PS3MAPI
	notify_restore = syscalls_removed;
	#endif

	#ifdef COBRA_ONLY
	if(syscalls_removed)
	{
		peekq = lv2_peek_ps3mapi;
		pokeq = lv2_poke_ps3mapi;
		lv2_poke_fan = (payload_ps3hen) ? lv2_poke_fan_hen : lv2_poke_ps3mapi;

		peek_lv1 = lv1_peek_ps3mapi;
		poke_lv1 = lv1_poke_ps3mapi;
	}
	#endif
/*
	#ifdef COBRA_ONLY
	if(syscalls_removed)
	{
		CellRtcTick mTick; cellRtcGetCurrentTick(&mTick);
		ps3mapi_key = mTick.tick; for(u16 r = 0; r < (ps3mapi_key & 0xFFF) + 0xF; r++) {ps3mapi_key = ((ps3mapi_key<<15) ^ (ps3mapi_key>>49)) ^ (ps3mapi_key + 1);}
	}
	#endif
*/
}

static void disable_cfw_syscalls(bool keep_ccapi)
{
	#ifdef COBRA_ONLY
	{ PS3MAPI_ENABLE_ACCESS_SYSCALL8 }
	#endif

	syscalls_removed = CFW_SYSCALLS_REMOVED(TOC);

	delete_history(true);

	if(syscalls_removed)
	{
		if(!webman_config->nobeep) { BEEP2 }
		show_msg_with_icon(ICON_EXCLAMATION, STR_CFWSYSALRD);
	}
	else
	{
#ifndef ENGLISH_ONLY
		char STR_RMVCFWSYS[136];//	= "History files & CFW Syscalls deleted OK!";
		char STR_RMVCFWSYSF[80];//	= "Failed to remove CFW Syscalls";

		language("STR_RMVCFWSYS", STR_RMVCFWSYS, "History files & CFW Syscalls deleted OK!");
		language("STR_RMVCFWSYSF", STR_RMVCFWSYSF, "Failed to remove CFW Syscalls");

		close_language();
#endif
		restore_blocked_urls(false);

		remove_cfw_syscalls(keep_ccapi);

		if(syscalls_removed)
		{
			if(!webman_config->nobeep) play_rco_sound("snd_trophy");
			show_msg_with_icon(ICON_PSN, STR_RMVCFWSYS);
		}
		else
		{
			disable_signin_dialog();
			if(!webman_config->nobeep) { BEEP2 }
			show_msg_with_icon(ICON_ERROR, STR_RMVCFWSYSF);
		}
	}

	sys_ppu_thread_sleep(2);

	#ifdef COBRA_ONLY
	{ PS3MAPI_DISABLE_ACCESS_SYSCALL8 }
	#endif
}

#endif // #ifdef REMOVE_SYSCALLS

static bool block_url(u64 addr, u64 value)
{
	if(url_count >= MAX_BLOCKED_URL) return false;

	// backup original value @ poke address
	blocked_url[url_count][0] = addr;
	blocked_url[url_count][1] = peek_lv1(addr);

	poke_lv1(addr, value);

	url_count++, addr += 0x20;

	return true;
}

static void block_online_servers(bool notify)
{
	if(payload_ps3hen) return; // not allowed due poke LV1 requirement

	if(url_count == 0)
	{
		if(IS_INGAME) return; // not in XMB

		if(notify) show_msg_with_icon(ICON_PSN, "Blocking PSN servers");

		#ifdef COBRA_ONLY
		{ PS3MAPI_ENABLE_ACCESS_SYSCALL8 }

		{ PS3MAPI_REENABLE_SYSCALL8 }
		#endif

		if(peekq(TOC) == SYSCALLS_UNAVAILABLE)
		{
			show_msg_with_icon(ICON_EXCLAMATION, STR_CFWSYSALRD);
		}
		else
		{
			led(YELLOW, BLINK_FAST);

			u64 mem = 0; u8 pcount = 0;

			// LV1
			for(u64 addr = 0x880000; addr < 0xE1FFFFULL; addr += 4)//16MB
			{
				mem = peek_lv1(addr);

				if(     mem == 0x733A2F2F61757468ULL)  // s://auth
					{if(!block_url(addr,   0x733A2F2F00000000ULL)) break;}

				else if(mem == 0x2E7073332E757064ULL)  // .ps3.upd
					{if(!block_url(addr-8, 0x3A2F2F0000000000ULL)) break;}
				else if(mem == 0x656E612E6E65742EULL)  // ena.net.
					{if(!block_url(addr,   0x0000000000000000ULL)) break;}
				else if(mem == 0x687474703A2F2F70ULL)  // http://p
					{if(!block_url(addr,   0x687474703A2F2F00ULL)) break; pcount++; if(pcount>=44) break;}
			}

			// LV2
			u64 start_addr = 0x300000ULL + LV2_OFFSET_ON_LV1, stop_addr = 0x7FFFF8ULL + LV2_OFFSET_ON_LV1;

			for(u64 addr = start_addr; addr < stop_addr; addr += 4)//8MB
			{
				mem = peek_lv1(addr);
				if(     mem == 0x733A2F2F6E73782EULL)   // s://nsx.
					{if(!block_url(addr,   0x733A2F2F00000000ULL)) break;}
				else if(mem == 0x3A2F2F6E73782D65ULL)   // ://nsx-e
					{if(!block_url(addr,   0x3A2F2F0000000000ULL)) break;}
				else if(mem == 0x3A2F2F786D622D65ULL)   // ://xmb-e
					{if(!block_url(addr,   0x3A2F2F0000000000ULL)) break;}
				else if(mem == 0x2E7073332E757064ULL)   // .ps3.upd
					{if(!block_url(addr-8, 0x3A2F2F0000000000ULL)) break;}
				else if(mem == 0x702E616470726F78ULL)   // p.adprox
					{if(!block_url(addr-8, 0x733A2F2F00000000ULL)) break;}
				else if(mem == 0x656E612E6E65742EULL)   // ena.net.
					{if(!block_url(addr,   0x0000000000000000ULL)) break;}
				else if(mem == 0x702E7374756E2E70ULL)   // p.stun.p
					{if(!block_url(addr-4, 0x0000000000000000ULL)) break;}
				else if(mem == 0x2E7374756E2E706CULL)   // .stun.pl
					{if(!block_url(addr-4, 0x0000000000000000ULL)) break;}
				//else if(mem == 0x63726565706F2E77ULL)   // creepo.w
				//	{if(!block_url(addr,   0x0000000000000000ULL)) break;}
			}

			led(YELLOW, OFF);
			led(GREEN, ON);
		}

		#ifdef COBRA_ONLY
		{ PS3MAPI_RESTORE_SC8_DISABLE_STATUS }

		{ PS3MAPI_DISABLE_ACCESS_SYSCALL8 }
		#endif
	}

	if(notify)
	{
		if(url_count > 0)
		{
			if(!webman_config->nobeep) play_rco_sound("snd_trophy");
			show_msg_with_icon(ICON_PSN, "PSN servers blocked");
		}
	}
}
