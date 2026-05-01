# x86_64 Guest Port Plan

This document tracks the work needed to move iSH from an i386 guest ABI to an
x86_64 guest ABI without losing the ability to test and debug along the way.

## Current state

The current runtime is not "mostly x86_64 with a few missing pieces". The guest
ABI is hardcoded as i386 in several layers:

- Guest address types are now 64-bit-capable, but process layout and stack
  placement still use conservative 32-bit-era assumptions.
- The live CPU state still executes on 32-bit compat registers
  (`eax`..`edi`, `eip`), even though some x86_64 TLS storage groundwork now
  exists.
- The decoder now recognizes `REX`, `syscall`, `REX.W`, `REX.R/B/X`
  register/index extension for the tested paths, RIP-relative addressing, and
  FS/GS base additions for the current low-address long-mode subset. It still
  lacks complete long-mode instruction and address semantics.
- The ELF loader can now parse ELF64, static ELF64 smoke binaries can start
  with a 64-bit-shaped initial stack, x86_64 `PT_INTERP` handoff works, and
  Alpine x86_64 dynamic busybox shell smokes (`echo`, `sh`, `uname`, `ls -l`,
  and `date`) have been validated. A full Alpine/OpenRC boot is still
  unproven.
- The VDSO is still built as `i386-linux` and uses `int 0x80`.
- Signal frames, ptrace register layouts, and many syscall-facing user structs
  still use the i386 ABI. Syscall dispatch now has an early x86_64 table and a
  64-bit argument/result boundary, but layout-sensitive syscalls still need
  dedicated ABI work.

That means the port must be staged. A partial "accept ELF64" patch would still
crash immediately on startup.

## Progress snapshot

Work already landed on this branch:

- Milestone 0 groundwork: guest ABI selection now exists as an explicit kernel
  concept and `exec`/`mmap`/`uname` use ABI-driven constants instead of raw
  i386 literals.
- Milestone 1 groundwork: the memory backend no longer stores mappings in a
  fixed i386-sized top-level page-directory array; it now uses sparse mapping
  blocks.
- Milestone 1 groundwork: guest addresses used by the runtime (`addr_t`,
  `page_t`, `pages_t`) are now 64-bit-capable.
- Milestone 2 groundwork: kernel-visible register and TLS access now route
  through helper boundaries instead of direct `eax`/`eip`/`esp` field access.
- Milestone 2 groundwork: `struct cpu_state` now stores staged `rax`..`r15`
  and `rip`, synchronized with the compat register file at execution
  boundaries.
- Milestone 3 groundwork: the decoder now reserves `REX` prefixes in long mode
  and recognizes `0x0f 0x05` as `syscall`.
- Milestone 3 first execution slice: the AArch64 host backend now executes a
  small but real x86_64 long-mode subset for the low eight registers, including
  64-bit register moves, stack/control, basic ALU ops, and simple shifts.
- Milestone 3 progress: 64-bit compare/branch flags, RIP-relative addressing,
  and an initial `REX.R/B` high-register path for `r8`..`r15` now work for the
  tested static smoke programs.
- Milestone 3 progress: dynamic-loader-driven instruction fixes now include
  64-bit `lea`, `cdqe`/`cqo`, `movsxd`, two-operand `imul`,
  `bt`/`bts`/`btr`/`btc`, 64-bit string op coverage, low-byte
  `spl`/`bpl`/`sil`/`dil`, SIB `REX.X` index handling for `r12`, and
  `REX.B` decoding for `xchg rax,r8..r15`.
- Milestone 3 progress: Alpine shell startup now survives the tested 64-bit
  `lock`-prefixed atomic memory family and 64-bit `adc`/`sbb` arithmetic path.
- Milestone 3 progress: long-mode immediate/relative decoding now sign-extends
  the x86_64 cases that require it, SSE `movd` can read high 32-bit GPRs from
  `r8d`..`r15d`, and the AArch64 backend has 64-bit `rol`/`ror` gadgets for
  the paths hit by BusyBox/musl.
- Milestone 4 groundwork: syscall argument extraction now knows the x86_64
  register order, and kernel dispatch has an initial table of Linux x86_64
  syscall numbers for ABI-safe calls.
- Milestone 4 progress: the generic syscall dispatch function type now carries
  `addr_t` arguments/results, and anonymous x86_64 `mmap` has a guest read/write
  smoke test.
- Milestone 4 progress: x86_64 syscall `7` now dispatches to the existing
  `poll(2)` implementation, unblocking BusyBox `sh` interactive terminal
  sessions that previously reported `poll: function not implemented`.
