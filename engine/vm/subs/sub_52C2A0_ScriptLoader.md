# sub_52C2A0 -- Script Loader (Edit and Continue)

| Field | Value |
|-------|-------|
| Address | 0x52C2A0 |
| Size | 382 bytes (0x17E) |
| Prototype | `char __cdecl ScriptLoader(const char* filename)` |
| Category | VM Initialization |

---

## Purpose

Loads a compiled Kallis script file (SCT format, version 13) for "Edit and Continue" mode. Validates the file, parses the header, sets up all VM table pointers, and initializes the string resolver.

---

## Loading Sequence

```c
char ScriptLoader(const char* filename) {
    // 1. Open file
    int file = sub_4E5BE0(filename);
    if (!file) return 0;

    // 2. Allocate memory
    E561C0 = 0x100000;
    sub_4DE2A0(0x4000000, &E561C0, "Edit and Continue");
    int buf = sub_4DE530(0x80000, 128);  // 512KB, aligned

    // 3. Read and validate header (52 bytes)
    sub_4E5CF0(file, buf, 52);
    if (buf[0]!='S' || buf[1]!='C' || buf[2]!='T' || buf[3]!=0) return 0;
    if (*(int*)(buf+4) != 13) {
        printf("Incorrect script version. Got %d, expected %d", version, 13);
        return 0;
    }
    if (!(*(byte*)(buf+46) & 2)) {
        printf("This script was not built for edit and continue!");
        return 0;
    }

    // 4. Read rest of file
    sub_4E5CF0(file, buf+52, *(int*)(buf+8) - 52);
    sub_4E5CA0(&file);  // Close

    // 5. Set up string table (must be NTV)
    E561DC = buf + *(int*)(buf+12);
    if (!sub_52BE50()) return 0;  // Validate "NTV\0"

    // 6. Set up function table
    E561FC = buf + *(int*)(buf+16);

    // 7. Set up class table (must be VTL)
    E561E0 = buf + *(int*)(buf+20);
    if (!sub_52BF80(E561E0)) return 0;  // Validate "VTL\0"

    // 8. Fixup function table (add base address)
    sub_52BFA0(buf);

    // 9. Fixup type list
    int typeList = buf + *(int*)(buf+28);
    sub_52BFD0(buf);

    // 10. Copy vtable pointers from new type list to existing types
    for (int i = 0; i < *E561E8; i++) {
        *(E561E8[1+i] + 20) = *(typeList[1+i] + 20);
    }

    // 11. Set up string resolver
    E561E4 = buf + *(int*)(buf+24);
    sub_58C0E0();
    sub_58C100(buf);

    printf("Loaded %s for edit and continue", filename);
    return 1;
}
```

---

## Error Messages

| Condition | Message |
|-----------|---------|
| Wrong version | `"{gSCRIPT}: {rERROR}: Incorrect script version. Got %d, expected %d"` |
| No E&C flag | `"{gSCRIPT}: {rERROR}: This script was not built for edit and continue!"` |
| Success | `"{gSCRIPT}: Loaded %s for edit and continue"` |

---

## Related

- [VM_BYTECODE_FORMAT.md](../VM_BYTECODE_FORMAT.md) -- SCT file format
- [VM_FUNCTION_TABLE.md](../VM_FUNCTION_TABLE.md) -- Function table structure
