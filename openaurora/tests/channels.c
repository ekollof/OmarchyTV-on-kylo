/* Runtime active-list ingestion tests over the recovered ACTIVECHANNELLIST
 * XML shape. No runtime EASI data or TV access. */
#include "openaurora/channels.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *two_rows =
    "<active_list network='Cable' count='2' total_size='2'>"
    "<service rsn='1' name='Alpha &amp; Beta' handle='11' onid='1' "
    "tsid='2' sid='3' type='1'/>"
    "<service rsn='2' name='O&apos;Neil' handle='12' onid='4' "
    "tsid='5' sid='6' type='2'/>"
    "</active_list>";

static void test_ingest(void)
{
    const oa_channel_iface *iface = &oa_channel_runtime_iface;
    iface->clear();
    assert(!iface->snapshot());

    assert(iface->ingest_xml(two_rows, strlen(two_rows)) == 2);
    const struct oa_channel_snapshot *snap = iface->snapshot();
    assert(snap && snap->count == 2 && snap->source == 1);
    assert(snap->lcn_known == 0 && snap->visibility_known == 0);
    assert(snap->entries[0].rsn == 1 && snap->entries[0].handle == 11);
    assert(snap->entries[0].onid == 1 && snap->entries[0].tsid == 2 &&
        snap->entries[0].sid == 3 && snap->entries[0].type == 1);
    assert(!strcmp(snap->entries[0].name, "Alpha & Beta"));
    assert(snap->entries[1].rsn == 2 && !strcmp(snap->entries[1].name,
        "O'Neil"));

    const char *one_row =
        "<active_list network='x' count='1' total_size='1'>"
        "<service type='1' sid='6' tsid='5' onid='4' handle='12' "
        "name='Replaced' rsn='9'/></active_list>";
    assert(iface->ingest_xml(one_row, strlen(one_row)) == 1);
    snap = iface->snapshot();
    assert(snap && snap->count == 1 && snap->entries[0].rsn == 9);
    assert(!strcmp(snap->entries[0].name, "Replaced"));
}

static void test_reject(void)
{
    const oa_channel_iface *iface = &oa_channel_runtime_iface;
    iface->clear();
    assert(iface->ingest_xml(two_rows, strlen(two_rows)) == 2);
    const struct oa_channel_snapshot *before = iface->snapshot();
    assert(before && before->count == 2);

    const char *no_close = "<active_list><service rsn='1' name='X' "
        "handle='1' onid='1' tsid='1' sid='1' type='1'/>";
    assert(iface->ingest_xml(no_close, strlen(no_close)) == -EINVAL);
    assert(iface->snapshot() == before);

    const char *missing_attr = "<active_list><service rsn='1' name='X' "
        "handle='1' onid='1' tsid='1' type='1'/></active_list>";
    assert(iface->ingest_xml(missing_attr, strlen(missing_attr)) == -EINVAL);
    assert(iface->snapshot() == before);

    const char *bad_onid = "<active_list><service rsn='1' name='X' "
        "handle='1' onid='65536' tsid='1' sid='1' type='1'/>"
        "</active_list>";
    assert(iface->ingest_xml(bad_onid, strlen(bad_onid)) == -EINVAL);
    assert(iface->snapshot() == before);

    const char *long_name = "<active_list><service rsn='1' name='"
        "123456789012345678901234567890123456789012345678901234567890"
        "123456789012345678901234567890123456789012345678901234567890"
        "' handle='1' onid='1' tsid='1' sid='1' type='1'/>"
        "</active_list>";
    assert(iface->ingest_xml(long_name, strlen(long_name)) == -EINVAL);
    assert(iface->snapshot() == before);

    const char *empty = "<active_list count='0'></active_list>";
    assert(iface->ingest_xml(empty, strlen(empty)) == 0);
    assert(!iface->snapshot());
    iface->clear();
    assert(!iface->snapshot());
}

static void test_bounds(void)
{
    const oa_channel_iface *iface = &oa_channel_runtime_iface;
    iface->clear();
    char *xml = malloc(65536);
    assert(xml);
    size_t used = 0;
    used += (size_t)snprintf(xml + used, 65536 - used, "<active_list>");
    for (unsigned i = 0; i <= OA_CHANNELS_MAX; i++) {
        used += (size_t)snprintf(xml + used, 65536 - used,
            "<service rsn='%u' name='S%u' handle='%u' onid='1' tsid='1' "
            "sid='1' type='1'/>", i + 1, i + 1, i + 1);
    }
    used += (size_t)snprintf(xml + used, 65536 - used, "</active_list>");
    assert(iface->ingest_xml(xml, used) == -ENOSPC);
    assert(!iface->snapshot());
    free(xml);
}

int main(void)
{
    test_ingest();
    test_reject();
    test_bounds();
    puts("ok channels: runtime active-list XML order, fields and rejection");
    return 0;
}
