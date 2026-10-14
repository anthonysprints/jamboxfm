/* SPDX-License-Identifier: GPL-3.0-only */
/* Compile the actual parser as Sloop: a JamBox RAM section cannot be mistaken
 * for a retained project and subsequently flushed into Sloop's flash slots. */
#define JAMBOX_PROTO 0
#define main hostsim_main
#include "hostsim.c"
#undef main
#define PROJ_HOST 1
static uint32_t trk_def_engine(uint32_t i)
{ static const uint8_t e[NPART] = {0,1,3}; return i < NPART ? e[i] : 0; }
#include "../firmware/src/project.c"
int main(void)
{
    project_t source, parsed;
    host_tracks_init(); proj_capture(&source);
    if (!proj_ok(&source) || !proj_import(&parsed, &source, sizeof source)) {
        puts("Sloop positive-control project rejected"); return 1;
    }
    source.magic = 0x4A524D30u; source.sum = proj_sum(&source);
    if (proj_ok(&source) || proj_import(&parsed, &source, sizeof source)) {
        puts("Sloop accepted a checksum-valid JamBox JRM0 RAM section"); return 1;
    }
    source.magic = 0x4A524D31u; source.sum = proj_sum(&source);
    if (proj_ok(&source) || proj_import(&parsed, &source, sizeof source)) {
        puts("Sloop accepted a JamBox JRM1 magic at the legacy size"); return 1;
    }
    uint32_t larger[4584u / 4u] = {0};
    memcpy(larger, &source, sizeof source - 4u);
    larger[1] = sizeof larger;
    larger[sizeof larger / 4u - 1u] = proj_hash(larger, sizeof larger - 4u);
    if (proj_import(&parsed, larger, sizeof larger)) {
        puts("Sloop accepted a checksum-valid full-size JamBox JRM1 snapshot"); return 1;
    }
    puts("Sloop parser: valid FUN6 accepted; JamBox JRM0 and JRM1 rejected");
    return 0;
}
