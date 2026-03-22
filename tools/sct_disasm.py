#!/usr/bin/env python3
"""
SCT Bytecode Disassembler for the Kallis VM (Stormfront Studios' Ogre engine)
Supports SCT format version 13.

Usage: python sct_disasm.py <sct_file> [options]
  --func <index>    Disassemble only function at given index
  --raw             Show raw bytes alongside disassembly
  --no-header       Skip printing header/table info
"""

import struct
import sys
import os
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Tuple

# ---------------------------------------------------------------------------
# Opcode definitions
# ---------------------------------------------------------------------------

# Instruction sizes: number of bytes *including* the opcode byte
#   1  = opcode only
#   3  = opcode + 2-byte operand (u16 / s16)
#   5  = opcode + 4-byte operand (two u16, or one u32)
OPCODES = {
    0x00: ("RET",               5),
    0x01: ("CALL_SCRIPT",       5),
    0x02: ("CALL_NATIVE",       5),
    0x03: ("CALL_METHOD",       5),
    0x04: ("JUMP",              3),
    0x05: ("JUMP_IF_FALSE",     3),
    0x06: ("JUMP_IF_TRUE",      3),
    0x07: ("LOAD",              3),
    0x08: ("STORE",             3),
    0x09: ("PUSH_NULL",         1),
    0x0A: ("PUSH_1",            1),
    0x0B: ("POP",               1),
    0x0C: ("POP_N",             3),
    0x0D: ("PUSH_N",            3),
    0x0E: ("NEG",               1),
    0x0F: ("NOT",               1),
    0x10: ("ADD_INT",           1),
    0x11: ("SUB_INT",           1),
    0x12: ("MUL_INT",           1),
    0x13: ("DIV_INT",           1),
    0x14: ("MOD_INT",           1),
    0x15: ("AND",               1),
    0x16: ("OR",                1),
    0x17: ("LT_INT",           1),
    0x18: ("GT_INT",           1),
    0x19: ("LE_INT",           1),
    0x1A: ("GE_INT",           1),
    0x1B: ("EQ_INT",           1),
    0x1C: ("NE_INT",           1),
    0x1D: ("NEG_FLOAT",        1),
    0x1E: ("ADD_FLOAT",        1),
    0x1F: ("SUB_FLOAT",        1),
    0x20: ("MUL_FLOAT",        1),
    0x21: ("DIV_FLOAT",        1),
    0x22: ("LT_FLOAT",        1),
    0x23: ("GT_FLOAT",        1),
    0x24: ("LE_FLOAT",        1),
    0x25: ("GE_FLOAT",        1),
    0x26: ("EQ_FLOAT",        1),
    0x27: ("NE_FLOAT",        1),
    0x28: ("INT_TO_FLOAT",     1),
    0x29: ("FLOAT_TO_INT",     1),
    0x2A: ("PUSH_IMM",         5),
    0x2B: ("EQ_STRING",        1),
    0x2C: ("COPY",             5),
    0x2D: ("DUP",              1),
    0x2E: ("ADDR_CALC",        3),
    0x2F: ("DEREF",            1),
    0x30: ("STORE_DEREF",      1),
    0x31: ("EXCEPTION_HANDLER",3),
    0x32: ("CLEAR_EXCEPTION",  1),
    0x33: ("EXCEPTION_HANDLED",1),
    0x34: ("SET_DEBUG",        5),
    0x35: ("CALL_VIRTUAL",     5),
    0x36: ("TRACE",            5),
    0x37: ("SAVE_STATE",       5),
    0x38: ("NOP",              1),
    0x39: ("NOP_39",           1),
    0x3A: ("NOP_3A",           1),
    0x3B: ("NOP_3B",           1),
    0x3C: ("NOP_3C",           1),
    0x3D: ("NOP_3D",           1),
    0x3E: ("NOP_3E",           1),
    0x3F: ("NOP_3F",           1),
}

BANK_NAMES = ["global", "frame", "bank2", "bank3"]

# ---------------------------------------------------------------------------
# Data structures
# ---------------------------------------------------------------------------

@dataclass
class SCTHeader:
    magic: bytes
    version: int
    file_size: int
    ntv_offset: int
    func_table_offset: int
    vtl_offset: int
    string_resolve_offset: int
    type_list_offset: int
    save_data_offset: int
    # remaining header bytes (offsets 0x24..0x33) stored raw
    extra: bytes = b""


@dataclass
class StringResolveEntry:
    name_id: int
    string_offset: int
    name: str = ""


