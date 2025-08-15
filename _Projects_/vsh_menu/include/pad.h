//void VSHPadGetData(CellPadData *data);
void start_stop_vsh_pad(uint8_t flag);
void MyPadGetData(int32_t port_no, CellPadData *data);

// redefinition of pad bit flags
#define PAD_SELECT   (1<<0)
#define PAD_L3       (1<<1)
#define PAD_R3       (1<<2)
#define PAD_START    (1<<3)
#define PAD_UP       (1<<4)
#define PAD_RIGHT    (1<<5)
#define PAD_DOWN     (1<<6)
#define PAD_LEFT     (1<<7)
#define PAD_L2       (1<<8)
#define PAD_R2       (1<<9)
#define PAD_L1       (1<<10)
#define PAD_R1       (1<<11)
#define PAD_TRIANGLE (1<<12)
#define PAD_CIRCLE   (1<<13)
#define PAD_CROSS    (1<<14)
#define PAD_SQUARE   (1<<15)

#define PAD_SWAP_OX  (PAD_CROSS | PAD_CIRCLE)

static CellPadData pdata;
static uint32_t oldpad=0, curpad=0;
static int enter_button = 1;

/***********************************************************************
* search and return vsh_process toc
* Not the best way, but it work, it's generic and it is fast enough...
***********************************************************************/
static int32_t get_vsh_toc(void)
{
	uint32_t pm_start  = 0x10000UL;
	uint32_t v0 = 0, v1 = 0, v2 = 0;

	while(pm_start < 0x700000UL)
	{
		v0 = *(uint32_t*)(pm_start+0x00);

		if(v0 == 0x10200UL/* .init_proc() */)
		{
			v1 = *(uint32_t*)(pm_start+0x04);
			v2 = *(uint32_t*)(pm_start+0x0C);
			if (v1 == v2) break;
		}

		pm_start+=4;
	}

	return (int32_t)v1;
}

/***********************************************************************
* get vsh io_pad_object
***********************************************************************/
static int32_t io_pad_object_address = 0;
static int32_t get_vsh_pad_obj(void)
{
	if(io_pad_object_address) return io_pad_object_address;
	uint32_t (*base)(uint32_t) = (void*)sys_io_3733EA3C;        // get pointer to cellPadGetData()
	int16_t idx = *(uint32_t*)(*(uint32_t*)base) & 0x0000FFFF;  // get got_entry idx from first instruction,
	int32_t got_entry = (idx + get_vsh_toc());                  // get got_entry of io_pad_object
	io_pad_object_address = (int32_t)(*(int32_t*)got_entry);    // cache io_pad_object address
	return io_pad_object_address;                               // return io_pad_object address
}

/***********************************************************************
* get pad data struct of vsh process
*
* CellPadData *data = data struct for holding pad_data
***********************************************************************/
/*
static void *vsh_pdata_addr = NULL;

void VSHPadGetData(CellPadData *data)
{
	if(!vsh_pdata_addr)        // first time, get address
	{
		uint32_t pm_start = 0x10000UL;
		uint64_t pat[2]   = {0x380000077D3F4B78ULL, 0x7D6C5B787C0903A6ULL};

		while(pm_start < 0x700000UL)
		{
			if((*(uint64_t*)pm_start == pat[0]) && (*(uint64_t*)(pm_start+8) == pat[1]))
			{
				vsh_pdata_addr = (void*)(uint32_t)((int32_t)((*(uint32_t*)(pm_start + 0x234) & 0x0000FFFF) <<16) +
				                                   (int16_t)( *(uint32_t*)(pm_start + 0x244) & 0x0000FFFF));
				break;
			}

			pm_start+=4;
		}
	}

	if(vsh_pdata_addr) memcpy(data, vsh_pdata_addr, 0x80);
}
*/
/***********************************************************************
* set/unset io_pad_library init flag
*
* uint8_t flag = 0(unset) or 1(set)
*
* To prevent vsh pad events during vsh-menu, we set this flag to 0
* (pad_library not init). Each try of vsh to get pad_data leads intern
* to error 0x80121104(CELL_PAD_ERROR_UNINITIALIZED) and nothing will
* happen. The lib is of course not deinit, there are pad(s) mapped and we
* can get pad_data direct over syscall sys_hid_manager_read() in our
* own function MyPadGetData().
***********************************************************************/
void start_stop_vsh_pad(uint8_t flag)
{
  uint32_t lib_init_flag = get_vsh_pad_obj();
  *(uint8_t*)lib_init_flag = flag;
}

/***********************************************************************
* get pad data direct over syscall
* (very simple, no error-handling, no synchronization...)
*
* int32_t port_no   = pad port number (0 - 7)
* CellPadData *data = data struct for holding pad_data
***********************************************************************/
void MyPadGetData(int32_t port_no, CellPadData *data)
{
	uint32_t port = *(uint32_t*)(*(uint32_t*)(get_vsh_pad_obj() + 4) + 0x104 + port_no * 0xE8);

	// sys_hid_manager_read()
	system_call_4(0x1F6, (uint64_t)port, /*0x02*//*0x82*/0xFF, (uint64_t)(uint32_t)data+4, 0x80);

	data->len = (int32_t)p1;
}

static void pad_read(void)
{
	// check only pad all ports
	for(int32_t port=0; port<8; port++)
		{MyPadGetData(port, &pdata); curpad = (pdata.button[2] | (pdata.button[3] << 8)); if(curpad && (pdata.len > 0)) break;}  // use MyPadGetData() during VSH menu

	/* Analog left stick management */
	if (pdata.button[6] < 0x10)
		curpad |= PAD_LEFT;
	else if (pdata.button[6] > 0xe0)
		curpad |= PAD_RIGHT;

	if (pdata.button[7] < 0x10)
		curpad |= PAD_UP;
	else if (pdata.button[7] > 0xe0)
		curpad |= PAD_DOWN;

	/* Analog right stick management */
	if (pdata.button[4] < 0x10)
		curpad |= PAD_LEFT;
	else if (pdata.button[4] > 0xe0)
		curpad |= PAD_RIGHT;

	if (pdata.button[5] < 0x10)
		curpad |= PAD_UP;
	else if (pdata.button[5] > 0xe0)
		curpad |= PAD_DOWN;

	if(!enter_button && (curpad & PAD_SWAP_OX))
	{
		curpad ^= PAD_SWAP_OX; // jap mode
	}

	sys_timer_usleep(20000);
}

static void release_cross(void)
{
	while(true) {pad_read(); if(curpad & PAD_CROSS) sys_timer_usleep(150000); else break;}
}
