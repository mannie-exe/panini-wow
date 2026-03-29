if PaniniClassicWoW == nil then
	return
end

local DIALOG_W = 400
local SLIDER_W = 280
local SLIDER_H = 16
local EDITBOX_W = 58
local EDITBOX_H = 20
local STEP_DIVISOR = 200

local function SetSize(frame, w, h)
	frame:SetWidth(w)
	frame:SetHeight(h)
end

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

local function FormatValue(value, decimals)
	return string.format("%." .. (decimals or 4) .. "f", value)
end

local function Clamp(val, lo, hi)
	if val < lo then
		return lo
	end
	if val > hi then
		return hi
	end
	return val
end

local function CreateSlider(parent, name, labelText, lowText, highText, minVal, maxVal, configKey, cvar, opts)
	opts = opts or {}
	local decimals = opts.decimals or 4
	local customOnChange = opts.onValueChanged

	local step = opts.step or ((maxVal - minVal) / STEP_DIVISOR)
	local slider = CreateFrame("Slider", name, parent, "OptionsSliderTemplate")
	slider:SetMinMaxValues(minVal, maxVal)
	slider:SetValueStep(step)
	slider:SetWidth(SLIDER_W)
	slider:SetHeight(SLIDER_H)

	local textObj = getglobal(slider:GetName() .. "Text")
	local lowObj = getglobal(slider:GetName() .. "Low")
	local highObj = getglobal(slider:GetName() .. "High")
	textObj:SetText(labelText)
	lowObj:SetText(lowText)
	highObj:SetText(highText)

	local updatingFromEditBox = false
	local eb = CreateFrame("EditBox", name .. "Input", parent)
	SetSize(eb, EDITBOX_W, EDITBOX_H)
	eb:SetPoint("LEFT", slider, "RIGHT", 8, -8)
	eb:SetFont("Fonts\\FRIZQT__.TTF", 11)
	eb:SetJustifyH("CENTER")
	eb:SetMaxLetters(8)
	eb:SetAutoFocus(false)
	eb:SetText(FormatValue(minVal, decimals))
	eb:SetTextColor(1, 1, 1)
	eb:SetBackdrop({
		bgFile = "Interface\\ChatFrame\\ChatFrameBackground",
		edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
		tile = true,
		tileSize = 16,
		edgeSize = 8,
		insets = { left = 2, right = 2, top = 2, bottom = 2 },
	})
	eb:SetBackdropColor(0, 0, 0, 0.8)
	eb:SetBackdropBorderColor(0.4, 0.4, 0.4, 1)

	local function ApplyEditBox()
		local val = tonumber(eb:GetText())
		if val then
			val = Clamp(val, minVal, maxVal)
			eb:SetText(FormatValue(val, decimals))
			updatingFromEditBox = true
			slider:SetValue(val)
			updatingFromEditBox = false
		else
			eb:SetText(FormatValue(slider:GetValue(), decimals))
		end
	end

	eb:SetScript("OnEnterPressed", function()
		ApplyEditBox()
		eb:ClearFocus()
	end)

	eb:SetScript("OnEscapePressed", function()
		eb:SetText(FormatValue(slider:GetValue(), decimals))
		eb:ClearFocus()
		local dlg = getglobal("PaniniSettingsDialog")
		if dlg and dlg:IsShown() then
			dlg:Hide()
		end
	end)

	eb:SetScript("OnEditFocusLost", function()
		ApplyEditBox()
	end)

	slider:SetScript("OnValueChanged", function()
		local value = this:GetValue()
		local c = GetConfig()
		c[configKey] = value
		SafeSetCVar(cvar, tostring(value))
		if not updatingFromEditBox then
			eb:SetText(FormatValue(value, decimals))
		end
		if customOnChange then
			customOnChange(value)
		end
	end)

	return slider
end

local function CreateCheckbox(parent, name, labelText, tooltipText, onClick)
	local cb = CreateFrame("CheckButton", name, parent, "OptionsCheckButtonTemplate")
	getglobal(cb:GetName() .. "Text"):SetText(labelText)
	cb.tooltipText = tooltipText or ""
	cb:SetScript("OnClick", onClick)
	return cb
