#include "igt.h"
#include "igt_kmod.h"

/**
 * TEST: Xe driver live kunit tests
 * Description: Xe driver live dmabuf unit tests
 * Category: Software building block
 * Sub-category: kunit
 * Functionality: kunit
 * Test category: functionality test
 *
 * SUBTEST: xe_bo
 * Description:
 *	Kernel dynamic selftests to check if GPU buffer objects are
 *	being handled properly.
 * Functionality: bo
 *
 * SUBTEST: xe_dma_buf
 * Description: Kernel dynamic selftests for dmabuf functionality.
 * Functionality: dmabuf
 *
 * SUBTEST: xe_migrate
 * Description:
 *	Kernel dynamic selftests to check if page table migrations
 *	are working properly.
 * Functionality: migrate
 *
 * SUBTEST: xe_mocs
 * Description:
 *	Kernel dynamic selftests to check mocs configuration.
 * Functionality: mocs
 */

struct kunit_tests {
	const char *kunit;
	const char *name;
};

static const struct kunit_tests live_tests[] = {
	{ "xe_bo_test",		"xe_bo" },
	{ "xe_dma_buf_test",	"xe_dma_buf" },
	{ "xe_migrate_test",	"xe_migrate" },
	{ "xe_mocs_test",	"xe_mocs" },
};

igt_main
{
	int i;

	for (i = 0; i < ARRAY_SIZE(live_tests); i++)
		igt_kunit(live_tests[i].kunit, live_tests[i].name, NULL);
}
