# x86_64 Guest Port Status

This file is the short operational recap of the x86_64 guest port. The full
roadmap lives in `docs/x86_64-port-plan.md`; this document answers two simpler
questions:

- What has already been done?
- What is still missing before iSH can actually run an x86_64 userspace?

## Honest status today

The project is not yet a general-purpose x86_64 guest emulator.

What exists today is the first real slice of the port:

- The guest ABI is now an explicit kernel concept instead of being hardcoded as
  i386 in scattered places.
- The guest address type exposed inside the runtime is now 64-bit-capable.
- The ELF loader now understands both ELF32 and ELF64 layouts at parse time.
- Kernel-facing CPU/TLS access is now routed through a guest-ABI boundary
  instead of direct `eax`/`eip`/`esp` field access.
- `cpu_state` now carries staged `rax`..`r15` and `rip` storage synchronized
  with the compat register file.
- The long-mode decoder now reserves `REX` prefixes and recognizes `syscall`
  instead of aliasing those opcodes to i386 behavior.
- The AArch64 host backend now has a first real long-mode execution slice for
  the low eight general registers and an initial `REX.R/B` path for
  `r8`..`r15`, including 64-bit register moves, basic arithmetic/logic,
  shifts, compare/branch flags, and 64-bit stack/control gadgets.
- The kernel now has an initial x86_64 syscall register/numbering path for a
  safe subset of Linux x86_64 syscalls, and the syscall dispatch boundary now
  passes 64-bit arguments/results instead of truncating everything to
  `dword_t`. `poll(2)` is now wired as x86_64 syscall `7`, which is required
  by BusyBox `sh` in interactive terminal sessions. x86_64 syscalls `13` and
  `14` (`rt_sigaction` and `rt_sigprocmask`) are also wired for userland
  compatibility.
- The process bootstrap can now build a guest-word-sized initial stack and auxv
  for x86_64 static ELF64 smoke binaries.
- Long-mode memory references now support basic RIP-relative addressing and
  FS/GS-base segment additions in the low-address execution model.
- The iOS app startup can now be built with `ISH_GUEST_ARCH_X86_64=1`, causing
  the first app task to use the x86_64 guest ABI instead of the default i386
  ABI.
- Xcode can now package a local rootfs tarball via `ROOTFS_PATH`, which avoids
  hard-coding the existing i386 rootfs while testing x86_64 app builds.
- `tools/make-x86_64-devroot.sh` now regenerates the x86_64 app dev rootfs
  from the local Alpine x86_64 root when available, while replacing
  `/sbin/init` with a tiny static ELF64 init that stays alive quietly during
  app boot.
- A real Alpine x86_64 minirootfs now reaches the dynamic linker and can run
  basic dynamic busybox commands through the CLI emulator, including
  `/bin/busybox echo alpine`, `/bin/busybox sh -c 'echo alpine-shell'`, and
  `/bin/busybox uname -m`.
- The packaged x86_64 app rootfs now includes real Alpine userspace and uses a
  classic terminal launch command (`/bin/login -f root`) so interactive shell
  startup follows the normal Alpine flow instead of the temporary line-by-line
  `/bin/sh -c` wrapper.
- The x86_64 rootfs generator now writes upstream Alpine x86_64 repositories
  based on `/etc/alpine-release`, for example
  `https://dl-cdn.alpinelinux.org/alpine/v3.23/main` and `community` for the
  current Alpine 3.23 rootfs. x86_64 app builds skip iSH's legacy built-in APK
  installer prompt, which points at the old i386 `/ish/apk/main/x86/...`
  package flow.
- x86_64 `rt_sigaction` now reads/writes the 64-bit userspace structure, and
  custom x86_64 signal handlers are deliberately not delivered yet because the
  signal frame and `rt_sigreturn` path are still i386-shaped.
- The tree builds successfully with the current preparatory refactors.

What does not exist yet:

- A real 64-bit guest virtual address space.
- A complete live 64-bit CPU/register model for every instruction surface.
- Complete long-mode instruction decode and execution.
- A full Alpine/OpenRC boot path with real device nodes and service management.

