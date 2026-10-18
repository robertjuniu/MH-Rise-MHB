"""ARM64 tests: original life stores/ABI, local-player filtering and repeated faint deadlines."""
import os, re, struct, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm64_const import *
from toolchain import tool
OUT=ROOT/'build/player-life-tests';OUT.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,TMP=str(OUT),TEMP=str(OUT),TMPDIR=str(OUT))
header=(ROOT/'src/player_life_code.h').read_text()
def macro(name):return int(re.search(r'#define '+name+r' (0x[0-9a-f]+|\d+)',header)[1],0)
for base in (0,0x7100000000):
 for kind,register,dying in (('faint',19,1),('revive',20,0)):
  tag=kind.upper();hook=base+macro('MHR_'+tag+'_HOOK');cave=base+macro('MHR_'+tag+'_CAVE')
  payload=bytes(int(x,16) for x in re.findall(r'0x([0-9a-f]{2})',re.search('mhr_'+kind+r'_code\[\] = \{(.*?)\};',header,re.S)[1]))
  dump=Path(os.environ.get('MHR_MAIN_NSO','main.nso'))
  if dump.is_file():
   with dump.open('rb') as f:
    f.seek(256+hook-base);assert f.read(4)==struct.pack('<I',macro('MHR_'+tag+'_ORIGINAL'))
    f.seek(256+cave-base);assert f.read(len(payload))==bytes(len(payload))
  assert macro('MHR_FAINT_CAVE')>=0x07d46d00+112
  assert macro('MHR_FAINT_CAVE')+len(payload)<=macro('MHR_REVIVE_CAVE')
  assert macro('MHR_REVIVE_CAVE')+len(payload)<=0x07d47000
  cpu=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
  for page in (hook&~4095,cave&~4095,0x7200000000,0x7300000000,0x7400000000):cpu.mem_map(page,4096)
  code=bytearray(payload);struct.pack_into('<Q',code,macro('MHR_'+tag+'_HANDLER_OFFSET'),0x7400000000)
  cpu.mem_write(cave,bytes(code));cpu.mem_write(hook,struct.pack('<I',macro('MHR_'+tag+'_BRANCH')))
  for i in range(31):cpu.reg_write(UC_ARM64_REG_X0+i,0x1000+i)
  for i in range(32):cpu.reg_write(UC_ARM64_REG_Q0+i,0xaabbccddeeff00112233+i)
  cpu.reg_write(UC_ARM64_REG_X0+register,0x7200000000)
  if dying:cpu.reg_write(UC_ARM64_REG_X21,1)
  cpu.reg_write(UC_ARM64_REG_SP,0x7300000f00);cpu.reg_write(UC_ARM64_REG_NZCV,0xa0000000)
  cpu.mem_write(0x720000062a,bytes([1-dying]))
  regs=[UC_ARM64_REG_X0+i for i in range(31)]+[UC_ARM64_REG_Q0+i for i in range(32)]+[UC_ARM64_REG_SP,UC_ARM64_REG_NZCV]
  before=[cpu.reg_read(r) for r in regs];events=[]
  def callback(uc,address,size,user):
   if address!=0x7400000000:return
   assert uc.reg_read(UC_ARM64_REG_X0)==0x7200000000 and uc.reg_read(UC_ARM64_REG_X1)==dying
   assert uc.reg_read(UC_ARM64_REG_SP)%16==0
   events.append(dying);ret=uc.reg_read(UC_ARM64_REG_X30)
   for i in range(31):uc.reg_write(UC_ARM64_REG_X0+i,0xdead0000+i)
   for i in range(32):uc.reg_write(UC_ARM64_REG_Q0+i,0xffff0000+i)
   uc.reg_write(UC_ARM64_REG_NZCV,0x60000000);uc.reg_write(UC_ARM64_REG_PC,ret)
  cpu.hook_add(UC_HOOK_CODE,callback);cpu.emu_start(hook,hook+4,count=1000)
  assert events==[dying] and cpu.mem_read(0x720000062a,1)==bytes([dying])
  assert [cpu.reg_read(r) for r in regs]==before

