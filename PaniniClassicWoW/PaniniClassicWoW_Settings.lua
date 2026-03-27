-- PaniniClassicWoW_Settings.lua
-- Standalone settings dialog with Settings and Debug tab pages.

local function GetConfig()
    return PaniniClassicWoW_Config or {}
end

local function SafeSetCVar(name, value)
    PaniniClassicWoW.SafeSetCVar(name, value)
end

local function SafeGetCVar(name)
    return PaniniClassicWoW.SafeGetCVar(name)
end

local function EnablePanini()
    PaniniClassicWoW.EnablePanini()
end

local function DisablePanini()
    PaniniClassicWoW.DisablePanini()
end

local SLIDER_W = 280
local SLIDER_H = 16

-- ============================================================
-- Main dialog frame
-- ============================================================

local dialog = CreateFrame("Frame", "PaniniSettingsDialog", UIParent)
dialog:SetSize(340, 420)
dialog:SetPoint("CENTER")
dialog:SetBackdrop({
    bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
    edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
    tile = true,
    tileSize = 32,
    edgeSize = 16,
    insets = { left = 4, right = 4, top = 4, bottom = 4 }
})
dialog:SetBackdropColor(0, 0, 0, 0.85)
dialog:EnableMouse(true)
dialog:SetMovable(true)
dialog:RegisterForDrag("LeftButton")
dialog:SetScript("OnDragStart", function()
    this:StartMoving()
end)
dialog:SetScript("OnDragStop", function()
    this:StopMovingOrSizing()
    local c = GetConfig()
    local _, _, _, x, y = this:GetPoint(1)
    c.posX = math.floor(x + 0.5)
    c.posY = math.floor(y + 0.5)
end)
dialog:Hide()

-- ============================================================
-- Title
-- ============================================================

local title = dialog:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
title:SetPoint("TOPLEFT", 18, -18)
title:SetText("Panini Projection")
title:SetTextColor(1, 0.85, 0)

-- ============================================================
-- Close button
-- ============================================================

local close = CreateFrame("Button", "PaniniSettingsDialogClose", dialog, "UIPanelCloseButton")
close:SetPoint("TOPRIGHT", -4, -4)
close:SetScript("OnClick", function()
    dialog:Hide()
end)

-- ============================================================
-- Tab pages container (below title, above tabs)
-- ============================================================

local tabPageContainer = CreateFrame("Frame", "PaniniSettingsPageContainer", dialog)
tabPageContainer:SetPoint("TOPLEFT", 10, -42)
tabPageContainer:SetPoint("BOTTOMRIGHT", -10, 36)

-- ============================================================
-- Helper: Create slider
-- ============================================================

local function CreateSlider(parent, name, labelText, lowText, highText, minVal, maxVal, step, configKey, cvar, chatMsg)
    local slider = CreateFrame("Slider", name, parent, "OptionsSliderTemplate")
    slider:SetMinMaxValues(minVal, maxVal)
    slider:SetValueStep(step)
    slider:SetWidth(SLIDER_W)
    slider:SetHeight(SLIDER_H)

    local textName = slider:GetName() .. "Text"
    local lowName = slider:GetName() .. "Low"
    local highName = slider:GetName() .. "High"

    getglobal(textName):SetText(labelText)
    getglobal(lowName):SetText(lowText)
    getglobal(highName):SetText(highText)

    slider:SetScript("OnValueChanged", function(self, value)
        local c = GetConfig()
        c[configKey] = value
        SafeSetCVar(cvar, tostring(value))
        DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r " .. chatMsg .. ": " .. value)
    end)

    return slider
end

-- ============================================================
-- Helper: Create checkbox
-- ============================================================

local function CreateCheckbox(parent, name, labelText, tooltipText, onClick)
    local cb = CreateFrame("CheckButton", name, parent, "OptionsCheckButtonTemplate")
    getglobal(cb:GetName() .. "Text"):SetText(labelText)
    cb.tooltipText = tooltipText or ""
    cb:SetScript("OnClick", onClick)
    return cb
