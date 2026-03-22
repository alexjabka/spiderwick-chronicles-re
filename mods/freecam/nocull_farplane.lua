-- ============================================================
-- nocull_farplane.lua  —  Anti-Room-Culling + Far Plane Override
-- Spiderwick Chronicles
-- ============================================================
-- Usage (CE Lua console):
--   nocull_enable()       enable anti-cull + far plane
--   nocull_disable()      restore everything
--   nocull_toggle()       toggle on/off
--   nocull_setfar(N)      change far plane distance (default 50000)
--
-- How it works:
--   1. NOPs call to PerformRoomCulling (sub_564950) at +88B69
--      → rooms stop being hidden per-frame
--   2. Freezes sector bitmask at 0x01340080 = 0xFFFFFFFF
--      → all sectors stay loaded in memory
--   3. Hooks SetProjection (sub_50B760) to override far plane arg
--      → distant geometry stops being clipped
-- ============================================================

local base = getAddress("Spiderwick.exe")
if not base then
    print("[NoCull] ERROR: Spiderwick.exe not found!")
    return
end

-- ============================================================
-- Config
-- ============================================================
local FAR_PLANE_DEFAULT = 50000.0
local BITMASK_ADDR      = 0x01340080

-- Patch sites
local CULL_CALL_ADDR = base + 0x88B69   -- call PerformRoomCulling (5 bytes)
local SETPROJ_ADDR   = base + 0x0B760   -- SetProjection entry (7 bytes: fld+sub esp)
local SETPROJ_RESUME = base + 0x0B767   -- continue after hook

-- ============================================================
-- State
-- ============================================================
local _nc = {
    active       = false,
    orig_cull    = nil,     -- saved 5 bytes at culling call
    orig_proj    = nil,     -- saved 7 bytes at SetProjection
    bitmask_tmr  = nil,     -- timer for bitmask freeze
    far_alloc    = nil,     -- allocated float for g_noCullFarPlane
    hook_ok      = false,   -- whether AA hook installed successfully
}

-- ============================================================
-- Enable
-- ============================================================
function nocull_enable()
    if _nc.active then
        print("[NoCull] Already active.")
        return
    end

    -- ---- 1. NOP the PerformRoomCulling call (5 bytes) ----
    _nc.orig_cull = readBytes(CULL_CALL_ADDR, 5, true)
    writeBytes(CULL_CALL_ADDR, 0x90, 0x90, 0x90, 0x90, 0x90)

    -- ---- 2. Freeze sector bitmask = 0xFFFFFFFF ----
    writeInteger(BITMASK_ADDR, 0xFFFFFFFF)
    _nc.bitmask_tmr = createTimer(nil, false)
    _nc.bitmask_tmr.Interval = 100
    _nc.bitmask_tmr.OnTimer = function()
        writeInteger(BITMASK_ADDR, 0xFFFFFFFF)
    end
    _nc.bitmask_tmr.Enabled = true

    -- ---- 3. Hook SetProjection to override far plane ----
    _nc.far_alloc = allocateMemory(4)
    writeFloat(_nc.far_alloc, FAR_PLANE_DEFAULT)
    registerSymbol("g_noCullFarPlane", _nc.far_alloc)

    _nc.orig_proj = readBytes(SETPROJ_ADDR, 7, true)

    -- Hook first 7 bytes of SetProjection:
    --   Original:  fld [esp+04]  /  sub esp, 10h   (D9 44 24 04 83 EC 10)
    --   Replaced:  jmp SetProjHook / nop / nop
    --
    -- Stack after sub esp,10h:
    --   [esp+14] = fov,  [esp+18] = aspect,  [esp+1C] = near,  [esp+20] = far
    -- After push eax:  far is at [esp+24]
    local aaEnable = string.format([=[
alloc(SetProjHook, 128)
registersymbol(SetProjHook)

SetProjHook:
  fld dword ptr [esp+04]
  sub esp, 10
  push eax
  mov eax, [g_noCullFarPlane]
  mov [esp+24], eax
  pop eax
  jmp %X

%X:
  jmp SetProjHook
  nop
  nop
]=], SETPROJ_RESUME, SETPROJ_ADDR)

    _nc.hook_ok = autoAssemble(aaEnable)

    _nc.active = true

    if _nc.hook_ok then
        print(string.format("[NoCull] ENABLED — culling NOPed, bitmask frozen, far=%.0f",
            FAR_PLANE_DEFAULT))
    else
        print("[NoCull] ENABLED — culling NOPed, bitmask frozen, far plane hook FAILED")
    end
end

-- ============================================================
-- Disable
-- ============================================================
function nocull_disable()
    if not _nc.active then
        print("[NoCull] Not active.")
        return
    end

    -- ---- 1. Restore culling call ----
    if _nc.orig_cull then
        for i, b in ipairs(_nc.orig_cull) do
            writeBytes(CULL_CALL_ADDR + i - 1, b)
        end
        _nc.orig_cull = nil
    end

    -- ---- 2. Stop bitmask freeze ----
    if _nc.bitmask_tmr then
        _nc.bitmask_tmr.Enabled = false
        _nc.bitmask_tmr.destroy()
        _nc.bitmask_tmr = nil
    end

    -- ---- 3. Restore SetProjection hook ----
    if _nc.hook_ok and _nc.orig_proj then
        for i, b in ipairs(_nc.orig_proj) do
            writeBytes(SETPROJ_ADDR + i - 1, b)
        end
        autoAssemble([[
dealloc(SetProjHook)
unregistersymbol(SetProjHook)
]])
        _nc.hook_ok = false
    end
    _nc.orig_proj = nil

    if _nc.far_alloc then
        unregisterSymbol("g_noCullFarPlane")
        deAllocateMemory(_nc.far_alloc)
        _nc.far_alloc = nil
    end

    _nc.active = false
    print("[NoCull] DISABLED — all patches restored.")
end

-- ============================================================
-- Helpers
-- ============================================================
function nocull_toggle()
    if _nc.active then nocull_disable() else nocull_enable() end
end

function nocull_setfar(dist)
    if _nc.far_alloc then
        writeFloat(_nc.far_alloc, dist)
    end
    FAR_PLANE_DEFAULT = dist
    print(string.format("[NoCull] Far plane = %.0f", dist))
end

-- ============================================================
print("[NoCull] Loaded. Commands:")
print("  nocull_enable()     — enable anti-cull + far plane")
print("  nocull_disable()    — restore original behavior")
print("  nocull_toggle()     — toggle on/off")
print("  nocull_setfar(N)    — set far plane (default 50000)")
