# sub_52CE40 -- PushObjRef

**Address:** `0x52CE40` (`Spiderwick.exe+0ECE40`) | **Calling convention:** cdecl

## Purpose

Pushes an object reference onto the Kallis VM **return stack**. Takes a native object pointer, reads its VM handle field at offset `+0xA8`, and encodes it as a relative offset from the object reference base (`dword_E56160`). This is the inverse of `PopObjRef` (sub_52C860).

## Decompiled

```c
int __cdecl PushObjRef(int obj) {
    if (obj != 0 && *(int *)(obj + 0xA8) != 0) {
        // Encode: handle - base = relative reference
        int ref = *(int *)(obj + 0xA8) - dword_E56160;
        *(int *)dword_E5616C = ref;
        dword_E5616C += 4;
        return ref;
    } else {
        // Null object or null handle: push 0
        *(int *)dword_E5616C = 0;
        dword_E5616C += 4;
        return 0;
    }
}
```

## Disassembly

```asm
52CE40  mov     eax, [esp+arg_0]        ; eax = obj pointer
52CE44  test    eax, eax                ; null check
52CE46  jz      short loc_52CE68        ; jump if null
52CE48  mov     eax, [eax+0A8h]         ; eax = *(obj + 0xA8) — VM handle
52CE4E  test    eax, eax                ; handle null check
52CE50  jz      short loc_52CE68        ; jump if null handle
; --- non-null path ---
52CE52  sub     eax, dword_E56160       ; eax = handle - object_ref_base
52CE58  mov     ecx, dword_E5616C       ; ecx = return stack pointer
52CE5E  mov     [ecx], eax              ; *return_stack_ptr = encoded ref
52CE60  add     dword_E5616C, 4         ; advance return stack pointer
52CE67  retn
; --- null path ---
52CE68  mov     edx, dword_E5616C       ; edx = return stack pointer
52CE6E  mov     dword ptr [edx], 0      ; *return_stack_ptr = 0
52CE74  add     dword_E5616C, 4         ; advance return stack pointer
52CE7B  retn
```

## Key Addresses

| Address | Description |
|---------|-------------|
| `0x52CE40` | Function entry point |
| `0xE5616C` | `dword_E5616C` -- return stack pointer (grows upward) |
| `0xE56160` | `dword_E56160` -- object reference resolution base |

## Notes

- Offset `+0xA8` (168 decimal) on the object is the **VM handle field**. This is a pointer/address that the VM uses to identify the object. Not all objects have valid handles -- the null check ensures safety.
- The encoding formula: `ref = *(obj + 0xA8) - dword_E56160` is the exact inverse of PopObjRef's resolution: `ptr = dword_E56160 + 4 * (ref / 4)`.
- Both null object pointers and objects with null handles push `0` to the return stack (representing a null reference in script space).
- The return stack grows upward, same as `PushResult`.
- Used by `sauResolvePlayer` (sub_493A80) to return the resolved player character object to script code.

## Called By

- `sub_493A80` (sauResolvePlayer handler) -- pushes the resolved player object
- Any native handler that returns an object reference to Kallis script code

## Related

- [sub_52C860_PopObjRef.md](sub_52C860_PopObjRef.md) -- PopObjRef (inverse: pop + resolve)
- [sub_52CC70_PushResult.md](sub_52CC70_PushResult.md) -- PushResult (also pushes to return stack, but bool)
- [sub_52C610_PopArg.md](sub_52C610_PopArg.md) -- PopArg (pops from arg stack)
- [../VM_STACK_OPERATIONS.md](../VM_STACK_OPERATIONS.md) -- Stack operations overview
- [../KALLIS_VM.md](../KALLIS_VM.md) -- VM architecture overview
