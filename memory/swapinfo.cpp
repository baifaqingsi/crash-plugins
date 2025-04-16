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

#include "swapinfo.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-arith"

void Swapinfo::cmd_main(void) {

}

Swapinfo::Swapinfo(std::shared_ptr<Zraminfo> zram): zram_ptr(zram){
    init_command();
}

Swapinfo::Swapinfo(){
    zram_ptr = std::make_shared<Zraminfo>();
    init_command();
    //print_table();
}

bool Swapinfo::is_zram_enable(){
    return zram_ptr->is_zram_enable();
}

void Swapinfo::init_command(){
    field_init(swap_info_struct,pages);
    field_init(swap_info_struct,inuse_pages);
    field_init(swap_info_struct,swap_file);
    field_init(swap_info_struct,swap_vfsmnt);
    field_init(swap_info_struct,old_block_size);
    field_init(swap_info_struct,bdev);
    field_init(swap_info_struct,swap_extent_root);
    field_init(block_device,bd_disk);
    field_init(gendisk,private_data);
    field_init(swap_extent,rb_node);
    field_init(block_device,bd_start_sect);
    field_init(block_device,bd_part);
    field_init(hd_struct,start_sect);

    struct_init(swap_info_struct);
    field_init(address_space,i_pages);
    field_init(address_space,page_tree);
    struct_init(address_space);

    field_init(vm_area_struct,vm_start);
    field_init(vm_area_struct,vm_file);
    field_init(file,f_vfsmnt);
    parser_swap_info();
}

Swapinfo::~Swapinfo(){
    for (const auto& pair : memory_page_cahe) {
        if (pair.second != nullptr) {
            free(pair.second);
        }
    }
    for (const auto& pair : zram_page_cahe) {
        if (pair.second != nullptr) {
            free(pair.second);
        }
    }
}

ulonglong Swapinfo::pte_handle_index(std::shared_ptr<swap_info> swap_ptr, ulonglong pte_val){
    ulonglong swp_offset = 0;
    if (THIS_KERNEL_VERSION >= LINUX(2, 6, 0)){
        swp_offset = (ulonglong)__swp_offset(pte_val);
    }else{
        swp_offset = (ulonglong)SWP_OFFSET(pte_val);
    }
    ulong swap_extent_root = read_pointer(swap_ptr->addr + field_offset(swap_info_struct,swap_extent_root),"rb_root");
    int offset = field_offset(swap_extent,rb_node);
    std::vector<ulong> swap_extent_list = for_each_rbtree(swap_extent_root,offset);
    struct swap_extent extent;
    for (const auto& addr : swap_extent_list) {
        BZERO(&extent, sizeof(struct swap_extent));
        if(!read_struct(addr,&extent,sizeof(extent),"swap_extent")){
            continue;
        }
        if (extent.start_page < swp_offset < (extent.start_page + extent.nr_pages)){
            if(debug)fprintf(fp, "swap_extent:%lx, start_block:%lld, start_page:%ld, nr_pages:%ld\n", addr, extent.start_block,extent.start_page,extent.nr_pages);
            break;
        }
    }
    ulonglong start_sect = 0;
    if (THIS_KERNEL_VERSION >= LINUX(5,10,0)) {
        start_sect = read_ulonglong(swap_ptr->bdev + field_offset(block_device,bd_start_sect),"block_device bd_start_sect");
    }else {
        ulong bd_part = read_ulong(swap_ptr->bdev + field_offset(block_device,bd_part),"block_device bd_part");
        start_sect = read_ulong(bd_part + field_offset(hd_struct,start_sect),"hd_struct start_sect");
    }
    // if(debug)fprintf(fp, "start_sect:%lld\n", start_sect);
    ulonglong index = start_sect + (extent.start_block + (swp_offset - extent.start_page));
    return index;
}

