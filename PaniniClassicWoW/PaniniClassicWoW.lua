-- PaniniClassicWoW: configuration addon for PaniniWoW.dll.
-- Requires SuperWoW for CVar infrastructure.
-- FOV is managed by SuperAPI; PaniniWoW reads the camera FOV each frame.

if not SUPERWOW_VERSION then
    DEFAULT_CHAT_FRAME:AddMessage("|cffff0000PaniniClassicWoW|r requires SuperWoW.")
    return
end

local defaults = {
    enabled = true,
    strength = 0.01,
    verticalComp = 0.0,
    fill = 1.0,
    fov = 2.82,
    fxaaEnabled = true,
    sharpen = 0.2,
}

local cvarMap = {
    enabled        = { cvar = "paniniEnabled",  fmt = "bool" },
    strength       = { cvar = "paniniStrength", fmt = "float" },
    verticalComp   = { cvar = "paniniVertComp", fmt = "float" },
    fill           = { cvar = "paniniFill",     fmt = "float" },
    fov            = { cvar = "paniniFov",      fmt = "float" },
    fxaaEnabled    = { cvar = "ppFxaa",         fmt = "bool" },
    sharpen        = { cvar = "ppSharpen",      fmt = "float" },
}

PaniniClassicWoW = PaniniClassicWoW or {}
PaniniClassicWoW.defaults = defaults

local frame = CreateFrame("Frame", "PaniniClassicWoWFrame", UIParent)
frame:RegisterEvent("ADDON_LOADED")
frame:RegisterEvent("PLAYER_ENTERING_WORLD")

local dllLoaded = false
local lastKnownFov = 0
local fovTrackSuppress = 0

local function SafeGetCVar(name)
    local ok, val = pcall(GetCVar, name)
    if ok then return val end
    return nil
end

local function SafeSetCVar(name, value)
    local ok = pcall(SetCVar, name, value)
    return ok
end

local function GetUserFov()
    local v = tonumber(SafeGetCVar("paniniUserFov"))
    if v and v >= 0 then return v end
    return nil
end

local function SetUserFov(v)
    SafeSetCVar("paniniUserFov", tostring(v))
end

local function InitUserFov()
    local saved = GetUserFov()
    if saved then
        lastKnownFov = saved
        return
    end
    local current = tonumber(SafeGetCVar("FoV"))
    if current and current > 0 then
        SetUserFov(current)
        lastKnownFov = current
    else
        SetUserFov(1.57)
        lastKnownFov = 1.57
    end
end

local function SyncCVarsToDLL()
    local c = PaniniClassicWoW_Config
    if not c then return end
    dllLoaded = SafeSetCVar("paniniEnabled", c.enabled and "1" or "0")
    if not dllLoaded then return end
    for key, spec in cvarMap do
        if key ~= "enabled" then
            local val = c[key]
            if val == nil then val = defaults[key] end
            if spec.fmt == "bool" then
                SafeSetCVar(spec.cvar, val and "1" or "0")
            elseif spec.fmt == "int" then
                SafeSetCVar(spec.cvar, tostring(math.floor(val)))
            else
                SafeSetCVar(spec.cvar, tostring(val))
            end
        end
    end
end

local function EnablePanini()
    local c = PaniniClassicWoW_Config
    local currentFov = tonumber(SafeGetCVar("FoV"))
    if currentFov and currentFov > 0 then
        SetUserFov(currentFov)
    end
    c.enabled = true
    local pf = c.fov or 2.82
    SafeSetCVar("FoV", tostring(pf))
    SafeSetCVar("paniniEnabled", "1")
    lastKnownFov = pf
    fovTrackSuppress = 3
end

local function DisablePanini()
    local c = PaniniClassicWoW_Config
    c.enabled = false
    local uf = GetUserFov() or 1.57
    SafeSetCVar("FoV", tostring(uf))
    SafeSetCVar("paniniEnabled", "0")
    lastKnownFov = uf
    fovTrackSuppress = 3
end

