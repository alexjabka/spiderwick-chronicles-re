# sub_52EC30 -- VMInitObject

| Field | Value |
|-------|-------|
| Address | 0x52EC30 |
| Size | 129 bytes (0x81) |
| Prototype | `int __cdecl VMInitObject(int vmTypeInfo, int nativeObject)` |
| Called by | CreateCharacter_Internal (0x44FABE) and .kallis code |
| Category | VM Object Lifecycle |

---

## Purpose

Initializes a VM object by linking it to a native C++ game object. Sets up the bidirectional pointer chain, resolves the debug name, calls the object dispatch function, marks the object as initialized, and invokes the script "Init" function.

---

## Pseudocode

```c
int __cdecl VMInitObject(int vmTypeInfo, int nativeObject) {
    // Set type tag from class vtable
    *(short*)(vmTypeInfo + 24) = *(short*)(*(vmTypeInfo + 20) + 12);

    // Link: VM -> native
    *(int*)(vmTypeInfo + 16) = nativeObject;

    // Resolve debug name
    const char* name;
    if (E561E4)
        name = sub_58C190(*(vmTypeInfo + 8));  // String resolve
    else
        name = NULL;
    *(int*)(vmTypeInfo + 12) = name;

    if (nativeObject) {
        // Link: native -> VM
        *(int*)(nativeObject + 0xA8) = vmTypeInfo;

        // Set debug name on native object
        if (E561E4)
            name = sub_58C190(*(vmTypeInfo + 8));
        sub_441140(nativeObject + 4, name);

        // Object dispatch (register with VM object manager)
        E561D4(nativeObject);
    }

    // Mark as initialized
    *(short*)(vmTypeInfo + 26) |= 0x4000;

    // Call script "Init" function
    sub_52D920(vmTypeInfo, "Init");
    return sub_52EA70();
}
```

---

## The Bidirectional Link

```
nativeObject + 0xA8 ---> vmTypeInfo
vmTypeInfo + 0x10   ---> nativeObject
```

This link is used everywhere:
- Native code gets VM object: `*(gameObj + 0xA8)`
- VM code gets native object: `*(vmObj + 16)`

---

## Object Dispatch

`E561D4` is a function pointer set to `sub_4D7790` during initialization. This function calls `sub_552800` (thunk to .kallis) which registers the native object with the VM's internal object manager.