ulong Swapinfo::lookup_swap_cache(ulonglong pte_val){
    struct list_pair lp;
    bool is_xarray = false;
    ulong swp_type = SWP_TYPE(pte_val);
    ulonglong swp_offset = (ulonglong)__swp_offset(pte_val);
    if (!csymbol_exists("swapper_spaces")){
        fprintf(fp, "swapper_spaces doesn't exist in this kernel!\n");
        return 0;
    }
    ulong swp_space = csymbol_value("swapper_spaces");
    swp_space += swp_type * sizeof(void *);
    swp_space = read_pointer(swp_space,"address_space addr");
    // fprintf(fp, "swp_space:%lx\n", swp_space);
    swp_space += (swp_offset >> SWAP_ADDRESS_SPACE_SHIFT) * struct_size(address_space);
    if(debug)fprintf(fp, "swap address_space :%lx\n", swp_space);
    if (!is_kvaddr(swp_space)) {
        fprintf(fp, "address_space address is invalid !\n");
        return 0;
    }
    std::string i_pages_type = MEMBER_TYPE_NAME(TO_CONST_STRING("address_space"), TO_CONST_STRING("i_pages"));
    if (i_pages_type == "xarray"){
        is_xarray = true;
    }else{
        is_xarray = false;
    }
    int i_pages_offset = field_offset(address_space,i_pages);
    if (i_pages_offset == -1){
       i_pages_offset = field_offset(address_space,page_tree);
    }
    ulong page = 0;
    lp.index = swp_offset;
    if (is_xarray){
        if(do_xarray(swp_space + i_pages_offset, XARRAY_SEARCH, &lp)){
            if ((ulong)lp.value & 1){
                return 0;
            }
            page = (ulong)lp.value;
        }
    }else{
        if (do_radix_tree(swp_space + i_pages_offset, RADIX_TREE_SEARCH, &lp)){
            if ((ulong)lp.value & RADIX_TREE_EXCEPTIONAL_ENTRY){
                return 0;
            }
            page = (ulong)lp.value;
        }
    }
    return page;
}

std::string Swapinfo::uread_cstring(ulonglong task_addr,ulonglong uvaddr,int len, const std::string& note){
    std::string res;
    char* buf = uread_memory(task_addr,uvaddr,len, note);
    if(buf != nullptr){
        res = buf;
        std::free(buf);
    }
    return res;
}

bool Swapinfo::uread_bool(ulonglong task_addr,ulonglong uvaddr,const std::string& note){
    char* buf = uread_memory(task_addr,uvaddr,sizeof(bool), note);
    if(buf == nullptr){
        return false;
    }
    bool res = BOOL(buf);
    std::free(buf);
    return res;
}

int Swapinfo::uread_int(ulonglong task_addr,ulonglong uvaddr,const std::string& note){
    char* buf = uread_memory(task_addr,uvaddr,sizeof(int), note);
    if(buf == nullptr){
        return 0;
    }
    int res = INT(buf);
    std::free(buf);
    return res;
}

uint Swapinfo::uread_uint(ulonglong task_addr,ulonglong uvaddr,const std::string& note){
    char* buf = uread_memory(task_addr,uvaddr,sizeof(uint), note);
    if(buf == nullptr){
        return 0;
    }
    uint res = UINT(buf);
    std::free(buf);
    return res;
}

long Swapinfo::uread_long(ulonglong task_addr,ulonglong uvaddr,const std::string& note){
    char* buf = uread_memory(task_addr,uvaddr,sizeof(long), note);
    if(buf == nullptr){
        return 0;
    }
    long res = LONG(buf);
    std::free(buf);
    return res;
}

ulong Swapinfo::uread_ulong(ulonglong task_addr,ulonglong uvaddr,const std::string& note){
    char* buf = uread_memory(task_addr,uvaddr,sizeof(ulong), note);
    if(buf == nullptr){
        return 0;
    }
    ulong res = ULONG(buf);
    std::free(buf);
    return res;
}

