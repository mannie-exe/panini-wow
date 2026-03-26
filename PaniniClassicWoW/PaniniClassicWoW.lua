-- PaniniClassicWoW.lua
-- Configuration addon for PaniniWoW.dll
-- Requires SuperWoW (for ExportFile and FoV CVar)

local CONF_PATH = "WTF/panini.conf"

local defaults = {
    enabled = false,
    strength = 0.5,
    verticalComp = 0.0,
    fill = 0.8,
    debug = false,
}

-- Initialization

local frame = CreateFrame("Frame", "PaniniClassicWoWFrame", UIParent)
frame:RegisterEvent("ADDON_LOADED")
frame:RegisterEvent("PLAYER_ENTERING_WORLD")

frame:SetScript("OnEvent", function()
    if event == "ADDON_LOADED" and arg1 == "PaniniClassicWoW" then
        if not SUPERWOW_VERSION then
            DEFAULT_CHAT_FRAME:AddMessage("|cffff0000PaniniClassicWoW|r: SuperWoW not detected. This addon requires SuperWoW.")
            return
        end

        if not PaniniClassicWoW_Config then
            PaniniClassicWoW_Config = {}
        end
        for k, v in pairs(defaults) do
            if PaniniClassicWoW_Config[k] == nil then
                PaniniClassicWoW_Config[k] = v
            end
        end

        PaniniClassicWoW_WriteConfig()
        DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPaniniClassicWoW|r loaded. Type /panini for commands.")
    elseif event == "PLAYER_ENTERING_WORLD" then
        PaniniClassicWoW_WriteConfig()
    end
end)

-- Config file writer (DLL reads this file)

function PaniniClassicWoW_WriteConfig()
    local cfg = PaniniClassicWoW_Config
    if not cfg then return end

    local content = string.format(
        "# PaniniClassicWoW config (written by addon)\n" ..
        "enabled=%d\n" ..
        "strength=%.3f\n" ..
        "vertical_comp=%.3f\n" ..
        "fill=%.3f\n" ..
        "debug=%d\n",
        cfg.enabled and 1 or 0,
        cfg.strength,
        cfg.verticalComp,
        cfg.fill,
        cfg.debug and 1 or 0
    )

    if ExportFile then
        ExportFile(CONF_PATH, content)
    end
end

-- Slash commands

SLASH_PANINI1 = "/panini"
SLASH_PANINI2 = "/pw"

SlashCmdList["PANINI"] = function(msg)
    local args = {}
    for word in string.gfind(msg, "%S+") do
        table.insert(args, word)
    end

    local cmd = args[1] or "help"
    local val = args[2]
    local cfg = PaniniClassicWoW_Config

    if cmd == "toggle" then
        cfg.enabled = not cfg.enabled
        DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r " .. (cfg.enabled and "enabled" or "disabled"))
    elseif cmd == "on" then
        cfg.enabled = true
        DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r enabled")
    elseif cmd == "off" then
        cfg.enabled = false
        DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r disabled")
    elseif cmd == "strength" or cmd == "s" then
        if val then
            local n = tonumber(val)
            if n and n >= 0 and n <= 1 then
                cfg.strength = n
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r strength: " .. n)
            else
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r strength must be 0.0-1.0")
            end
        else
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r strength: " .. cfg.strength)
        end
    elseif cmd == "vertical" or cmd == "v" then
        if val then
            local n = tonumber(val)
            if n and n >= -1 and n <= 1 then
                cfg.verticalComp = n
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r vertical: " .. n)
            else
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r vertical must be -1.0 to 1.0")
            end
        else
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r vertical: " .. cfg.verticalComp)
        end
    elseif cmd == "fill" or cmd == "f" then
        if val then
            local n = tonumber(val)
            if n and n >= 0 and n <= 1 then
                cfg.fill = n
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r fill: " .. n)
            else
                DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r fill must be 0.0-1.0")
            end
        else
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r fill: " .. cfg.fill)
        end
    elseif cmd == "debug" then
        cfg.debug = not cfg.debug
        DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r debug " .. (cfg.debug and "on" or "off"))
    elseif cmd == "status" then
        DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPaniniClassicWoW|r status:")
        DEFAULT_CHAT_FRAME:AddMessage("  enabled: " .. tostring(cfg.enabled))
        DEFAULT_CHAT_FRAME:AddMessage("  strength: " .. cfg.strength)
        DEFAULT_CHAT_FRAME:AddMessage("  vertical: " .. cfg.verticalComp)
        DEFAULT_CHAT_FRAME:AddMessage("  fill: " .. cfg.fill)
        DEFAULT_CHAT_FRAME:AddMessage("  debug: " .. tostring(cfg.debug))
    else
        DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPaniniClassicWoW|r commands:")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini toggle      - Toggle on/off")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini on|off      - Enable/disable")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini strength N  - Set strength (0-1)")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini vertical N  - Set vertical comp (-1 to 1)")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini fill N      - Set fill zoom (0-1)")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini debug       - Toggle debug overlay")
        DEFAULT_CHAT_FRAME:AddMessage("  /panini status      - Show current settings")
    end

    PaniniClassicWoW_WriteConfig()
end
