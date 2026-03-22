# Diagnostic Tools — Approach & Inventory

## Philosophy

When reverse engineering, don't brute-force manual searches. Write **temporary
diagnostic scripts** that hook into known code paths, capture data automatically,
and self-remove. This turns hours of guesswork into seconds of automated scanning.

## Tool Inventory

### find_caller.lua — Find Who Calls a Generic Function
**Problem:** sub_5A7DC0 (generic memcpy) has 187 callers. Which one copies camera position?

**Approach:**
1. Temporarily hook sub_5A7DC0 entry
2. Check if ECX (destination) is within camera position range
3. Log the return address from the stack
4. Auto-remove hook after 3 seconds

**Result:** Found sub_4356F0 as the specific caller. Took 3 seconds instead
of manually checking 187 xrefs in IDA.

**Key technique:** The hook does NOT block the function — it logs and passes
through. Safe for production use during diagnostics.

### find_yaw.lua — Auto-Find Yaw/Pitch in Camera Struct
**Problem:** Camera yaw/pitch stored somewhere in a large object hierarchy.
Manual CE scanning is tedious and gives raw addresses (no pointer path).

**Approach:**
1. Compute camera object base from known pointer: `pCamStruct - 0x480`
2. Read component pointers from the object's component array
3. Snapshot ALL float values across the object and its components
4. Wait while user rotates camera
5. Diff snapshots — show only values that changed
6. Sort by delta magnitude — yaw/pitch will be at the top

**Key technique:** Uses knowledge of the object hierarchy (from pseudocode
analysis) to scan the RIGHT memory regions, not the entire address space.
Much faster than a blind CE scan.

## When to Write a Diagnostic Tool

- When a function has too many callers to check manually (use caller-finder)
- When you know the data EXISTS but not WHERE in a large struct (use diff-scanner)
- When timing matters (hook at the exact point in the pipeline)
- When you need data from registers/stack that aren't in persistent memory

## Template Pattern

```lua
-- 1. Hook or snapshot
-- 2. Wait (let user perform action)
-- 3. Capture result
-- 4. Clean up (remove hooks, dealloc)
-- 5. Print findings

local t = createTimer(nil)
t.Interval = 3000
t.OnTimer = function(timer)
    timer.Enabled = false
    timer.Destroy()
    -- capture, cleanup, print
end
t.Enabled = true
```

## ASM Hook Diagnostic Template

```asm
alloc(logHook, 256)
alloc(logData, 16)
registersymbol(logData)

logHook:
  // check condition
  // if match: save data to logData
  // always: execute original instructions
  jmp return_label

// Install hook
assert(target_address, original_bytes)
target_address:
  jmp logHook
return_label:
```

Key: hook does NOT skip/block the function. It's a transparent tap that
logs data and passes through. Remove after capturing.
