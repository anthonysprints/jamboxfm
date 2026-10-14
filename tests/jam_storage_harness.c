/* SPDX-License-Identifier: GPL-3.0-only */
/* Real instrument/UI/editor/storage code against a byte-addressed NOR fixture.
 * Raw flash mocks intentionally DO NOT enforce policy: unexpected writes reach
 * the image and fail snapshot/counter assertions. Production gates are included.
 * Inbound retained app state is discarded. SET3/PAN5 remain shared formats:
 * another firmware can observe JamBox RAM preferences on a warm return if it
 * lacks a valid flash settings copy. Only NOR preservation is claimed here. */
#define JAMBOX_PROTO 1
#define FELUCCA_FLASH 1
#define FELUCCA_OTA 1
#define FELUCCA_PERSIST 0
#define FELUCCA_ARRANGER 1
#define FELUCCA_SLICE 0
#define FELUCCA_VERSION "JamBox storage test"
#include <stdint.h>
static uint8_t nor[0x100000], before[0x100000];
#define SMP_USER_XIP(k) (nor + SMP_USER_OFF(k))
#include "../firmware/src/app_policy.h"
#include "../firmware/hal/fm1_flash_ranges.h"
#define ota_wire_send jam_unused_usb_send
#define main hostsim_main
#include "hostsim.c"
#undef main
#undef ota_wire_send
#include "../firmware/src/jam_palette.c"
#include <assert.h>
static uint16_t screen[240*240];
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ uint32_t i,j; assert(x+w<=240 && y+h<=240); for(j=0;j<h;j++) for(i=0;i<w;i++) screen[(y+j)*240+x+i]=p[j*w+i]; }
#include "../firmware/src/gfx.c"
static void lcd_fill(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint16_t c)
{ uint32_t i,j; assert(x+w<=240&&y+h<=240); for(j=0;j<h;j++)for(i=0;i<w;i++)screen[(y+j)*240+x+i]=swap16(c); }
static int32_t encs[7];
static uint32_t fm1_ticks(void) { return fm1_ms * 1000u * 24u; }
#define FM1_TICKS_PER_US 24u
static int32_t fm1_enc_take(uint32_t e) { int32_t s = encs[e]; encs[e] = 0; return s; }
static uint8_t fm1_led[16], fm1_led_dim[16], fm1_led_bg[16];
static volatile uint16_t fm1_led_bg_ns;
#define FM1_NCOL 16u
static const int8_t FM1_KEYMAP[5][16];
static void fm1_led_key(uint32_t id, int on) { (void)id; (void)on; }
static uint32_t edges_btn, notes_seen;
static uint32_t fm1_input_edges(int x) { uint32_t e = edges_btn; (void)x; edges_btn = 0; return e; }
static uint32_t fm1_input_note_edges(void) { uint32_t e = fm1_in.notes & ~notes_seen; notes_seen = fm1_in.notes; return e; }
static uint32_t wdt_tick_ms;                       /* (stress_test: 1, the blocking PANEL setup's clock runs out) */
static void fm1_wdt_feed(void) { fm1_ms += wdt_tick_ms; }
static int32_t fm1_adc_read(int c) { (void)c; return -1; }
static struct { uint32_t magic, stage, page, home, ui_frames; } felucca_dbg;
#define FELUCCA_ICONS 0
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N], scope_bufr[SCOPE_N];
static uint32_t scope_w;
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
#include "../firmware/src/ui_song.c"
#include "../firmware/src/ui_studio.c"
#include "../firmware/src/icons.c"
#include "../firmware/src/ui_draw.c"
#include "../firmware/src/ui_vis.c"
#include "../firmware/src/ui_layers.c"
#include "../firmware/src/ui_menu.c"
#include "../firmware/src/ui_input.c"