@dataclass
class FunctionEntry:
    index: int
    offset: int          # byte offset in file to function bytecode area
    name: str = ""       # resolved name if available


@dataclass
class VTLClass:
    index: int
    raw: bytes = b""
    name: str = ""


# ---------------------------------------------------------------------------
# Parsing helpers
# ---------------------------------------------------------------------------

def read_u8(data: bytes, off: int) -> int:
    return data[off]

def read_u16(data: bytes, off: int) -> int:
    return struct.unpack_from("<H", data, off)[0]

def read_s16(data: bytes, off: int) -> int:
    return struct.unpack_from("<h", data, off)[0]

def read_u32(data: bytes, off: int) -> int:
    return struct.unpack_from("<I", data, off)[0]

def read_s32(data: bytes, off: int) -> int:
    return struct.unpack_from("<i", data, off)[0]

def read_f32(data: bytes, off: int) -> float:
    return struct.unpack_from("<f", data, off)[0]

def read_cstring(data: bytes, off: int) -> str:
    end = data.index(b"\x00", off)
    return data[off:end].decode("ascii", errors="replace")


# ---------------------------------------------------------------------------
# SCT Parser
# ---------------------------------------------------------------------------

class SCTFile:
    def __init__(self, path: str):
        with open(path, "rb") as f:
            self.data = f.read()
        self.path = path
        self.header: Optional[SCTHeader] = None
        self.ntv_strings: List[str] = []           # indexed by ID
        self.string_resolve: List[StringResolveEntry] = []
        self.name_id_to_string: Dict[int, str] = {}
        self.functions: List[FunctionEntry] = []
        self.func_flags: int = 0
        self.vtl_classes: List[VTLClass] = []

    # -- header ----------------------------------------------------------

    def parse_header(self):
        d = self.data
        if len(d) < 52:
            raise ValueError(f"File too small for SCT header ({len(d)} bytes)")
        magic = d[0:4]
        if magic != b"SCT\x00":
            raise ValueError(f"Bad magic: {magic!r} (expected b'SCT\\x00')")
        self.header = SCTHeader(
            magic=magic,
            version=read_u32(d, 0x04),
            file_size=read_u32(d, 0x08),
            ntv_offset=read_u32(d, 0x0C),
            func_table_offset=read_u32(d, 0x10),
            vtl_offset=read_u32(d, 0x14),
            string_resolve_offset=read_u32(d, 0x18),
            type_list_offset=read_u32(d, 0x1C),
            save_data_offset=read_u32(d, 0x20),
            extra=d[0x24:0x34],
        )
        if self.header.version != 13:
            print(f"WARNING: expected version 13, got {self.header.version}")

    # -- NTV string table ------------------------------------------------

    def parse_ntv(self):
        d = self.data
        off = self.header.ntv_offset
        magic = d[off:off+4]
        if magic != b"NTV\x00":
            raise ValueError(f"Bad NTV magic at 0x{off:X}: {magic!r}")
        count = read_u32(d, off + 4)
        pos = off + 8
        self.ntv_strings = []
        for _ in range(count):
            s = read_cstring(d, pos)
            self.ntv_strings.append(s)
            pos += len(s) + 1  # skip null terminator

    # -- String resolve table --------------------------------------------

    def parse_string_resolve(self):
        d = self.data
        off = self.header.string_resolve_offset
        if off == 0:
            return
        # Determine the extent: read pairs until we hit a boundary.
        # We know other tables' offsets, so find the next table after this one.
        boundaries = [len(d)]
        for o in [self.header.ntv_offset, self.header.func_table_offset,
                   self.header.vtl_offset, self.header.type_list_offset,
                   self.header.save_data_offset]:
            if o > off:
                boundaries.append(o)
        end = min(boundaries)

        self.string_resolve = []
        pos = off
        while pos + 8 <= end:
            name_id = read_u32(d, pos)
            str_off = read_u32(d, pos + 4)
            if str_off == 0 and name_id == 0 and pos > off:
                # Likely end-of-table sentinel
                break
            entry = StringResolveEntry(name_id=name_id, string_offset=str_off)
            if 0 < str_off < len(d):
                try:
                    entry.name = read_cstring(d, str_off)
                except (ValueError, IndexError):
                    entry.name = f"<bad_str@0x{str_off:X}>"
            self.string_resolve.append(entry)
            pos += 8

        # Build lookup
        self.name_id_to_string = {}
        for e in self.string_resolve:
            self.name_id_to_string[e.name_id] = e.name

    # -- Function table --------------------------------------------------

    def parse_functions(self):
        d = self.data
        off = self.header.func_table_offset
        self.func_flags = read_u32(d, off)
        count = read_u32(d, off + 4)
        self.functions = []
        for i in range(count):
            entry_off = read_u32(d, off + 8 + i * 4)
            fe = FunctionEntry(index=i, offset=entry_off)
            self.functions.append(fe)

    # -- VTL class table -------------------------------------------------

    def parse_vtl(self):
        d = self.data
        off = self.header.vtl_offset
        if off == 0:
            return
        magic = d[off:off+4]
        if magic != b"VTL\x00":
            print(f"WARNING: bad VTL magic at 0x{off:X}: {magic!r}")
            return
        count = read_u32(d, off + 4)
        # VTL entries are variable-length; just note count for now
        self.vtl_classes = []
        # We won't deeply parse VTL in this version; just record count
        for i in range(count):
            self.vtl_classes.append(VTLClass(index=i))

    # -- Main parse ------------------------------------------------------

    def parse(self):
        self.parse_header()
        self.parse_ntv()
        self.parse_string_resolve()
        self.parse_functions()
        self.parse_vtl()

    # -- Resolve function names ------------------------------------------

    def resolve_func_names(self):
        """Try to give functions human-readable names from the string resolve table."""
        for fe in self.functions:
            if fe.index in self.name_id_to_string:
                fe.name = self.name_id_to_string[fe.index]

    # -- NTV lookup ------------------------------------------------------

    def ntv_name(self, idx: int) -> str:
        if 0 <= idx < len(self.ntv_strings):
            return self.ntv_strings[idx]
        return f"ntv#{idx}"

    def resolve_name(self, name_id: int) -> str:
        if name_id in self.name_id_to_string:
            return self.name_id_to_string[name_id]
        return f"nameId#{name_id}"