source=OUT/'case.c'
source.write_text('#include "'+(ROOT/'src/player_life.c').as_posix()+'"\n'
 '#include "hud_life_gate.h"\n'
 'void event_case(uintptr_t base,uintptr_t player,unsigned dying){game_base=base;life_event(player,dying);}\n'
 'int gate_case(struct MhrHudLifeGate*g,uint64_t n,uint64_t f,uint64_t r,uint32_t s){return mhr_life_visible(g,n,f,r,s);}\n'
 'int frame_case(struct MhrHudLifeGate*g,uint64_t n,uint32_t s){uint64_t f,r;mhr_read_player_life(&f,&r);return mhr_life_visible(g,n,f,r,s);}\n')
stub=OUT/'stubs.S';stub.write_text('.text\n.global mhr_query_memory,mhr_get_tick,mhr_log_line\n'
 'mhr_query_memory: ret\nmhr_get_tick: ret\nmhr_log_line: ret\n')
for path in (source,stub):
 subprocess.run([str(tool("clang")),'--target=aarch64-none-elf','-O2','-ffreestanding','-fno-stack-protector','-ffunction-sections','-fdata-sections','-I'+str(ROOT/'src'),'-c',str(path),'-o',str(path.with_suffix('.o'))],check=True,env=env)
elf=OUT/'case.elf'
subprocess.run([str(tool("ld.lld")),'-Ttext=0x10000','--gc-sections','--entry=event_case','--undefined=gate_case','--undefined=frame_case',str(source.with_suffix('.o')),str(stub.with_suffix('.o')),'-o',str(elf)],check=True,env=env)
symbols={line.split()[-1]:int(line.split()[0],16) for line in subprocess.check_output([str(tool("llvm-nm")),'-an',str(elf)]).decode().splitlines() if len(line.split())==3}
data=elf.read_bytes();cpu=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
phoff=struct.unpack_from('<Q',data,32)[0];phsize,count=struct.unpack_from('<HH',data,54)
mapped=set()
for i in range(count):
 typ,flags,off,addr,_,size,memsize,_=struct.unpack_from('<IIQQQQQQ',data,phoff+i*phsize)
 if typ!=1:continue
 for page in range(addr&~4095,(addr+memsize+4095)&~4095,4096):
  if page not in mapped:cpu.mem_map(page,4096);mapped.add(page)
 cpu.mem_write(addr,data[off:off+size])
cpu.mem_map(0x40000000,0x10000);cpu.mem_map(0x7100000000+0x12684000,4096)
cpu.mem_map(0x50000000,0x2000)
base=0x7100000000;holder=0x40001000;manager=0x40002000;master=0x40003000;players=0x40004000;local=0x40005000;remote=0x40006000
def put64(a,v):cpu.mem_write(a,struct.pack('<Q',v))
put64(base+0x12684a10,holder);put64(holder,manager);put64(manager+0xa8,master);put64(manager+0x58,players)
cpu.mem_write(players+0x1c,struct.pack('<I',4))
mock={'now':100,'queries':0,'events':0}
def native(uc,address,size,user):
 if address==symbols['mhr_query_memory']:
  target=uc.reg_read(UC_ARM64_REG_X2);mock['queries']+=1
  readable=(0x40000000<=target<0x40010000 or base+0x12684000<=target<base+0x12685000)
  if readable:uc.mem_write(uc.reg_read(UC_ARM64_REG_X0),struct.pack('<QQIIIIII',target&~4095,4096,4,0,1,0,0,0))
  uc.reg_write(UC_ARM64_REG_X0,0 if readable else 1)
 elif address==symbols['mhr_get_tick']:uc.reg_write(UC_ARM64_REG_X0,mock['now'])
 elif address==symbols['mhr_log_line']:mock['events']+=1
 else:return
 uc.reg_write(UC_ARM64_REG_PC,uc.reg_read(UC_ARM64_REG_X30))