ulonglong Swapinfo::uread_ulonglong(ulonglong task_addr,ulonglong uvaddr,const std::string& note){
    char* buf = uread_memory(task_addr,uvaddr,sizeof(ulonglong), note);
    if(buf == nullptr){
        return 0;
    }
    ulonglong res = ULONGLONG(buf);
    std::free(buf);
    return res;
}

ushort Swapinfo::uread_ushort(ulonglong task_addr,ulonglong uvaddr,const std::string& note){
    char* buf = uread_memory(task_addr,uvaddr,sizeof(ushort), note);
    if(buf == nullptr){
        return 0;
    }
    ushort res = USHORT(buf);
    std::free(buf);
    return res;
}

short Swapinfo::uread_short(ulonglong task_addr,ulonglong uvaddr,const std::string& note){
    char* buf = uread_memory(task_addr,uvaddr,sizeof(short), note);
    if(buf == nullptr){
        return 0;
    }
    short res = SHORT(buf);
    std::free(buf);
    return res;
}

ulong Swapinfo::uread_pointer(ulonglong task_addr,ulonglong uvaddr,const std::string& note){
    char* buf = uread_memory(task_addr,uvaddr,sizeof(void *), note);
    if(buf == nullptr){
        return 0;
    }
    ulong res = (ulong)VOID_PTR(buf);
    std::free(buf);
    return res;
}

unsigned char Swapinfo::uread_byte(ulonglong task_addr,ulonglong uvaddr,const std::string& note){
    char* buf = uread_memory(task_addr,uvaddr,1, note);
    if(buf == nullptr){
        return 0;
    }
    unsigned char res = UCHAR(buf);
    std::free(buf);
    return res;
}

std::string Swapinfo::read_start_args(ulong& task_addr){
    if (!is_kvaddr(task_addr))return "";
    ulong mm_addr = read_pointer(task_addr + field_offset(task_struct, mm), "task_struct_mm");
    void *buf = read_struct(mm_addr,"mm_struct");
    ulong arg_start = ULONG(buf + field_offset(mm_struct, arg_start));
    ulong arg_end = ULONG(buf + field_offset(mm_struct, arg_end));
    FREEBUF(buf);
    ulong len = arg_end - arg_start;
    struct task_context *tc = task_to_context(task_addr);
    char* args = uread_memory(task_addr,arg_start,len, "read args");
    std::string result;
    if (args != nullptr){
        result = args;
    }
    if (result.empty()){
        result = tc->comm;
    }
    return result;
}

// read data across many pages
// -----------------------------------------------------------------
// |                              |                                |
// -----------------------------------------------------------------
//                  ^                              ^
//                  |                              |
char* Swapinfo::uread_memory(ulonglong task_addr,ulonglong uvaddr,int len, const std::string& note){
    int remain = len;
    char* result = (char*)std::malloc(len);
    BZERO(result, len);
    while(remain > 0){
        // read one page
        char* buf_page = do_swap_page(task_addr,uvaddr);
        if(buf_page == nullptr){
            std::free(result);
            return nullptr;
        }
        int offset_in_page = (uvaddr & ~page_mask);
        // std::cout << std::hex << "uvaddr:" << uvaddr << " offset_in_page:" << std::hex << offset_in_page << std::endl;
        int read_len = min(remain, page_size - offset_in_page);
        memcpy(result + (len - remain), buf_page + offset_in_page, read_len);
        remain -= read_len;
        uvaddr += read_len;
    }
    // std::cout << hexdump(uvaddr,result,len) << std::endl;
    return result;
}

bool Swapinfo::is_swap_pte(ulong pte){
    int present = 0;
#if defined(ARM64)
        present = pte & (PTE_VALID | machdep->machspec->PTE_PROT_NONE);
#endif

#if defined(ARM)
        #define L_PTE_PRESENT           (1 << 0)
        present = pte & L_PTE_PRESENT;
#endif
    return pte && !present;
}

