# sub_52CE00 -- VMReturnObjDirect

| Field | Value |
|-------|-------|
| Address | 0x52CE00 |
| Size | 50 bytes (0x32) |
| Prototype | `int __cdecl VMReturnObjDirect(int objPtr)` |
| Category | VM Stack Push |

---

## Purpose

Pushes a VM object reference onto the evaluation stack by computing a byte offset from the object base. This is the "direct" variant that takes a raw pointer to a VM object descriptor (as opposed to sub_52CE40 which goes through the +0xA8 indirection).

---

## Pseudocode

```c
int __cdecl VMReturnObjDirect(int objPtr) {
    if (objPtr) {
        int vmRef = objPtr - E56160[0];  // Byte offset from object base
        *(int*)E5616C = vmRef;
    } else {
        *(int*)E5616C = 0;  // Null reference
    }
    E5616C += 4;
    return objPtr;
}
```

---

## Encoding

The VM reference is a **signed byte offset** from `E56160[0]`. To decode back to a pointer:
```c
objPtr = E56160 + 4 * (vmRef / 4);  // Aligned to 4 bytes
```

---

## Usage

Called by handlers that already have a direct pointer to a VM object descriptor:
- `sub_40D670` -- object reference return
- `sub_492DD0`, `sub_493410`, `sub_493610` -- entity query results
- `sub_497020` -- camera/target lookups