- Milestone 4 progress: x86_64 syscalls `13` and `14` now dispatch to
  `rt_sigaction`/`rt_sigprocmask` adapters. `rt_sigaction` understands the
  64-bit userspace layout, but custom handler delivery is suppressed until
  long-mode signal frames and `rt_sigreturn` exist.
- Milestone 5 groundwork: initial stack and auxv serialization now follow the
  guest word size, which is enough for static ELF64 smoke binaries to reach
  `_start`.
- Milestone 5 groundwork: stored `fsbase`/`gsbase` state exists in
  `struct cpu_state`, and `arch_prctl(ARCH_{SET,GET}_{FS,GS})` now stores and
  returns that state.
- Milestone 5 progress: long-mode FS/GS segment-prefix memory references now
  consume those stored bases in the current low-address model.
- Milestone 5 groundwork: the ELF loader now parses both ELF32 and ELF64
  headers/program headers into a shared internal representation.
- Milestone 5 progress: x86_64 dynamic interpreters are no longer rejected at
  exec time, a synthetic `PT_INTERP` handoff to a fake x86_64 interpreter has
  been verified, and a real Alpine 3.23.3 x86_64 minirootfs can run
  `/bin/busybox echo alpine`, `/bin/sh`, `/bin/busybox uname -m`, `ls -l
/bin/sh`, and `date -u -d @0` through musl and exit cleanly.
- App integration progress: the iOS app can now be built with
  `ISH_GUEST_ARCH_X86_64=1`, and Xcode can package a local experimental rootfs
  with upstream Alpine x86_64 repositories while skipping iSH's legacy i386 APK
  installer prompt.
  via `ROOTFS_PATH` instead of always downloading the default i386 rootfs.
- Build progress: an iOS device app bundle has been verified with
  `ISH_GUEST_ARCH_X86_64=1` and `build/ish-x86_64-devroot.tar.gz`; the dev
  rootfs now contains Alpine x86_64 userspace plus a static ELF64 init that
  keeps app boot stable.
- Build progress: `tools/make-x86_64-devroot.sh` now regenerates that Alpine
  x86_64 app dev rootfs by default, while `--smoke` keeps the older static
  smoke root available.
- App smoke progress: the first task no longer crashes when inheriting from a
  parent whose `mm` has already been released; local builds can also fall back
  to the normal app container when the App Group entitlement is unavailable.
- App smoke progress: x86_64 builds import the bundled root as
  `x86_64-alpine-ishsh` and now default the terminal launch command to the
  classic Alpine login path (`/bin/login -f root`) instead of the temporary
  line-by-line `/bin/sh -c` launcher.

What is still intentionally blocked:

- Dynamic x86_64 startup now works for basic real Alpine busybox shell smokes,
  but broader rootfs use is still expected to hit missing 64-bit VDSO, signal
  ABI, syscall-layout, or instruction-coverage blockers.
- The app-level x86_64 rootfs now contains Alpine userspace, but `/sbin/init`
  is intentionally a static dev init. Full Alpine/OpenRC boot remains a later
  milestone.
- The live execution engine still runs on the compat register file, so the new
  x86_64 TLS state and staged 64-bit register state are only partially
  consumed by long-mode execution.
- Guest mappings are now stored sparsely, but user-visible layout policy is
  still effectively 32-bit, which remains a major blocker before long-mode
  execution can become real.
- Host memory gadgets still calculate effective addresses through low 32-bit
  address registers, so canonical high x86_64 addresses remain out of scope for
  the current slice.

## Porting strategy

Keep the port incremental and always leave a bootable configuration at the end
of each milestone.

### Milestone 0: Guest ABI abstraction

Goal: stop baking i386 assumptions into unrelated subsystems.

Deliverables:

- Introduce an explicit guest architecture/ABI descriptor.
- Route the ELF loader's class/machine/platform checks through that descriptor.
- Store the selected guest ABI in `struct mm`.
- Replace scattered literals such as `"i686"` and the word-sized stack gap with
  ABI-driven constants.

Exit criteria:

- Existing i386 behavior remains unchanged.
- Future x86_64 work can hang off a single guest ABI abstraction instead of new
  one-off constants.

### Milestone 1: 64-bit guest address space

Goal: represent 64-bit virtual addresses without exploding memory usage.

Deliverables:

- Widen guest address/page types from 32-bit to 64-bit capable types.
- Replace the current fixed 2-level page directory assumptions in
  `kernel/memory.c` with a sparse structure that can cover canonical x86_64
  addresses.
