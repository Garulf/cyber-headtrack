-- CyberHeadtrack settings window, shown while the Cyber Engine Tweaks overlay is open.
-- Each profile (on foot, vehicle) has its own settings; the one in use switches
-- automatically when V gets in or out of a vehicle. Lean (moving the camera with your head
-- position) starts off for vehicles. Eye tracking in scan mode is shared by both profiles.
-- Changes apply immediately and are written to config.toml when the overlay closes.

local PROFILES = {
    { id = "on_foot", label = "On foot" },
    { id = "vehicle", label = "Vehicle" },
}

local SECTIONS = {
    {
        title = "Rotation",
        sliders = {
            { key = "smoothing_min_cutoff_hz", label = "Smoothing at rest (Hz)", min = 0.05, max = 5.0, format = "%.2f" },
            { key = "smoothing_beta", label = "Speed response (beta)", min = 0.0, max = 1.0, format = "%.3f" },
            { key = "yaw_scale", label = "Yaw scale (turn)", min = 0.0, max = 5.0, format = "%.2fx" },
            { key = "pitch_scale", label = "Pitch scale (nod)", min = 0.0, max = 5.0, format = "%.2fx" },
            { key = "roll_scale", label = "Roll scale (tilt)", min = 0.0, max = 5.0, format = "%.2fx" },
            { key = "max_yaw_deg", label = "Max yaw (deg)", min = 0.0, max = 180.0, format = "%.0f" },
            { key = "max_pitch_deg", label = "Max pitch (deg)", min = 0.0, max = 180.0, format = "%.0f" },
            { key = "max_roll_deg", label = "Max roll (deg)", min = 0.0, max = 180.0, format = "%.0f" },
        },
        flags = {
            { key = "invert_yaw", label = "Invert yaw (turn)" },
            { key = "invert_pitch", label = "Invert pitch (nod)" },
            { key = "invert_roll", label = "Invert roll (tilt)" },
        },
    },
    {
        title = "Lean (head position)",
        flags = {
            { key = "lean_enabled", label = "Lean enabled" },
        },
        sliders = {
            { key = "lean_smoothing_min_cutoff_hz", label = "Lean smoothing at rest (Hz)", min = 0.05, max = 5.0, format = "%.2f" },
            { key = "lean_smoothing_beta", label = "Lean speed response (beta)", min = 0.0, max = 1.0, format = "%.3f" },
            { key = "lean_scale", label = "Lean scale", min = 0.0, max = 5.0, format = "%.2fx" },
            { key = "max_lean_side_cm", label = "Max side (cm)", min = 0.0, max = 50.0, format = "%.0f" },
            { key = "max_lean_up_cm", label = "Max up/down (cm)", min = 0.0, max = 50.0, format = "%.0f" },
            { key = "max_lean_forward_cm", label = "Max forward/back (cm)", min = 0.0, max = 50.0, format = "%.0f" },
        },
        trailingFlags = {
            { key = "invert_lean_side", label = "Invert side" },
            { key = "invert_lean_up", label = "Invert up/down" },
            { key = "invert_lean_forward", label = "Invert forward/back" },
        },
    },
    {
        title = "Aiming down sights",
        flags = {
            { key = "ads_tracking_off", label = "Head tracking off while aiming" },
        },
        sliders = {
            { key = "ads_ease_seconds", label = "Ease time (s)", min = 0.0, max = 2.0, format = "%.2f" },
        },
        choices = {
            { key = "ads_eye", label = "Aiming eye", options = { { -1, "Left" }, { 0, "Centre" }, { 1, "Right" } } },
        },
        trailingSliders = {
            { key = "ads_eye_offset_cm", label = "Eye offset (cm)", min = 0.0, max = 10.0, format = "%.1f" },
        },
    },
}

-- Not a profile: its settings are "gaze.<key>" and apply on foot and in vehicles alike.
local GAZE = { id = "gaze" }

local GAZE_SECTION = {
    title = "Eye tracking (scan mode)",
    flags = {
        { key = "scan_enabled", label = "Scanner reticle follows your eyes" },
        { key = "sticky_lock", label = "Keep the target while looking away" },
    },
    sliders = {
        { key = "lock_radius", label = "Lock radius (screen heights)", min = 0.01, max = 0.3, format = "%.3f" },
        { key = "switch_dwell_seconds", label = "Switch delay (s)", min = 0.0, max = 2.0, format = "%.2f" },
        { key = "smoothing_min_cutoff_hz", label = "Smoothing at rest (Hz)", min = 0.05, max = 5.0, format = "%.2f" },
        { key = "smoothing_beta", label = "Speed response (beta)", min = 0.0, max = 5.0, format = "%.3f" },
    },
}

