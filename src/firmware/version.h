/*
 * bbs-fw
 *
 * Copyright (C) Daniel Nilsson, 2022.
 *
 * Released under the GPL License, Version 3
 */

#ifndef _VERSION_H_
#define _VERSION_H_

#ifndef VERSION_MAJOR
#define VERSION_MAJOR 2
#endif
#ifndef VERSION_MINOR
#define VERSION_MINOR 0
#endif
#ifndef VERSION_PATCH
#define VERSION_PATCH 0
#endif

#if VERSION_MAJOR < 0 || VERSION_MAJOR > 255 || VERSION_MINOR < 0 || VERSION_MINOR > 255 || VERSION_PATCH < 0 || VERSION_PATCH > 255
#error Firmware version components must fit in one byte
#endif


#if defined(BBSHD)
	#define CTRL_TYPE		1
#elif defined(BBS02)
	#define CTRL_TYPE		2
#elif defined(TSDZ2)
	#define CTRL_TYPE		3
#else
	#define CTRL_TYPE		0
#endif

#endif
