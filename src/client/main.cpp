// SPDX-License-Identifier: Apache-2.0

/*
 * Machine-level entry point of client program.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

extern int biz_main(int argc, char **argv);

int main(int argc, char **argv)
{
    return biz_main(argc, argv);
}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-04-13, Man Hung-Coeng <udc577@126.com>:
 *  01. Ported from another private personal project.
 *
 * >>> 2026-04-18, Man Hung-Coeng <udc577@126.com>:
 *  01. Call biz_main() directly for the sake of customization.
 */

