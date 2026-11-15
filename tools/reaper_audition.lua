-- Monkey's Ear audition panel for REAPER. Uses the host's Virtual MIDI Keyboard;
-- MIDI is sent to armed tracks listening to that input, just like REAPER's own.
local r = reaper
local name = "VST3i: Monkey's Ear (Ultramonkeydog Studios)"
local n = r.CountTracks(0)
r.InsertTrackAtIndex(n, true)
local track = r.GetTrack(0, n)
r.GetSetMediaTrackInfo_String(track, 'P_NAME', "Monkey's Ear - Audition", true)
local fx = r.TrackFX_AddByName(track, name, false, -1)
if fx < 0 then
  r.DeleteTrack(track)
  r.ShowMessageBox("Monkey's Ear was not found. Install the current VST3 and rescan in REAPER.", "Audition", 0)
  return
end
r.SetMediaTrackInfo_Value(track, 'I_RECARM', 1)
r.SetMediaTrackInfo_Value(track, 'I_RECMON', 1)
-- VST3 MIDI input device 62 is REAPER's virtual keyboard, all channels.
r.SetMediaTrackInfo_Value(track, 'I_RECINPUT', 4096 + 62 * 32)
r.SetOnlyTrackSelected(track)
r.TrackList_AdjustWindows(false)
r.TrackFX_Show(track, fx, 3)

local base, velocity = 48, 100
local active, end_time, looping, next_step, step = nil, 0, false, 0, 1
local mouse_down = false
local phrase = {0, 4, 7, 12, 7, 4, 2, 7}
local qwerty = {a=0,w=1,s=2,e=3,d=4,f=5,t=6,g=7,y=8,h=9,u=10,j=11,k=12}
local labels = {'C','C#','D','D#','E','F','F#','G','G#','A','A#','B','C'}
local function note_off()
  if active then r.StuffMIDIMessage(0, 0x80, active, 0); active=nil end
end
local function play(note, duration)
  note_off()
  active=math.max(0, math.min(127, note))
  r.StuffMIDIMessage(0, 0x90, active, velocity)
  end_time=r.time_precise()+duration
end
local function stop_loop() looping=false; note_off() end
local function button(x,y,w,h,text,lit)
  gfx.set(lit and .14 or .13, lit and .38 or .17, lit and .34 or .20, 1)
  gfx.rect(x,y,w,h,1)
  gfx.set(.88,.92,.91,1); gfx.x=x+10; gfx.y=y+10; gfx.drawstr(text)
end
local function hit(x,y,w,h) return gfx.mouse_x>=x and gfx.mouse_x<x+w and gfx.mouse_y>=y and gfx.mouse_y<y+h end
local function draw()
  gfx.set(.055,.07,.08,1); gfx.rect(0,0,gfx.w,gfx.h,1)
  gfx.setfont(1,'Arial',19,'b'); gfx.set(.9,.95,.93,1); gfx.x=18;gfx.y=14;gfx.drawstr("MONKEY'S EAR  /  AUDITION")
  gfx.setfont(2,'Arial',14)
  gfx.set(.69,.76,.75,1); gfx.x=18;gfx.y=44;gfx.drawstr('Click a note or use A W S E D F T G Y H U J K while this panel is focused.')
  gfx.x=18;gfx.y=64;gfx.drawstr('Shape the patch in the instrument window; the phrase keeps playing.')
  button(18,92,162,38,looping and 'STOP PHRASE' or 'PLAY PHRASE',looping)
  button(190,92,122,38,'OCTAVE  -',false)
  button(320,92,122,38,'OCTAVE  +',false)
  button(450,92,170,38,'OPEN INSTRUMENT',false)
  gfx.set(.72,.8,.77,1); gfx.x=18;gfx.y=145;gfx.drawstr('Octave: C'..tostring(math.floor(base/12)-1)..'   |   Velocity: 100')
  local margin=18; local width=(gfx.w-36)/13
  for i=0,12 do
    local x=margin+i*width
    local black=(i==1 or i==3 or i==6 or i==8 or i==10)
    gfx.set(black and .18 or .81,black and .22 or .87,black and .24 or .85,1)
    gfx.rect(x,175,width-3,105,1)
    gfx.set(black and .85 or .12,black and .9 or .18,black and .88 or .16,1)
    gfx.x=x+4;gfx.y=250;gfx.drawstr(labels[i+1])
  end
  gfx.set(.69,.76,.75,1); gfx.x=18;gfx.y=297
  gfx.drawstr('Uses REAPER virtual MIDI: other armed tracks listening to it can also receive notes.')
  gfx.update()
end
local function run()
  local ch=gfx.getchar()
  if ch<0 or ch==27 then stop_loop(); return end
  local now=r.time_precise()
  if active and now>=end_time then note_off() end
  if looping and now>=next_step then
    play(base+phrase[step],.24); step=step%#phrase+1; next_step=now+.35
  end
  if ch>0 and ch<128 then
    local k=string.char(ch):lower()
    if qwerty[k] then stop_loop();play(base+qwerty[k],.35)
    elseif k==' ' then
      if looping then stop_loop() else looping=true;step=1;next_step=now end
    end
  end
  local down=(gfx.mouse_cap & 1)==1
  if down and not mouse_down then
    if hit(18,92,162,38) then
      if looping then stop_loop() else looping=true;step=1;next_step=now end
    elseif hit(190,92,122,38) then stop_loop();base=math.max(24,base-12)
    elseif hit(320,92,122,38) then stop_loop();base=math.min(96,base+12)
    elseif hit(450,92,170,38) then r.TrackFX_Show(track,fx,3)
    elseif hit(18,175,gfx.w-36,105) then
      stop_loop();play(base+math.min(12,math.floor((gfx.mouse_x-18)/((gfx.w-36)/13))),.35)
    end
  end
  mouse_down=down
  draw()
  r.defer(run)
end
r.atexit(note_off)
gfx.init("Monkey's Ear - Audition", 740, 325, 0)
run()
