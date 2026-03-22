-- Debug Camera Input Controller
-- Reads mouse + WASD and writes movement values to the debug camera object.
-- Run AFTER enabling debugcam_pipeline.cea and calling dbgCamActivate.
--
-- The engine's CameraUpdate (sub_43C4E0) multiplies these by deltaTime,
-- so we write speed values (not positions).

local dbgCam = getAddress("dbgCamObj")
if not dbgCam or dbgCam == 0 then
  print("[DebugCam] ERROR: dbgCamObj not found — enable debugcam_pipeline.cea first")
  return
end

-- Movement speeds
local MOVE_SPEED = 300.0      -- forward/right/up speed
local LOOK_SPEED = 5.0        -- mouse look sensitivity
local FAST_MULT  = 3.0        -- shift multiplier

-- Mouse delta addresses (game writes these every frame)
local MOUSE_DX = 0x72FC14     -- yaw delta
local MOUSE_DY = 0x72FC10     -- pitch delta

-- Key codes (virtual key codes)
local VK_W     = 0x57
local VK_A     = 0x41
local VK_S     = 0x53
local VK_D     = 0x44
local VK_SPACE = 0x20
local VK_CTRL  = 0x11
local VK_SHIFT = 0x10
local VK_F8    = 0x77         -- toggle debug camera
local VK_MMB   = 0x04         -- middle mouse = teleport

-- Object offsets
local OFF_PITCH   = 0x14
local OFF_YAW     = 0x18
local OFF_FORWARD = 0x1C
local OFF_RIGHT   = 0x20
local OFF_UP      = 0x24
local OFF_TELEPORT = 0x28

local function isKeyDown(vk)
  return isKeyPressed(vk)
end

-- Stop any existing timer
if dbgCamTimer then
  dbgCamTimer.destroy()
  dbgCamTimer = nil
  print("[DebugCam] Previous timer stopped")
end

local active = true

dbgCamTimer = createTimer(nil, false)
dbgCamTimer.Interval = 16  -- ~60fps
dbgCamTimer.OnTimer = function(t)
  if not active then return end

  -- Check if debug camera is still registered
  local camActive = readInteger(getAddress("dbgCamActive"))
  if not camActive or camActive == 0 then return end

  -- F8 toggle
  if isKeyDown(VK_F8) then
    active = false
    -- Deactivate
    executeCodeEx(0, 5000, getAddress("dbgCamDeactivate"))
    print("[DebugCam] Deactivated (F8)")
    return
  end

  local speed = MOVE_SPEED
  if isKeyDown(VK_SHIFT) then
    speed = speed * FAST_MULT
  end

  -- Read mouse deltas from game's input buffer
  local dx = readFloat(MOUSE_DX) or 0
  local dy = readFloat(MOUSE_DY) or 0

  -- Movement
  local fwd = 0
  local right = 0
  local up = 0

  if isKeyDown(VK_W) then fwd = fwd + speed end
  if isKeyDown(VK_S) then fwd = fwd - speed end
  if isKeyDown(VK_D) then right = right + speed end
  if isKeyDown(VK_A) then right = right - speed end
  if isKeyDown(VK_SPACE) then up = up + speed end
  if isKeyDown(VK_CTRL)  then up = up - speed end

  -- Write to debug camera object (inverted mouse axes)
  writeFloat(dbgCam + OFF_PITCH,   -dy * LOOK_SPEED)
  writeFloat(dbgCam + OFF_YAW,     -dx * LOOK_SPEED)
  writeFloat(dbgCam + OFF_FORWARD, fwd)
  writeFloat(dbgCam + OFF_RIGHT,   right)
  writeFloat(dbgCam + OFF_UP,      up)

  -- Middle mouse = teleport player to camera
  if isKeyDown(VK_MMB) then
    writeBytes(dbgCam + OFF_TELEPORT, 1)
  end
end

dbgCamTimer.Enabled = true
print("[DebugCam] Input timer started — WASD+Mouse, Shift=fast, F8=off, MMB=teleport")
