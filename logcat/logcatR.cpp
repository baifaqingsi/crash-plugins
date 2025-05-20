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

#include "logcatR.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-arith"

LogcatR::LogcatR(std::shared_ptr<Swapinfo> swap) : Logcat(swap){

}

ulong LogcatR::parser_logbuf_addr(){
    size_t logbuf_addr;
    struct_init(LogBufferElement);
    if (struct_size(LogBufferElement) != -1 && !logd_symbol.empty()){
        fprintf(fp, "Looking for static logbuf \n");
        logbuf_addr = get_logbuf_addr_from_bss();
        if (logbuf_addr != 0){
            return logbuf_addr;
        }
    }

    get_rw_vma_list();
    fprintf(fp, "vma count:%zu, vaddr_mask:%#lx \n", rw_vma_list.size(), vaddr_mask);
    fprintf(fp, "looking for std::list \n");
    auto start = std::chrono::high_resolution_clock::now();
    logbuf_addr = get_stdlist_addr_from_vma();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    fprintf(fp, "time: %.6f s\n",elapsed.count());
    if (logbuf_addr != 0){
        freeResource();
        return logbuf_addr;
    }
    freeResource();
    return 0;
}

size_t LogcatR::get_logbuf_addr_from_bss(){
    ulong logbuf_addr = swap_ptr->get_var_addr_by_bss("logBuf", tc_logd->task, logd_symbol);
    if (logbuf_addr == 0){
        return 0;
    }
    // static LogBuffer* logBuf = nullptr
    if (is_compat) {
        logbuf_addr = swap_ptr->uread_uint(tc_logd->task,logbuf_addr,"read logbuf addr") & vaddr_mask;
    }else{
        logbuf_addr = swap_ptr->uread_ulong(tc_logd->task,logbuf_addr,"read logbuf addr")& vaddr_mask;
    }
    fprintf(fp, "logbuf_addr:0x%lx \n",logbuf_addr);
    return logbuf_addr;
}

size_t LogcatR::get_stdlist_addr_from_vma(){
    int index = 0;
    for (const auto& vma_ptr : rw_vma_list) {
        if (!(vma_ptr->vm_flags & VM_READ) || !(vma_ptr->vm_flags & VM_WRITE)) {
            continue;
        }
        if (vma_ptr->vma_name.find("alloc") == std::string::npos){
            continue;
        }
        if (debug){
            fprintf(fp, "check vma:[%d]%#lx-%#lx %s\n", index,vma_ptr->vm_start,vma_ptr->vm_end,vma_ptr->vma_name.c_str());
        }
        // check if this node is LogBufferElement
        auto callback = [&](ulong node_addr) -> bool {
            if (!is_uvaddr(node_addr,tc_logd)){
                return false;
            }
            ulong data_addr = node_addr + 2 * pointer_size;
            data_addr = swap_ptr->uread_pointer(tc_logd->task,data_addr,"LogBufferElement addr");
            if (!is_uvaddr(data_addr,tc_logd)){
                return false;
            }
            char buf_element[sizeof(LogBufferElement)];
            if(!swap_ptr->uread_buffer(tc_logd->task,data_addr,buf_element,sizeof(LogBufferElement), "LogBufferElement")){
                return false;
            }
            LogBufferElement* element = (LogBufferElement*)buf_element;
            if (element->mDropped > 1){
                return false;
            }
            if (element->mLogId > 8){
                return false;
            }
            return true;
        };
        ulong list_addr = vma_ptr->vm_start;
        // save the search start addr;
        // ulong search_addr = list_addr;
        // search result addr will output by list_addr
        if (search_stdlist_in_vma(vma_ptr,callback, list_addr)){
            if (debug) fprintf(fp, "Found list at %#lx \n",list_addr);
            return list_addr;
        }
        index++;
    }
    return 0;
}

bool LogcatR::search_stdlist_in_vma(std::shared_ptr<vma_info> vma_ptr, std::function<bool (ulong)> callback, ulong& start_addr) {
    auto check_stdlist = (BITS64() && !is_compat) ? &LogcatR::check_stdlist64 : &LogcatR::check_stdlist32;
    for (size_t addr = start_addr; addr < vma_ptr->vm_end; addr += pointer_size) {
        ulong list_addr = (this->*check_stdlist)(addr, callback);
        if (list_addr != 0) {
            // this is a probility list addr
            start_addr = list_addr;
            return true;
        }
    }
    return false;
}

void LogcatR::parser_logbuf(ulong buf_addr){
    ulong LogElements_list_addr = buf_addr;
    fprintf(fp, "LogElements_list_addr:0x%lx \n",LogElements_list_addr);
    ulong LogBufferElement_addr = 0;
    for(auto data_node: for_each_stdlist(LogElements_list_addr)){
        if (is_compat) {
            LogBufferElement_addr = swap_ptr->uread_uint(tc_logd->task,data_node,"read data_node") & vaddr_mask;
        }else{
            LogBufferElement_addr = swap_ptr->uread_ulong(tc_logd->task,data_node,"read data_node") & vaddr_mask;
        }
        parser_LogBufferElement(LogBufferElement_addr);
    }
}

void LogcatR::parser_LogBufferElement(ulong vaddr){
    // fprintf(fp, "LogBufferElement_addr:0x%lx \n",vaddr);
    if (!is_uvaddr(vaddr,tc_logd)){
        return;
    }
    char buf_element[sizeof(LogBufferElement)];
    if(!swap_ptr->uread_buffer(tc_logd->task,vaddr,buf_element,sizeof(LogBufferElement), "LogBufferElement")){
        return;
    }
    LogBufferElement* element = (LogBufferElement*)buf_element;
    if (element->mDropped == 1){
        return;
    }
    if (element->mLogId < MAIN || element->mLogId > KERNEL) {
        return;
    }
    char log_msg[element->mMsgLen];
    std::shared_ptr<LogEntry> log_ptr = std::make_shared<LogEntry>();
    log_ptr->uid = element->mUid;
    log_ptr->pid = element->mPid;
    log_ptr->tid = element->mTid;
    log_ptr->timestamp = formatTime(element->mRealTime.tv_sec,element->mRealTime.tv_nsec);
    if (log_ptr->logid == SYSTEM || log_ptr->logid == MAIN || log_ptr->logid == KERNEL
        || log_ptr->logid == RADIO || log_ptr->logid == CRASH){
        if(swap_ptr->uread_buffer(tc_logd->task,reinterpret_cast<ulong>(element->mMsg),log_msg,element->mMsgLen, "read msg log")){
            parser_system_log(log_ptr,log_msg,element->mMsgLen);
        }
    }else{
        if(swap_ptr->uread_buffer(tc_logd->task,reinterpret_cast<ulong>(element->mMsg),log_msg,element->mMsgLen, "read bin log")){
            parser_event_log(log_ptr,log_msg,element->mMsgLen);
        }
    }
    log_list.push_back(log_ptr);
    return;
}

#pragma GCC diagnostic pop
