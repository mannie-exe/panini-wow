if not PaniniClassicWoW then return end

local DEFAULT_ANGLE = 225
local RADIUS = 80

local btn = CreateFrame("Button", "PaniniMinimapButton", Minimap)
btn:SetWidth(31)
btn:SetHeight(31)
btn:SetFrameStrata("MEDIUM")
btn:SetFrameLevel(8)
btn:EnableMouse(true)
btn:SetMovable(true)
btn:RegisterForClicks("LeftButtonUp")

local overlay = btn:CreateTexture(nil, "OVERLAY")
overlay:SetWidth(53)
overlay:SetHeight(53)
overlay:SetTexture("Interface\\Minimap\\MiniMap-TrackingBorder")
overlay:SetPoint("TOPLEFT", btn, "TOPLEFT", 0, 0)

local bg = btn:CreateTexture(nil, "BACKGROUND")
bg:SetWidth(20)
bg:SetHeight(20)
bg:SetTexture("Interface\\Minimap\\UI-Minimap-Background")
bg:SetPoint("TOPLEFT", btn, "TOPLEFT", 7, -5)

local icon = btn:CreateTexture(nil, "ARTWORK")
icon:SetWidth(17)
icon:SetHeight(17)
icon:SetTexture("Interface\\Icons\\INV_Misc_Food_33")
icon:SetPoint("TOPLEFT", btn, "TOPLEFT", 7, -6)

btn:SetHighlightTexture("Interface\\Minimap\\UI-Minimap-ZoomButton-Highlight")

local function UpdatePosition(angle)
    btn:ClearAllPoints()
    btn:SetPoint("CENTER", Minimap, "CENTER",
        math.cos(math.rad(angle)) * RADIUS,
        math.sin(math.rad(angle)) * RADIUS)
end

UpdatePosition(DEFAULT_ANGLE)
btn:Show()

btn:RegisterForDrag("LeftButton")

btn:SetScript("OnDragStart", function()
    btn:SetScript("OnUpdate", function()
        local mx, my = Minimap:GetCenter()
        local px, py = GetCursorPosition()
        local scale = UIParent:GetEffectiveScale()
        px = px / scale
        py = py / scale
        local angle = math.deg(math.atan2(py - my, px - mx))
        if angle < 0 then angle = angle + 360 end
        PaniniClassicWoW_Config.minimapPos = angle
        UpdatePosition(angle)
    end)
end)

btn:SetScript("OnDragStop", function()
    btn:SetScript("OnUpdate", nil)
end)

btn:SetScript("OnClick", function()
    PaniniClassicWoW.ToggleSettings()
end)

btn:SetScript("OnEnter", function()
    GameTooltip:SetOwner(this, "ANCHOR_LEFT")
    GameTooltip:AddLine("Panini Projection", 1, 0.85, 0)
    GameTooltip:AddLine("Left-click to open settings", 1, 1, 1)
    GameTooltip:Show()
end)

btn:SetScript("OnLeave", function()
    GameTooltip:Hide()
end)

local loader = CreateFrame("Frame")
loader:RegisterEvent("PLAYER_LOGIN")
loader:SetScript("OnEvent", function()
    local angle = DEFAULT_ANGLE
    if PaniniClassicWoW_Config and PaniniClassicWoW_Config.minimapPos then
        angle = PaniniClassicWoW_Config.minimapPos
    end
    UpdatePosition(angle)
    btn:Show()
end)
