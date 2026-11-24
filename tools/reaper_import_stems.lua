-- REAPER Lua Importer for Monkey's Ear Aligned Stem Bundles (manifest.json + stem_XX.wav)
-- Run interactively (picks manifest.json) or headless with MONKEYS_EAR_STEM_DIR env var.

local function read_all(path)
    local f = io.open(path, "rb")
    if not f then return nil end
    local text = f:read("*a")
    f:close()
    return text
end

local function write_report(path, text)
    if not path or path == "" then return end
    local f = io.open(path, "wb")
    if f then
        f:write(text)
        f:close()
    end
end

local function main()
    local stem_dir = os.getenv("MONKEYS_EAR_STEM_DIR") or ""
    local report_path = os.getenv("MONKEYS_EAR_STEM_REPORT") or ""

    if stem_dir == "" then
        local ok, chosen = reaper.GetUserFileNameForRead("", "Select Monkey's Ear manifest.json", "json")
        if not ok or not chosen or chosen == "" then return end
        stem_dir = chosen:match("^(.*)[/\\][^/\\]+$") or ""
    end
    if stem_dir == "" then
        write_report(report_path, "FAIL: no stem directory selected\n")
        return
    end

    local sep = package.config:sub(1, 1)
    local manifest_path = stem_dir .. sep .. "manifest.json"
    local raw = read_all(manifest_path)
    if not raw then
        write_report(report_path, "FAIL: missing " .. manifest_path .. "\n")
        reaper.ShowMessageBox("Could not open manifest.json in " .. stem_dir, "Monkey's Ear Stem Import", 0)
        return
    end

    local bpm = tonumber(raw:match('"tempo_bpm"%s*:%s*([%d%.]+)')) or 120.0
    local num, den = raw:match('"meter"%s*:%s*%[(%d+)%s*,%s*(%d+)%]')
    num = tonumber(num) or 4
    den = tonumber(den) or 4

    reaper.Undo_BeginBlock()
    reaper.SetCurrentBPM(0, bpm, true)
    if reaper.SetTempoTimeSigMarker then
        reaper.SetTempoTimeSigMarker(0, -1, 0.0, -1, -1, bpm, num, den, false)
    end

    local count = 0
    for name, file in raw:gmatch('"name"%s*:%s*"([^"]+)"%s*,%s*"file"%s*:%s*"([^"]+)"') do
        local wav_path = stem_dir .. sep .. file
        local track_idx = reaper.CountTracks(0)
        reaper.InsertTrackAtIndex(track_idx, true)
        local track = reaper.GetTrack(0, track_idx)
        reaper.GetSetMediaTrackInfo_String(track, "P_NAME", name, true)
        local item = reaper.AddMediaItemToTrack(track)
        local take = reaper.AddTakeToMediaItem(item)
        local src = reaper.PCM_Source_CreateFromFile(wav_path)
        if src then
            reaper.SetMediaItemTake_Source(take, src)
            local len = reaper.GetMediaSourceLength(src)
            reaper.SetMediaItemInfo_Value(item, "D_POSITION", 0.0)
            reaper.SetMediaItemInfo_Value(item, "D_LENGTH", len)
        end
        count = count + 1
    end
    reaper.UpdateTimeline()
    reaper.Undo_EndBlock("Import Monkey's Ear Stems", -1)

    local msg = string.format("PASS: imported %d aligned stems at %.2f BPM (%d/%d) from %s\n",
                              count, bpm, num, den, stem_dir)
    write_report(report_path, msg)
end

main()