# ---------------------------------------------------------------------------
# Disassembler
# ---------------------------------------------------------------------------

class Disassembler:
    def __init__(self, sct: SCTFile, show_raw: bool = False):
        self.sct = sct
        self.data = sct.data
        self.show_raw = show_raw

    def _format_raw(self, data_slice: bytes) -> str:
        return " ".join(f"{b:02X}" for b in data_slice)

    def _format_float(self, raw32: int) -> str:
        """Format a 32-bit value as float if it looks reasonable."""
        bs = struct.pack("<I", raw32 & 0xFFFFFFFF)
        fval = struct.unpack("<f", bs)[0]
        return f"{fval:g}"

    def disasm_function(self, func: FunctionEntry, end_offset: int) -> List[str]:
        """
        Disassemble one function.
        func.offset is the file offset to the function's area.
        The first 12 bytes are the function header; bytecode starts at +12.
        end_offset is the first byte past this function's bytecode.
        """
        d = self.data
        base = func.offset
        if base + 12 > len(d):
            return [f"  ; ERROR: function offset 0x{base:X} out of bounds"]

        # Read 12-byte function header
        hdr = d[base:base+12]
        hdr_fields = struct.unpack_from("<III", hdr, 0)

        lines: List[str] = []
        lines.append(f"  ; header: [{self._format_raw(hdr)}]  "
                      f"({hdr_fields[0]:#x}, {hdr_fields[1]:#x}, {hdr_fields[2]:#x})")

        code_start = base + 12
        pc = code_start
        limit = min(end_offset, len(d))

        # Collect jump targets for labeling
        jump_targets = set()
        # First pass: find targets
        scan = code_start
        while scan < limit:
            if scan >= len(d):
                break
            raw_byte = d[scan]
            opcode_id = raw_byte >> 2
            if opcode_id not in OPCODES:
                scan += 1
                continue
            _, size = OPCODES[opcode_id]
            if scan + size > limit:
                break
            if opcode_id in (0x04, 0x05, 0x06, 0x31):  # JUMP, JIF, JIT, EXCEPTION_HANDLER
                if scan + 3 <= limit:
                    off16 = read_s16(d, scan + 1)
                    target = scan + off16
                    if code_start <= target < limit:
                        jump_targets.add(target)
            scan += size

        # Second pass: disassemble
        while pc < limit:
            if pc >= len(d):
                break

            raw_byte = d[pc]
            opcode_id = raw_byte >> 2
            bank = raw_byte & 3

            if opcode_id not in OPCODES:
                lines.append(f"  0x{pc - code_start:04X}:  [{raw_byte:02X}]  ; UNKNOWN opcode {opcode_id:#x}")
                pc += 1
                continue

            mnem, size = OPCODES[opcode_id]

            # Safety: check we have enough bytes
            if pc + size > limit:
                lines.append(f"  0x{pc - code_start:04X}:  ; TRUNCATED (need {size} bytes)")
                break

            raw_bytes = d[pc:pc+size]

            # Label if jump target
            label = ""
            if pc in jump_targets:
                label = f"L_{pc - code_start:04X}:\n"

            # Build operand string
            operand = ""
            bank_str = f"  ; bank={BANK_NAMES[bank]}" if bank != 0 else ""

            if size == 1:
                # No operands beyond the opcode byte
                if bank != 0 and opcode_id not in (0x09, 0x0A, 0x0B, 0x0E, 0x0F,
                    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
                    0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23,
                    0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2B, 0x2D, 0x2F, 0x30,
                    0x32, 0x33, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F):
                    operand = bank_str
                pass

            elif size == 3:
                val16 = read_u16(d, pc + 1)
                sval16 = read_s16(d, pc + 1)

                if opcode_id in (0x04, 0x05, 0x06):
                    # Jump: signed offset from instruction start
                    target = pc + sval16
                    target_rel = target - code_start
                    operand = f"L_{target_rel:04X}  ; offset {sval16:+d} -> file 0x{target:X}"
                elif opcode_id == 0x31:
                    # Exception handler
                    target = pc + sval16
                    target_rel = target - code_start
                    operand = f"L_{target_rel:04X}  ; handler at offset {sval16:+d}"
                elif opcode_id in (0x07, 0x08):
                    # LOAD / STORE: bank-relative slot
                    operand = f"[{BANK_NAMES[bank]}+{val16}]  ; slot {val16}"
                elif opcode_id == 0x2E:
                    # ADDR_CALC
                    operand = f"[{BANK_NAMES[bank]}+{val16}]"
                elif opcode_id in (0x0C, 0x0D):
                    # POP_N / PUSH_N
                    operand = f"{val16}"
                else:
                    operand = f"{val16}"

            elif size == 5:
                lo16 = read_u16(d, pc + 1)
                hi16 = read_u16(d, pc + 3)
                imm32 = read_u32(d, pc + 1)

                if opcode_id == 0x00:
                    # RET: popCount, retCount
                    operand = f"pop={lo16}, ret={hi16}"

                elif opcode_id == 0x01:
                    # CALL_SCRIPT: funcIdx, selfRef
                    fname = ""
                    if 0 <= lo16 < len(self.sct.functions):
                        fe = self.sct.functions[lo16]
                        if fe.name:
                            fname = fe.name
                        else:
                            fname = f"func_{lo16}"
                    else:
                        fname = f"func_{lo16}"
                    operand = f"{fname}  ; funcIdx={lo16}, selfRef={hi16}"

                elif opcode_id == 0x02:
                    # CALL_NATIVE: funcIdx, argCount
                    nname = self.sct.resolve_name(lo16)
                    operand = f"{nname}  ; nativeIdx={lo16}, args={hi16}"

                elif opcode_id == 0x03:
                    # CALL_METHOD: methodIdx, selfRef
                    mname = self.sct.resolve_name(lo16)
                    operand = f"{mname}  ; methodIdx={lo16}, selfRef={hi16}"

                elif opcode_id == 0x2A:
                    # PUSH_IMM: 32-bit immediate
                    fstr = self._format_float(imm32)
                    sval = read_s32(d, pc + 1)
                    operand = f"0x{imm32:08X}  ; int={sval}, float={fstr}"

                elif opcode_id == 0x2C:
                    # COPY
                    operand = f"lo={lo16}, hi={hi16}"

                elif opcode_id == 0x34:
                    # SET_DEBUG
                    operand = f"0x{imm32:08X}"

                elif opcode_id == 0x35:
                    # CALL_VIRTUAL: vtableIdx, classIdx, ...
                    operand = f"vtable={lo16}, class={hi16}"

                elif opcode_id == 0x36:
                    # TRACE
                    operand = f"0x{imm32:08X}"

                elif opcode_id == 0x37:
                    # SAVE_STATE
                    operand = f"lo={lo16}, hi={hi16}"

                else:
                    operand = f"0x{imm32:08X}"

            # Assemble line
            prefix = f"  0x{pc - code_start:04X}:"
            raw_str = ""
            if self.show_raw:
                raw_str = f"  [{self._format_raw(raw_bytes)}]"
                raw_str = raw_str.ljust(20)

            # Show bank for LOAD/STORE even on 3-byte ops (already included)
            line = f"{label}{prefix}{raw_str}  {mnem:<22s}{operand}"
            lines.append(line)

            pc += size

        return lines

    def disassemble_all(self, func_filter: Optional[int] = None) -> str:
        """Disassemble all (or one) functions, return formatted text."""
        out: List[str] = []

        # Determine function end offsets by sorting and using next function's start
        sorted_funcs = sorted(self.sct.functions, key=lambda f: f.offset)
        func_ends: Dict[int, int] = {}

        # Build a list of all known section starts that can bound a function
        section_starts = sorted(set(
            [f.offset for f in self.sct.functions] +
            [o for o in [
                self.sct.header.ntv_offset,
                self.sct.header.func_table_offset,
                self.sct.header.vtl_offset,
                self.sct.header.string_resolve_offset,
                self.sct.header.type_list_offset,
                self.sct.header.save_data_offset,
                len(self.data),
            ] if o > 0]
        ))

        for i, sf in enumerate(sorted_funcs):
            # End = next boundary after this function's offset
            end = len(self.data)
            for s in section_starts:
                if s > sf.offset:
                    end = s
                    break
            # But if next function in sorted order is closer, use that
            if i + 1 < len(sorted_funcs):
                next_off = sorted_funcs[i + 1].offset
                if next_off < end:
                    end = next_off
            func_ends[sf.offset] = end

        # Disassemble
        for func in self.sct.functions:
            if func_filter is not None and func.index != func_filter:
                continue

            end = func_ends.get(func.offset, len(self.data))
            name = func.name if func.name else f"func_{func.index}"
            out.append(f"\n; ===== Function {func.index}: {name} (file offset 0x{func.offset:X}) =====")
            lines = self.disasm_function(func, end)
            out.extend(lines)

        return "\n".join(out)


