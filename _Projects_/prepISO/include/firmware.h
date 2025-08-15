#define SC_PEEK_LV2 	(6)

// ofw build date
#define FW466	0x323031342F31312FULL // 2014/11/
#define FW475	0x323031352F30342FULL // 2015/04/
#define FW476	0x323031352F30382FULL // 2015/08/
#define FW478	0x323031352F31322FULL // 2015/12/
#define FW480	0x323031362F30342FULL // 2016/04/
#define FW481	0x323031362F31302FULL // 2016/10/
#define FW482	0x323031372F30382FULL // 2017/08/
#define FH482	0x323031372F30392FULL // 2017/09/
#define FW483	0x323031382F30392FULL // 2018/09/
#define FW484	0x323031392F30312FULL // 2019/01/
#define FW485	0x323031392F30382FULL // 2019/08/
#define FW486	0x323032302F30312FULL // 2020/01/
#define FW487	0x323032302F30372FULL // 2020/07/
#define FW488	0x323032312F30342FULL // 2021/04/

static float c_firmware = 0.0f;
static u8 dex_mode = 0;

static uint64_t peek(uint64_t addr) //lv2
{
	lv2syscall1(SC_PEEK_LV2, (addr | 0x8000000000000000ULL));
	return (uint64_t) p1;
}

static void detect_firmware(void)
{
	const uint64_t CEX = 0x4345580000000000ULL;
	const uint64_t DEX = 0x4445580000000000ULL;
	const uint64_t DEH = 0x4445480000000000ULL;

	dex_mode = 0;

	if(peek(0x2ED818ULL) == CEX) {c_firmware =  (peek(0x2FCB68ULL) == FW487) ? 4.87f :
												(peek(0x2FCB68ULL) == FW486) ? 4.86f :
												(peek(0x2FCB68ULL) == FW485) ? 4.85f :
												(peek(0x2FCB68ULL) == FW484) ? 4.84f :
												(peek(0x2FCB68ULL) == FW483) ? 4.83f :
												(peek(0x2FCB68ULL) == FW482) ? 4.82f :
												(peek(0x2FCB68ULL) == FW481) ? 4.81f :
												(peek(0x2FCB68ULL) == FW478) ? 4.78f :
												(peek(0x2FCB68ULL) == FW476) ? 4.76f :
												(peek(0x2FCB68ULL) == FW475) ? 4.75f : 4.88f;} else
	if(peek(0x2ED808ULL) == CEX) {c_firmware = 4.80f;} else
	if(peek(0x2ED778ULL) == CEX) {c_firmware = 4.70f;} else
	if(peek(0x2ED860ULL) == CEX) {c_firmware = (peek(0x2FC938ULL) == 0x323031342F31312FULL)?4.66f:4.65f;} else
	if(peek(0x2ED850ULL) == CEX) {c_firmware = 4.60f;} else
	if(peek(0x2EC5E0ULL) == CEX) {c_firmware = 4.55f;} else
	if(peek(0x2E9D70ULL) == CEX) {c_firmware = 4.53f;} else
	if(peek(0x2E9BE0ULL) == CEX) {c_firmware = 4.50f;} else
	if(peek(0x2EA9B8ULL) == CEX) {c_firmware = 4.46f;} else
	if(peek(0x2EA498ULL) == CEX) {c_firmware = 4.41f;} else
	if(peek(0x2EA488ULL) == CEX) {c_firmware = 4.40f;} else
	if(peek(0x2E9F18ULL) == CEX) {c_firmware = 4.31f;} else
	if(peek(0x2E9F08ULL) == CEX) {c_firmware = 4.30f;} else
	if(peek(0x2E8610ULL) == CEX) {c_firmware = 4.21f;} else
	if(peek(0x2D83D0ULL) == CEX) {c_firmware = 3.55f;} else

	if(peek(0x30F3B0ULL) == DEX) {c_firmware =  (peek(0x31F028ULL) == FW487) ? 4.87f :
												(peek(0x31F028ULL) == FW486) ? 4.86f :
												(peek(0x31F028ULL) == FW485) ? 4.85f :
												(peek(0x31F028ULL) == FW484) ? 4.84f :
												(peek(0x31F028ULL) == FW482) ? 4.82f :
												(peek(0x31F028ULL) == FW481) ? 4.81f : 4.88f; dex_mode = 2;} else
	if(peek(0x30F3A0ULL) == DEX) {c_firmware = 4.80f; dex_mode = 2;} else
	if(peek(0x30F2D0ULL) == DEX) {c_firmware =  (peek(0x31EF48ULL) == FW478) ? 4.78f :
												(peek(0x31EF48ULL) == FW476) ? 4.76f : 4.75f; dex_mode = 2;} else
	if(peek(0x30F240ULL) == DEX) {c_firmware = 4.70f; dex_mode = 2;} else
	if(peek(0x30F1A8ULL) == DEX) {c_firmware = (peek(0x31EBA8ULL) == 0x323031342F31312FULL) ? 4.66f : 4.65f; dex_mode = 2;} else
	if(peek(0x30F198ULL) == DEX) {c_firmware = 4.60f; dex_mode = 2;} else
	if(peek(0x30D6A8ULL) == DEX) {c_firmware = 4.55f; dex_mode = 2;} else
	if(peek(0x30AEA8ULL) == DEX) {c_firmware = 4.53f; dex_mode = 2;} else
	if(peek(0x309698ULL) == DEX) {c_firmware = 4.50f; dex_mode = 2;} else
	if(peek(0x305410ULL) == DEX) {c_firmware = 4.46f; dex_mode = 2;} else
	if(peek(0x304EF0ULL) == DEX) {c_firmware = 4.41f; dex_mode = 2;} else
	if(peek(0x304EE0ULL) == DEX) {c_firmware = 4.40f; dex_mode = 2;} else
	if(peek(0x304640ULL) == DEX) {c_firmware = 4.31f; dex_mode = 2;} else
	if(peek(0x304630ULL) == DEX) {c_firmware = 4.30f; dex_mode = 2;} else
	if(peek(0x302D88ULL) == DEX) {c_firmware = 4.21f; dex_mode = 2;} else
	if(peek(0x2EFE20ULL) == DEX) {c_firmware = 3.55f; dex_mode = 2;} else

	if(peek(0x319F78ULL) == DEH) {c_firmware = 3.55f; dex_mode = 1;} else
	if(peek(0x32EDC8ULL) == DEH) {c_firmware = 4.60f; dex_mode = 1;} else
	if(peek(0x32EB60ULL) == DEH) {c_firmware =  (peek(0x344B70ULL) == FW487) ? 4.87f :
												(peek(0x344B70ULL) == FW486) ? 4.86f :
												(peek(0x344B70ULL) == FW485) ? 4.85f :
												(peek(0x344B70ULL) == FW484) ? 4.84f :
												(peek(0x344B70ULL) == FW483) ? 4.83f :
												(peek(0x344B70ULL) == FH482) ? 4.82f :
												(peek(0x344B70ULL) == FW481) ? 4.81f :
												(peek(0x344B70ULL) == FW478) ? 4.78f :
												(peek(0x344B70ULL) == FW476) ? 4.76f : 4.75f; dex_mode = 1;} else
	if(peek(0x32EB60ULL) == DEH) {c_firmware = 4.80f; dex_mode = 1;} else

	if(peek(0x2CFF98ULL) == CEX) {c_firmware=3.41f;}
	//if(peek(0x2E79C8ULL)==DEX) {c_firmware=3.41f; dex_mode=2;}	else
}
