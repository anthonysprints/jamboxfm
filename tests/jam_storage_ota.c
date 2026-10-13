/* SPDX-License-Identifier: GPL-3.0-only */
/* Keep the inherited full OTA session suite, then check a patterned foreign
 * flash image byte-for-byte across successful staging and abort cleanup.
 * Hardware/ROM commit remains mocked: this is not a physical recovery test. */
#define JAMBOX_PROTO 1
#define FELUCCA_FLASH 1
#define FELUCCA_OTA 1
#define FELUCCA_PERSIST 0
#include "../firmware/src/app_policy.h"
#define main jam_inherited_ota_main
#include "ota_test.c"
#undef main
static uint8_t protected_before[sizeof nor];
static void foreign_fixture(void)
{
    reset_dev();
    /* Keep the actual boot head that the protocol compares with the package. */
    for (uint32_t i = 0x4000; i < sizeof nor; ++i)
        if (i < OTA_AREA || i >= OTA_AREA + OTA_AREA_LEN)
            nor[i] = (uint8_t)(i * 13u + (i >> 9) + 27u);
    memcpy(protected_before, nor, sizeof nor);
}
static int protected_equal(void)
{
    return !memcmp(protected_before, nor, OTA_AREA) &&
           !memcmp(protected_before + OTA_AREA + OTA_AREA_LEN, nor + OTA_AREA + OTA_AREA_LEN,
                   sizeof nor - OTA_AREA - OTA_AREA_LEN);
}
int main(int argc, char **argv)
{
    int bad = jam_inherited_ota_main(argc, argv);
    if (bad) return bad;
    foreign_fixture();
    int rc = ota_session(); uint32_t full_requests = requests;
    bad += check("JamBox real OTA session stages and commits with foreign user data present", !rc && committed && !bad_erase);
    bad += check("successful staging preserves every non-OTA flash byte", protected_equal());
    bad += check("boot cleanup erases only staging with foreign data present", ota_boot_cleanup() == 1 && area_erased() && !bad_erase);
    bad += check("boot cleanup preserves all protected data", protected_equal());
    foreign_fixture(); mute_after = 4;
    rc = ota_session();
    bad += check("disconnected updater aborts without committing", rc < 0 && !committed && area_erased() && !bad_erase);
    bad += check("abort cleanup preserves every non-OTA byte", protected_equal());
    foreign_fixture(); mute_after = full_requests - 1;
    rc = ota_session();
    bad += check("missing host confirmation cleans staging without commit", rc == -13 && !committed && area_erased() && !bad_erase);
    bad += check("unconfirmed update preserves all protected data", protected_equal());
    puts(bad ? "JamBox OTA isolation FAILED" : "JamBox OTA isolation: 8 extra checks passed");
    return bad != 0;
}
