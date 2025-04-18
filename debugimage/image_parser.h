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

#ifndef IMAGE_PARSER_DEFS_H_
#define IMAGE_PARSER_DEFS_H_

#include "plugin.h"

struct Dump_entry;

class ImageParser : public PaserPlugin {
public:
    ImageParser();
    uint64_t createMask(int a, int b);
    uint64_t pac_ignore(uint64_t data);
    std::string get_cmm_path(int core, bool secure);
    void cmd_main(void) override;
    virtual void generate_cmm(std::shared_ptr<Dump_entry> entry_ptr)=0;
    virtual void print_stack(std::shared_ptr<Dump_entry> entry_ptr)=0;
};

#endif // IMAGE_PARSER_DEFS_H_