Because of that, this is still not a complete x86_64 Linux environment. What
now works is a useful, concrete slice: static ELF64 smoke binaries can reach
`_start`, read the 64-bit-shaped initial stack, execute `syscall`, use simple
compiled C prologues/calls, access RIP-relative data, use FS-base TLS, survive
common `REX.W`/`REX.R`/`REX.B` register and stack code, and Alpine's dynamic
linker can launch real x86_64 `busybox` far enough to run shell commands such
as `uname`, `ls -l`, and `date` cleanly.

## What has been done

### 1. Guest ABI abstraction

The runtime now has a single place describing guest architecture assumptions.

Implemented:

- Added `kernel/guest.h` with explicit guest ABI descriptors for `x86_32` and
  `x86_64`.
- Added guest architecture selection plumbing in process bootstrap.
- Stored the selected guest architecture inside `struct mm`.
- Replaced scattered i386 literals in loader/startup-adjacent code with
  ABI-driven values such as platform name, stack gap, stack top, and ELF
  class/machine checks.

Main files touched:

- `kernel/guest.h`
- `kernel/init.c`
- `kernel/init.h`
- `kernel/mm.h`
- `kernel/mmap.c`
- `kernel/exec.c`
- `kernel/uname.c`
- `xX_main_Xx.h`

Why this matters:

- New x86_64 work can now attach to one guest ABI layer instead of duplicating
  one-off conditionals all over the tree.

### 2. Memory and mapping groundwork

Some preparation for 64-bit guest addresses already landed, but this milestone
is not finished.

Implemented:

- Widened `addr_t`, `page_t`, and `pages_t` to 64-bit-capable types.
- Replaced the fixed top-level i386-style page-directory array with sparse,
  ordered page-table blocks.
- Removed some assumptions that hole-finding must always use a single hardcoded
  i386 range.
- Added ABI-aware mapping hole selection through `mm_find_hole(...)`.
- Stopped address-space clone and several memory walks from depending on a
  fixed `MEM_PAGES` storage layout.
- Updated `TLB_PAGE(...)` to avoid a baked-in 32-bit page mask.

Main files touched:

- `emu/mmu.h`
- `emu/tlb.h`
- `kernel/memory.h`
- `kernel/memory.c`
- `kernel/mm.h`
- `kernel/mmap.c`

What is important here:

- The types are more future-proof now.
- The memory backend storage is now sparse, which is a real prerequisite for
  x86_64.
- The process bootstrap, live CPU model, decoder, and user-visible ABI layouts
  are still largely 32-bit, so the runtime still cannot execute a true 64-bit
  userspace even though the address-space plumbing is now much closer.

### 3. ELF loader refactor

The loader no longer assumes that every ELF header in the world is `ELF32`.

Implemented:

- Added raw `ELF64` header/program-header layouts.
- Added an internal normalized representation (`elf_info`,
  `elf_prg_info`) used by the loader for both ELF32 and ELF64.
- Updated `exec` to parse headers based on guest ABI expectations.
- Added validation around offsets, address truncation, and basic overflow
  conditions during program-header handling.
- Updated auxv/header plumbing to use normalized loader metadata instead of
  only `ELF32` struct sizes.

Main files touched:

- `kernel/elf.h`
- `kernel/exec.c`

What this means in practice:

- ELF64 binaries are now parsed honestly.
- Static ELF64 smoke binaries can now be started with a 64-bit-shaped initial
  stack in low guest addresses.
- Dynamic x86_64 startup is no longer rejected up front, but it is still
  expected to expose missing instruction, syscall-layout, VDSO, and signal ABI
  work once tested with a real x86_64 rootfs.

### 4. CPU/TLS boundary groundwork

Implemented:

- Added a kernel-side CPU ABI helper layer so syscall dispatch, `exec`,
  `fork`, signals, and ptrace do not directly reach into `eax`/`eip`/`esp`
  field names anymore.
- Added explicit compat-register helpers in `emu/cpu.h` so the current i386
  register storage is addressed through one boundary.
- Added stored `fsbase`/`gsbase` state to `struct cpu_state`.
- Added staged long-mode register storage (`rax`..`r15`, `rip`) to
  `struct cpu_state`.