PaniniClassicWoW.SafeSetCVar = SafeSetCVar
PaniniClassicWoW.SafeGetCVar = SafeGetCVar
PaniniClassicWoW.EnablePanini = EnablePanini
PaniniClassicWoW.DisablePanini = DisablePanini
PaniniClassicWoW.UpdateFovTracking = function(value)
    lastKnownFov = value
    fovTrackSuppress = 2
end
PaniniClassicWoW.ToggleSettings = function() end

frame:SetScript("OnEvent", function()
    if event == "ADDON_LOADED" and arg1 == "PaniniClassicWoW" then
        if not PaniniClassicWoW_Config then
            PaniniClassicWoW_Config = {}
        end
        for k, v in pairs(defaults) do
            if PaniniClassicWoW_Config[k] == nil then
                PaniniClassicWoW_Config[k] = v
            end
        end
        SyncCVarsToDLL()
        InitUserFov()
        if dllLoaded then
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPaniniClassicWoW|r loaded. /panini for commands.")
        else
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPaniniClassicWoW|r loaded (DLL not detected; settings saved locally).")
        end
    elseif event == "PLAYER_ENTERING_WORLD" then
        SyncCVarsToDLL()
    end
end)

frame:SetScript("OnUpdate", function()
    if fovTrackSuppress > 0 then
        fovTrackSuppress = fovTrackSuppress - 1
        return
    end
    local currentFov = tonumber(SafeGetCVar("FoV"))
    if currentFov and currentFov > 0 and currentFov ~= lastKnownFov then
        SetUserFov(currentFov)
        lastKnownFov = currentFov
    end
end)

SLASH_PANINI1 = "/panini"
SLASH_PANINI2 = "/pw"

