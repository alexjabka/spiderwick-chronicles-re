"""Apply all discovered function/global renames to a fresh IDA database.

Usage: Run in IDA's Python console (File → Script file) or via MCP py_eval.
"""
import idc

FUNCTIONS = [
    # Camera system
    (0x5299A0, "BuildLookAtMatrix"),
    (0x5AA1B0, "BuildViewMatrix"),
    (0x4368B0, "GetCameraObject"),
    (0x50B760, "SetProjection"),
    (0x4356F0, "CopyPositionBlock"),
    (0x43DA50, "MonocleUpdate"),
    (0x43DC10, "CameraVtable7_MouseInput"),
    (0x43E2B0, "MainCameraComponent"),
    (0x447AD0, "GetCameraObject_Thunk"),
    (0x436190, "CameraComponentPipeline"),

    # Visibility pipeline
    (0x488DD0, "CameraSectorUpdate"),
    (0x488B30, "UpdateVisibility"),
    (0x564950, "PerformRoomCulling"),
    (0x488970, "SectorVisibilityUpdate2"),
    (0x516B10, "IsSectorSystemActive_Mode0"),
    (0x516E30, "IsSectorSystemActive_Mode1"),

    # Sector system
    (0x487D70, "SectorDistanceCheck"),
    (0x4391D0, "SectorTransition"),
    (0x57E2E0, "SectorStateMachine1"),
    (0x57ED60, "SectorStateMachine2"),

    # Transform
    (0x51B2A0, "SetTransform"),

    # Character system
    (0x44F890, "GetPlayerCharacter"),
    (0x44F8F0, "GetPlayerCharacter2"),
    (0x44F7C0, "FindClosestCharacter"),
    (0x44F820, "FindClosestCharacterInRadius"),
    (0x44F950, "CountPlayerCharacters"),
    (0x44FE90, "GetCharacterManager"),
    (0x44FBB0, "CharacterListOperation"),
    (0x463BF0, "ClPlayerObj_Constructor"),
    (0x462E00, "ClPlayerObj_IsPlayerControlled"),
    (0x454330, "ClCharacterObj_IsPlayerControlled_Base"),
    (0x487C70, "ClPlayerProximityService_Init"),
    (0x487CF0, "ClPlayerProximityService_InitWithPlayer"),
    (0x487EE0, "CreatePlayerProximityService"),
    (0x487F50, "CreatePlayerProximityServiceWithPlayer"),
    (0x43D850, "GetPlayerCharacter_Thunk"),

    # Cheat system
    (0x443EC0, "CheatInputHandler"),
    (0x443B10, "SetCheatFlag"),
    (0x444140, "CheatSystemInit"),
    (0x4E0AE0, "ReadGameData"),

    # Debug camera
    (0x440910, "DebugCameraManager_Constructor"),
    (0x440790, "DebugCameraManager_InputHandler"),
    (0x440630, "DebugCameraManager_InputValidator"),
    (0x43C740, "DebugCameraManager_ReadInput"),
    (0x43C4E0, "DebugCameraManager_CameraUpdate"),
    (0x43C4C0, "DebugCameraManager_Activate"),
    (0x43C450, "DebugCameraManager_Deactivate"),
    (0x440660, "DebugCameraManager_ActivateKallis"),
    (0x440690, "DebugCameraManager_DeactivateKallis"),

    # Character creation / switching
    (0x44FB50, "CreateCharacter_Wrapper"),
    (0x44FA20, "CreateCharacter_Internal"),
    (0x4892B0, "sauRespawn_Impl"),
    (0x538E60, "ResolveCharacterMode"),
    (0x4659F0, "ClThimbletackObj_Constructor"),
    (0x4D6110, "ClPlayerObj_Factory"),

    # Widget / UI
    (0x418290, "CreateWidget"),

    # Input system
    (0x5522D0, "CheckInputAction"),
    (0x5522F0, "GetInputAnalogValue"),
    (0x405380, "HashString"),
]

GLOBALS = [
    (0x7307D8, "g_CharacterListHead"),
    (0x730754, "g_CharacterManager"),
]

ok = fail = 0
for addr, name in FUNCTIONS + GLOBALS:
    if idc.set_name(addr, name, 0x1):
        ok += 1
    else:
        fail += 1
        print(f"  FAIL: 0x{addr:X} -> {name}")

print(f"Renames applied: {ok} OK, {fail} FAIL out of {ok + fail}")
