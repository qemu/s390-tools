/*
 * s390-tools/include/zt_common.h
 *   common s390-tools definitions.
 *
 * Copyright IBM Corp. 2004, 2006.
 *
 * Author(s): Gerhard Tonn (ton@de.ibm.com)
 */

#ifndef ZT_COMMON_H
#define ZT_COMMON_H

#define STRINGIFY_1(x)			#x
#define STRINGIFY(x)			STRINGIFY_1(x)

#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#else
# define UNUSED(x) x
#endif

#define RELEASE_STRING	STRINGIFY (S390_TOOLS_RELEASE)

#endif /* ZT_COMMON_H */
