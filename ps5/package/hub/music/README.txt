The hub's music. The first .wav here (in name order) plays behind the hub
while SETTINGS > Game > Hub music is on: 16-bit PCM or 32-bit float, 44.1 or
48 kHz, mono or stereo. A smpl chunk's loop start, if the file has one, is
where the music loops back to, so an opening plays once.

The repository ships no music. ps5/tools/make_hub_music.py makes a looping
WAV from an MP3 or WAV of your choice (it finds the song's repeat and writes
the loop point):

    python3 ps5/tools/make_hub_music.py song.mp3 ps5/package/hub/music/theme.wav