char* Swapinfo::do_swap_page(ulonglong task_addr,ulonglong uvaddr){
    physaddr_t paddr = 0;
    // struct task_context *tc = CURRENT_CONTEXT();
    // if(tc->task != task_addr){
    //     tc = task_to_context(task_addr);
    // }
    struct task_context *tc = task_to_context(task_addr);
    if (!tc){
        fprintf(fp, "Not found task %llx in dump\n",task_addr);
        return nullptr;
    }
    ulonglong page_start = uvaddr & page_mask;
    // if(debug)fprintf(fp, "comm:%s  pid:%lu\n", tc->comm,tc->pid);
    if (!IS_UVADDR(page_start, tc)){
        return nullptr;
    }
    char* page_data;
    int page_exist = uvtop(tc, page_start, &paddr, 0);
    if (page_exist){
        if (memory_page_cahe.find(paddr) != memory_page_cahe.end()) {
            return memory_page_cahe[paddr];
        }
        if(debug)fprintf(fp, "read %llx from page_vaddr:%llx, page_paddr:%llx\n\n",uvaddr,page_start,(ulonglong)paddr);
        void* buf = (char*)read_memory(paddr, page_size, "do_swap_page",false);
        if (buf == nullptr){
            if(debug)fprintf(fp, "read %llx from memory failed \n",uvaddr);
            return nullptr;
        }
        page_data = (char*)std::malloc(page_size);
        BZERO(page_data, page_size);
        memcpy(page_data, buf, page_size);
        memory_page_cahe[paddr] = page_data;
        FREEBUF(buf);
        return page_data;
    }else{
        ulong pte = paddr;
        #if defined(ARM)
            pte = get_arm_pte(tc->task,page_start);
        #endif
        if(debug)fprintf(fp, "Pid:%ld vaddr:%llx PTE:%lx\n",tc->pid, page_start, pte);
        if(is_swap_pte(pte)){
            if (zram_page_cahe.find(pte) != zram_page_cahe.end()) {
                return zram_page_cahe[pte];
            }
            if(debug){
                ulong swp_type = SWP_TYPE(pte);
                ulonglong swp_offset = (ulonglong)__swp_offset(pte);
                fprintf(fp, "PTE:%#lx, type:%ld, offset:%lld\n",pte, swp_type,swp_offset);
            }
            ulong swap_page = lookup_swap_cache(pte);
            if (is_kvaddr(swap_page)){
                ulong page_paddr = page_to_phy(swap_page);
                if(debug)fprintf(fp, "read %llx from swapcache page_vaddr:%lx, page_paddr:%lx\n\n",uvaddr,swap_page,page_paddr);
                void* buf = read_memory(page_paddr, page_size, "do_swap_page",false);
                if (buf == nullptr){
                    if(debug)fprintf(fp, "read swap page %llx from memory failed \n",uvaddr);
                    return nullptr;
                }
                page_data = (char*)std::malloc(page_size);
                BZERO(page_data, page_size);
                memcpy(page_data, buf, page_size);
                zram_page_cahe[pte] = page_data;
                FREEBUF(buf);
                return page_data;
            }
            //in zram
            std::shared_ptr<swap_info> swap_ptr = get_swap_info(pte);
            if(swap_ptr == nullptr){
                fprintf(fp, "can't found swap_info !\n");
                return nullptr;
            }
            ulong zram_addr = get_zram_addr(swap_ptr,pte);
            if (!is_kvaddr(zram_addr)){
                fprintf(fp, "invaild zram addr: %#lx !\n",zram_addr);
                return nullptr;
            }
            ulonglong index = pte_handle_index(swap_ptr,pte);
            if(debug)fprintf(fp, "read %llx from zram:%lx, index:%lld \n",page_start,zram_addr, index);
            return zram_ptr->read_zram_page(zram_addr,index);
        }
        fprintf(fp, "invaild PTE:%#lx\n",pte);
        return nullptr;
    }
}