SlashCmdList["PANINI"] = function(msg)
    local args = {}
    for word in string.gfind(msg, "%S+") do
        table.insert(args, word)
    end

    local cmd = args[1] or ""
    local val = args[2]
    local c = PaniniClassicWoW_Config

    if cmd == "" or cmd == "settings" or cmd == "config" or cmd == "open" then
        PaniniClassicWoW.ToggleSettings()
        return
    elseif cmd == "toggle" then
        if c.enabled then
            DisablePanini()
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r disabled")
        else
            EnablePanini()
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r enabled")
        end

    elseif cmd == "on" then
        EnablePanini()
        DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r enabled")

    elseif cmd == "off" then
        DisablePanini()
        DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r disabled")

    elseif cmd == "strength" or cmd == "s" then
        if val then
            local n = tonumber(val)
            if n and n >= 0 and n <= 0.10 then
                c.strength = n
                SafeSetCVar("paniniStrength", tostring(n))
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r strength: " .. n)
            else
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r strength must be 0.0 to 0.1")
            end
        else
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r strength: " .. c.strength)
        end

    elseif cmd == "vertical" or cmd == "v" then
        if val then
            local n = tonumber(val)
            if n and n >= -1 and n <= 1 then
                c.verticalComp = n
                SafeSetCVar("paniniVertComp", tostring(n))
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r vertical: " .. n)
            else
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r vertical must be -1.0 to 1.0")
            end
        else
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r vertical: " .. c.verticalComp)
        end

    elseif cmd == "fill" or cmd == "f" then
        if val then
            local n = tonumber(val)
            if n and n >= 0 and n <= 1 then
                c.fill = n
                SafeSetCVar("paniniFill", tostring(n))
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r fill: " .. n)
            else
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r fill must be 0.0 to 1.0")
            end
        else
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r fill: " .. c.fill)
        end

    elseif cmd == "fov" then
        if val then
            local n = tonumber(val)
            if n and n >= 0.1 and n <= 3.14 then
                c.fov = n
                SafeSetCVar("paniniFov", tostring(n))
                if c.enabled then
                    SafeSetCVar("FoV", tostring(n))
                    lastKnownFov = n
                    fovTrackSuppress = 3
                end
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r fov: " .. n)
            else
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r fov must be 0.1 to 3.14")
            end
        else
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r fov: " .. c.fov)
        end

    elseif cmd == "fxaa" then
        if val == "on" or val == "1" then
            c.fxaaEnabled = true
            SafeSetCVar("ppFxaa", "1")
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r FXAA enabled")
        elseif val == "off" or val == "0" then
            c.fxaaEnabled = false
            SafeSetCVar("ppFxaa", "0")
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r FXAA disabled")
        else
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r FXAA: " .. (c.fxaaEnabled and "on" or "off"))
        end

    elseif cmd == "sharpen" then
        if val then
            local n = tonumber(val)
            if n and n >= 0 and n <= 1 then
                c.sharpen = n
                SafeSetCVar("ppSharpen", tostring(n))
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r sharpen: " .. n)
            else
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r sharpen must be 0.0 to 1.0")
            end
        else
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r sharpen: " .. (c.sharpen or 0.2))
        end

    elseif cmd == "debug" then
        if val == "tint" then
            local tintOn = SafeGetCVar("ppDebugTint")
            if tintOn == "1" then
                SafeSetCVar("ppDebugTint", "0")
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r debug tint off")
            else
                SafeSetCVar("ppDebugTint", "1")
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r debug tint on (red channel -33%)")
            end
        elseif val == "uv" then
            local uvOn = SafeGetCVar("ppDebugUV")
            if uvOn == "1" then
                SafeSetCVar("ppDebugUV", "0")
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r debug uv off")
            else
                SafeSetCVar("ppDebugUV", "1")
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r debug uv on (UV visualization)")
            end
        elseif val == "off" then
            SafeSetCVar("ppDebugTint", "0")
            SafeSetCVar("ppDebugUV", "0")
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r all debug modes off")
        else
            local tintVal = SafeGetCVar("ppDebugTint") or "0"
            local uvVal = SafeGetCVar("ppDebugUV") or "0"
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r debug state:")
            DEFAULT_CHAT_FRAME:AddMessage("  tint: " .. (tintVal == "1" and "on" or "off"))
            DEFAULT_CHAT_FRAME:AddMessage("  uv: " .. (uvVal == "1" and "on" or "off"))
        end

    elseif cmd == "reset" then
        if val == "ui" then
            if PaniniClassicWoW.ResetDialogPosition then
                PaniniClassicWoW.ResetDialogPosition()
            end
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r dialog position reset to center")
        elseif val == "settings" or val == nil then
            PaniniClassicWoW_Config = {}
            for k, v in pairs(defaults) do
                PaniniClassicWoW_Config[k] = v
            end
            SyncCVarsToDLL()
            SafeSetCVar("ppDebugTint", "0")
            SafeSetCVar("ppDebugUV", "0")
            InitUserFov()
            if PaniniClassicWoW_Config.enabled then
                SafeSetCVar("FoV", tostring(PaniniClassicWoW_Config.fov))
                lastKnownFov = PaniniClassicWoW_Config.fov
                fovTrackSuppress = 3
            end
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r reset to defaults")
        else
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r reset subcommands: settings, ui")
        end


    elseif cmd == "status" then
        local tintVal = SafeGetCVar("ppDebugTint") or "0"
        local uvVal = SafeGetCVar("ppDebugUV") or "0"
        local uf = GetUserFov()
        DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPaniniClassicWoW|r status:")
        DEFAULT_CHAT_FRAME:AddMessage("  enabled: " .. tostring(c.enabled))
        DEFAULT_CHAT_FRAME:AddMessage("  debug tint: " .. (tintVal == "1" and "on" or "off"))
        DEFAULT_CHAT_FRAME:AddMessage("  debug uv: " .. (uvVal == "1" and "on" or "off"))
        DEFAULT_CHAT_FRAME:AddMessage("  strength: " .. c.strength)
        DEFAULT_CHAT_FRAME:AddMessage("  vertical: " .. c.verticalComp)
        DEFAULT_CHAT_FRAME:AddMessage("  fill: " .. c.fill)
        DEFAULT_CHAT_FRAME:AddMessage("  paniniFov: " .. c.fov)
        DEFAULT_CHAT_FRAME:AddMessage("  fxaa: " .. (c.fxaaEnabled and "on" or "off"))
        DEFAULT_CHAT_FRAME:AddMessage("  sharpen: " .. (c.sharpen or 0.2))
        DEFAULT_CHAT_FRAME:AddMessage("  savedFov: " .. tostring(uf or 1.57))
        DEFAULT_CHAT_FRAME:AddMessage("  dll: " .. (dllLoaded and "connected" or "not detected"))

    elseif cmd == "cvars" then
        local names = {
            "paniniEnabled", "paniniStrength", "paniniVertComp",
            "paniniFill", "paniniFov", "paniniUserFov",
            "ppFxaa", "ppSharpen",
            "ppDebugTint", "ppDebugUV", "FoV",
        }
        DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r CVar readback:")
        for _, name in ipairs(names) do
            local val = SafeGetCVar(name)
            if val then
                DEFAULT_CHAT_FRAME:AddMessage("  " .. name .. " = " .. val)
            else
                DEFAULT_CHAT_FRAME:AddMessage("  " .. name .. " = (nil/not found)")
            end
        end

    elseif cmd == "debuginfo" then
        DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r debug info:")
        DEFAULT_CHAT_FRAME:AddMessage("  SUPERWOW_VERSION: " .. tostring(SUPERWOW_VERSION))
        DEFAULT_CHAT_FRAME:AddMessage("  dllLoaded: " .. tostring(dllLoaded))
        DEFAULT_CHAT_FRAME:AddMessage("  config.enabled: " .. tostring(c.enabled))
        DEFAULT_CHAT_FRAME:AddMessage("  config.strength: " .. tostring(c.strength))
        DEFAULT_CHAT_FRAME:AddMessage("  config.verticalComp: " .. tostring(c.verticalComp))
        DEFAULT_CHAT_FRAME:AddMessage("  config.fill: " .. tostring(c.fill))
        DEFAULT_CHAT_FRAME:AddMessage("  config.fov: " .. tostring(c.fov))
        DEFAULT_CHAT_FRAME:AddMessage("  config.fxaaEnabled: " .. tostring(c.fxaaEnabled))
        DEFAULT_CHAT_FRAME:AddMessage("  config.sharpen: " .. tostring(c.sharpen or 0.2))
        DEFAULT_CHAT_FRAME:AddMessage("  ppDebugTint: " .. tostring(SafeGetCVar("ppDebugTint") or "0"))
        DEFAULT_CHAT_FRAME:AddMessage("  ppDebugUV: " .. tostring(SafeGetCVar("ppDebugUV") or "0"))
        DEFAULT_CHAT_FRAME:AddMessage("  paniniUserFov: " .. tostring(GetUserFov()))
        DEFAULT_CHAT_FRAME:AddMessage("  lastKnownFov: " .. tostring(lastKnownFov))
        local fov = SafeGetCVar("FoV")
        DEFAULT_CHAT_FRAME:AddMessage("  FoV CVar: " .. tostring(fov))

    elseif cmd == "help" then
        DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPaniniClassicWoW|r commands:")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini               open settings")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini settings      open settings")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini help          show this help")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini toggle        toggle on/off")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini on|off        enable/disable")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini reset         reset settings to defaults")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini reset ui      reset dialog position to center")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini fov N         set panini FoV (0.1 to 3.14)")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini strength N    set strength (0 to 0.1)")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini vertical N    set vertical comp (-1 to 1)")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini fill N        set fill zoom (0 to 1)")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini fxaa on|off   toggle FXAA")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini sharpen N     set sharpen (0 to 1)")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini debug         show debug state")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini debug tint   toggle debug tint (red -33%)")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini debug uv     toggle debug UV visualization")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini debug off    disable all debug modes")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini status        show current settings")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini cvars         show CVar readback from engine")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini debuginfo     show debug diagnostics")
    end
end