- Implemented stored `arch_prctl(ARCH_{SET,GET}_{FS,GS})` behavior for the
  x86_64 guest ABI.
- Added long-mode FS/GS segment-prefix consumption in the execution path, so
  `%fs:offset` and `%gs:offset` references use the stored base values in the
  current low-address model.
- Synchronized the staged 64-bit register file with the live compat register
  file at execution boundaries so future long-mode work has persistent state to
  build on.
- Made the TLS setup path guest-aware so the existing i386 `set_thread_area`
  flow and the future x86_64 raw-FS-base flow do not have to share one fake
  calling convention.
- Exported generated offsets for the new TLS base fields so host glue can use
  them later without another layout hunt.

Main files touched:

- `emu/cpu.h`
- `kernel/cpu.h`
- `kernel/calls.c`
- `kernel/exec.c`
- `kernel/fork.c`
- `kernel/signal.c`
- `kernel/ptrace.c`
- `kernel/misc.c`
- `kernel/tls.c`
- `asbestos/asbestos.c`
- `asbestos/helpers.c`
- `asbestos/offsets.c`

Why this matters:

- The next register-file change no longer has to edit every kernel path that
  logs, boots, clones, signals, or ptraces a task.
- TLS state for x86_64 now has a real place to live and the first long-mode
  memory path consumes it for FS/GS-prefixed references.
- `rip` and the upper x86_64 registers now have a stable home in task state
  before the decoder learns how to execute them.

### 5. Decoder and syscall ABI groundwork

Implemented:

- Taught the decoder to treat `0x40..0x4f` as `REX` prefixes in long mode
  instead of one-byte `inc`/`dec`.
- Added early long-mode recognition of `0x0f 0x05` as `syscall`.
- Added guest-word-size plumbing so decode/generation can distinguish 32-bit
  and 64-bit guest mode.
- Switched syscall argument extraction to the real x86_64 register order
  (`rax`, `rdi`, `rsi`, `rdx`, `r10`, `r8`, `r9`) at the kernel CPU boundary.
- Added an initial x86_64 syscall dispatch table keyed by Linux x86_64 syscall
  numbers for ABI-safe calls such as `read`, `write`, `openat`,
  `arch_prctl`, `futex`, and `exit_group`.
- Added first x86_64-specific syscall adapters for `mmap` and `clone` where
  the ABI does not match the i386 syscall shape one-to-one.
- Widened the generic syscall function boundary to `addr_t` arguments/results
  so x86_64 pointers and mapping results are no longer truncated at kernel
  entry.

Main files touched:

- `emu/mmu.h`
- `emu/decode.h`
- `asbestos/gen.h`
- `asbestos/gen.c`
- `kernel/cpu.h`
- `kernel/calls.c`

Why this matters:

- x86_64 `syscall` no longer collides with leftover i386 opcode handling.
- Kernel entry no longer assumes that every guest syscall came from
  `eax`/`ebx`/`ecx`-style register assignment.
- The remaining x86_64 syscall work is now mostly about ABI-sensitive syscalls
  and guest-visible data layouts, not about lacking a dispatch skeleton.

### 6. First long-mode execution slice

Implemented:

- Added runtime operand-size selection for `REX.W` in the generator/decoder.
- Added 64-bit stack-width handling for long-mode `push`/`pop`/`leave`.
- Added 64-bit `call`/`call_indir`/`ret`/`pushf`/`popf` gadget variants.
- Made the AArch64 host backend load and save the staged low x86_64 register
  file (`rax`..`rdi`, `rsp`, `rbp`, `rip`) instead of dropping back to compat
  storage at every execution boundary.
- Added first 64-bit low-register gadgets for `load`, `store`, `add`, `sub`,
  `and`, `or`, `xor`, and `shl`/`shr`/`sar` on the AArch64 backend.
- Fixed gadget-array emission order so the new 64-bit slots are actually
  reachable at runtime instead of decoding to null entries.
- Added 64-bit compare/branch flag fidelity for the current long-mode subset.
- Added initial `REX.R/B` ModRM register extension for `r8`..`r15`, including
  high-register load/store/xchg and common ALU source/destination paths.