std::shared_ptr<swap_info> Swapinfo::get_swap_info(ulonglong pte_val){
    if (!csymbol_exists("swap_info")){
        fprintf(fp, "swap_info doesn't exist in this kernel!\n");
        return nullptr;
    }
    swap_info_init();
    ulong swap_addr = csymbol_value("swap_info");
    ulong swp_type = SWP_TYPE(pte_val);
    ulonglong swp_offset = (ulonglong)__swp_offset(pte_val);
    if (vt->flags & SWAPINFO_V2) {
        swap_addr += (swp_type * sizeof(void *));
        swap_addr = read_pointer(swap_addr,"swap_info_struct addr");
    } else {
        swap_addr += (struct_size(swap_info_struct) * swp_type);
    }
    if (!is_kvaddr(swap_addr)) {
        fprintf(fp, "swap_info address is invalid !\n");
        return nullptr;
    }
    // fprintf(fp, "swap_info_struct: %#lx\n",swap_addr);
    for (const auto& swap_ptr : swap_list) {
        if (swap_ptr->addr == swap_addr){
            return swap_ptr;
        }
    }
    return nullptr;
}

ulong Swapinfo::get_zram_addr(std::shared_ptr<swap_info> swap_ptr, ulonglong pte_val){
    if (swap_ptr->swap_file.rfind("zram") == std::string::npos) {
        return 0;
    }
    if (!is_kvaddr(swap_ptr->bdev))return 0;
    ulong bd_disk = read_pointer(swap_ptr->bdev + field_offset(block_device,bd_disk),"block_device bd_disk");
    if (!is_kvaddr(bd_disk))return 0;
    ulong data = read_pointer(bd_disk + field_offset(gendisk,private_data),"gendisk private_data");
    return data;
}

void Swapinfo::parser_swap_info(){
    if (!csymbol_exists("nr_swapfiles")){
        fprintf(fp, "nr_swapfiles doesn't exist in this kernel!\n");
        return;
    }
    if (!csymbol_exists("swap_info")){
        fprintf(fp, "swap_info doesn't exist in this kernel!\n");
        return;
    }
    char buf[BUFSIZE];
    swap_info_init();
    ulong swap_info_addr = csymbol_value("swap_info");
    nr_swap = read_int(csymbol_value("nr_swapfiles"),"nr_swapfiles");
    ulong swp_space = csymbol_value("swapper_spaces");
    for (size_t i = 0; i < nr_swap; i++){
        ulong addr = read_pointer(swap_info_addr + i * sizeof(void *),"swap_info_struct addr");
        if (!is_kvaddr(addr))continue;
        void *swap_info_buf = read_struct(addr,"swap_info_struct");
        if(swap_info_buf == nullptr) continue;
        std::shared_ptr<swap_info> swap_ptr = std::make_shared<swap_info>();
        swap_ptr->addr = addr;
        swap_ptr->swap_space = read_pointer(swp_space + i * sizeof(void *),"swapper_space addr");
        swap_ptr->pages = UINT(swap_info_buf + field_offset(swap_info_struct,pages));
        swap_ptr->inuse_pages = UINT(swap_info_buf + field_offset(swap_info_struct,inuse_pages));
        ulong swap_file = ULONG(swap_info_buf + field_offset(swap_info_struct,swap_file));
        if (is_kvaddr(swap_file)){
            if (field_offset(swap_info_struct,swap_vfsmnt) != -1){
                ulong vfsmnt = ULONG(swap_info_buf + field_offset(swap_info_struct,swap_vfsmnt));
                get_pathname(swap_file, buf, BUFSIZE,1, vfsmnt);
            }else if (field_offset(swap_info_struct,old_block_size) != -1){
                get_pathname(file_to_dentry(swap_file),buf, BUFSIZE, 1, file_to_vfsmnt(swap_file));
            }else{
                get_pathname(swap_file, buf, BUFSIZE, 1, 0);
            }
        }
        swap_ptr->swap_file = buf;
        swap_ptr->bdev = ULONG(swap_info_buf + field_offset(swap_info_struct,bdev));
        FREEBUF(swap_info_buf);
        if(debug)fprintf(fp, "swap_file:%s\n", swap_ptr->swap_file.c_str());
        swap_list.push_back(swap_ptr);
    }
}