static uint32_t raw_erases, raw_programs, raw_reads, plain_inits;
static uint32_t jedec = 0x856014u;
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
#define FL_FAR(fn) (fn)
static int fl_read_ram(uint32_t off, void *dst, uint32_t n)
{
    assert(off <= sizeof nor && n <= sizeof nor - off);
    ++raw_reads; memcpy(dst, nor + off, n); return 0;
}
static int fl_erase4k_ram(uint32_t off, uint32_t *took)
{
    assert(!(off & 4095u) && off <= sizeof nor - 4096u);
    ++raw_erases; memset(nor + off, 255, 4096); *took = 1; return 0;
}
static int fl_write(uint32_t off, const void *src, uint32_t n)
{
    assert(off <= sizeof nor && n <= sizeof nor - off);
    ++raw_programs;
    for (uint32_t i = 0; i < n; ++i) nor[off + i] &= ((const uint8_t *)src)[i];
    return 0;
}
static uint32_t fl_jedec_ram(void) { return jedec; }
static void fl_plain_window_init(void) { ++plain_inits; }
static int16_t abuf[1024];
#include "../firmware/src/flash_access.c"
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"
static void fl_inval(uint32_t off, uint32_t n) { (void)off; (void)n; }
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void) { ++fm1_ms; }
static uint8_t reply[640];
static uint32_t reply_len;
static int ota_wire_send(const uint8_t *p, uint32_t n)
{ assert(n <= sizeof reply); memcpy(reply, p, n); reply_len = n; return 0; }
#include "../firmware/src/editor.c"
#include "../firmware/src/instrument_init.c"
#define OTA_AREA 0xE0000u
#define OTA_AREA_LEN 0x5000u
#include "../firmware/src/ota_flash_access.c"