end

-- ============================================================
-- Page 1: Settings
-- ============================================================

local pageSettings = CreateFrame("Frame", "PaniniSettingsPageSettings", tabPageContainer)
pageSettings:SetAllPoints(tabPageContainer)

local y = -5

local cbEnabled = CreateCheckbox(pageSettings, "PaniniSettingsEnabled",
    "Enable Panini Projection",
    "Toggle panini/cylindrical camera projection",
    function(self)
        if self:GetChecked() then
            EnablePanini()
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r enabled")
        else
            DisablePanini()
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r disabled")
        end
    end
)
cbEnabled:SetPoint("TOPLEFT", 10, y)

y = y - 45

local sStrength = CreateSlider(pageSettings, "PaniniSettingsStrength",
    "Strength", "0", "1", 0, 1, 0.001,
    "strength", "paniniStrength", "strength"
)
sStrength:SetPoint("TOPLEFT", 20, y)

y = y - 55

local sVert = CreateSlider(pageSettings, "PaniniSettingsVertical",
    "Vertical Comp", "-1", "1", -1, 1, 0.01,
    "verticalComp", "paniniVertComp", "vertical"
)
sVert:SetPoint("TOPLEFT", 20, y)

y = y - 55

local sFill = CreateSlider(pageSettings, "PaniniSettingsFill",
    "Fill", "0", "1", 0, 1, 0.01,
    "fill", "paniniFill", "fill"
)
sFill:SetPoint("TOPLEFT", 20, y)

y = y - 55

local sFov = CreateSlider(pageSettings, "PaniniSettingsFov",
    "FOV (rad)", "0.1", "3.14", 0.1, 3.14, 0.01,
    "fov", "paniniFov", "fov"
)
sFov:SetPoint("TOPLEFT", 20, y)
sFov:SetScript("OnValueChanged", function(self, value)
    local c = GetConfig()
    c.fov = value
    SafeSetCVar("paniniFov", tostring(value))
    if c.enabled then
        SafeSetCVar("FoV", tostring(value))
    end
    DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r fov: " .. value)
end)

-- ============================================================
-- Page 2: Debug
-- ============================================================

local pageDebug = CreateFrame("Frame", "PaniniSettingsPageDebug", tabPageContainer)
pageDebug:SetAllPoints(tabPageContainer)
pageDebug:Hide()

local y2 = -5

local cbTint = CreateCheckbox(pageDebug, "PaniniSettingsDebugTint",
    "Debug Tint (red -33%)",
    "Visualize panini effect with red channel reduction",
    function(self)
        if self:GetChecked() then
            SafeSetCVar("paniniDebugTint", "1")
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r debug tint on (red channel -33%)")
        else
            SafeSetCVar("paniniDebugTint", "0")
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r debug tint off")
        end
    end
)
cbTint:SetPoint("TOPLEFT", 10, y2)

y2 = y2 - 35

local cbUV = CreateCheckbox(pageDebug, "PaniniSettingsDebugUV",
    "Debug UV Visualization",
    "Show UV coordinate visualization",
    function(self)
        if self:GetChecked() then
            SafeSetCVar("paniniDebugUV", "1")
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r debug uv on (UV visualization)")
        else
            SafeSetCVar("paniniDebugUV", "0")
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffPanini|r debug uv off")
        end
    end
)
cbUV:SetPoint("TOPLEFT", 10, y2)

y2 = y2 - 50

local btnReset = CreateFrame("Button", "PaniniSettingsReset", pageDebug, "UIPanelButtonTemplate")
btnReset:SetSize(130, 24)
btnReset:SetPoint("TOPLEFT", 20, y2)
btnReset:SetText("Reset Defaults")
btnReset:SetScript("OnClick", function()
    SlashCmdList["PANINI"]("reset")
    PaniniSettingsDialog_OnShow()
end)