uint64_t Swapinfo::read_sections(std::string& section_name,std::string& libname, int *align) {
    int fd = open(libname.c_str(), O_RDONLY);
    if (fd == -1) {
        fprintf(fp, "Failed to open %s\n", libname.c_str());
        return 0;
    }
    if (elf_version(EV_CURRENT) == EV_NONE) {
        fprintf(fp, "ELF library initialization failed: %s\n", elf_errmsg(-1));
        close(fd);
        return 0;
    }
    Elf* e = elf_begin(fd, ELF_C_READ, nullptr);
    if (!e) {
        fprintf(fp, "elf_begin() failed: %s\n", elf_errmsg(-1));
        close(fd);
        return 0;
    }
    if (elf_kind(e) != ELF_K_ELF) {
        fprintf(fp, "%s is not an ELF file.\n", libname.c_str());
        elf_end(e);
        close(fd);
        return 0;
    }
    size_t shstrndx;
    if (elf_getshdrstrndx(e, &shstrndx) != 0) {
        fprintf(fp, "elf_getshdrstrndx() failed: %s\n", elf_errmsg(-1));
        elf_end(e);
        close(fd);
        return 0;
    }
    Elf_Scn* scn = nullptr;
    while ((scn = elf_nextscn(e, scn)) != nullptr) {
        GElf_Shdr shdr;
        if (gelf_getshdr(scn, &shdr) != &shdr) {
            fprintf(fp, "gelf_getshdr() failed: %s\n", elf_errmsg(-1));
            elf_end(e);
            close(fd);
            return 0;
        }
        std::string name = elf_strptr(e, shstrndx, shdr.sh_name);
        if (name == section_name) {
            elf_end(e);
            close(fd);
            *align = shdr.sh_addralign;
            return shdr.sh_addr;
        }
    }
    elf_end(e);
    close(fd);
    return 0;
}

uint64_t Swapinfo::read_symbol(std::string& symbol_name,std::string& libname) {
    int fd = open(libname.c_str(), O_RDONLY);
    if (fd == -1) {
        fprintf(fp, "Failed to open %s\n", libname.c_str());
        return 0;
    }
    if (elf_version(EV_CURRENT) == EV_NONE) {
        fprintf(fp, "ELF library initialization failed: %s\n", elf_errmsg(-1));
        close(fd);
        return 0;
    }
    Elf* e = elf_begin(fd, ELF_C_READ, nullptr);
    if (!e) {
        fprintf(fp, "elf_begin() failed: %s\n", elf_errmsg(-1));
        close(fd);
        return 0;
    }
    if (elf_kind(e) != ELF_K_ELF) {
        fprintf(fp, "%s is not an ELF file.\n", libname.c_str());
        elf_end(e);
        close(fd);
        return 0;
    }
    size_t shstrndx;
    if (elf_getshdrstrndx(e, &shstrndx) != 0) {
        fprintf(fp, "elf_getshdrstrndx() failed: %s\n", elf_errmsg(-1));
        elf_end(e);
        close(fd);
        return 0;
    }
    Elf_Scn* scn = nullptr;
    while ((scn = elf_nextscn(e, scn)) != nullptr) {
        GElf_Shdr shdr;
        if (gelf_getshdr(scn, &shdr) != &shdr) {
            fprintf(fp, "gelf_getshdr() failed: %s\n", elf_errmsg(-1));
            elf_end(e);
            close(fd);
            return 0;
        }
        if (shdr.sh_type == SHT_SYMTAB || shdr.sh_type == SHT_DYNSYM) {
            Elf_Data* data = elf_getdata(scn, nullptr);
            int count = shdr.sh_size / shdr.sh_entsize;
            for (int i = 0; i < count; ++i) {
                GElf_Sym sym;
                gelf_getsym(data, i, &sym);
                std::string name = elf_strptr(e, shdr.sh_link, sym.st_name);
                if (name.find(symbol_name) != std::string::npos && name.find("_Z") != std::string::npos){
                    elf_end(e);
                    close(fd);
                    return sym.st_value;
                }
            }
        }
    }
    elf_end(e);
    close(fd);
    return 0;
}

