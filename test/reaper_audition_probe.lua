local source=debug.getinfo(1,'S').source:sub(2)
local dir=source:match('^(.*[/\\])')
local root=dir..'../'
local log=assert(io.open(root..'build/reaper_audition_probe.txt','w'))
local ok,err=pcall(function()
 local sent,clock,callback,key,exitfn={},0,nil,0,nil
 local track={}
 reaper={
  CountTracks=function() return 0 end, InsertTrackAtIndex=function() end,
  GetTrack=function() return track end,GetSetMediaTrackInfo_String=function() end,
  TrackFX_AddByName=function() return 0 end, SetMediaTrackInfo_Value=function(_,k,v) track[k]=v end,
  SetOnlyTrackSelected=function() end,TrackList_AdjustWindows=function() end,
  TrackFX_Show=function() end, StuffMIDIMessage=function(_,status,note,vel) sent[#sent+1]={status,note,vel} end,
  time_precise=function() return clock end, defer=function(f) callback=f end, atexit=function(f) exitfn=f end,
 }
 gfx={w=740,h=325,mouse_x=0,mouse_y=0,mouse_cap=0,init=function() end,getchar=function() local c=key;key=0;return c end,
      set=function() end,rect=function() end,setfont=function() end,drawstr=function() end,update=function() end}
 dofile(root..'tools/reaper_audition.lua')
 assert(track.I_RECARM==1 and track.I_RECMON==1 and track.I_RECINPUT==4096+62*32,'virtual MIDI track setup')
 key=string.byte('a');callback();assert(sent[#sent][1]==0x90 and sent[#sent][2]==48,'keyboard note on')
 clock=.4;callback();assert(sent[#sent][1]==0x80 and sent[#sent][2]==48,'timed note off')
 gfx.mouse_x=30;gfx.mouse_y=100;gfx.mouse_cap=1;callback();gfx.mouse_cap=0;callback()
 assert(sent[#sent][1]==0x90 and sent[#sent][2]==48,'phrase starts')
 clock=.8;callback();assert(sent[#sent][1]==0x90 and sent[#sent][2]==52,'phrase advances')
 gfx.mouse_cap=1;callback();gfx.mouse_cap=0;callback();assert(sent[#sent][1]==0x80,'phrase stop releases note')
 key=string.byte('s');callback();exitfn();assert(sent[#sent][1]==0x80 and sent[#sent][2]==50,'close releases note')
end)
log:write(ok and 'PASS: audition track, keyboard, phrase, timing, and cleanup\n' or ('FAIL: '..tostring(err)..'\n'));log:close()