static unsigned checks, failures;
static void check(int ok, const char *label)
{
    ++checks; failures += !ok;
    printf("storage: %-76s %s\n", label, ok ? "ok" : "FAIL");
}
static int all_zero(const void *p, size_t n)
{
    const uint8_t *b = p;
    for (size_t i = 0; i < n; ++i) if (b[i]) return 0;
    return 1;
}
static int project_slots_zero(void)
{
    for (uint32_t i = 0; i < 4u; i++)
        if (!all_zero(project_slot(i), sizeof(project_t))) return 0;
    return 1;
}
static void project_slots_snapshot(project_t out[4])
{ for (uint32_t i = 0; i < 4u; i++) out[i] = *project_slot(i); }
static int project_slots_equal(const project_t expected[4])
{
    for (uint32_t i = 0; i < 4u; i++)
        if (memcmp(&expected[i], project_slot(i), sizeof(project_t))) return 0;
    return 1;
}
static void snapshot(void)
{ memcpy(before, nor, sizeof nor); raw_erases = raw_programs = 0; }
static void unchanged(const char *label)
{ check(!raw_erases && !raw_programs && !memcmp(nor, before, sizeof nor), label); }
/* Fixture creation is intentionally outside production writers. */
static void seed_object(uint32_t obj, uint32_t copy, uint32_t seq, const void *p, uint32_t n)
{
    st_hdr_t h = { .magic = ST_MAGIC, .type = obj, .slot = copy, .seq = seq, .len = n };
    uint32_t off = st_sector(obj, copy);
    assert(n <= ST_PAYLOAD_MAX);
    h.crc = st_crc32(p, n); h.hcrc = st_crc32(&h, sizeof h - 4);
    memset(nor + off, 255, ST_SECTOR);
    memcpy(nor + off, &h, sizeof h);
    memcpy(nor + off + ST_PAYLOAD_OFF, p, n);
}
static void seed_settings(uint32_t magic, uint32_t len, const panel_t *mapping)
{
    uint8_t p[ST_PAYLOAD_MAX];
    memset(p, 0xA7, sizeof p);
    memcpy(p, &magic, 4);
    memcpy(p + (magic == 0x50455231u ? 8 : 16), mapping, sizeof *mapping);
    memset(nor + st_sector(OBJ_SETTINGS, 1), 255, ST_SECTOR);
    seed_object(OBJ_SETTINGS, 0, 1, p, len);
}
static void ordinary_boot(void)
{
    /* These variables are normal BSS on a hardware reset, unlike proj_slot,
     * panel and settings, which are deliberately left retained in this test. */
    song.playing = 0; transport_req = 0; fm1_in.notes = fm1_in.buttons = 0;
    flash_boot(); persist_boot(); settings_init(); panel_init(); felucca_init();
}
static panel_t alternative_panel(void)
{
    panel_t p = PANEL_DEFAULT;
    uint8_t b = p.btn[0]; p.btn[0] = p.btn[4]; p.btn[4] = b;
    b = p.enc[0]; p.enc[0] = p.enc[3]; p.enc[3] = b;
    p.dir[2] = -1;
    return p;
}
static void test_boot(void)
{
    project_v6_t foreign;
    project_t current;
    up_bank_t bank = { .magic = UP_BANK_MAGIC, .rsize = sizeof(up_rec_t), .nslot = UP_PER_BANK };
    panel_t calibrated = alternative_panel();
    memset(nor, 255, sizeof nor);
    for (uint32_t i = 0; i < 4u; i++) memset(project_slot(i), 0x55, sizeof(project_t));
    memset(&settings, 0x55, sizeof settings); memset(&panel, 0x55, sizeof panel);
    snapshot(); ordinary_boot();
    check(flash_ok && plain_inits == 1, "ordinary boot detects flash independently of persistence");
    check(project_slots_zero() && !sec_dirty && !song_dirty, "ordinary boot clears retained slots and dirty state");
    check(settings.palette == 4 && !settings.lowcut && !settings.zoom, "ordinary boot applies JamBox display defaults");
    check(lights_notes == 1, "ordinary device boot enables sounding-note lights without browser overrides");
    check(!memcmp(&panel, &PANEL_DEFAULT, sizeof panel), "ordinary boot uses default panel when flash has no calibration");
    check(song.g[G_BPM] == 88 && TDRUM->p[P_SLEN] == 32 && song.sel == jam_role_track(2), "real instrument initializer creates the playable Dusty session");
    unchanged("ordinary boot does not modify any NOR byte");

    /* A complete valid foreign project, bank and settings object coexist with
     * patterned samples/FM6/globals. Only the panel calibration may be read in. */
    for (uint32_t i = 0; i < sizeof nor; ++i) nor[i] = (uint8_t)(i * 17u + (i >> 11) + 33u);
    proj_capture(&current);
    memcpy(&foreign, &current, __builtin_offsetof(project_v6_t, sum));
    foreign.magic = PROJ_MAGIC_V6; foreign.size = sizeof foreign; foreign.g[G_BPM] = 173;
    foreign.sum = proj_hash(&foreign, sizeof foreign - 4u);
    for (uint32_t i = 0; i < 4; ++i) {
        seed_object(OBJ_PROJECT0 + i, 0, 20 + i, &foreign, sizeof foreign);
        memset(nor + st_sector(OBJ_PROJECT0 + i, 1), 255, ST_SECTOR);
        memcpy(project_slot(i), &foreign, sizeof foreign);
    }
    seed_object(OBJ_AUTOSAVE, 0, 5, &foreign, sizeof foreign);
    memset(nor + st_sector(OBJ_AUTOSAVE, 1), 255, ST_SECTOR);
    bank.r[0].used = UP_USED; bank.r[0].ver = UP_VER; bank.r[0].np = P_COUNT;
    memcpy(bank.r[0].name, "FOREIGN", 8);
    seed_object(OBJ_UPRESET0, 0, 2, &bank, sizeof bank);
    seed_settings(0x50455233u, 1592, &calibrated);
    /* Valid foreign preferences and bank data, not merely arbitrary bytes
     * that an accidentally enabled importer could reject for other reasons. */
    uint8_t foreign_settings[1592] = {0};
    persist_t foreign_p = {.magic=PERSIST_MAGIC,.palette=1,.lowcut=1,.zoom=1,.panel=calibrated,.lights=0x17073};
    arr_defaults(&foreign_p.arrangement);
    dsu_defaults(); dsu_bank_t foreign_dsu=dsu; memcpy(foreign_dsu.k[0].name,"FOREIGN!",8);
    memcpy(foreign_settings,&foreign_p,sizeof foreign_p);
    memcpy(foreign_settings+sizeof foreign_p,&foreign_dsu,sizeof foreign_dsu);
    uint32_t mo=sizeof foreign_p+sizeof foreign_dsu+2u*ML_N;
    foreign_settings[mo]=12; foreign_settings[mo+1]=13; foreign_settings[mo+2]=14;
    foreign_settings[mo+3]=15; foreign_settings[mo+4]=7;
    seed_object(OBJ_SETTINGS,0,1,foreign_settings,sizeof foreign_settings);
    for (uint32_t i=0;i<SMP_USER_SLOTS;++i) {
        smp_user_hdr_t h={.magic=SMP_USER_MAGIC,.version=1,.nz=1,.data_len=2};
        memcpy(h.name,"FOREIGN",8);
        h.zone[0]=(smp_zone_t){.n=4,.le=3,.rate=65536,.root16=60*16,.hi=127};
        uint8_t adpcm[2]={0x11,0x22}; h.crc=st_crc32(adpcm,sizeof adpcm);
        memcpy(nor+SMP_USER_OFF(i),&h,sizeof h);
        memcpy(nor+SMP_USER_OFF(i)+SMP_USER_DATA,adpcm,sizeof adpcm);
    }
    settings.magic = SETTINGS_MAGIC; settings.palette = 1; settings.lowcut = settings.zoom = 1;
    panel = calibrated; up_bank[0] = bank; sec_dirty = 15; song_dirty = settings_later = 1;
    snapshot(); ordinary_boot();
    check(project_slots_zero() && !arrangement_ready(), "foreign retained FUN6 slots and flash projects are not imported");
    check(!up_count() && all_zero(up_bank, sizeof up_bank), "foreign user preset bank is not imported and RAM banks are empty");
    check(settings.palette == 4 && !settings.lowcut && !settings.zoom, "foreign settings and valid retained preferences are discarded");
    check(!memcmp(&panel, &calibrated, sizeof panel), "only validated foreign physical panel mapping is imported");
    check(song.g[G_BPM] == 88 && !sec_dirty && !song_dirty && !settings_later, "foreign autosave and pending saves cannot replace fresh session");
    check(memcmp(dsu.k[0].name,"FOREIGN!",8) && midi_set[0]==1 && midi_set[1]==2 && midi_set[2]==3 &&
          midi_set[3]==10 && midi_set[5]==2 && !usb_serial && !lights_lvl && !lights_sync,
          "valid foreign SYN/MIDI/lighting/USB preferences are not imported");
    check(all_zero(usr_nz, sizeof usr_nz) && !fm6_bank_read, "boot does not import foreign sample indexes or attach FM6 bank");
    unchanged("foreign retained/flash boot preserves all NOR bytes");
    for (uint32_t i = 0; i < 4; ++i) {
        song.g[G_BPM] = (int16_t)(101 + i); section_store(i);
        check(proj_ok(project_slot(i)) && project_slot(i)->magic == PROJ_MAGIC, "internal arranger snapshot uses valid private JRM1 format");
    }
    project_t parsed;
    memset(&parsed,0xC3,sizeof parsed);
    check(proj_import(&parsed,project_slot(2),sizeof(project_t)) && parsed.g[G_BPM]==103,
          "private JamBox RAM snapshot remains readable before a reset");
    snapshot(); ordinary_boot();
    check(project_slots_zero() && song.g[G_BPM] == 88, "warm boot discards every different retained JamBox section");
    unchanged("warm JamBox boot neither flushes nor migrates retained sections");
    project_t untouched; memset(&untouched,0xC3,sizeof untouched); parsed=untouched;
    check(!proj_import(&parsed, &foreign, sizeof foreign) && !memcmp(&parsed,&untouched,sizeof parsed),
          "JamBox rejects checksum-valid FUN6 without touching the destination");
    project_v5_t v5={.magic=PROJ_MAGIC_V5,.size=sizeof(project_v5_t)};
    v5.g[G_BPM]=143; v5.sum=proj_hash(&v5,sizeof v5-4u);
    check(!proj_import(&parsed,&v5,sizeof v5) && !memcmp(&parsed,&untouched,sizeof parsed),
          "JamBox rejects checksum-valid historical FUN5 without destination mutation");
    memcpy(project_slot(0), &foreign, sizeof foreign); settings.magic=SETTINGS_MAGIC; settings.palette=1; panel=calibrated;
    uint32_t reads_before=raw_reads;
    jedec = 0; snapshot(); ordinary_boot();
    check(raw_reads==reads_before && project_slots_zero() && settings.palette==4,
          "unknown JEDEC still clears retained app data without any raw flash read");
    check(!flash_ok && !memcmp(&panel, &PANEL_DEFAULT, sizeof panel), "unrecognized flash identity falls back to default calibration");
    unchanged("unrecognized flash identity cannot enable app storage");
    jedec = 0x856014u; ordinary_boot();
}
static void test_device_snap_lights(void)
{
    track_t *t = &trk[2];
    snapshot();
    song.sel = 2; ui.layer = LY_PLAY;
    t->p[P_ROOT] = 3; t->p[P_SCALE] = 1; /* D# major. */
    t->p[P_QUANT] = 1; t->p[P_CHORD] = 0;
    t->engine = t->eng_req;
    fm1_in.notes = 1u << 10; keyboard_block(); pads_tick(); /* D#4. */
    check(kb_nt[10][0] == 63 && keys_lit() == (1u << 10),
          "device SNAP lights only the pressed D# when it is already in scale");
    fm1_in.notes = 0; keyboard_block();
    for (uint32_t i = 0; i < 8; ++i) pads_tick();
    check(!keys_lit(), "device SNAP lights release after the short note-hit flash");
    fm1_in.notes = 1u << 11; keyboard_block(); pads_tick(); /* E4 -> D#4. */
    check(kb_nt[11][0] == 63 && keys_lit() == ((1u << 10) | (1u << 11)),
          "device SNAP lights pressed E and sounding D# when the note is corrected");
    mi_set(MI_NOTES, -1);
    check(keys_lit() == (1u << 11), "NOTES off still allows pressed-key feedback");
    mi_set(MI_NOTES, 1);
    check(keys_lit() == ((1u << 10) | (1u << 11)), "NOTES on restores sounding-key feedback immediately");
    fm1_in.notes = 0; keyboard_block();
    for (uint32_t i = 0; i < 8; ++i) pads_tick();
    check(!keys_lit(), "both corrected-key lights release cleanly");
    unchanged("device note lighting and its menu setting leave NOR untouched");
}
static void test_calibration(void)
{
    static const struct { uint32_t magic, len; } formats[] = {
        {0x50455231,40}, {0x50455232,48}, {0x50455232,52}, {0x50455233,52},
        {0x50455233,84}, {0x50455233,88}, {0x50455233,1552}, {0x50455233,1584},
        {0x50455233,1588}, {0x50455233,1592}, {0x50455234,572}, {0x50455235,764}
    };
    panel_t good = alternative_panel();
    uint8_t prefs[sizeof settings], midi[sizeof midi_set];
    settings.palette = 2; settings.lowcut = 1; settings.zoom = 1;
    lights_sync = 3; lights_mout = 1; lights_min = 2;
    memcpy(prefs, &settings, sizeof prefs); memcpy(midi, midi_set, sizeof midi);
    for (uint32_t i = 0; i < sizeof formats / sizeof formats[0]; ++i) {
        char label[96];
        seed_settings(formats[i].magic, formats[i].len, &good); snapshot();
        snprintf(label, sizeof label, "audited calibration schema PER%u / %u bytes accepted", formats[i].magic & 15u, formats[i].len);
        check(jam_calibration_boot() == 1 && !memcmp(&panel, &good, sizeof panel), label);
        check(!memcmp(prefs, &settings, sizeof prefs) && !memcmp(midi, midi_set, sizeof midi) &&
              lights_sync == 3 && lights_mout == 1 && lights_min == 2, "calibration read imports no preferences, MIDI or lighting state");
        unchanged("calibration import is read-only across the whole NOR image");
    }
    for (uint32_t i = 0; i < 9; ++i) {
        panel_t bad = good;
        switch (i) {
        case 0: bad.magic ^= 1; break;
        case 1: bad.btn[0] = bad.btn[1]; break;
        case 2: bad.btn[0] = 14; break;
        case 3: bad.enc[0] = bad.enc[1]; break;
        case 4: bad.enc[0] = 7; break;
        case 5: bad.dir[0] = 0; break;
        case 6: bad.dir[0] = 2; break;
        case 7: bad.btn[0] = 255; break;
        case 8: bad.enc[0] = 255; break;
        }
        seed_settings(0x50455233, 1592, &bad); snapshot(); panel = good;
        check(!jam_calibration_boot() && !memcmp(&panel, &PANEL_DEFAULT, sizeof panel), "invalid panel magic/permutation/direction falls back to defaults");
        unchanged("rejected panel payload leaves NOR untouched");
    }
    for (uint32_t i = 0; i < 4; ++i) {
        seed_settings(i == 0 ? 0x50455236 : 0x50455233, i == 1 ? 1593 : i == 2 ? 51 : 1592, &good);
        if (i == 3) nor[st_sector(OBJ_SETTINGS,0) + ST_PAYLOAD_OFF + 1591] ^= 1;
        snapshot(); panel = good;
        check(!jam_calibration_boot() && !memcmp(&panel, &PANEL_DEFAULT, sizeof panel), "unknown schema/length or full-payload CRC failure rejected");
        unchanged("rejected settings object leaves NOR untouched");
    }
    /* Valid older A survives a corrupt newer B, through the real A/B reader. */
    seed_settings(0x50455233, 1592, &good);
    uint8_t data[1592]; memcpy(data, nor + st_sector(OBJ_SETTINGS,0) + ST_PAYLOAD_OFF, sizeof data);
    seed_object(OBJ_SETTINGS, 1, 2, data, sizeof data);
    nor[st_sector(OBJ_SETTINGS,1) + ST_PAYLOAD_OFF + 1591] ^= 1;
    snapshot(); check(jam_calibration_boot() == 1 && !memcmp(&panel, &good, sizeof panel), "torn newer calibration copy falls back to intact older copy");
    unchanged("A/B calibration fallback does not repair or write flash");
    ordinary_boot();
}
static void test_writers(void)
{
    project_t slots[4]; up_bank_t banks[2]; up_rec_t record = {0};
    song.playing = 0; transport_req = 0;
    section_store(0); project_slots_snapshot(slots); memcpy(banks, up_bank, sizeof banks);
    snapshot();
    for (uint32_t i = 0; i < 4; ++i) {
        project_save(i); check(!strcmp(ui.msg, "STORAGE DISABLED"), "explicit project SAVE reports storage disabled");
        project_erase(i); check(!strcmp(ui.msg, "STORAGE DISABLED"), "explicit project ERASE reports storage disabled");
    }
    check(project_slots_equal(slots), "forbidden explicit project operations preserve RAM section contents");
    check(up_put(0, &record) == 6 && up_put(0, NULL) == 6 && up_store(0, "TEST") == 6,
          "preset PUT/erase/store return explicit disabled status");
    up_ui(1,0); check(!strcmp(ui.msg, "STORAGE DISABLED"), "preset UI erase reports disabled");
    up_ui(2,0); check(!strcmp(ui.msg, "STORAGE DISABLED"), "preset UI save reports disabled");
    check(!memcmp(banks, up_bank, sizeof banks), "forbidden preset operations preserve RAM banks");
    uint32_t autosave_before = autosave_hash;
    fm1_ms = 120000; ui_input_ms = 0; ui.menu = 0;
    autosave_tick(); autosave_resume(); settings_save();
    check(autosave_hash == autosave_before, "idle autosave and resume cannot claim a stored working project");
    sec_dirty = 15; song_dirty = settings_later = 1;
    sections_flush(); persist_flush_now(); settings_save();
    unchanged("SAVE/autosave/settings/pre-reset flush cannot mutate NOR");
    uint8_t byte = 0;
    static const uint32_t offsets[] = {0,0x97000,0x9f000,0xa0000,0xdc000,0xe0000,0xe5000,0xe7000,0xfc000,0xfd000,0xfe000,0xff000,0xfffff};
    for (uint32_t i = 0; i < sizeof offsets / sizeof offsets[0]; ++i) {
        check(st_prog(offsets[i], &byte, 1) == -8, "production app program hook rejects protected and OTA addresses");
        check(st_erase(offsets[i] & ~4095u) == -8, "production app erase hook rejects every range with persistence off");
    }
    for (uint32_t i = 0; i < OBJ_COUNT; ++i)
        check(st_save(i, &byte, 1) != 0, "real A/B object save cannot bypass disabled app hooks");
    unchanged("direct app hooks and object storage cannot bypass session policy");
}
static void command(uint32_t cmd, const uint8_t *args, uint32_t n)
{
    uint8_t frame[640] = {0x7D,0x46,0x4C,0};
    assert(n <= sizeof frame - 4); frame[3] = (uint8_t)cmd;
    if (n) memcpy(frame + 4, args, n);
    reply_len = 0; ed_handle(frame, n + 4);
    check(reply_len >= 6 && reply[0] == 0xF0 && reply[4] == cmd && reply[reply_len - 1] == 0xF7, "real editor dispatcher returns correctly framed command reply");
}
static void refused(uint32_t cmd, const uint8_t *args, uint32_t n, uint32_t payload_len)
{
    command(cmd,args,n);
    check(reply_len == payload_len + 6 && reply[reply_len - 2] == 6, "editor persistent writer returns status 6 in its normal reply shape");
}
static void test_editor(void)
{
    uint8_t a[512] = {0};
    project_t slots[4]; up_bank_t banks[2]; dsu_bank_t ds;
    (void)dsu_kit(0); ds = dsu;
    project_slots_snapshot(slots); memcpy(banks, up_bank, sizeof banks); snapshot();
    a[0]=1; a[1]=0; refused(ED_PROJECT,a,2,4);
    a[0]=0; refused(ED_SMP_BEGIN,a,1,2); refused(ED_SMP_ERASE,a,1,2);
    a[1]=0; a[2]=4; a[3]=0; a[4]=0; a[5]=64; refused(ED_SMP_WRITE,a,6,5);
    memset(a,0,sizeof a); refused(ED_SMP_END,a,80,2);
    refused(ED_UP_PUT,a,180,2); a[1]='A'; a[2]=0; refused(ED_UP_STORE,a,3,2); refused(ED_UP_ERASE,a,1,2);
    a[0]=ED_FM6_BANK; a[1]=0; memcpy(a+2,FM6_FACTORY[0],FM6_PACKED); refused(ED_FM6_PUT,a,FM6_PACKED+2,3);
    a[0]=0; refused(ED_FM6_ERASE,a,1,2); refused(ED_DSYN_STORE,NULL,0,1);
    for (uint32_t op=0; op<4; ++op) { a[0]=op; a[1]=0; refused(ED_BK_PUT,a,op==0?11:op==1?8:2,3); }
    command(ED_BK_LIST,NULL,0); check(reply_len==8 && reply[5]==6 && reply[6]==0,"backup list advertises disabled, empty inventory");
    memset(a,0,sizeof a); command(ED_BK_GET,a,6); check(reply_len==15 && reply[6]==6 && reply[12]==0 && reply[13]==0,"backup GET returns disabled with no data");
    check(project_slots_equal(slots) && !memcmp(banks,up_bank,sizeof banks) && !memcmp(&ds,&dsu,sizeof ds), "all forbidden editor writes preserve RAM projects/preset/drum banks");
    check(ed_smp_erase(0,1)==6 && ed_smp_end(0,a,1)==6 && ed_fm6_bank_write(0,a)==6 && ed_dsyn_store()==6,
          "direct sample/FM6/SYN helpers also refuse storage beneath the dispatcher");
    unchanged("all editor save/sample/backup/FM6/drum-store paths preserve NOR");
    /* RAM editing remains meaningful, not a blanket editor shutdown. */
    song.sel=jam_role_track(1); a[0]=0; a[1]=P_LEVEL; a[2]=(8192+83)&127; a[3]=(8192+83)>>7;
    command(ED_SET,a,4); check(TSEL->p[P_LEVEL]==83,"editor synth parameter editing still changes the live RAM sound");
    a[0]=1; a[1]=1; a[2]=60; a[3]=0; a[4]=0; a[5]=0; a[6]=ST_NOTE; a[7]=0; a[8]=96;
    command(ED_STEP_SET,a,9); check(TSEL->step[1].n==1 && TSEL->step[1].note[0]==60,"editor step recording remains editable in RAM");
    a[0]=ED_FM6_TRACK; a[1]=0; memcpy(a+2,FM6_FACTORY[0],FM6_PACKED); a[2+118]='J';
    command(ED_FM6_PUT,a,FM6_PACKED+2); uint8_t packed[FM6_PACKED]; fm6_pack(fm6_patch[0],packed);
    check(reply_len==9 && reply[7]==0 && packed[118]=='J',"editor FM6 track patch remains editable in RAM");
    a[0]=0; a[1]=17; a[2]=0; a[3]=1;
    command(ED_DSYN_PUT,a,4); check(reply_len==9 && reply[7]==0 && dsu.k[0].src==1 && dsu_dirty,"editor synthesized drum kit remains editable in RAM");
    command(ED_DSYN_LIST,NULL,0); check(reply[7]==0,"editor never reports RAM drum kits as stored");
    unchanged("allowed live editor edits still cannot modify NOR");
}
static void test_ui(void)
{
    project_t slots[4]; up_bank_t banks[2];
    project_slots_snapshot(slots); memcpy(banks,up_bank,sizeof banks); snapshot();
    song.sel=jam_role_track(1); song.playing=0; transport_req=0;
    for(uint32_t pg=0;pg<NPAGES;++pg) if(PAGES[pg].graph==GR_SLOTS || PAGES[pg].graph==GR_USER) {
        ui.page=pg; ui.home=0;
        edit_param(PAGES[pg].graph==GR_SLOTS ? 1 : 2,1);
        check(!strcmp(ui.msg,"STORAGE DISABLED") && !ui.arm,"physical page ERASE rejects without arming");
        edit_param(3,1); check(!strcmp(ui.msg,"STORAGE DISABLED") && !ui.arm,"physical page SAVE rejects without arming");
    }
    for(uint32_t k=0;k<32;++k) if(punch_key(k)>=4 && punch_key(k)<8) {
        layer_key(LY_SONG,k,1); check(!strcmp(ui.msg,"STORAGE DISABLED") && !sec_armed,"SAVE layer section-storage gesture is explicitly refused");
    }
    song_screen_input(1u<<panel.btn[B_REC],0); check(!strcmp(ui.msg,"STORAGE DISABLED") && !song_store_armed,"song-screen REC refuses a persistent section capture");
    song_screen_input(1u<<panel.btn[B_SAVE],0); check(!strcmp(ui.msg,"STORAGE DISABLED") && !song_store_armed,"song-screen SAVE refuses persistent storage");
    uint8_t serial=usb_serial; mi_set(MI_SERIAL,1);
    check(usb_serial==serial && !strcmp(ui.msg,"STORAGE DISABLED"),"disabled serial preference cannot offer a reboot-only change");
    menu_close(); check(!strcmp(ui.msg,"SETTINGS IN RAM ONLY"),"settings menu labels temporary preferences truthfully");
    check(project_slots_equal(slots) && !memcmp(banks,up_bank,sizeof banks),"physical save gestures cannot mutate RAM sections or user banks");
    unchanged("physical UI settings/save/erase gestures preserve NOR");
}
static void test_ota_hooks(void)
{
    static const struct {uint32_t off,n;} rejected[] = {
        {0,1},{0x97000,1},{0xdffff,2},{0xe4fff,2},{0xe5000,1},{0xe7000,1},{0xfc000,1},
        {0xe0000,0x5001},{0xe0000,UINT32_MAX},{UINT32_MAX,2},{0x1e0000,1}
    };
    uint8_t data[257]; memset(data,0x5A,sizeof data); snapshot();
    for(uint32_t i=0;i<sizeof rejected/sizeof rejected[0];++i) {
        check(!FL_RANGE_OK(rejected[i].off,rejected[i].n),"production final RAM-driver range policy rejects non-staging/wrapped spans");
        check(ota_prog(rejected[i].off,data,rejected[i].n)==-8,"real updater program hook rejects non-staging/wrapped spans");
    }
    check(ota_erase(0xd0000)==-8 && ota_erase(0xe5000)==-8 && ota_erase(0xe0001)==-8,"real updater erase hook rejects protected/misaligned sectors");
    unchanged("rejected OTA hook requests do not reach raw NOR writers");
    flash_ok=0;
    check(ota_prog(OTA_AREA,data,sizeof data)==-8 && ota_erase(OTA_AREA)==-8,"updater hooks refuse writes without validated flash identity");
    unchanged("missing flash identity cannot alter staging or protected data"); flash_ok=1;
    for(uint32_t off=OTA_AREA;off<OTA_AREA+OTA_AREA_LEN;off+=4096) check(ota_erase(off)==0,"real updater may erase each of its five staging sectors");
    check(ota_prog(OTA_AREA,data,sizeof data)==0 && ota_prog(OTA_AREA+OTA_AREA_LEN-1,data,1)==0,"real updater may program first and last bounded staging bytes");
    check(raw_erases==5 && raw_programs==2,"only requested bounded updater mutations reached raw flash hooks");
    check(!memcmp(before,nor,OTA_AREA) && !memcmp(before+OTA_AREA+OTA_AREA_LEN,nor+OTA_AREA+OTA_AREA_LEN,sizeof nor-OTA_AREA-OTA_AREA_LEN),"allowed update staging preserves every byte outside its 20 KiB window");
    uint8_t readback[257]; check(ota_fread(OTA_AREA,readback,sizeof readback)==0 && !memcmp(readback,data,sizeof data),"updater staging readback uses the actual shared flash read hook");
}
#ifndef JAM_STORAGE_LIBRARY
int main(void)
{
    test_boot(); test_device_snap_lights(); test_calibration(); test_writers(); test_editor(); test_ui(); test_ota_hooks();
    printf("JamBox temporary-session storage: %u checks, %u failures\n",checks,failures);
    return failures ? 1 : 0;
}
#endif /* JAM_STORAGE_LIBRARY */
