// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdint.h>
#include <stddef.h>
#include "player_life_code.h"

struct MemoryInfo { uint64_t address, size; uint32_t type, attributes, permission, ipc_refs, device_refs, reserved; };
extern uint32_t mhr_query_memory(struct MemoryInfo *, uint32_t *, uintptr_t);
extern uint32_t mhr_get_info(uint64_t *, uint32_t, uint32_t, uint64_t);
extern uint32_t mhr_map_process(uintptr_t, uint32_t, uintptr_t, uint64_t);
extern uint32_t mhr_unmap_process(uintptr_t, uint32_t, uintptr_t, uint64_t);
extern uint32_t mhr_close_handle(uint32_t);
extern void mhr_cache_sync(uintptr_t, uintptr_t, size_t);
extern long long mhr_get_tick(void);
extern void mhr_log_line(const char *);
static uintptr_t game_base;
static uint64_t faint_tick, revive_tick;

static int read_safe(uintptr_t address, void *out, size_t size) {
    struct MemoryInfo memory = {0};
    uint32_t page;
    if (address < 0x10000 || address + size < address ||
        mhr_query_memory(&memory, &page, address) || !(memory.permission & 1) ||
        address < memory.address || address + size > memory.address + memory.size) return 0;
    for (size_t i=0;i<size;++i) ((unsigned char *)out)[i]=((volatile unsigned char *)address)[i];
    return 1;
}

// Called only from the game's confirmed faint/reset events, never polled.
static void life_event(uintptr_t player, unsigned dying) {
    uint64_t last_faint = __atomic_load_n(&faint_tick, __ATOMIC_ACQUIRE);
    uint64_t last_revive = __atomic_load_n(&revive_tick, __ATOMIC_ACQUIRE);
    if ((dying && last_faint > last_revive) || (!dying && last_faint <= last_revive)) return;
    uintptr_t holder=0, manager=0, master=0, players=0, local=0;
    unsigned char index=255;
    uint32_t count=0;
    if (!read_safe(game_base+0x12684a10,&holder,8) || !read_safe(holder,&manager,8) ||
        !read_safe(manager+0xa8,&master,8) || !read_safe(master+0x10,&index,1) || index>3 ||
        !read_safe(manager+0x58,&players,8) || !read_safe(players+0x1c,&count,4) ||
        count<=index || count>16 || !read_safe(players+0x20+index*8,&local,8) || local!=player) return;
    uint64_t now=(uint64_t)mhr_get_tick();
    if (dying) {
        __atomic_store_n(&faint_tick,now,__ATOMIC_RELEASE);
        mhr_log_line("[MHR-HUD] local hunter faint confirmed; hide deadline in 5 seconds");
    } else {
        __atomic_store_n(&revive_tick,now,__ATOMIC_RELEASE);
        mhr_log_line("[MHR-HUD] local hunter returned; waiting for a fresh combat update");
    }
}

void mhr_read_player_life(uint64_t *faint, uint64_t *revive) {
    *faint=__atomic_load_n(&faint_tick,__ATOMIC_ACQUIRE);
    *revive=__atomic_load_n(&revive_tick,__ATOMIC_ACQUIRE);
}

static int patch(uintptr_t hook, uintptr_t cave, const unsigned char *code, size_t size,
                 unsigned pointer_offset, uint32_t branch) {
    uint64_t process=0, aslr=0, aslr_size=0;
    if (mhr_get_info(&process,65001,0,0)) return 0;
    if (mhr_get_info(&aslr,12,0xffff8001,0) || mhr_get_info(&aslr_size,13,0xffff8001,0)) {
        mhr_close_handle((uint32_t)process); return 0;
    }
    uintptr_t alias=0, cursor=aslr;
    for (unsigned i=0;i<4096 && cursor<aslr+aslr_size;++i) {
        struct MemoryInfo memory={0}; uint32_t page;
        if (mhr_query_memory(&memory,&page,cursor)) break;
        uintptr_t candidate=(cursor+0xfff)&~(uintptr_t)0xfff;
        if ((memory.type&0xff)==0 && candidate+0x2000<=memory.address+memory.size &&
            candidate+0x2000<=aslr+aslr_size &&
            !mhr_map_process(candidate,(uint32_t)process,hook&~(uintptr_t)0xfff,0x1000)) {
            if (!mhr_map_process(candidate+0x1000,(uint32_t)process,cave&~(uintptr_t)0xfff,0x1000)) { alias=candidate;break; }
            mhr_unmap_process(candidate,(uint32_t)process,hook&~(uintptr_t)0xfff,0x1000);
        }
        uintptr_t next=memory.address+memory.size; if(next<=cursor)break; cursor=next;
    }
    if(!alias) { mhr_close_handle((uint32_t)process);return 0; }
    unsigned char *dest=(void *)(alias+0x1000+(cave&0xfff));
    for(size_t i=0;i<size;++i)dest[i]=code[i];
    *(uint64_t *)(dest+pointer_offset)=(uintptr_t)life_event;
    mhr_cache_sync(cave,(uintptr_t)dest,size);
    *(uint32_t *)(alias+(hook&0xfff))=branch;
    mhr_cache_sync(hook,alias+(hook&0xfff),4);
    mhr_unmap_process(alias+0x1000,(uint32_t)process,cave&~(uintptr_t)0xfff,0x1000);
    mhr_unmap_process(alias,(uint32_t)process,hook&~(uintptr_t)0xfff,0x1000);
    mhr_close_handle((uint32_t)process);
    return *(const uint32_t *)hook==branch &&
        *(const uint64_t *)(cave+pointer_offset)==(uintptr_t)life_event;
}

int mhr_install_player_life(uintptr_t base) {
    // Validate both event sites and the local-player getter before writing anything.
    if (*(const uint32_t *)(base+MHR_FAINT_HOOK)!=MHR_FAINT_ORIGINAL ||
        *(const uint32_t *)(base+MHR_FAINT_HOOK+4)!=0xa9427bfd ||
        *(const uint32_t *)(base+MHR_REVIVE_HOOK)!=MHR_REVIVE_ORIGINAL ||
        *(const uint32_t *)(base+0x39b900)!=0xf9405428 ||
        *(const uint32_t *)(base+0x39b904)!=0x39404100) return 0;
    for(size_t i=0;i<sizeof(mhr_faint_code);++i) if(((const unsigned char *)(base+MHR_FAINT_CAVE))[i])return 0;
    for(size_t i=0;i<sizeof(mhr_revive_code);++i) if(((const unsigned char *)(base+MHR_REVIVE_CAVE))[i])return 0;
    game_base=base;
    if(!patch(base+MHR_REVIVE_HOOK,base+MHR_REVIVE_CAVE,mhr_revive_code,sizeof(mhr_revive_code),MHR_REVIVE_HANDLER_OFFSET,MHR_REVIVE_BRANCH))return 0;
    if(!patch(base+MHR_FAINT_HOOK,base+MHR_FAINT_CAVE,mhr_faint_code,sizeof(mhr_faint_code),MHR_FAINT_HANDLER_OFFSET,MHR_FAINT_BRANCH))return 0;
    mhr_log_line("[MHR-HUD] local hunter faint/return event hooks installed");
    return 1;
}
