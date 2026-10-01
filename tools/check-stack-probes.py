#!/usr/bin/env python3
"""List functions in an ARM ELF whose stack frame is 4 KB or more and that
don't probe the stack page by page (-fstack-clash-protection).

   checks/check-stack-probes.py PROGRAM.elf [OBJDUMP]

Why: ARMEABISupport gives a RISC OS program its stack a page at a time,
behind a guard page. A frame bigger than a page that doesn't touch each
page in turn can jump over the guard page, and the program aborts.
Everything, including every library, must be built with
-fstack-clash-protection. Exit status 1 if any frame is unprobed.

OBJDUMP defaults to arm-riscos-gnueabihf-objdump from $RISCOS_TOOLCHAIN.
From riscos-warzone2100 (tools/check-stack-probes.py).
"""
import os, re, subprocess, sys

if len(sys.argv) < 2:
    sys.exit(__doc__)
elf = sys.argv[1]
objdump = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
    os.environ.get("RISCOS_TOOLCHAIN", ""), "bin", "arm-riscos-gnueabihf-objdump")
if not os.path.isfile(objdump):
    objdump = "arm-riscos-gnueabihf-objdump"
out = subprocess.run([objdump, "-d", "--no-show-raw-insn", elf],
                     capture_output=True, text=True, check=True).stdout

fn = None; res = {}; regs = {}; probed = set(); probe_regs = set(); last_sub4096 = False
for line in out.splitlines():
    m = re.match(r'^[0-9a-f]+ <(.+)>:$', line)
    if m:
        fn = m.group(1); regs = {}; probe_regs = {'ip'}; continue
    # GCC probes below sp through a scratch register: "sub rX, sp, ip" then
    # "str r0, [rX]" / "str r0, [rX, #-n]". rX is usually ip, sometimes r2..
    m = re.search(r'\tsub\s+(r\d+|ip), sp, ip$', line)
    if m: probe_regs.add(m.group(1))
    m = re.search(r'\tstr\s+r0, \[(r\d+|ip)(, #-\d+)?\]', line)
    if m and m.group(1) in probe_regs: probed.add(fn)
    # Variable-sized allocations (VLAs, alloca) are probed by a loop:
    # "sub sp, sp, #4096" then "str r0, [sp, #4092]"; the remainder is < 4 KB.
    if re.search(r'\tstr\s+r0, \[sp, #40\d\d\]', line) and last_sub4096: probed.add(fn)
    last_sub4096 = bool(re.search(r'\tsub\s+sp, sp, #4096', line)) or \
        (last_sub4096 and not re.search(r'\t(str|b|bl|pop|ldr)', line))
    m = re.search(r'\t(movw|mov)\s+(r\d+|ip|lr|fp|sl), #(\d+)', line)
    if m: regs[m.group(2)] = int(m.group(3)); continue
    m = re.search(r'\tmovt\s+(r\d+|ip|lr|fp|sl), #(\d+)', line)
    if m and m.group(1) in regs: regs[m.group(1)] += int(m.group(2)) << 16; continue
    n = None
    m = re.search(r'\tsub\s+sp, sp, #(\d+)', line)
    if m: n = int(m.group(1))
    m = re.search(r'\tsub\s+sp, sp, (r\d+|ip|lr|fp|sl)$', line)
    if m and m.group(1) in regs: n = regs[m.group(1)]
    if n and n >= 4096: res[fn] = max(res.get(fn, 0), n)

bad = {f: n for f, n in res.items() if f not in probed}
for f, n in sorted(bad.items(), key=lambda x: -x[1]):
    print("UNPROBED", n, f)
print(f"{len(res)} functions with frames >= 4096; {len(bad)} without stack probes")
sys.exit(1 if bad else 0)
