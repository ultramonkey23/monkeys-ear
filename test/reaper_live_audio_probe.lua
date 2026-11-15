-- REAPER live monitoring smoke: generated virtual MIDI to a temporary VST3i track.
-- The test can be silent when the host audio device is unavailable; that is a failure, not a pass.
local r=reaper
local source=debug.getinfo(1,'S').source:sub(2)
local root=source:match('^(.*[/\\])')..'../'
local f=assert(io.open(root..'build/reaper_live_audio_probe.txt','w'))
local idx=r.CountTracks(0);r.InsertTrackAtIndex(idx,true)
local t=r.GetTrack(0,idx)
local fx=r.TrackFX_AddByName(t,"VST3i: Monkey's Ear (Ultramonkeydog Studios)",false,-1)
if fx<0 then f:write('FAIL: instrument not found\n');f:close();r.DeleteTrack(t);return end
local _,fxname=r.TrackFX_GetFXName(t,fx,'')
f:write('instrument='..fxname..' enabled='..tostring(r.TrackFX_GetEnabled(t,fx))..' params='..r.TrackFX_GetNumParams(t,fx)..'\n')
for _,i in ipairs({0,6,8,18,173}) do local _,name=r.TrackFX_GetParamName(t,fx,i,'');f:write('param '..i..' '..name..'='..tostring(r.TrackFX_GetParamNormalized(t,fx,i))..'\n') end
r.SetMediaTrackInfo_Value(t,'I_RECARM',1);r.SetMediaTrackInfo_Value(t,'I_RECMON',1)
r.SetMediaTrackInfo_Value(t,'I_RECINPUT',4096+62*32)
local idx2=r.CountTracks(0);r.InsertTrackAtIndex(idx2,true);local reference=r.GetTrack(0,idx2)
local ref_fx=r.TrackFX_AddByName(reference,'VSTi: ReaSynth (Cockos)',false,-1)
r.SetMediaTrackInfo_Value(reference,'I_RECARM',1);r.SetMediaTrackInfo_Value(reference,'I_RECMON',1)
r.SetMediaTrackInfo_Value(reference,'I_RECINPUT',4096+62*32)
local start=r.time_precise();local peak,ref_peak=0,0;local sent=false;local off=false;local done=false;local monitored=false
local function cleanup()
 if done then return end;done=true
 if sent and not off then r.StuffMIDIMessage(0,0x80,60,0) end
 if r.ValidatePtr(t,'MediaTrack*') then r.DeleteTrack(t) end
 if r.ValidatePtr(reference,'MediaTrack*') then r.DeleteTrack(reference) end
 f:write('peak='..tostring(peak)..'\n')
 f:write('reference_fx='..tostring(ref_fx)..' reference_peak='..tostring(ref_peak)..'\n')
 f:write('audio_running='..tostring(r.Audio_IsRunning())..' monitor_effective='..tostring(monitored)..'\n')
 -- REAPER's meters have a nonzero silence floor (~1.94e-5 here).
 f:write(peak>0.001 and 'PASS: generated REAPER virtual MIDI reached instrument audio\n' or 'FAIL: no monitored output observed; inspect audio device, MIDI and monitoring\n')
 f:close()
 if MONKEYS_EAR_LAUNCH_AFTER_PROBE then dofile(root..'tools/reaper_audition.lua') end
end
r.atexit(cleanup)
local function loop()
 local elapsed=r.time_precise()-start
 if r.GetMediaTrackInfo_Value(t,'B_RECMON_IN_EFFECT')>0 then monitored=true end
 if elapsed>.4 and not sent then r.StuffMIDIMessage(0,0x90,60,100);sent=true end
 if sent and not off then peak=math.max(peak,r.Track_GetPeakInfo(t,0),r.Track_GetPeakInfo(t,1));ref_peak=math.max(ref_peak,r.Track_GetPeakInfo(reference,0),r.Track_GetPeakInfo(reference,1)) end
 if elapsed>1.2 and sent and not off then r.StuffMIDIMessage(0,0x80,60,0);off=true end
 if elapsed>1.6 then cleanup();return end
 r.defer(loop)
end
loop()
