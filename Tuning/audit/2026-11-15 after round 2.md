# Audio audit 2026-11-15 14.34

One note (MIDI 60, velocity 0.8) held 1.5 s then released, 4 s in all, at 48 kHz through the voices and the effect chain. onset = the first millisecond's peak against the note's peak (0 = a hard edge); attack = time to 90% of the held level; release = time from note-off to -40 dB; shape = t(-6 dB) / t(-30 dB) of the release (0.2 exponential, 0.5 linear); onset, attack, release and shape are measured on the dry voice, the rest after the effects; alias = oscillator A alone at C7, energy between the harmonics against the harmonics.

| patch | kind | rms dB | peak dB | onset dB | attack ms | release ms | shape | centroid Hz | >5k % | <150 % | width | alias dB | flags |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Dark Techno Rumble | Bass | -22.0 | -11.0 | -58.7 | 720.0 | 1868 | 0.21 | 280.936 | 0.0 | 29.5 | 0.14 | -88.7 |  |
| Deep Pulse | Bass | -17.8 | -10.1 | -48.3 | 180.0 | 2500 | 0.00 | 267.746 | 0.0 | 70.9 | 0.10 | -89.1 | still ringing |
| Drenched Bass Pulse | Bass | -21.3 | -7.5 | -44.7 | 298.0 | 1468 | 0.11 | 753.371 | 0.0 | 24.3 | 0.09 | -91.4 |  |
| Gentle Moog Bass | Bass | -25.9 | -12.9 | -36.5 | 78.0 | 696 | 0.02 | 158.242 | 0.0 | 70.8 | 0.16 | -91.1 |  |
| Grit Rumble | Bass | -38.1 | -13.4 | -25.0 | 8.0 | 0 | 0.00 | 422.93 | 0.0 | 4.1 | 0.03 | -91.2 | quiet, no low end |
| Gritty FM Bass | Bass | -35.6 | -9.0 | -23.7 | 18.0 | 0 | 0.00 | 1176.34 | 0.1 | 3.2 | 0.58 | -89.3 | quiet, no low end |
| Growling Wobble | Bass | -22.5 | -7.9 | -23.5 | 6.0 | 0 | 0.00 | 13.2019 | 0.0 | 100.0 | 0.00 | -88.0 |  |
| Hammond Rumble | Bass | -18.1 | -6.5 | -55.5 | 82.0 | 1268 | 0.14 | 165.404 | 0.0 | 61.2 | 0.09 | -91.4 |  |
| Metallic Thrum | Bass | -26.8 | -12.9 | -61.9 | 1218.0 | 1784 | 0.02 | 153.592 | 0.0 | 80.9 | 0.04 | -91.4 |  |
| Neuro Grit Bass | Bass | -29.2 | -7.3 | -30.2 | 28.0 | 0 | 0.00 | 721.731 | 0.0 | 15.5 | 0.06 | -89.4 |  |
| Pulse Bounce | Bass | -33.9 | -6.4 | -34.2 | 24.0 | 0 | 0.00 | 729.726 | 0.3 | 1.4 | 0.03 | -90.2 | quiet, no low end |
| Rubbery FM Bass | Bass | -32.5 | -11.0 | -35.5 | 42.0 | 0 | 0.00 | 611.828 | 0.0 | 24.4 | 0.03 | -89.2 | quiet |
| Slow Tape Hum | Bass | -22.7 | -13.2 | -51.5 | 680.0 | 2500 | 0.00 | 469.88 | 0.0 | 70.1 | 0.07 | -89.1 | still ringing |
| Vibrant Bass | Bass | -37.1 | -8.6 | -28.2 | 8.0 | 0 | 0.00 | 572.93 | 0.0 | 1.1 | 0.10 | -91.3 | quiet, no low end |
| Vintage Engine | Bass | -21.2 | -9.5 | -65.8 | 1396.0 | 2500 | 0.07 | 431.302 | 0.0 | 42.5 | 0.05 | -90.4 | still ringing |
| Warm Analog Bass | Bass | -14.4 | -9.9 | -46.8 | 288.0 | 802 | 0.26 | 370.894 | 0.3 | 42.6 | 0.10 | -91.5 |  |
| Warm Funky Bass | Bass | -11.8 | -4.6 | -53.0 | 60.0 | 294 | 0.05 | 129.427 | 0.0 | 96.8 | 0.04 | -90.2 |  |
| Warm Moog Bass | Bass | -21.6 | -9.5 | -43.1 | 138.0 | 692 | 0.09 | 359.349 | 0.0 | 26.0 | 0.10 | -90.4 |  |
| Wobble Growl | Bass | -32.2 | -8.9 | -28.5 | 10.0 | 0 | 0.00 | 13.2019 | 0.0 | 100.0 | 0.10 | -90.4 | quiet |
| Wobbly Bass Drums | Bass | -19.4 | -10.9 | -51.9 | 268.0 | 416 | 0.00 | 220.664 | 0.0 | 53.5 | 0.08 | -91.4 |  |
| Growling Bass | Bass | -34.5 | -10.2 | -22.8 | 20.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.08 | -88.8 | quiet, no low end |
| Squelchy Acid Bass | Bass | -31.0 | -9.5 | -39.4 | 40.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.02 | -89.1 | quiet, no low end |
| Sub Growl | Bass | -31.6 | -6.3 | -45.7 | 8.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.03 | -89.1 | quiet, no low end |
| Wobble Bass | Bass | -30.8 | -10.1 | -30.1 | 44.0 | 0 | 0.00 | 274.898 | 0.0 | 46.0 | 0.19 | -91.3 | quiet |
| Wobbling Bass | Bass | -25.2 | -10.1 | -31.7 | 28.0 | 0 | 0.00 | 412.583 | 0.2 | 58.1 | 0.01 | -89.1 |  |
| Airborne Chime | Bell | -41.9 | -27.1 | -20.0 | 14.0 | 160 | 0.24 | 477.473 | 0.0 | 0.0 | 0.16 | -91.5 | quiet |
| Cold Glass Bell | Bell | -15.8 | -1.4 | -43.4 | 210.0 | 1986 | 0.24 | 488.262 | 0.0 | 1.0 | 0.58 | -91.1 |  |
| Deep Glass Bell | Bell | -26.2 | -10.7 | -37.0 | 48.0 | 666 | 0.08 | 383.758 | 0.4 | 45.0 | 0.17 | -91.3 |  |
| Deep Siren | Bell | -27.8 | -8.9 | -25.3 | 928.0 | 484 | 0.00 | 2839.79 | 12.7 | 0.0 | 0.20 | -89.1 |  |
| Distant Chime | Bell | -22.1 | -8.9 | -65.6 | 1330.0 | 1692 | 0.09 | 1308.41 | 0.1 | 0.0 | 0.40 | -91.4 |  |
| Frosted Chime | Bell | -30.7 | -9.8 | -16.0 | 6.0 | 0 | 0.00 | 284.011 | 0.0 | 0.1 | 0.00 | -91.4 | quiet, dull |
| Gentle Chime | Bell | -33.3 | -9.6 | -22.1 | 10.0 | 0 | 0.00 | 265.165 | 0.0 | 1.9 | 0.06 | -91.5 | quiet, dull |
| Glass Bell | Bell | -34.2 | -11.8 | -23.0 | 6.0 | 0 | 0.00 | 295.165 | 0.0 | 0.0 | 0.21 | -91.0 | quiet, dull |
| Glassy Whisper | Bell | -43.6 | -26.2 | -33.4 | 42.0 | 174 | 0.25 | 988.24 | 0.0 | 0.8 | 0.10 | -91.2 | quiet |
| Metallic Chime | Bell | -20.5 | -8.6 | -42.5 | 856.0 | 268 | 0.01 | 500.733 | 0.0 | 0.5 | 0.14 | -91.4 |  |
| Wobbly Siren | Bell | -19.1 | -9.4 | -31.5 | 34.0 | 538 | 0.15 | 334.396 | 0.9 | 84.3 | 0.07 | -89.0 | dull |
| Crystal Chime | Bell | -34.7 | -9.8 | -16.2 | 8.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.08 | -91.3 | quiet, dull |
| Crystal Ring | Bell | -38.5 | -10.3 | -13.3 | 4.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.00 | -90.2 | quiet, dull |
| Metallic Bell | Bell | -21.3 | -9.1 | -49.0 | 224.0 | 2254 | 0.05 | 1313.11 | 0.0 | 0.0 | 0.49 | -91.1 | still ringing |
| Metallic Ring | Bell | -33.6 | -10.6 | -22.4 | 16.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.12 | -91.4 | quiet, dull |
| Vocal Bell | Bell | -23.6 | -13.2 | -57.2 | 112.0 | 1380 | 0.18 | 493.772 | 0.0 | 3.4 | 0.52 | -88.0 |  |
| Vocal Chime | Bell | -35.0 | -11.7 | -25.3 | 6.0 | 224 | 0.07 | 931.707 | 0.0 | 0.0 | 0.00 | -88.0 | quiet |
| Storm Echoes | Drone | -18.5 | -8.5 | -64.5 | 1228.0 | 2500 | 0.00 | 235.142 | 0.1 | 65.8 | 0.09 | -90.9 | dull, still ringing |
| Tape Worn Drums | Drone | -18.4 | -10.6 | -56.8 | 926.0 | 2500 | 0.00 | 350.584 | 0.0 | 64.1 | 0.05 | -91.5 | still ringing |
| Tape Worn Melancholy | Drone | -18.8 | -9.8 | -61.8 | 946.0 | 2500 | 0.00 | 977.855 | 0.1 | 14.9 | 0.08 | -91.5 | still ringing |
| Warm Drawbar | Drone | -19.4 | -8.1 | -61.0 | 552.0 | 2500 | 0.19 | 568.094 | 0.0 | 6.6 | 0.07 | -90.2 | still ringing |
| Deep Space Hum | Drone | -24.1 | -11.7 | -65.8 | 1006.0 | 2500 | 0.05 | 487.697 | 0.0 | 54.2 | 0.12 | -90.8 | still ringing |
| Harp Drone | Drone | -26.3 | -14.8 | -43.1 | 54.0 | 1788 | 0.05 | 267.927 | 0.0 | 0.8 | 0.14 | -89.2 | dull |
| Quantum Ripple | Drone | -22.7 | -10.9 | -70.6 | 980.0 | 2500 | 0.00 | 307.561 | 0.0 | 34.6 | 0.25 | -89.1 | dull, still ringing |
| Crystal Rings | Keys | -27.4 | -9.9 | -37.0 | 36.0 | 128 | 0.13 | 1364.29 | 0.1 | 0.0 | 0.14 | -91.2 |  |
| Dreamy Piano | Keys | -23.5 | -12.6 | -46.1 | 302.0 | 1152 | 0.00 | 372.551 | 0.0 | 0.0 | 0.12 | -89.0 |  |
| Glass Chime | Keys | -21.9 | -11.9 | -35.3 | 36.0 | 692 | 0.09 | 365.474 | 0.0 | 0.2 | 0.89 | -91.3 | still ringing |
| Gritty Wurlitzer | Keys | -21.1 | -10.5 | -27.6 | 28.0 | 434 | 0.48 | 619.914 | 0.0 | 0.2 | 0.09 | -91.4 | linear tail |
| Metallic Clavinet | Keys | -33.4 | -9.2 | -28.6 | 32.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.07 | -90.5 | quiet, dies early, dull |
| Percussive Clavinet | Keys | -34.4 | -8.9 | -32.3 | 34.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.09 | -88.6 | quiet, dies early, dull |
| Piano Chime | Keys | -25.0 | -12.2 | -30.6 | 40.0 | 884 | 0.35 | 300.088 | 0.1 | 0.2 | 0.24 | -91.4 | dull |
| Singing Metal | Keys | -24.0 | -11.7 | -34.7 | 68.0 | 78 | 0.21 | 2104.19 | 1.1 | 0.2 | 0.08 | -90.1 |  |
| Warm Rhodes | Keys | -21.4 | -6.5 | -38.2 | 84.0 | 450 | 0.33 | 439.93 | 0.0 | 7.1 | 0.28 | -91.3 |  |
| Smooth Jazz Piano | Keys | -21.8 | -10.7 | -30.7 | 24.0 | 668 | 0.29 | 678.249 | 0.0 | 0.0 | 0.10 | -90.7 |  |
| Sustained Wood | Keys | -17.3 | -9.8 | -42.5 | 910.0 | 646 | 0.25 | 267.126 | 0.0 | 0.0 | 0.06 | -91.0 | dull |
| Warm Vintage Piano | Keys | -18.8 | -10.6 | -40.0 | 44.0 | 450 | 0.18 | 375.612 | 0.0 | 0.1 | 0.12 | -90.3 |  |
| Warm Wood | Keys | -19.1 | -8.6 | -31.8 | 36.0 | 308 | 0.26 | 483.215 | 0.7 | 0.0 | 0.09 | -90.6 |  |
| Detuned Brass Stab 2 | Lead | -43.3 | -9.5 | 0.0 | 0.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.16 | -90.9 | quiet, hard edge, dies early, dull |
| Detuned Brass Stab | Lead | -40.5 | -8.3 | -8.0 | 2.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.09 | -90.3 | quiet, dies early, dull |
| Digital Pulse | Lead | -26.7 | -8.2 | -14.9 | 8.0 | 280 | 0.15 | 700.764 | 0.3 | 0.0 | 0.18 | -89.2 |  |
| Glass Tine | Lead | -20.2 | -9.0 | -38.1 | 966.0 | 384 | 0.14 | 1171.74 | 0.0 | 0.0 | 0.10 | -91.3 |  |
| Gritty Acid | Lead | -24.6 | -10.9 | -31.3 | 164.0 | 462 | 0.03 | 885.095 | 0.2 | 0.1 | 0.12 | -87.9 | dies early |
| Gritty Lead | Lead | -32.8 | -13.3 | -20.8 | 14.0 | 720 | 0.11 | 4815.94 | 60.2 | 0.1 | 0.26 | -89.1 | quiet, fizzy |
| Industrial Scream | Lead | -32.9 | -8.9 | -30.0 | 26.0 | 158 | 0.25 | 3792.33 | 24.5 | 7.0 | 0.08 | -89.2 | quiet |
| Metallic Glitch | Lead | -31.9 | -9.1 | -14.6 | 4.0 | 208 | 0.10 | 413.113 | 0.0 | 0.0 | 0.01 | -89.0 | quiet |
| Metallic Pulse | Lead | -25.7 | -11.0 | -69.8 | 1228.0 | 1102 | 0.22 | 2016.94 | 7.9 | 31.5 | 0.36 | -88.3 |  |
| Metallic Rumble | Lead | -33.2 | -9.7 | -21.6 | 12.0 | 0 | 0.00 | 13.2019 | 0.0 | 100.0 | 0.02 | -89.3 | quiet, dies early, dull |
| Psychedelic Vibrato | Lead | -37.9 | -11.5 | -24.0 | 20.0 | 0 | 0.00 | 1000.06 | 5.1 | 0.0 | 0.17 | -90.3 | quiet, dies early |
| Resonant Pulse | Lead | -29.6 | -8.4 | -29.0 | 16.0 | 0 | 0.00 | 711.358 | 0.0 | 0.1 | 0.32 | -91.1 | dies early |
| Screaming Distortion | Lead | -32.1 | -10.2 | -33.0 | 20.0 | 0 | 0.00 | 13.2019 | 0.0 | 100.0 | 0.13 | -88.8 | quiet, dies early, dull |
| Shimmering Lead | Lead | -26.9 | -10.0 | -31.9 | 38.0 | 1000 | 0.22 | 1646.55 | 0.2 | 0.1 | 0.41 | -89.5 |  |
| Soulful Lead | Lead | -33.6 | -11.7 | -21.4 | 10.0 | 0 | 0.00 | 499.906 | 0.0 | 1.2 | 0.16 | -90.8 | quiet, dies early |
| Spectral Soar | Lead | -34.2 | -11.8 | -19.9 | 28.0 | 0 | 0.00 | 1662.29 | 2.9 | 0.1 | 0.36 | -91.4 | quiet, dies early |
| Stereo Future Bass | Lead | -39.2 | -10.9 | -23.6 | 10.0 | 0 | 0.00 | 1046.91 | 3.1 | 10.2 | 0.21 | -88.7 | quiet, dies early |
| Trance Supersaw | Lead | -40.4 | -12.2 | -28.3 | 16.0 | 0 | 0.00 | 1553.78 | 2.3 | 4.0 | 0.27 | -89.2 | quiet, dies early |
| Vibrant Pulse | Lead | -28.6 | -11.2 | -29.8 | 22.0 | 244 | 0.18 | 528.785 | 0.0 | 5.5 | 0.01 | -89.6 |  |
| Warm Glide | Lead | -28.5 | -10.7 | -34.6 | 46.0 | 468 | 0.05 | 455.059 | 0.0 | 0.8 | 0.01 | -88.6 |  |
| Warm Vintage Lead | Lead | -34.8 | -14.0 | -26.4 | 14.0 | 0 | 0.00 | 720.45 | 0.5 | 0.1 | 0.00 | -89.9 | quiet, dies early |
| Wide Future Bass | Lead | -35.6 | -10.3 | -34.3 | 10.0 | 0 | 0.00 | 436.835 | 0.0 | 15.7 | 0.21 | -89.0 | quiet, dies early |
| Airy Flute | Lead | -18.7 | -9.8 | -27.3 | 32.0 | 412 | 0.13 | 371.589 | 0.0 | 4.9 | 0.17 | -88.6 |  |
| Ethereal Pulse | Lead | -31.3 | -11.6 | -38.7 | 42.0 | 0 | 0.00 | 305.779 | 0.0 | 0.5 | 0.07 | -90.4 | quiet, dies early, dull |
| Singing Wire | Lead | -29.1 | -6.8 | -23.9 | 14.0 | 832 | 0.20 | 2169.95 | 17.2 | 9.3 | 0.17 | -91.3 |  |
| Vocal Growl | Lead | -30.3 | -8.2 | -31.7 | 22.0 | 218 | 0.21 | 1347.88 | 7.8 | 0.7 | 0.20 | -91.3 | quiet |
| Vocal Whisper | Lead | -27.9 | -9.1 | -35.4 | 42.0 | 300 | 0.24 | 949.501 | 0.5 | 6.6 | 0.05 | -90.7 |  |
| Wavering Glow | Lead | -31.0 | -13.1 | -34.5 | 42.0 | 0 | 0.00 | 414.727 | 0.0 | 6.4 | 0.14 | -91.4 | quiet, dies early |
| Whispering Wind | Lead | -23.6 | -10.1 | -18.1 | 8.0 | 84 | 0.22 | 284.838 | 0.0 | 0.1 | 0.19 | -91.5 | dull |
| Breath Of Light | Pad | -20.4 | -10.2 | -63.7 | 1230.0 | 2500 | 0.00 | 266.511 | 0.0 | 3.6 | 0.10 | -90.8 | dull, still ringing |
| Cinematic Choir | Pad | -28.5 | -9.6 | -59.7 | 1274.0 | 2500 | 0.33 | 286.385 | 0.0 | 8.2 | 0.30 | -90.6 | dull, still ringing |
| Distant Chorus | Pad | -21.6 | -10.7 | -51.2 | 316.0 | 2466 | 0.30 | 455.773 | 0.0 | 0.5 | 0.32 | -88.2 |  |
| Gentle Vibrato Pad | Pad | -18.3 | -9.0 | -44.8 | 120.0 | 2500 | 0.19 | 372.137 | 0.0 | 1.5 | 0.20 | -90.8 | still ringing |
| Glacial Sweep | Pad | -19.6 | -10.6 | -39.3 | 92.0 | 794 | 0.19 | 394.787 | 0.0 | 3.0 | 0.20 | -91.1 |  |
| Glassy Bell | Pad | -17.7 | -9.5 | -58.2 | 1162.0 | 2500 | 0.48 | 379.621 | 0.1 | 0.8 | 0.06 | -91.3 | linear tail, still ringing |
| Glassy Drift | Pad | -21.5 | -10.3 | -59.7 | 698.0 | 2062 | 0.03 | 319.919 | 0.1 | 0.0 | 0.35 | -91.4 | dull |
| Glitchy Vibe | Pad | -22.6 | -8.2 | -49.6 | 214.0 | 1398 | 0.24 | 577.126 | 0.2 | 10.1 | 0.21 | -90.2 |  |
| Glowing Formant Pad | Pad | -29.2 | -17.7 | -87.7 | 366.0 | 2500 | 0.00 | 206.318 | 0.0 | 42.6 | 0.15 | -89.1 | dull, still ringing |
| Glowing Halo Pad | Pad | -24.0 | -14.1 | -54.8 | 516.0 | 2500 | 0.00 | 398.416 | 0.0 | 1.5 | 0.38 | -91.3 | still ringing |
| Silent Drift | Pad | -22.3 | -10.5 | -55.1 | 436.0 | 1148 | 0.21 | 752.618 | 0.7 | 0.0 | 0.20 | -91.4 |  |
| Tape Memory | Pad | -20.2 | -11.5 | -51.5 | 280.0 | 2210 | 0.00 | 188.263 | 0.0 | 56.6 | 0.11 | -90.7 | dull, still ringing |
| Tapered Glow | Pad | -18.8 | -10.2 | -66.6 | 950.0 | 2490 | 0.20 | 279.263 | 0.0 | 0.1 | 0.08 | -90.9 | dull, still ringing |
| Vocal Lullaby | Pad | -21.2 | -8.6 | -52.2 | 392.0 | 2500 | 0.08 | 607.543 | 0.0 | 34.5 | 0.42 | -90.1 | still ringing |
| Vocal Pulse | Pad | -22.4 | -9.3 | -147.5 | 346.0 | 1588 | 0.09 | 617.978 | 0.0 | 49.3 | 0.30 | -91.0 |  |
| Warm Analog Pad | Pad | -17.9 | -7.6 | -46.1 | 150.0 | 2500 | 0.00 | 268.544 | 0.0 | 5.5 | 0.15 | -89.8 | dull, still ringing |
| Warm Analog | Pad | -29.5 | -9.3 | -62.5 | 318.0 | 1426 | 0.04 | 1746.06 | 0.7 | 0.1 | 0.12 | -89.6 |  |
| Warm Bell Pad | Pad | -22.9 | -10.2 | -58.2 | 484.0 | 2500 | 0.19 | 1365.64 | 0.0 | 2.0 | 0.22 | -89.1 | still ringing |
| Warm Breath | Pad | -25.4 | -11.0 | -100.5 | 760.0 | 2020 | 0.03 | 150.27 | 0.0 | 76.4 | 0.18 | -90.4 | dull |
| Warm Glass | Pad | -19.3 | -9.5 | -60.7 | 332.0 | 1212 | 0.04 | 263.346 | 0.0 | 0.3 | 0.20 | -91.2 | dull |
| Warm Supersaw Pad | Pad | -20.9 | -8.6 | -51.9 | 222.0 | 2500 | 0.04 | 627.728 | 0.2 | 7.6 | 0.20 | -88.7 |  |
| Choir Blend | Pad | -21.7 | -10.4 | -49.4 | 246.0 | 1390 | 0.07 | 412.621 | 0.1 | 20.1 | 0.43 | -90.4 |  |
| Cold Formant | Pad | -22.6 | -11.4 | -54.6 | 474.0 | 1698 | 0.17 | 308.452 | 0.0 | 46.9 | 0.13 | -88.1 | dull |
| Glassy Evolution | Pad | -19.1 | -10.1 | -55.0 | 558.0 | 2500 | 0.00 | 290.119 | 0.3 | 38.6 | 0.13 | -91.2 | dull, still ringing |
| Glassy Halo | Pad | -25.7 | -9.9 | -50.6 | 360.0 | 2500 | 0.00 | 537.068 | 1.0 | 25.5 | 0.94 | -91.4 | still ringing |
| Vocal Breath | Pad | -22.7 | -9.4 | -55.4 | 570.0 | 2500 | 0.00 | 509.026 | 0.0 | 42.7 | 0.70 | -88.1 | still ringing |
| Vocal Harmonies | Pad | -21.9 | -10.8 | -59.4 | 468.0 | 1832 | 0.10 | 311.255 | 0.0 | 7.3 | 0.39 | -90.4 | dull |
| Vocal Resonance | Pad | -25.9 | -13.1 | -62.0 | 862.0 | 2500 | 0.07 | 1140.46 | 0.0 | 0.0 | 0.27 | -87.9 | still ringing |
| Vocal Shimmer | Pad | -23.2 | -10.6 | -53.1 | 154.0 | 566 | 0.04 | 313.855 | 0.0 | 0.7 | 0.13 | -88.1 | dull |
| Warm Strings | Pad | -23.9 | -13.5 | -47.5 | 178.0 | 690 | 0.09 | 284.87 | 0.0 | 0.2 | 0.17 | -89.7 | dull |
| 8bit Blip | Pluck | -38.3 | -8.7 | -21.8 | 4.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.00 | -90.7 | quiet, dull |
| Bounce Trigger | Pluck | -34.5 | -10.6 | -27.8 | 8.0 | 0 | 0.00 | 399.48 | 0.0 | 0.0 | 0.22 | -88.9 | quiet |
| Brass Pluck 2 | Pluck | -34.8 | -13.5 | -20.6 | 2.0 | 138 | 0.08 | 1016.49 | 0.0 | 3.0 | 0.18 | -90.0 | quiet |
| Brass Pluck | Pluck | -29.3 | -10.7 | -18.3 | 8.0 | 308 | 0.19 | 2790.12 | 24.6 | 4.0 | 0.15 | -90.5 |  |
| Faint Pluck | Pluck | -35.7 | -10.1 | -19.5 | 8.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.00 | -91.4 | quiet, dull |
| Future Saw | Pluck | -44.5 | -17.3 | -27.6 | 6.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.08 | -89.6 | quiet, dull |
| Glassy Pluck | Pluck | -36.1 | -7.4 | -22.6 | 10.0 | 0 | 0.00 | 627.882 | 0.1 | 0.1 | 0.08 | -91.2 | quiet |
| Guitar Pluck | Pluck | -38.0 | -13.0 | -28.3 | 4.0 | 0 | 0.00 | 733.27 | 0.0 | 0.0 | 0.13 | -88.9 | quiet |
| Hard Trance Pluck | Pluck | -32.1 | -11.7 | -27.8 | 18.0 | 0 | 0.00 | 422.087 | 0.0 | 4.1 | 0.38 | -89.2 | quiet |
| Metallic Pluck | Pluck | -33.7 | -11.6 | -22.6 | 6.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.14 | -91.5 | quiet, dull |
| Punchy Bass | Pluck | -38.1 | -11.2 | -27.8 | 20.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.25 | -90.5 | quiet, dull |
| Shimmering Pluck | Pluck | -37.5 | -9.7 | -23.8 | 8.0 | 0 | 0.00 | 1047.33 | 0.0 | 0.0 | 0.16 | -91.5 | quiet |
| Soft Click | Pluck | -33.9 | -11.2 | -14.9 | 8.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.06 | -91.5 | quiet, dull |
| Sub Pluck | Pluck | -36.5 | -9.6 | -17.9 | 8.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.10 | -90.3 | quiet, dull |
| Tape Bell | Pluck | -34.3 | -9.8 | -30.0 | 30.0 | 0 | 0.00 | 931.294 | 0.1 | 0.1 | 0.24 | -91.4 | quiet |
| Tape Dust | Pluck | -29.1 | -9.9 | -27.7 | 6.0 | 0 | 0.00 | 408.997 | 0.0 | 0.0 | 0.11 | -91.3 |  |
| Vintage Tape Pulse | Pluck | -23.3 | -8.7 | -40.2 | 88.0 | 1082 | 0.15 | 1271.88 | 0.2 | 47.9 | 0.11 | -89.0 |  |
| Vocal Kalimba | Pluck | -33.7 | -8.1 | -27.8 | 12.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.06 | -88.3 | quiet, dull |
| Warm Chime | Pluck | -32.5 | -10.4 | -23.6 | 20.0 | 0 | 0.00 | 271.95 | 0.0 | 0.7 | 0.01 | -91.4 | quiet, dull |
| Wobbly Tape Pluck | Pluck | -35.3 | -10.2 | -24.2 | 6.0 | 0 | 0.00 | 321.87 | 0.0 | 0.7 | 0.11 | -91.4 | quiet, dull |
| Glitch Pulse | Pluck | -48.1 | -11.3 | -22.0 | 2.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.00 | -89.9 | quiet, dull |
| Harp String | Pluck | -31.3 | -11.9 | -29.4 | 26.0 | 0 | 0.00 | 263.472 | 0.0 | 0.0 | 0.15 | -89.2 | quiet, dull |
| Metallic House Pulse | Pluck | -19.8 | -8.7 | -16.9 | 8.0 | 62 | 0.17 | 1278.91 | 4.0 | 6.2 | 0.00 | -91.3 |  |
| Punchy House Stab | Pluck | -21.8 | -10.1 | -26.1 | 16.0 | 74 | 0.18 | 577.821 | 0.0 | 8.1 | 0.27 | -91.4 |  |
| Deep Current | Texture | -24.2 | -8.3 | -67.5 | 350.0 | 2142 | 0.13 | 345.799 | 0.0 | 32.3 | 0.11 | -88.6 | dull |
| Digital Noise | Texture | -32.1 | -9.9 | -16.9 | 6.0 | 0 | 0.00 | 700.312 | 0.0 | 10.8 | 0.06 | -91.5 | quiet, dies early |
| Distorted Pulse | Texture | -26.5 | -4.2 | -30.1 | 28.0 | 0 | 0.00 | 275.895 | 0.0 | 0.0 | 0.01 | -90.3 | dies early, dull |
| Drizzle Pulse | Texture | -22.3 | -10.7 | -50.4 | 254.0 | 1686 | 0.11 | 231.235 | 0.0 | 32.8 | 0.13 | -91.5 | dull |
| Echoing Ghost | Texture | -22.4 | -7.2 | -61.4 | 754.0 | 2500 | 0.02 | 807.523 | 0.0 | 0.6 | 0.51 | -91.3 | still ringing |
| Gentle Wind | Texture | -18.5 | -10.1 | -54.2 | 588.0 | 2264 | 0.18 | 450.641 | 0.0 | 0.9 | 0.17 | -91.3 |  |
| Ghostly Vocal | Texture | -24.5 | -10.8 | -62.6 | 390.0 | 2500 | 0.02 | 570.344 | 0.0 | 0.7 | 0.42 | -90.9 | still ringing |
| Glassy Hiss | Texture | -34.4 | -10.9 | -17.8 | 10.0 | 0 | 0.00 | 428.537 | 0.4 | 0.0 | 0.01 | -91.3 | quiet, dies early |
| Glowing Depths | Texture | -23.7 | -12.1 | -54.8 | 282.0 | 1508 | 0.13 | 403.056 | 0.0 | 16.4 | 0.12 | -91.1 |  |
| Grit Morph | Texture | -23.2 | -7.6 | -57.4 | 442.0 | 710 | 0.03 | 783.888 | 0.0 | 0.2 | 0.00 | -90.5 |  |
| Gritty Scrape | Texture | -28.6 | -10.3 | -31.1 | 38.0 | 204 | 0.11 | 1252.4 | 4.7 | 20.6 | 0.10 | -89.0 |  |
| Raindrop Texture | Texture | -20.0 | -10.2 | -49.3 | 392.0 | 2500 | 0.17 | 333.186 | 0.1 | 10.3 | 0.18 | -91.2 | dull |
| Vocal Stutter | Texture | -32.7 | -9.9 | -17.7 | 10.0 | 0 | 0.00 | 277.266 | 0.0 | 0.2 | 0.14 | -88.1 | quiet, dies early, dull |
| Vocal Texture Pad | Texture | -20.9 | -9.0 | -50.3 | 390.0 | 2500 | 0.15 | 584.796 | 0.0 | 0.2 | 0.27 | -89.3 | still ringing |
| Whispering Leaves | Texture | -19.6 | -9.1 | -71.3 | 1258.0 | 2500 | 0.08 | 250.105 | 0.0 | 8.5 | 0.07 | -91.4 | dull |
| Deep Sea Whisper | Texture | -26.8 | -10.5 | -69.4 | 176.0 | 0 | 0.00 | 147.854 | 0.0 | 78.3 | 0.07 | -91.1 | dies early, dull, dc offset |
| Gamelan Hum | Texture | -24.2 | -9.9 | -23.4 | 14.0 | 624 | 0.14 | 6591.28 | 46.3 | 0.1 | 0.09 | -87.7 | fizzy |
| Glissando Pulse | Texture | -31.1 | -9.6 | -42.2 | 98.0 | 0 | 0.00 | 520.301 | 0.0 | 2.5 | 0.18 | -89.5 | quiet, dies early |
| Pulse Texture | Texture | -30.1 | -9.1 | -34.1 | 96.0 | 0 | 0.00 | 631.424 | 0.0 | 0.8 | 0.09 | -90.0 | quiet, dies early |
| Tide Pulse | Texture | -27.6 | -11.8 | -37.4 | 74.0 | 0 | 0.00 | 322.996 | 0.0 | 31.9 | 0.23 | -91.4 | dies early, dull |

165 patches at MIDI 60, 7.35618 s.
median rms -26.3 dB (10-90% spread 17.4 dB), median peak -10.1 dB, median attack 42 ms, median release 384 ms, median release shape 0.02, median centroid 412.583 Hz, median width 0.12, median alias -90.5 dB, worst alias -87.7 dB.
flags: dc offset 1, dies early 25, dull 54, fizzy 2, hard edge 1, linear tail 2, no low end 7, quiet 62, still ringing 29