- Rework hole finding, stack placement, `mmap`, `brk`, and procfs map dumping
  around 64-bit addresses.

Exit criteria:

- The emulator can map, access, and dump sparse 64-bit guest ranges.
- i386 still works under the new memory backend.

### Milestone 2: 64-bit CPU model

Goal: extend the guest register file and instruction-visible state to long mode.

Deliverables:

- Add `rax`..`r15`, `rip`, widened `rflags`, and separate FS/GS base state.
- Keep legacy 8/16/32-bit register views working on top of the same storage.
- Update helpers, ptrace glue, and any assembly offsets generated from
  `struct cpu_state`.

Exit criteria:

- Both i386 and x86_64 register views are representable.
- Host gadget backends build against the widened CPU layout.

### Milestone 3: Decoder and execution engine

Goal: decode and execute x86_64 instructions correctly.

Deliverables:

- Add `REX` prefix handling and stop treating `0x40..0x4f` as one-byte
  `inc/dec` in long mode.
- Add 64-bit operand/address-size semantics.
- Extend ModRM/SIB decoding for long mode register encoding.
- Audit instructions that implicitly use `eip`, `esp`, or 32-bit stack pushes.

Current progress:

- `REX.W` now reaches a first real execution path on the AArch64 backend for
  low-register `mov`, `push`/`pop`, `call`/`ret`, `pushf`/`popf`,
  `add`/`sub`/`and`/`or`/`xor`, and `shl`/`shr`/`sar`.
- `REX.R/B` now reaches an initial high-register path for common ModRM register
  operands, `push/pop r8..r15`, and register-immediate moves.
- RIP-relative memory operands now work for the low-address x86_64 model.
- SIB `REX.X` now handles the special raw index `100b` case correctly, so
  encodings such as `(%rdx,%r12,8)` no longer collapse to "no index".
- The `xchg rax,reg` opcode family now honors `REX.B`, including
  `xchg rax,r12` patterns emitted by Alpine busybox.
- The tested 64-bit `lock`-prefixed atomic memory operations now include
  `add`, `sub`, `adc`, `sbb`, `and`, `or`, `xor`, `inc`, `dec`, and `xadd`.
- The tested 64-bit `adc`/`sbb` arithmetic path now exists on the AArch64 host
  backend.
- Dynamic-loader coverage now includes the tested 64-bit `lea`, `movsxd`,
  `imul`, bit-test, string, and low-byte REX-register cases needed to launch
  Alpine busybox.
- Static smoke binaries now survive common x86_64 prologue patterns such as
  `mov %rsp,%rbp`, `sub/add %rsp`, `push/pop %rbp`, and a simple 64-bit
  shift/add sequence.
- 64-bit compare/branch flags work for the current subset.
- Full long-mode ModRM/SIB semantics, complete high-register instruction
  coverage, vector register extension, and high canonical addresses are still
  missing.

Exit criteria:

- Simple x86_64 test programs can execute arithmetic, stack, branch, and memory
  instructions.
- i386 instruction behavior remains stable.

### Milestone 4: x86_64 syscall ABI

Goal: enter the kernel with the correct x86_64 calling convention.

Deliverables:

- Add a separate x86_64 syscall table and dispatch path.
- Implement argument extraction from `rax`, `rdi`, `rsi`, `rdx`, `r10`, `r8`,
  `r9`.
- Split structs that currently encode i386 user layouts (`stat`, `mmap`,
  `rusage`, socket structs, time structs, etc.) into guest-ABI-specific
  versions.
- Revisit compat-only syscalls such as `socketcall` and `set_thread_area`.

Current progress:

- The register extraction boundary and a first x86_64 syscall-number dispatch
  table already exist for ABI-safe syscalls.
- Initial x86_64 adapters now exist for `mmap` and `clone`, but they still
  operate within the current low-32-bit-safe execution/runtime limits.
- The syscall call boundary now passes `addr_t` arguments/results, which is
  required before real x86_64 pointers can flow through `syscall`.
- `mmap`, `clone`, signal-related entry/return, and other ABI-sensitive calls
  still need dedicated x86_64 handling before this milestone is complete.
- x86_64 syscall numbers `13` and `14` (`rt_sigaction` and `rt_sigprocmask`)
  are now wired. Signal-heavy workloads should still be treated as blocked
  until the x86_64 signal ABI exists because custom handler delivery is not
  enabled yet.

Exit criteria:

