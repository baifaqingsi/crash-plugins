/**
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef MEMINFO_DEFS_H_
#define MEMINFO_DEFS_H_

#include <map>
#include <cmath>
#include <regex>
#include "plugin.h"
#include "buddy.h"

class Meminfo : public PaserPlugin {
private:
    std::vector<ulong> node_page_state;
    std::vector<ulong> zone_page_state;
    std::map<std::string, ulong> enums;
    std::map<std::string, ulong> g_param;

    void parse_meminfo(void);
    bool is_digits(const std::string& str);
    ulong get_wmark_low(void);
    ulong get_available(ulong);
    ulong get_blockdev_nr_pages(void);
    ulong get_to_be_unused_nr_pages(void);
    ulong get_vm_commit_pages(ulong);
    ulong get_mm_committed_pages(void);
    ulong get_vmalloc_total(void);

public:
    Meminfo();
    void cmd_main(void) override;
    void print_meminfo(void);
    DEFINE_PLUGIN_INSTANCE(Meminfo)
};


#endif // MEMINFO_DEFS_H_