//  f1e79bb0  b61db000  b6206000     71  /system/lib/bootstrap/libc.so
//  f1e79990  b6206000  b628e000     75  /system/lib/bootstrap/libc.so
//  f1e79b28  b628e000  b6293000 100071  /system/lib/bootstrap/libc.so
//  f1e79cc0  b6293000  b6296000 100073  /system/lib/bootstrap/libc.so
//  get the first vma start address,such as b61db000
ulong Swapinfo::get_file_min_vaddr(ulong task_addr, std::string libname){
    char buf[BUFSIZE];
    std::vector<ulong> res_list;
    for (auto &vma_addr : for_each_vma(task_addr)){
        void *vma_buf = read_struct(vma_addr, "vm_area_struct");
        if(vma_buf == nullptr){
            fprintf(fp, "Failed to read vm_area_struct at address %lx\n", vma_addr);
            continue;
        }
        ulong vm_start = ULONG(vma_buf + field_offset(vm_area_struct, vm_start));
        ulong vm_file = ULONG(vma_buf + field_offset(vm_area_struct, vm_file));
        FREEBUF(vma_buf);
        if (!is_kvaddr(vm_file)){
            continue;
        }
        if (field_offset(file, f_vfsmnt) != -1) {
            get_pathname(file_to_dentry(vm_file), buf, BUFSIZE, 1, file_to_vfsmnt(vm_file));
        } else {
            get_pathname(file_to_dentry(vm_file), buf, BUFSIZE, 1, 0);
        }
        std::string file_name = buf;
        if (file_name.find(libname) != std::string::npos){
            res_list.push_back(vm_start);
        }
    }
    std::sort(res_list.begin(), res_list.end());
    return res_list[0];
}

ulong Swapinfo::get_var_addr_by_bss(std::string var_name, ulong task_addr, std::string lib){
    struct task_context *tc = task_to_context(task_addr);
    if (!tc){
        return 0;
    }
    int verbose = 0;
    std::string libname = lib;
    size_t pos = lib.find_last_of("/\\");
    if (pos != std::string::npos) {
        libname = lib.substr(pos + 1);
    }
    // get the min vaddr of the lib
    ulong vraddr = get_file_min_vaddr(task_addr, libname);
    if (verbose){
        fprintf(fp, "Min vaddr:0x%lx \n",vraddr);
    }
    // get the static addr of bss.
    int bss_align = 0;
    std::string section_name = ".bss";
    size_t bss_saddr = read_sections(section_name,lib,&bss_align);
    if (verbose){
        fprintf(fp, "bss_saddr:%#zx \n",bss_saddr);
        fprintf(fp, "bss_align:%d \n",bss_align);
    }
    // calc the runtime addr of bss for lib
    size_t bss_vaddr = vraddr + bss_saddr;
    bss_vaddr = roundup(bss_vaddr,bss_align);
    if (verbose){
        fprintf(fp, "bss_vaddr:%#zx \n",bss_vaddr);
    }
    // get the static addr of var_name
    size_t var_saddr = read_symbol(var_name,lib);
    if (var_saddr == 0){
        return 0;
    }
    if (verbose){
        fprintf(fp, "var_saddr:%#zx \n",var_saddr);
    }
    // calc the runtime addr of var_name
    size_t var_vaddr = bss_vaddr + (var_saddr - bss_saddr);
    if (verbose){
        fprintf(fp, "var_vaddr:%#zx \n",var_vaddr);
    }
    if (!is_uvaddr(var_vaddr,tc)){
        return 0;
    }
    return var_vaddr;
}

#pragma GCC diagnostic pop