- A dynamically linked x86_64 userspace can reach `execve`, `mmap`, `brk`,
  `arch_prctl`, `read`, `write`, and exit cleanly.

### Milestone 5: Loader, VDSO, TLS, and process startup

Goal: start real x86_64 ELF binaries.

Deliverables:

- Add ELF64 header/program-header parsing.
- Build a 64-bit initial stack and auxv layout.
- Add an x86_64 VDSO and symbol lookup path.
- Implement `arch_prctl(ARCH_SET_FS/ARCH_GET_FS)` and FS-base-backed TLS.
- Add x86_64 interpreter loading and relocation assumptions.

Exit criteria:

- `/bin/sh` from an x86_64 rootfs reaches user code.

Current progress:

- ELF64 header/program-header parsing is already done.
- The initial stack and auxv are now serialized at guest word size.
- Static ELF64 smoke binaries can now reach `_start`, execute `syscall`, and
  read `argc` from the 64-bit-shaped initial stack.
- Minimal compiled C static ELF64 smoke binaries now run at `-O0` and `-O2`.
- FS-base-backed TLS works for a direct `%fs:offset` smoke test.
- A synthetic x86_64 `PT_INTERP` handoff to a fake interpreter now works.
- Real Alpine x86_64 dynamic startup now reaches musl `__dls3`, hands off to
  busybox via a corrected auxv `AT_ENTRY`, and successfully runs
  `/bin/busybox echo alpine`, `/bin/sh`, `/bin/busybox uname -m`, `ls -l
/bin/sh`, and `date -u -d @0`.
- The app build can select x86_64 guest startup with `ISH_GUEST_ARCH_X86_64=1`
  and package a local rootfs tarball with `ROOTFS_PATH`.
- The app dev root now packages Alpine x86_64 userspace for `/bin/sh`, while
  using a static ELF64 `/sbin/init` to keep boot stable until full Alpine init
  is supported. Terminal startup now uses the classic login/shell flow.
- x86_64 VDSO, full Alpine/OpenRC boot, broad interactive rootfs validation,
  and signal-return plumbing are still missing.

### Milestone 6: Signals, ptrace, and debugging surfaces

Goal: keep runtime and debugging features usable on x86_64.

Deliverables:

- Add x86_64 `ucontext`, `sigcontext`, and signal frame layouts.
- Add x86_64 sigreturn trampolines.
- Split ptrace register transfer logic by guest ABI.
- Update procfs dumps and debugger helpers that assume 32-bit registers.

Exit criteria:

- Signal delivery/return works for x86_64.
- ptrace/GDB can inspect and modify x86_64 guest state.

### Milestone 7: Validation and rollout

Goal: prove the port works and avoid regressions.

Deliverables:

- Add targeted decoder/ABI tests for `REX`, FS-base TLS, syscall argument
  ordering, signal frames, and ELF64 startup.
- Add an x86_64 rootfs path to end-to-end tests.
- Keep i386 tests running until the project decides whether to retain dual-ABI
  support.

Exit criteria:

- x86_64 shell startup, basic package tooling, full init/service boot, and
  signal-heavy workloads pass.
- i386 regression coverage stays green during the transition.

## Suggested execution order

1. Finish Milestone 0 everywhere the loader and process startup still rely on
   i386 literals.
2. Land the 64-bit memory model before touching the decoder.
3. Widen the CPU state and fix generated assembly offsets.
4. Add long-mode decode and execution support.
5. Add the x86_64 syscall ABI and loader startup path.
6. Finish signals/ptrace and then expand test coverage.

## High-risk areas

- `kernel/memory.c`: sparse storage exists, but user-visible layout still uses
  a conservative low-address search window.
- `emu/cpu.h`, `emu/modrm.h`, `emu/decode.h`: several instruction families
  still assume 8 general registers or 32-bit instruction/address semantics.
- Host memory gadgets still use low-32-bit effective-address registers.
- `kernel/calls.c`: syscall dispatch has x86_64 numbering now, but many
  layout-sensitive syscalls still expose i386-shaped user structs.
- `kernel/signal.*` and `kernel/ptrace.*`: user-visible ABI layouts need
  separate 32-bit and 64-bit definitions.
- `vdso/`: currently hardcoded to `i386-linux` and `int 0x80`.

## What "done" looks like

A successful port is not just "ELF64 loads". It means an x86_64 userspace can
boot, perform dynamic linking, use TLS, make normal syscalls, receive signals,
and be debugged without regressing the existing emulator behavior mid-port.