end

local dialog = CreateFrame("Frame", "PaniniSettingsDialog", UIParent)
dialog:SetWidth(DIALOG_W)
dialog:SetPoint("CENTER", UIParent, "CENTER")
dialog:SetBackdrop({
	bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
	edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
	tile = true,
	tileSize = 32,
	edgeSize = 16,
	insets = { left = 4, right = 4, top = 4, bottom = 4 },
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
end)
dialog:RegisterEvent("PLAYER_REGEN_DISABLED")
dialog:SetScript("OnEvent", function()
	if event == "PLAYER_REGEN_DISABLED" and dialog:IsShown() then
		dialog:Hide()
	end
end)
dialog:Hide()

local _originalToggleGameMenu = ToggleGameMenu
ToggleGameMenu = function()
	if dialog:IsShown() then
		dialog:Hide()
		return
	end
	_originalToggleGameMenu()
end

local title = dialog:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
title:SetPoint("TOPLEFT", 18, -18)
title:SetText("Panini Projection")
title:SetTextColor(1, 0.85, 0)

local close = CreateFrame("Button", "PaniniSettingsDialogClose", dialog, "UIPanelCloseButton")
close:SetPoint("TOPRIGHT", -4, -4)
close:SetScript("OnClick", function()
	dialog:Hide()
end)

local tabPageContainer = CreateFrame("Frame", "PaniniSettingsPageContainer", dialog)
tabPageContainer:SetPoint("TOPLEFT", 10, -42)
tabPageContainer:SetPoint("BOTTOMRIGHT", -10, 36)

local pageSettings = CreateFrame("Frame", "PaniniSettingsPageSettings", tabPageContainer)
pageSettings:SetAllPoints(tabPageContainer)

local y = -5

local cbEnabled = CreateCheckbox(
	pageSettings,
	"PaniniSettingsEnabled",
	"Enable Panini Projection",
	"Toggle panini/cylindrical camera projection",
	function()
		if this:GetChecked() then
			EnablePanini()
		else
			DisablePanini()
		end
	end
)
cbEnabled:SetPoint("TOPLEFT", 10, y)

y = y - 50

local sStrength =
	CreateSlider(pageSettings, "PaniniSettingsStrength", "Strength", "0", "0.10", 0, 0.10, "strength", "paniniStrength")
sStrength:SetPoint("TOPLEFT", 20, y)

y = y - 60

local sVert = CreateSlider(
	pageSettings,
	"PaniniSettingsVertical",
	"Vertical Comp",
	"-1",
	"1",
	-1,
	1,
	"verticalComp",
	"paniniVertComp"
)
sVert:SetPoint("TOPLEFT", 20, y)

y = y - 60

local sFill = CreateSlider(pageSettings, "PaniniSettingsFill", "Fill", "0", "1", 0, 1, "fill", "paniniFill")
sFill:SetPoint("TOPLEFT", 20, y)

y = y - 60

local sFov =
	CreateSlider(pageSettings, "PaniniSettingsFov", "FOV (rad)", "0.001", "3.133", 0.001, 3.133, "fov", "paniniFov", {})
sFov:SetPoint("TOPLEFT", 20, y)

y = y - 60

local cbFxaa = CreateCheckbox(pageSettings, "PaniniSettingsFxaa", "FXAA", "Fast approximate anti-aliasing", function()
	local c = GetConfig()
	if this:GetChecked() then
		c.fxaaEnabled = true
		SafeSetCVar("ppFxaa", "1")
	else
		c.fxaaEnabled = false
		SafeSetCVar("ppFxaa", "0")
	end
end)
cbFxaa:SetPoint("TOPLEFT", 10, y)

y = y - 35

local sSharpen = CreateSlider(
	pageSettings,
	"PaniniSettingsSharpen",
	"Sharpen",
	"0",
	"1",
	0,
	1,
	"sharpen",
	"ppSharpen",
	{ decimals = 2 }
)
sSharpen:SetPoint("TOPLEFT", 20, y)

y = y - 60

local pageDebug = CreateFrame("Frame", "PaniniSettingsPageDebug", tabPageContainer)
pageDebug:SetAllPoints(tabPageContainer)
pageDebug:Hide()

local y2 = -5

local cbTint = CreateCheckbox(
	pageDebug,
	"PaniniSettingsDebugTint",
	"Debug Tint (red -33%)",
	"Visualize panini effect with red channel reduction",
	function() end
)
cbTint:SetPoint("TOPLEFT", 10, y2)

y2 = y2 - 35

local cbUV = CreateCheckbox(
	pageDebug,
	"PaniniSettingsDebugUV",
	"Debug UV Visualization",
	"Show UV coordinate visualization",
	function() end
)
cbUV:SetPoint("TOPLEFT", 10, y2)

cbTint:SetScript("OnClick", function()
	if this:GetChecked() then
		SafeSetCVar("ppDebugTint", "1")
		SafeSetCVar("ppDebugUV", "0")
		cbUV:SetChecked(nil)
	else
		SafeSetCVar("ppDebugTint", "0")
	end
end)

cbUV:SetScript("OnClick", function()
	if this:GetChecked() then
		SafeSetCVar("ppDebugUV", "1")
		SafeSetCVar("ppDebugTint", "0")
		cbTint:SetChecked(nil)
	else
		SafeSetCVar("ppDebugUV", "0")
	end
end)

y2 = y2 - 50

local btnReset = CreateFrame("Button", "PaniniSettingsReset", pageDebug, "UIPanelButtonTemplate")
SetSize(btnReset, 130, 24)
btnReset:SetPoint("TOPLEFT", 20, y2)
btnReset:SetText("Reset Defaults")
btnReset:SetScript("OnClick", function()
	SlashCmdList["PANINI"]("reset settings")
	PaniniSettingsDialog_OnShow()
end)

y2 = y2 - 40

local divider1 = pageDebug:CreateTexture(nil, "ARTWORK")
divider1:SetTexture(1, 1, 1, 0.15)
divider1:SetHeight(1)
divider1:SetPoint("TOPLEFT", 10, y2)
divider1:SetPoint("TOPRIGHT", -10, y2)

y2 = y2 - 12

local aboutTitle = pageDebug:CreateFontString(nil, "OVERLAY", "GameFontNormal")
aboutTitle:SetPoint("TOP", pageDebug, "TOP", 0, y2)
aboutTitle:SetJustifyH("CENTER")


y2 = y2 - 16

local aboutTagline = pageDebug:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
aboutTagline:SetPoint("TOP", pageDebug, "TOP", 0, y2)
aboutTagline:SetJustifyH("CENTER")
aboutTagline:SetText("Cylindrical projection mapping for better,\nhigher field-of-view/FoV rendering.")
y2 = y2 - 28

local aboutUrl = CreateFrame("EditBox", nil, pageDebug)
aboutUrl:SetWidth(300)
aboutUrl:SetHeight(20)
aboutUrl:SetPoint("TOP", pageDebug, "TOP", 0, y2)
aboutUrl:SetFont("Fonts\\FRIZQT__.TTF", 10)
aboutUrl:SetJustifyH("CENTER")
aboutUrl:SetTextInsets(4, 4, 3, 3)
aboutUrl:SetAutoFocus(false)
aboutUrl:SetText("https://github.com/mannie-exe/panini-classic-wow")
aboutUrl:SetTextColor(0.5, 0.5, 0.5, 1)
aboutUrl:SetBackdrop({
	bgFile = "Interface\\ChatFrame\\ChatFrameBackground",
	edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
	tile = true,
	tileSize = 16,
	edgeSize = 6,
	insets = { left = 3, right = 3, top = 3, bottom = 3 },
})
aboutUrl:SetBackdropColor(0, 0, 0, 0.8)
aboutUrl:SetBackdropBorderColor(0.4, 0.4, 0.4, 1)
aboutUrl:EnableMouse(true)
aboutUrl:SetScript("OnEditFocusGained", function()
	this:HighlightText()
end)
aboutUrl:SetScript("OnTextChanged", function()
	this:SetText("https://github.com/mannie-exe/panini-classic-wow")
	this:HighlightText()
end)
aboutUrl:SetScript("OnEscapePressed", function()
	this:ClearFocus()
end)

y2 = y2 - 20

local aboutSetup = pageDebug:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
aboutSetup:SetPoint("TOPLEFT", pageDebug, "TOPLEFT", 20, y2)
aboutSetup:SetPoint("TOPRIGHT", pageDebug, "TOPRIGHT", -20, y2)
aboutSetup:SetJustifyH("LEFT")
aboutSetup:SetText(
	"|cFFAAAAAASetup:|r\n"
		.. "|cFF8080801.|r Copy PaniniClassicWoW.dll to WoW/mods/\n"
		.. '|cFF8080802.|r Add "mods/PaniniClassicWoW.dll" to WoW/dlls.txt\n'
		.. "|cFF8080803.|r Requires d3d9.dll loader (DXVK or vanilla-tweaks)\n"
		.. "|cFF8080804.|r /reload or restart WoW"
)

-- Compute dialog height from page content + chrome (title bar 42 + tab strip 23)
-- 5 lines of GameFontDisableSmall (~12px each) in aboutSetup FontString
local setupTextHeight = 60
local settingsContentHeight = math.abs(y)
local debugContentHeight = math.abs(y2) + setupTextHeight
local contentHeight = math.max(settingsContentHeight, debugContentHeight)
dialog:SetHeight(contentHeight + 65)

local function CreateTabButton(parent, name, label, pageFrame, selected)
	local tab = CreateFrame("Button", name, parent)
	SetSize(tab, 80, 28)
	tab:SetText(label)
	local fs = tab:GetFontString()
	if fs then
		fs:SetFontObject("GameFontNormalSmall")
	end

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
tab1:SetPoint("BOTTOMLEFT", 118, 12)

local tab2 = CreateTabButton(dialog, "PaniniSettingsTabDebug", "Debug", pageDebug, false)
tab2:SetPoint("LEFT", tab1, "RIGHT", 4, 0)

local tabs = { tab1, tab2 }

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

function PaniniSettingsDialog_OnShow()
	local c = GetConfig()

	cbEnabled:SetChecked(c.enabled and 1 or nil)
	sStrength:SetValue(c.strength or 0.01)
	sVert:SetValue(c.verticalComp or 0)
	sFill:SetValue(c.fill or 1.0)
	sFov:SetValue(c.fov or 2.82)
	cbFxaa:SetChecked(c.fxaaEnabled and 1 or nil)
	sSharpen:SetValue(c.sharpen or 0.2)

	local tintVal = SafeGetCVar("ppDebugTint")
	cbTint:SetChecked(tintVal == "1" and 1 or nil)

	local uvVal = SafeGetCVar("ppDebugUV")
	cbUV:SetChecked(uvVal == "1" and 1 or nil)

	local dllVersion = SafeGetCVar("ppVersion")
	if dllVersion and dllVersion ~= "" then
		aboutTitle:SetText("|cFFFFD900v" .. dllVersion .. "|r")
	else
		aboutTitle:SetText("|cFFFF4D4DDLL NOT LOADED|r")
	end
end

dialog:SetScript("OnShow", PaniniSettingsDialog_OnShow)

function PaniniClassicWoW.ResetDialogPosition()
	dialog:ClearAllPoints()
	dialog:SetPoint("CENTER", UIParent, "CENTER")
end

function PaniniClassicWoW.ToggleSettings()
	if dialog:IsShown() then
		dialog:Hide()
	else
		dialog:Show()
	end
end

local ioPanel = CreateFrame("Frame", "PaniniSettingsIOPanel", UIParent)
ioPanel.name = "Panini Projection"

if InterfaceOptions_AddCategory then
	InterfaceOptions_AddCategory(ioPanel)
end

ioPanel:SetScript("OnShow", function()
	PaniniClassicWoW.ToggleSettings()
end)