# ---------------------------------------------------------------------------
# Pretty-print helpers
# ---------------------------------------------------------------------------

def print_header(sct: SCTFile):
    h = sct.header
    print("=" * 72)
    print(f"SCT File: {sct.path}")
    print(f"  Version:              {h.version}")
    print(f"  File size:            {h.file_size} (0x{h.file_size:X})")
    print(f"  NTV string table:     0x{h.ntv_offset:X}")
    print(f"  Function table:       0x{h.func_table_offset:X}")
    print(f"  VTL class table:      0x{h.vtl_offset:X}")
    print(f"  String resolve table: 0x{h.string_resolve_offset:X}")
    print(f"  Type list:            0x{h.type_list_offset:X}")
    print(f"  Save data:            0x{h.save_data_offset:X}")
    print(f"  Header extra:         {' '.join(f'{b:02X}' for b in h.extra)}")
    print()


def print_ntv(sct: SCTFile):
    print(f"NTV String Table ({len(sct.ntv_strings)} entries):")
    for i, s in enumerate(sct.ntv_strings):
        print(f"  [{i:4d}] {s}")
    print()


def print_string_resolve(sct: SCTFile):
    print(f"String Resolve Table ({len(sct.string_resolve)} entries):")
    for e in sct.string_resolve:
        print(f"  nameId={e.name_id:4d}  offset=0x{e.string_offset:06X}  -> \"{e.name}\"")
    print()


