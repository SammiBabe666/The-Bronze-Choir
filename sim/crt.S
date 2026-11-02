/* A minimal start for bfin-elf-run: a stack, main, exit. The simulator's syscalls (libgloss convention):
 * P0 the call number, R0 a pointer to its arguments, EXCPT 0. */
    .section .text
    .global __start
__start:
    sp.h = __stack_top; sp.l = __stack_top;
    fp = sp;
    r0 = syscfg;       /* the cycle counter on (SYSCFG.CCEN), as the OP-1's OS has it */
    bitset(r0, 1);
    syscfg = r0;
    csync;
    call _main;
    [--sp] = r0;
    r0 = sp;
    p0 = 1;            /* exit */
    excpt 0;
1:  jump 1b;

    .global _sim_write
_sim_write:            /* sim_write(fd, buf, len) */
    [--sp] = r2; [--sp] = r1; [--sp] = r0;
    r0 = sp;
    p0 = 5;            /* write */
    excpt 0;
    sp += 12;
    rts;

    .section .bss
    .align 4
    .space 65536
__stack_top:
