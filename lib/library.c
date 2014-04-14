#include "version.h"

/**
 * Get version of the library in 'major.minor.patch' format.
 *
 * @see @link http://semver.org/ Semantic Versioning @endlink
 *
 * @return Null terminated C "string" to version string.
 */
const char *bmVersion(void)
{
    return BM_VERSION;
}

/* vim: set ts=8 sw=4 tw=0 :*/