- Added long-mode `push/pop r8..r15`, `mov imm -> r8..r15`, and `movabs`
  support for the tested register-immediate forms.
- Added basic RIP-relative ModRM addressing for long-mode memory references.
- Added 64-bit `cdqe`/`cqo`, `movsxd`, unary ops, two-operand `imul`,
  `bt`/`bts`/`btr`/`btc`, 64-bit string op coverage, and a corrected
  64-bit `lea` address path for the tested dynamic-loader code.
- Added low-byte `REX` register handling for `spl`/`bpl`/`sil`/`dil`.
- Fixed long-mode SIB handling so raw index `100b` becomes `r12` when
  `REX.X` is set instead of always being treated as "no index".
- Fixed `0x90..0x97` `xchg rax,reg` decoding so `REX.B` selects
  `r8`..`r15`, including the `xchg rax,r12` pattern used by Alpine busybox.
- Added the AArch64 64-bit `adc`/`sbb` arithmetic paths and the tested 64-bit
  `lock`-prefixed atomic memory family (`add`, `sub`, `adc`, `sbb`, `and`,
  `or`, `xor`, `inc`, `dec`, and `xadd`) needed by Alpine busybox shell
  startup.
- Fixed AArch64 64-bit register-op emission for operations whose ARM mnemonic
  differs from the x86 mnemonic, such as `xor`/`or`.

Main files touched:

- `emu/decode.h`
- `emu/cpu.h`
- `asbestos/gen.c`
- `asbestos/gadgets-aarch64/gadgets.h`
- `asbestos/gadgets-aarch64/memory.S`
- `asbestos/gadgets-aarch64/control.S`
- `asbestos/gadgets-aarch64/math.S`
- `asbestos/gadgets-aarch64/bits.S`
- `asbestos/gadgets-x86_64/gadgets.h`
- `asbestos/gadgets-x86_64/math.S`
- `asbestos/gadgets-x86_64/memory.S`
- `asbestos/gadgets-x86_64/control.S`
- `asbestos/gadgets-x86_64/entry.S`

What this means in practice:

- On the current Apple Silicon host backend, static x86_64 smoke binaries now
  execute a small but real long-mode subset instead of only "32-bit-safe"
  instructions under low addresses.
- The boundary between staged `regs64` state and live execution is now strong
  enough that values survive across JIT block exits for this first long-mode
  slice.
- The high-register path is still partial, but normal smoke programs can now
  use `r8`/`r9` syscall arguments, copied high registers, high-register
  compare/branch, `r8d` zero-extension, `push/pop r12`, and high-register
  `lea`/shift/`xchg` patterns.
- The tested dynamic shell path now also survives a real `lock orq` memory
  operation and a later `adcq` carry-propagation sequence.

### 7. Process bootstrap groundwork

Implemented:

- Reworked initial stack serialization to use the selected guest word size
  instead of always writing 32-bit argc/argv/envp/auxv entries.
- Added guest-word-sized auxv serialization so x86_64 bootstrap no longer
  truncates pointers and aux values at stack-build time.
- Stopped wiring the current i386 VDSO into x86_64 auxv so static ELF64 smoke
  binaries are not handed a clearly wrong VDSO header.
- Relaxed `exec` enough to allow static ELF64 smoke binaries to start.
- Removed the unconditional `PT_INTERP` rejection for x86_64 so dynamic
  interpreters can now be attempted.
- Verified a synthetic ELF64 `PT_INTERP` path where a dynamic main transfers
  control to a fake x86_64 interpreter and exits from the interpreter.
- Fixed the `PT_INTERP` handoff so the CPU starts at the interpreter entry but
  auxv `AT_ENTRY` still points at the main executable entry, as expected by
  musl's `__dls3`.
- Verified that a static ELF64 `_start -> syscall exit(0)` smoke binary runs,
  and that a second ELF64 smoke binary can read `argc` from `(%rsp)` and exit
  with that value.
- Downloaded and verified the official Alpine 3.23.3 x86_64 minirootfs for
  dynamic-linker testing.
- Verified `/bin/busybox echo alpine` from that Alpine x86_64 rootfs: it
  reaches musl, runs real busybox code, writes `alpine`, and exits with status 0.
