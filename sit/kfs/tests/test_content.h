#ifndef KFS_TEST_CONTENT_H
#define KFS_TEST_CONTENT_H

#include "kfs_test_fixture.h"

int test_content_create_artifact(KFS_TestCtx* ctx);
int test_content_link_asset(KFS_TestCtx* ctx);
int test_content_topic_assign(KFS_TestCtx* ctx);
int test_content_epic_topic_link(KFS_TestCtx* ctx);
int test_content_note_assign(KFS_TestCtx* ctx);
int test_content_delete_artifact(KFS_TestCtx* ctx);
int test_content_legacy_save_text(KFS_TestCtx* ctx);

#endif /* KFS_TEST_CONTENT_H */