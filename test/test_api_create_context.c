#include "test.h"

TEST(test_api_create_context)
{
	xmp_context ctx;

	ctx = xmp_create_context();
	fail_unless(ctx != 0, "returned NULL");

	xmp_free_context(ctx);
}
END_TEST