local overlayOpen = false
local unsaved = false
local message = ""
local selected = 1
local values = {}

local function name(profile, key)
    return profile.id .. "." .. key
end

local function settingLists(section)
    return { section.flags or {}, section.sliders or {}, section.choices or {},
             section.trailingSliders or {}, section.trailingFlags or {} }
end

local function loadSection(profile, section)
    for _, list in ipairs(settingLists(section)) do
        for _, setting in ipairs(list) do
            values[name(profile, setting.key)] = HeadtrackSettings.Get(name(profile, setting.key))
        end
    end
end

local function loadValues()
    for _, profile in ipairs(PROFILES) do
        for _, section in ipairs(SECTIONS) do
            loadSection(profile, section)
        end
    end
    loadSection(GAZE, GAZE_SECTION)
end

local function apply(setting, value)
    local problem = HeadtrackSettings.Set(setting, value)
    if problem == "" then
        values[setting] = value
        unsaved = true
        message = ""
    else
        message = problem
    end
end

local function drawFlags(profile, flags)
    for _, flag in ipairs(flags or {}) do
        local key = name(profile, flag.key)
        local checked, pressed = ImGui.Checkbox(flag.label, values[key] ~= 0)
        if pressed then
            apply(key, checked and 1 or 0)
        end
    end
end

local function drawSliders(profile, sliders)
    for _, setting in ipairs(sliders or {}) do
        local key = name(profile, setting.key)
        local value, changed = ImGui.SliderFloat(setting.label, values[key], setting.min, setting.max, setting.format)
        if changed then
            apply(key, value)
        end
    end
end

local function drawChoices(profile, choices)
    for _, choice in ipairs(choices or {}) do
        local key = name(profile, choice.key)
        ImGui.Text(choice.label)
        for _, option in ipairs(choice.options) do
            ImGui.SameLine()
            if ImGui.RadioButton(option[2] .. "##" .. choice.key, values[key] == option[1]) then
                apply(key, option[1])
            end
        end
    end
end

local function save()
    if not unsaved then
        return
    end
    local problem = HeadtrackSettings.Save()
    unsaved = problem ~= ""
    message = problem == "" and "Saved to config.toml" or ("Save failed: " .. problem)
end

registerForEvent("onOverlayOpen", function()
    overlayOpen = true
    selected = HeadtrackSettings.ActiveProfile() + 1
    loadValues()
end)

registerForEvent("onOverlayClose", function()
    overlayOpen = false
    save()
end)

registerForEvent("onDraw", function()
    if not overlayOpen then
        return
    end
    if ImGui.Begin("CyberHeadtrack") then
        local enabled, toggled = ImGui.Checkbox("Tracking enabled", HeadtrackSettings.IsEnabled())
        if toggled then
            HeadtrackSettings.SetEnabled(enabled)
        end
        ImGui.SameLine()
        if ImGui.Button("Recenter") then
            HeadtrackSettings.Recenter()
        end

        ImGui.Separator()
        local active = HeadtrackSettings.ActiveProfile() + 1
        for index, profile in ipairs(PROFILES) do
            local label = profile.label
            if index == active then
                label = label .. " (in use)"
            end
            if ImGui.RadioButton(label, selected == index) then
                selected = index
            end
            ImGui.SameLine()
        end
        ImGui.NewLine()

        local profile = PROFILES[selected]
        ImGui.PushID(profile.id)
        for _, section in ipairs(SECTIONS) do
            ImGui.Separator()
            ImGui.Text(section.title)
            drawFlags(profile, section.flags)
            drawSliders(profile, section.sliders)
            drawChoices(profile, section.choices)
            drawSliders(profile, section.trailingSliders)
            drawFlags(profile, section.trailingFlags)
        end
        ImGui.PopID()

        ImGui.Separator()
        ImGui.PushID(GAZE.id)
        ImGui.Text(GAZE_SECTION.title)
        drawFlags(GAZE, GAZE_SECTION.flags)
        drawSliders(GAZE, GAZE_SECTION.sliders)
        ImGui.PopID()

        if message ~= "" then
            ImGui.Separator()
            ImGui.TextWrapped(message)
        end
    end
    ImGui.End()
end)