-- ============================================================
-- Tab buttons
-- ============================================================

local function CreateTabButton(parent, name, label, pageFrame, selected)
    local tab = CreateFrame("Button", name, parent)
    tab:SetSize(80, 28)
    tab:SetText(label)
    tab:SetNormalFontObject("GameFontNormalSmall")
    tab:SetHighlightFontObject("GameFontHighlightSmall")
    tab:SetDisabledFontObject("GameFontDisableSmall")

    local nt = tab:CreateTexture(nil, "BACKGROUND")
    nt:SetTexture("Interface\\DialogFrame\\UI-DialogBox-Background")
    nt:SetPoint("TOPLEFT", 2, -2)
    nt:SetPoint("BOTTOMRIGHT", -2, 2)

    local hl = tab:CreateTexture(nil, "HIGHLIGHT")
    hl:SetTexture("Interface\\Buttons\\UI-Panel-Button-Highlight")
    hl:SetBlendMode("ADD")
    hl:SetPoint("TOPLEFT", 2, -2)
    hl:SetPoint("BOTTOMRIGHT", -2, 2)

    tab.page = pageFrame
    tab:SetScript("OnClick", function()
        PaniniSettingsDialog_SelectTab(this)
    end)

    if selected then
        tab.selected = true
        tab:SetTextColor(1, 0.85, 0)
    else
        tab:SetTextColor(0.6, 0.6, 0.6)
    end

    return tab
end

local tab1 = CreateTabButton(dialog, "PaniniSettingsTabSettings", "Settings", pageSettings, true)
tab1:SetPoint("BOTTOMLEFT", 30, 5)

local tab2 = CreateTabButton(dialog, "PaniniSettingsTabDebug", "Debug", pageDebug, false)
tab2:SetPoint("LEFT", tab1, "RIGHT", 4, 0)

local tabs = { tab1, tab2 }
local pages = { pageSettings, pageDebug }

function PaniniSettingsDialog_SelectTab(selectedTab)
    for i, tab in tabs do
        if tab == selectedTab then
            tab.selected = true
            tab:SetTextColor(1, 0.85, 0)
            tab.page:Show()
        else
            tab.selected = false
            tab:SetTextColor(0.6, 0.6, 0.6)
            tab.page:Hide()
        end
    end
end

-- ============================================================
-- OnShow: sync all controls to current config
-- ============================================================

function PaniniSettingsDialog_OnShow()
    local c = GetConfig()

    if c.posX and c.posY and (c.posX ~= 0 or c.posY ~= 0) then
        dialog:ClearAllPoints()
        dialog:SetPoint("CENTER", UIParent, "CENTER", c.posX, c.posY)
    end

    cbEnabled:SetChecked(c.enabled and 1 or nil)
    sStrength:SetValue(c.strength or 0.0333)
    sVert:SetValue(c.verticalComp or 0)
    sFill:SetValue(c.fill or 1.0)
    sFov:SetValue(c.fov or 2.6)

    local tintVal = SafeGetCVar("paniniDebugTint")
    cbTint:SetChecked(tintVal == "1" and 1 or nil)

    local uvVal = SafeGetCVar("paniniDebugUV")
    cbUV:SetChecked(uvVal == "1" and 1 or nil)
end

dialog:SetScript("OnShow", PaniniSettingsDialog_OnShow)

-- ============================================================
-- Public open/close toggle
-- ============================================================

function PaniniClassicWoW.ToggleSettings()
    if dialog:IsShown() then
        dialog:Hide()
    else
        dialog:Show()
    end
end

-- ============================================================
-- Interface Options integration
-- ============================================================

local ioPanel = CreateFrame("Frame", "PaniniSettingsIOPanel", UIParent)
ioPanel.name = "Panini Projection"
InterfaceOptions_AddCategory(ioPanel)
ioPanel:SetScript("OnShow", function()
    PaniniClassicWoW.ToggleSettings()
end)