def print_functions(sct: SCTFile):
    print(f"Function Table (flags=0x{sct.func_flags:X}, {len(sct.functions)} entries):")
    for fe in sct.functions:
        name = fe.name if fe.name else ""
        print(f"  [{fe.index:4d}] offset=0x{fe.offset:06X}  {name}")
    print()


def print_vtl(sct: SCTFile):
    print(f"VTL Class Table ({len(sct.vtl_classes)} entries)")
    print()


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    import argparse

    parser = argparse.ArgumentParser(
        description="SCT bytecode disassembler for the Kallis VM (SCT v13)")
    parser.add_argument("sct_file", help="Path to .sct file")
    parser.add_argument("--func", type=int, default=None,
                        help="Disassemble only the function at this index")
    parser.add_argument("--raw", action="store_true",
                        help="Show raw bytes alongside disassembly")
    parser.add_argument("--no-header", action="store_true",
                        help="Skip printing header/table info")
    args = parser.parse_args()

    if not os.path.isfile(args.sct_file):
        print(f"Error: file not found: {args.sct_file}", file=sys.stderr)
        sys.exit(1)

    sct = SCTFile(args.sct_file)
    sct.parse()
    sct.resolve_func_names()

    if not args.no_header:
        print_header(sct)
        print_ntv(sct)
        print_string_resolve(sct)
        print_functions(sct)
        print_vtl(sct)

    dis = Disassembler(sct, show_raw=args.raw)
    output = dis.disassemble_all(func_filter=args.func)
    print(output)


if __name__ == "__main__":
    main()
