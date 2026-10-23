/* SPDX-License-Identifier: MPL-2.0 */
/* FIZZ's link: fm1-base's device/app.ld (ced9443) with three changes, used in its place by firmware/build.py.
 *   1. FIZZ's zero-initialised data (its units are the objects in build/o2/) spread over four regions, by unit (the
 *      compiler has no -fdata-sections: a unit's .bss is one piece):
 *        the pool          fz_port.c's (the one qd_app_t) and the effects' (qd_fx.c: the delay, the reverb, the chorus)
 *        RAM code region   after the RAM code: the voices' (quad_voice.c: the comb lines)
 *        NOINIT            after fm1-base's records: the view's and two screens' (view.c, ui_idle.c, ui_keys.c)
 *        RAM               the rest of FIZZ's, with the platform's
 *      crt0 clears only .bss and the pool: app.c clears the other two before FIZZ starts (_fz_rt_start.._fz_rt_end,
 *      _fz_ni_start.._fz_ni_end). firmware/BUDGET.md has the sizes.
 *   2. The data blob (core/qd_config.h QD_XDATA: the fonts, the TAP ape's lines; sections .qd_xdata.*) is linked at
 *      the XIP address of the data flash's second window, PLAT_XDATA1 (0xE5000..0xFBFFF: FM1_XIP(off) = 0x02000000 +
 *      off - 0x4000), read in place through the plain window main_fm1.c opens from 0x93000 up. It is not in the app
 *      image: firmware/build.py cuts it out (fizz-xdata.bin) and firmware/tools/fizz_xdata.py writes it there, with
 *      a 12-byte trailer after it (fz_port.c).
 *   3. (P2.6) Audio code in RAM: the mix (mix.c) whole, and the functions firmware/build.py renames .fzrt.* in their
 *      objects (HOT: the effects' hot seven; in the single-core build also the voices' quad_render), copied at boot
 *      with the rest of .ram_text: out of the flash cache's way (the drawing's code evicts it between halves). It runs
 *      only in the audio interrupt (and their set-up on the main loop), never while the flash is off (the flash is
 *      programmed and erased with the interrupts off), so like fm1-base's second-core code it may call into the
 *      flash: it sits in the window fm1-base's build check exempts (_c1_start.._c1_end). The single-core build links
 *      a copy of this script (build/fizz-1core.ld) with the voices' comb lines in the pool instead of .fz_rt.
 * The regions, the stacks and fm1-base's symbols are as app.ld has them. */
MEMORY
{
    XIP    (rx)  : ORIGIN = 0x02000120, LENGTH = 0x8DFBC
    RAMTEXT (rwx): ORIGIN = 0x01C00000, LENGTH = 0x8000
    RAM    (rwx) : ORIGIN = 0x01C08000, LENGTH = 96K
    POOL   (rw)  : ORIGIN = 0x01C20000, LENGTH = 0x54000
    NOINIT (rw)  : ORIGIN = 0x01C7C000, LENGTH = 0x3D50
    XDATA  (r)   : ORIGIN = 0x020E1000, LENGTH = 0x17000 - 12
}
SECTIONS
{
    .text : { KEEP(*(.text.crt0)) *(EXCLUDE_FILE(*/o2/build_units_c_mix.o) .text*) *(.rodata*)
              *(.bt_stack_code .ble_sm_code .hci_controller_code .ble_hci_code .ble_att_code .ble_db_code
                .classic_lmp_code .link_bulk_code .classic_rf_code .lmp_irq_code .ble_gatt_code .ble_ll_code
                .ble_rf_code .bt_rf_code .hci_interface_code .vendor_manager_code .device_manager_code)
              *(.*_const)
              . = ALIGN(4); _etext = .; } > XIP
    .ram_text : { _rt_start = .; KEEP(*(.ram_text*)) *(.volatile_ram_code) *(*.rf.text.cache.L2.rf_trim)
                  . = ALIGN(4); _c1_start = .; KEEP(*(.c1_text*))
                  *(.fzrt.*) */o2/build_units_c_mix.o(.text*)
                  . = ALIGN(4); _c1_end = .;
                  _rt_end = .; } > RAMTEXT AT > XIP
    _rt_load = LOADADDR(.ram_text);
    .data : { . = ALIGN(4); _data_start = .; *(.data*) *(.sdata*) *(.*_data) . = ALIGN(4); _data_end = .; } > RAM AT > XIP
    _data_load = LOADADDR(.data);
    .fz_rt (NOLOAD) : { . = ALIGN(4); _fz_rt_start = .; */o2/build_units_c_quad_voice.o(.bss* .sbss* COMMON)
                       . = ALIGN(4); _fz_rt_end = .; } > RAMTEXT
    .noinit (NOLOAD) : { *(.noinit*) } > NOINIT
    .fz_ni (NOLOAD) : { . = ALIGN(4); _fz_ni_start = .; */o2/build_units_c_view.o(.bss* .sbss* COMMON)
                        */o2/build_units_c_ui_idle.o(.bss* .sbss* COMMON) */o2/build_units_c_ui_keys.o(.bss* .sbss* COMMON)
                        . = ALIGN(4); _fz_ni_end = .; } > NOINIT
    .pool (NOLOAD) : { _pool_start = .; *(.pool*) */o2/fz_port.o(.bss* .sbss* COMMON)
                       */o2/build_units_c_qd_fx.o(.bss* .sbss* COMMON) . = ALIGN(4); _pool_end = .; } > POOL
    .bss (NOLOAD) : { _bss_start = .; *(.bss*) *(.sbss*) *(COMMON) *(.*_bss) *(.bd_base) . = ALIGN(4); _bss_end = .; } > RAM
    .qd_xdata : { _fz_xdata_start = .; *(.qd_xdata*) . = ALIGN(4); _fz_xdata_end = .; } > XDATA
    _guard0 = 0x01C74000; _ustack_lo = 0x01C74100; _ustack_top = 0x01C7A000;
    _guard1 = 0x01C7A000; _sstack_lo = 0x01C7A100; _sstack_top = 0x01C7C000;
    /DISCARD/ : { *(.comment) *(.note*) }
}