- Verified `/bin/busybox sh -c 'echo alpine-shell'` from the same rootfs:
  dynamic shell startup runs far enough to execute the command and exit with
  status 0.
- Verified `/bin/busybox uname -m`: the guest reports `x86_64` and exits with
  status 0.

Main files touched:

- `kernel/exec.c`

Why this matters:

- x86_64 startup is no longer blocked only by stack serialization.
- We now have a working bridge between the ELF64 loader and actual guest user
  code, even if that bridge is still narrow and limited to simple static test
  cases plus the first real Alpine dynamic busybox smoke.

### 8. Buildability of the current branch

Current state:

- `meson setup --wipe build` works with the Homebrew LLVM toolchain and `lld`
  installed.
- `CCACHE_DISABLE=1 ninja -C build` completes successfully.
- `xcodebuild` can produce an iOS device app bundle with
  `ISH_GUEST_ARCH_X86_64=1` and a local `ROOTFS_PATH` pointing at a minimal
  x86_64 rootfs tarball.
- The verified app product was
  `build/XcodeDerivedData/Build/Products/Debug-ApplePleaseFixFB19282108-iphoneos/iSH.app`,
  and its bundled `root.tar.gz` matched
  `build/ish-x86_64-devroot.tar.gz`.
- The x86_64 app rootfs can be regenerated with
  `tools/make-x86_64-devroot.sh`. By default it packages the local Alpine
  x86_64 rootfs and replaces `/sbin/init` with a static ELF64 init that prints
  a marker and sleeps forever, so app boot stays quiet while the terminal uses
  Alpine's real `/bin/sh`.
- `tools/make-x86_64-devroot.sh --smoke` still exists for the older static
  smoke root with the tiny echo-loop login shell.
- x86_64 app builds import the bundled root as `x86_64-alpine-ishsh` and
  prefer it as the default root, avoiding stale i386 or older x86_64 smoke/root
  imports in an existing development container.
- The app startup path now tolerates a task parent whose `mm` was already
  released and falls back to the selected default guest ABI instead of
  dereferencing `parent->mm`.
- The app group container fallback lets local development builds continue when
  the App Group entitlement is unavailable under a personal provisioning
  profile.

Practical note:

- `ccache` currently needs to stay disabled in this environment unless its
  cache directory permissions are fixed.
- In the Codex sandbox, Xcode asset compilation fails because `actool` cannot
  talk to `CoreSimulatorService`; running the same `xcodebuild` command outside
  the sandbox succeeds.
- `app/iSH.xcconfig` currently defaults local builds to x86_64 and points
  `ROOTFS_PATH` at `$(SRCROOT)/build/ish-x86_64-devroot.tar.gz`, so Xcode Run
  works after the smoke rootfs has been generated once.
- If app launch keeps reusing stale imported state, delete the app/group
  container and run again. On this Mac the local development group container was
  `~/Library/Group Containers/group.dev.lore.ish.x64local`.
- With the Alpine dev root, a successful terminal session should launch the
  normal Alpine login/shell path (`/bin/login -f root`) and then allow regular
  interactive shell usage.
- The packaged rootfs tarball has been re-extracted and tested through the CLI:
  `/sbin/init` stays alive under `timeout`, and `/bin/sh` can run `uname -m`,
  `ls -l /bin/sh`, and `date -u -d @0` from a real TTY.
- A separate extracted Alpine x86_64 minirootfs test also works from the CLI:
  `./build/ish -a x86_64 -r build/alpine-x86_64-rootfs /bin/busybox echo alpine`.
  Additional validated commands include `/bin/sh`, `uname -m`, `ls -l
/bin/sh`, `date -u -d @0`, and `date -d @1769548773`.
  This validates dynamic linking and basic real busybox shell startup, but it
  is not yet a full Alpine/OpenRC boot.

Command used for the app smoke build:

```sh
tools/make-x86_64-devroot.sh

xcodebuild -quiet \
  -project iSH.xcodeproj \
  -scheme iSH \
  -configuration Debug-ApplePleaseFixFB19282108 \
  -sdk iphoneos \
  -destination generic/platform=iOS \
  -derivedDataPath build/XcodeDerivedData \
  build \
  SUPPORTED_PLATFORMS=iphoneos
```

## Latest verified smoke tests

All of these were verified with `./build/ish -a x86_64 ...` on the current
Apple Silicon build:

- Static ELF64 `_start -> exit(0)`.
- Static ELF64 smoke `init` and `login` binaries that print a marker and remain
  alive under `timeout`.
- Interactive static ELF64 smoke session: `login` reads from a TTY and echoes
  input back through the terminal.
- Initial stack `argc` read from `(%rsp)`.
- `call`/`ret`, normal prologue/epilogue, and stack alignment smoke binaries.
- `REX.W` arithmetic and 64-bit compare/branch flags.
- `r8`, `r9 -> r10`, and high-register compare/branch.
- `r8d` zero-extension, `movabs` into `r8`, and `push/pop r12`.
- RIP-relative `.rodata` load.
- Minimal C `_start` at `-O0` and `-O2`.
- C function call with 9 arguments at `-O0` and `-O2`.
- Anonymous x86_64 `mmap` followed by guest read/write.
- `arch_prctl(ARCH_SET_FS)` followed by `%fs:offset` load.
- High-register shift plus `lea (%r8,%r9,4),%r10`.
- Synthetic ELF64 `PT_INTERP` handoff to a fake x86_64 interpreter.
- Official Alpine 3.23.3 x86_64 minirootfs dynamic smoke:
  `/bin/busybox echo alpine` prints `alpine` and exits cleanly.
- Official Alpine 3.23.3 x86_64 minirootfs dynamic shell smoke:
  `/bin/busybox sh -c 'echo alpine-shell'` prints `alpine-shell` and exits
  cleanly.
- Official Alpine 3.23.3 x86_64 minirootfs `uname` smoke:
  `/bin/busybox uname -m` prints `x86_64` and exits cleanly.
- Official Alpine 3.23.3 x86_64 minirootfs shell smoke:
  `/bin/sh` can run `uname -m`, `ls -l /bin/sh`, and `date -u -d @0` without
  the earlier musl `asctime_r` illegal-instruction failure.
- Official Alpine 3.23.3 x86_64 interactive-shell smoke:
  `/bin/sh -i -c 'echo interactive-cmd; uname -m'` runs without the earlier
  `sh: poll: function not implemented` failure.
- Packaged x86_64 app dev root smoke:
  re-extracting `build/ish-x86_64-devroot.tar.gz` and running `/bin/sh`
  from a real TTY can execute `echo`, `uname -m`, `ls -l /bin/sh`, and
  `date -u -d @0`; running `/sbin/init` prints `iSH x86_64 dev init alive`
  and remains alive under `timeout`.
- Xcode iOS-device app packaging with `ISH_GUEST_ARCH_X86_64=1` and a local
  minimal x86_64 rootfs tarball.
- Xcode product rootfs verification with `cmp -s` against the generated
  `build/ish-x86_64-devroot.tar.gz`.

## What is still missing

### 1. Real 64-bit guest address space

This is the next hard blocker.

Still missing:

- Finish the user-visible layout policy for canonical x86_64 user-space
  addresses instead of the current conservative low-address placement.
- Widen the host gadget address calculation path beyond the current low-32-bit
  effective-address model.
- Stop relying on limits such as `MEM_PAGES` as if the guest address space were
  still bounded like i386.
- Audit every address-bearing field that still assumes guest pointers will stay
  in the low 32-bit range during real execution.

Why it blocks the rest:

- Without this, even a correctly parsed ELF64 binary cannot be mapped into a
  realistic x86_64 address space.

### 2. 64-bit CPU state

Still missing:

- Make `rax`..`r15`, `rip`, and widened `rflags` the live execution register
  file instead of the current staged storage synchronized from the compat path.
- Keep the existing i386 aliases working on top of that widened state.
- Keep legacy 8/16/32-bit register aliases working on top of the widened
  register file.
- Fix all offset-sensitive host glue that depends on the current `struct
cpu_state` layout.

Key area:

- `emu/cpu.h`

### 3. Long-mode decoder and execution

Still missing:

- Full `REX` semantics instead of the current tested `REX.W` plus partial
  `REX.R/B` subset.
- Long-mode operand-size and address-size semantics.
- Complete 64-bit ModRM/SIB behavior beyond the fixed and currently tested
  `r12`/`REX.X`, no-index, RIP-relative, and low-address cases.
- Audit of implicit `eip`/`esp`/32-bit stack assumptions in execution helpers.

Key areas:

- `emu/decode.h`
- `emu/modrm.h`
- execution helpers under `emu/`

### 4. x86_64 syscall ABI

Still missing:

- Complete x86_64 syscall coverage for ABI-sensitive calls such as `mmap`,
  `clone`, `rt_sig*`, and other syscalls whose calling convention or
  userspace data layout differs from i386.
- Add guest-ABI-specific user struct layouts where the current code still
  exposes i386 formats.
- Review i386-only interfaces that do not make sense in x86_64 mode.

Key areas:

- `kernel/calls.c`
- syscall-facing ABI structs across `kernel/` and `fs/`

### 5. x86_64 process startup, TLS, and VDSO

Still missing:

- x86_64 VDSO build and symbol plumbing.
- Broader interpreter and dynamic-linker startup compatibility beyond the
  current Alpine busybox command-smoke paths.
- Broader startup compatibility once guest code relies on the remaining
  unimplemented long-mode instruction and syscall surfaces.

Important note:

- Static ELF64 smoke binaries can now start.
- Dynamic interpreter handoff works for a synthetic smoke test and now also for
  basic real Alpine busybox command smokes. A general-purpose interactive
  x86_64 shell/rootfs session is still not proven.

### 6. Signals, ptrace, and debugging surfaces

Still missing:

- x86_64 signal frame layout.
- x86_64 `ucontext`/`sigcontext`.
- x86_64 ptrace register transfer logic.
- procfs/debug output that can show 64-bit guest state correctly.

### 7. Tests and validation

Still missing:

- Decoder tests for `REX` and 64-bit register encoding.
- Loader/startup tests for ELF64.
- TLS tests for `arch_prctl`.
- Signal/ptrace regression tests for x86_64.
- End-to-end boot of an x86_64 rootfs into a usable shell.

## Current intentional limitations

These are not accidental bugs in the current branch; they are guardrails while
the port is still incomplete.

- Selecting `x86_64` does not mean an arbitrary x86_64 userspace can already
  boot.
- ELF64 startup now reaches user code for simple static and minimal compiled C
  smoke binaries, plus Alpine dynamic busybox `echo`, `sh -c`, and `uname`
  smokes, but not a validated interactive x86_64 rootfs yet.
- The runtime now carries 64-bit-capable guest addresses internally, but the
  process bootstrap and live CPU/decode path are not yet in long-mode shape.
- `arch_prctl` can now store/report FS/GS base state and the first long-mode
  memory path consumes it, but TLS-heavy dynamic loader behavior is not yet
  validated.
- The x86_64 syscall table is intentionally partial; `mmap`, `clone`, signal
  and signal ABI entry/return are only partially adapted, and layout-sensitive
  calls are still missing broader 64-bit support.
- x86_64 syscall numbers `13` and `14` now dispatch, but real signal-heavy
  workloads remain expected to fail until the x86_64 signal ABI is implemented.
  Custom x86_64 handlers are suppressed for now rather than delivered with an
  invalid i386-shaped frame.

## Recommended next order

The next safest order remains:

1. Make the widened 64-bit register file the live execution state.
2. Finish long-mode decode and execution support.
3. Complete x86_64 syscall coverage for ABI-sensitive and truly 64-bit calls.
4. Import/validate a real x86_64 rootfs shell and finish the dynamic-startup
   pieces it exposes, such as x86_64 VDSO and signal ABI surfaces.
5. Finish x86_64 signals/ptrace surfaces.
6. Add targeted tests and an end-to-end x86_64 rootfs boot.

## Definition of done

This port is only "done" when an x86_64 userspace can:

- load an ELF64 binary,
- map memory in realistic x86_64 ranges,
- start with the right stack and auxv,
- perform normal syscalls,
- use TLS,
- receive and return from signals,
- and be inspected by ptrace/debug tooling,

without regressing the existing i386 path during the transition.
