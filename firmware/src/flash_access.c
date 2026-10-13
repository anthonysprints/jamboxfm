/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* App flash hooks, also compiled against simulated NOR by the isolation tests. */
static uint8_t flash_ok;                 /* JEDEC id matched independently of app persistence */
static int st_read(uint32_t off, void *dst, uint32_t n)   /* 256-byte IRQ-off windows: audio keeps up */
{
    uint8_t *d = dst;
    while (n) {
        uint32_t k = n > 256u ? 256u : n, f = irq_save();
        int rc = FL_FAR(fl_read_ram)(off, d, k);
        irq_restore(f);
        if (rc)
            return rc;
        off += k;
        d += k;
        n -= k;
    }
    return 0;
}
static void audio_silence(void)                 /* IRQs off: the DMA would loop stale audio (buzz) */
{
    uint32_t i;
    for (i = 0; i < sizeof abuf / sizeof abuf[0]; i++)
        abuf[i] = 0;
}
/* a sector erase with the audio silenced inside the same interrupts-off window (silenced before it, the
 * audio interrupt could render a fresh half that the DMA then loops for the whole erase: a buzz) */
static int fl_erase4k_quiet(uint32_t off, uint32_t *took)
{
    uint32_t f = irq_save();
    int rc;
    audio_silence();
    rc = FL_FAR(fl_erase4k_ram)(off, took);
    irq_restore(f);
    return rc;
}
static int st_erase(uint32_t off)
{
    uint32_t took;
    if (!FELUCCA_PERSIST || !flash_ok || !FL_STORE_OK(off, 0x1000u))
        return -8;
    return fl_erase4k_quiet(off, &took);
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    if (!FELUCCA_PERSIST || !flash_ok || !FL_STORE_OK(off, n))
        return -8;
    return fl_write(off, src, n);
}
/* Only hardware identity/window initialization: no application object reads
 * or writes. Recovery probes separately without starting the instrument. */
static void flash_boot(void)
{
    uint32_t f = irq_save();
    flash_ok = FL_FAR(fl_jedec_ram)() == 0x856014u;
    irq_restore(f);
    if (flash_ok)
        fl_plain_window_init();
}