cpu.hook_add(UC_HOOK_CODE,native)
def call(name,*args):
 for i,x in enumerate(args):cpu.reg_write(UC_ARM64_REG_X0+i,x)
 cpu.reg_write(UC_ARM64_REG_SP,0x50001f00);cpu.reg_write(UC_ARM64_REG_X30,0x50000000)
 cpu.emu_start(symbols[name],0x50000000,count=10000)
 return cpu.reg_read(UC_ARM64_REG_X0)
def ticks():return tuple(struct.unpack('<Q',cpu.mem_read(symbols[s],8))[0] for s in ('faint_tick','revive_tick'))
for index in (0,2,3):
 put64(symbols['faint_tick'],0);put64(symbols['revive_tick'],0)
 cpu.mem_write(master+0x10,bytes([index]));put64(players+0x20+index*8,local)
 mock['now']=50;call('event_case',base,remote,1);assert ticks()==(0,0)
 for cycle in range(1,4):
  dead=cycle*1000;alive=dead+500
  mock['now']=dead;call('event_case',base,local,1);assert ticks()[0]==dead
  mock['now']=dead+100;call('event_case',base,local,1);assert ticks()[0]==dead
  call('event_case',base,remote,0);assert ticks()[1]<dead
  mock['now']=alive;call('event_case',base,local,0);assert ticks()==(dead,alive)
 # Invalid/unavailable manager must never trigger or dereference null.
 put64(holder,0);mock['now']=9999;old=ticks();call('event_case',base,local,1);assert ticks()==old;put64(holder,manager)

gate=0x40008000;cpu.mem_write(gate,bytes(32));hz=19200000
assert call('gate_case',gate,100*hz,0,0,2)==1 # normal combat/low HP cannot start a faint timer
seq=2
for cycle in range(3):
 f=(200+cycle*40)*hz;r=f+8*hz
 assert call('gate_case',gate,f-1,f,0,seq)==1 # concurrent publication of a newer tick
 assert call('gate_case',gate,f+5*hz-1,f,0,seq)==1
 assert call('gate_case',gate,f+5*hz,f,0,seq)==0
 seq+=2;assert call('gate_case',gate,f+7*hz,f,0,seq)==0 # monster damage while still down cannot revive HUD
 assert call('gate_case',gate,r,f,r,seq)==0
 assert call('gate_case',gate,r+hz,f,r,seq)==0 # stale HP stays hidden
 seq+=2;assert call('gate_case',gate,r+2*hz,f,r,seq)==1
# Return before the deadline still clears at exactly five seconds.
f=400*hz;r=f+2*hz
assert call('gate_case',gate,r,f,r,seq)==1
assert call('gate_case',gate,f+5*hz,f,r,seq)==0
assert call('gate_case',gate,f+6*hz,f,r,seq)==0
assert call('gate_case',gate,f+7*hz,f,r,seq+2)==1
# Per-frame death check must never query memory or produce event logs.
cpu.mem_write(gate,bytes(32));put64(symbols['faint_tick'],0);put64(symbols['revive_tick'],0)
old_queries=mock['queries'];old_events=mock['events'];executed=[0]
def instruction_counter(uc,address,size,user):executed[0]+=1
counter=cpu.hook_add(UC_HOOK_CODE,instruction_counter)
for frame in range(1000):assert call('frame_case',gate,frame*hz//30,seq)==1
cpu.hook_del(counter)
assert mock['queries']==old_queries and mock['events']==old_events
print('PASS: 4 relocated life trampolines; all GPR/SIMD/NZCV/SP preserved and original stores retained.')
print('PASS: native ARM64 handler filters remote/invalid players at local indices 0,2,3; 3 faint/return cycles each.')
print('PASS: repeated 5-second deadlines, duplicate events, no low-HP trigger, stale/companion updates and early return.')
print(f'PASS: 1000 normal frame checks, zero memory-query syscalls/logs; {executed[0]//1000} ARM64 instructions/check in fixture (not a timing benchmark).')

if not dump.is_file(): print("SKIP: optional original-game signature check (set MHR_MAIN_NSO)")
