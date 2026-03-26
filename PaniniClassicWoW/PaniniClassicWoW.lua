-- PaniniClassicWoW: configuration addon for PaniniWoW.dll.
-- Requires SuperWoW for CVar infrastructure.
-- The DLL handles FOV by writing directly to camera memory each frame;
-- no /reload needed, no SuperWoW FoV CVar dependency.

if not SUPERWOW_VERSION then
    DEFAULT_CHAT_FRAME:AddMessage("|cffff0000PaniniClassicWoW|r requires SuperWoW.")
    return
end

local defaults = {
    enabled = false,
    strength = 0.5,
    verticalComp = 0.0,
    fill = 0.8,
    paniniFov = 2.0,
}

local frame = CreateFrame("Frame", "PaniniClassicWoWFrame", UIParent)
frame:RegisterEvent("ADDON_LOADED")
frame:RegisterEvent("PLAYER_ENTERING_WORLD")

local dllLoaded = false

local function SafeGetCVar(name)
    local ok, val = pcall(GetCVar, name)
    if ok then return val end
    return nil
end

local function SafeSetCVar(name, value)
    local ok = pcall(SetCVar, name, value)
    return ok
end

local function SyncCVarsToDLL()
    local c = PaniniClassicWoW_Config
    if not c then return end
    dllLoaded = SafeSetCVar("paniniEnabled", c.enabled and "1" or "0")
    if not dllLoaded then return end
    SafeSetCVar("paniniStrength", tostring(c.strength))
    SafeSetCVar("paniniVertComp", tostring(c.verticalComp))
    SafeSetCVar("paniniFill", tostring(c.fill))
    SafeSetCVar("paniniFov", tostring(c.paniniFov))
end

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
        if dllLoaded then
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPaniniClassicWoW|r loaded. /panini for commands.")
        else
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPaniniClassicWoW|r loaded (DLL not detected; settings saved locally).")
        end
    elseif event == "PLAYER_ENTERING_WORLD" then
        SyncCVarsToDLL()
    end
end)

SLASH_PANINI1 = "/panini"
SLASH_PANINI2 = "/pw"

SlashCmdList["PANINI"] = function(msg)
    local args = {}
    for word in string.gfind(msg, "%S+") do
        table.insert(args, word)
    end

    local cmd = args[1] or "help"
    local val = args[2]
    local c = PaniniClassicWoW_Config

    if cmd == "toggle" then
        c.enabled = not c.enabled
        SafeSetCVar("paniniEnabled", c.enabled and "1" or "0")
        DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r " .. (c.enabled and "enabled" or "disabled"))

    elseif cmd == "on" then
        c.enabled = true
        SafeSetCVar("paniniEnabled", "1")
        DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r enabled")

    elseif cmd == "off" then
        c.enabled = false
        SafeSetCVar("paniniEnabled", "0")
        DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r disabled")

    elseif cmd == "strength" or cmd == "s" then
        if val then
            local n = tonumber(val)
            if n and n >= 0 and n <= 1 then
                c.strength = n
                SafeSetCVar("paniniStrength", tostring(n))
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r strength: " .. n)
            else
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r strength must be 0.0 to 1.0")
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
            if n and n >= 1.0 and n <= 3.14 then
                c.paniniFov = n
                SafeSetCVar("paniniFov", tostring(n))
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r fov: " .. n .. " rad (" .. string.format("%.0f", n * 57.2958) .. " deg)")
            else
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r fov must be 1.0 to 3.14 (radians)")
            end
        else
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r fov: " .. c.paniniFov .. " rad (" .. string.format("%.0f", c.paniniFov * 57.2958) .. " deg)")
        end

    elseif cmd == "debug" then
        local debugOn = SafeGetCVar("paniniDebug")
        if debugOn == "1" then
            SafeSetCVar("paniniDebug", "0")
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r debug off")
        else
            SafeSetCVar("paniniDebug", "1")
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r debug on (red channel -33%)")
        end

    elseif cmd == "status" then
        local debugVal = SafeGetCVar("paniniDebug") or "0"
        DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPaniniClassicWoW|r status:")
        DEFAULT_CHAT_FRAME:AddMessage("  enabled: " .. tostring(c.enabled))
        DEFAULT_CHAT_FRAME:AddMessage("  debug: " .. (debugVal == "1" and "on" or "off"))
        DEFAULT_CHAT_FRAME:AddMessage("  strength: " .. c.strength)
        DEFAULT_CHAT_FRAME:AddMessage("  vertical: " .. c.verticalComp)
        DEFAULT_CHAT_FRAME:AddMessage("  fill: " .. c.fill)
        DEFAULT_CHAT_FRAME:AddMessage("  panini fov: " .. c.paniniFov .. " rad (" .. string.format("%.0f", c.paniniFov * 57.2958) .. " deg)")
        DEFAULT_CHAT_FRAME:AddMessage("  dll: " .. (dllLoaded and "connected" or "not detected"))

    else
        DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPaniniClassicWoW|r commands:")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini toggle        toggle on/off")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini on|off        enable/disable")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini debug         toggle debug shader (red -33%)")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini strength N    set strength (0 to 1)")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini vertical N    set vertical comp (-1 to 1)")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini fill N        set fill zoom (0 to 1)")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini fov N         set panini FOV in radians (1.0 to 3.14)")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini status        show current settings")
    end
end
