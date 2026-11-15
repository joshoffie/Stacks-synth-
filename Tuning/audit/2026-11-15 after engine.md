# Audio audit 2026-11-15 04.02

One note (MIDI 60, velocity 0.8) held 1.5 s then released, 4 s in all, at 48 kHz through the voices and the effect chain. onset = the first millisecond's peak against the note's peak (0 = a hard edge); attack = time to 90% of the held level; release = time from note-off to -40 dB; shape = t(-6 dB) / t(-30 dB) of the release (0.2 exponential, 0.5 linear); onset, attack, release and shape are measured on the dry voice, the rest after the effects; alias = oscillator A alone at C7, energy between the harmonics against the harmonics.

| patch | kind | rms dB | peak dB | onset dB | attack ms | release ms | shape | centroid Hz | >5k % | <150 % | width | alias dB | flags |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Dark Techno Rumble | Bass | -19.2 | -7.4 | -55.6 | 990.0 | 2500 | 0.31 | 288.117 | 0.0 | 32.8 | 0.16 | -50.1 |  |
| Deep Pulse | Bass | -22.1 | -12.6 | -45.6 | 158.0 | 2500 | 0.00 | 350.195 | 0.0 | 62.4 | 0.20 | -45.9 | still ringing |
| Drenched Bass Pulse | Bass | -21.0 | -8.9 | -48.6 | 256.0 | 1756 | 0.25 | 661.688 | 0.0 | 13.2 | 0.11 | -84.8 |  |
| Gentle Moog Bass | Bass | -44.1 | -30.1 | -39.2 | 76.0 | 934 | 0.02 | 149.539 | 0.0 | 73.8 | 0.17 | -63.6 | quiet |
| Grit Rumble | Bass | -57.1 | -23.7 | -23.5 | 14.0 | 0 | 0.00 | 2651 | 6.1 | 0.0 | 0.10 | -63.0 | quiet, cut off, no low end |
| Gritty FM Bass | Bass | -40.7 | -15.1 | -34.2 | 28.0 | 0 | 0.00 | 736.536 | 0.0 | 8.7 | 0.51 | -50.1 | quiet, cut off |
| Growling Wobble | Bass | -25.2 | -9.9 | -20.9 | 6.0 | 0 | 0.00 | 13.2019 | 0.0 | 100.0 | 0.00 | -43.1 | cut off |
| Hammond Rumble | Bass | -28.6 | -15.2 | -57.9 | 52.0 | 1246 | 0.13 | 179.232 | 0.0 | 43.8 | 0.06 | -63.4 |  |
| Metallic Thrum | Bass | -48.0 | -21.2 | -92.0 | 1212.0 | 2500 | 0.00 | 45.0544 | 0.0 | 97.7 | 0.36 | -84.6 | quiet, still ringing |
| Neuro Grit Bass | Bass | -37.5 | -13.8 | -30.6 | 28.0 | 0 | 0.00 | 666.206 | 0.0 | 15.8 | 0.06 | -50.7 | quiet, cut off |
| Pulse Bounce | Bass | -58.6 | -24.0 | -36.9 | 10.0 | 0 | 0.00 | 2686.97 | 5.9 | 0.0 | 0.15 | -50.1 | quiet, cut off, no low end |
| Rubbery FM Bass | Bass | -43.1 | -21.4 | -37.6 | 42.0 | 0 | 0.00 | 586.224 | 0.0 | 10.0 | 0.15 | -54.5 | quiet, cut off |
| Slow Tape Hum | Bass | -21.9 | -13.8 | -46.1 | 184.0 | 2500 | 0.00 | 360.539 | 0.0 | 65.6 | 0.05 | -47.5 | still ringing |
| Vibrant Bass | Bass | -52.2 | -26.1 | -30.0 | 12.0 | 0 | 0.00 | 787.352 | 0.0 | 3.4 | 0.28 | -69.1 | quiet, cut off, no low end |
| Vintage Engine | Bass | -23.6 | -12.5 | -65.3 | 728.0 | 2500 | 0.00 | 335.361 | 0.0 | 7.5 | 0.09 | -50.3 | still ringing |
| Warm Analog Bass | Bass | -24.8 | -19.6 | -47.5 | 272.0 | 888 | 0.08 | 337.196 | 0.2 | 49.5 | 0.13 | -78.8 |  |
| Warm Funky Bass | Bass | -19.5 | -11.8 | -44.3 | 124.0 | 264 | 0.05 | 132.477 | 0.0 | 97.3 | 0.04 | -57.7 |  |
| Warm Moog Bass | Bass | -27.3 | -14.2 | -45.2 | 154.0 | 914 | 0.11 | 365.537 | 0.0 | 17.8 | 0.12 | -52.5 |  |
| Wobble Growl | Bass | -33.9 | -8.6 | -45.8 | 8.0 | 0 | 0.00 | 13.2019 | 0.0 | 100.0 | 0.59 | -54.5 | quiet, cut off |
| Wobbly Bass Drums | Bass | -23.7 | -15.3 | -56.3 | 196.0 | 542 | 0.23 | 265.364 | 0.0 | 26.0 | 0.09 | -67.9 |  |
| Growling Bass | Bass | -41.3 | -14.9 | -21.2 | 16.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.06 | -55.5 | quiet, cut off, no low end |
| Squelchy Acid Bass | Bass | -40.3 | -20.5 | -35.1 | 50.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.02 | -51.4 | quiet, cut off, no low end |
| Sub Growl | Bass | -51.9 | -30.4 | -66.9 | 10.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.04 | -47.8 | quiet, cut off, no low end |
| Wobble Bass | Bass | -39.5 | -16.7 | -31.7 | 42.0 | 0 | 0.00 | 496.685 | 0.0 | 22.1 | 0.23 | -70.2 | quiet, cut off |
| Wobbling Bass | Bass | -24.3 | -9.1 | -31.4 | 28.0 | 0 | 0.00 | 521.883 | 0.2 | 53.0 | 0.03 | -47.8 | cut off |
| Airborne Chime | Bell | -27.5 | -16.9 | -22.2 | 16.0 | 178 | 0.24 | 450.782 | 0.0 | 0.0 | 0.20 | -74.1 |  |
| Cold Glass Bell | Bell | -25.3 | -11.9 | -43.9 | 212.0 | 2500 | 0.06 | 469.476 | 0.0 | 1.5 | 0.48 | -57.6 | dc offset |
| Deep Glass Bell | Bell | -30.2 | -14.4 | -38.3 | 102.0 | 870 | 0.09 | 402.294 | 0.4 | 48.2 | 0.19 | -60.9 | quiet |
| Deep Siren | Bell | -39.3 | -20.4 | -40.1 | 28.0 | 924 | 0.04 | 4633.92 | 36.3 | 0.0 | 0.19 | -52.0 | quiet, fizzy |
| Distant Chime | Bell | -28.2 | -13.5 | -62.0 | 1238.0 | 2324 | 0.19 | 1898.16 | 3.9 | 0.0 | 0.41 | -61.5 | still ringing |
| Frosted Chime | Bell | -37.8 | -17.1 | -13.9 | 6.0 | 0 | 0.00 | 281.764 | 0.0 | 0.1 | 0.00 | -72.0 | quiet, cut off, dull |
| Gentle Chime | Bell | -51.9 | -25.4 | -16.5 | 6.0 | 0 | 0.00 | 288.053 | 0.0 | 20.3 | 1.05 | -91.4 | quiet, cut off, dull |
| Glass Bell | Bell | -43.1 | -20.1 | -18.3 | 8.0 | 0 | 0.00 | 435.983 | 0.0 | 1.2 | 1.08 | -57.3 | quiet, cut off |
| Glassy Whisper | Bell | -27.4 | -7.7 | -36.9 | 42.0 | 244 | 0.21 | 386.014 | 0.0 | 0.0 | 0.12 | -59.0 |  |
| Metallic Chime | Bell | -9.1 | -0.0 | -38.0 | 376.0 | 332 | 0.02 | 519.507 | 0.0 | 0.8 | 0.17 | -66.4 |  |
| Wobbly Siren | Bell | -27.6 | -14.0 | -33.7 | 788.0 | 688 | 0.02 | 424.544 | 1.3 | 80.0 | 0.08 | -56.1 |  |
| Crystal Chime | Bell | -41.1 | -16.1 | -17.1 | 8.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.14 | -61.7 | quiet, cut off, dull |
| Crystal Ring | Bell | -45.6 | -18.6 | -12.7 | 4.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.00 | -50.7 | quiet, cut off, dull |
| Metallic Bell | Bell | -24.8 | -12.9 | -42.6 | 484.0 | 2196 | 0.05 | 732.98 | 0.0 | 0.0 | 0.55 | -57.4 | still ringing |
| Metallic Ring | Bell | -35.9 | -13.6 | -21.7 | 16.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.10 | -75.3 | quiet, cut off, dull |
| Vocal Bell | Bell | -28.8 | -17.5 | -36.1 | 126.0 | 1360 | 0.00 | 457.985 | 0.0 | 7.9 | 0.49 | -50.3 |  |
| Vocal Chime | Bell | -38.0 | -15.6 | -28.1 | 6.0 | 262 | 0.19 | 960.126 | 0.0 | 0.0 | 0.00 | -48.6 | quiet |
| Storm Echoes | Drone | -24.9 | -15.8 | -58.0 | 1056.0 | 2500 | 0.00 | 275.811 | 0.0 | 83.6 | 0.10 | -55.2 | dull, still ringing |
| Tape Worn Drums | Drone | -24.1 | -15.0 | -69.8 | 764.0 | 2500 | 0.00 | 130.34 | 0.0 | 99.2 | 0.01 | -91.5 | dull, still ringing |
| Tape Worn Melancholy | Drone | -18.7 | -9.9 | -56.8 | 652.0 | 2500 | 0.00 | 381.909 | 0.0 | 15.9 | 0.09 | -91.5 | still ringing |
| Warm Drawbar | Drone | -39.6 | -30.8 | -96.0 | 984.0 | 2150 | 0.06 | 141.407 | 0.0 | 88.6 | 0.01 | -46.4 | quiet, dull, still ringing |
| Deep Space Hum | Drone | -22.5 | -9.9 | -63.8 | 1078.0 | 2500 | 0.00 | 319.521 | 0.0 | 44.3 | 0.14 | -54.1 | dull, still ringing |
| Harp Drone | Drone | -37.0 | -25.4 | -44.8 | 80.0 | 2500 | 0.00 | 295.069 | 0.0 | 1.2 | 0.15 | -51.7 | quiet, dull |
| Quantum Ripple | Drone | -38.0 | -8.8 | -80.2 | 1212.0 | 2500 | 0.00 | 100.028 | 0.0 | 99.3 | 0.16 | -46.1 | quiet, dull, still ringing |
| Crystal Rings | Keys | -32.5 | -18.4 | -26.1 | 46.0 | 204 | 0.16 | 649.086 | 0.0 | 0.7 | 0.18 | -59.7 | quiet |
| Dreamy Piano | Keys | -20.8 | -10.4 | -46.3 | 332.0 | 1652 | 0.03 | 266.227 | 0.0 | 0.0 | 0.22 | -45.8 | dull |
| Glass Chime | Keys | -26.0 | -14.8 | -35.5 | 44.0 | 1060 | 0.08 | 339.691 | 0.0 | 0.2 | 0.73 | -61.1 | dull, still ringing |
| Gritty Wurlitzer | Keys | -12.4 | -4.8 | -21.7 | 26.0 | 682 | 0.09 | 643.674 | 0.0 | 0.1 | 0.16 | -64.0 |  |
| Metallic Clavinet | Keys | -38.9 | -13.7 | -26.6 | 36.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.12 | -56.1 | quiet, cut off, dull |
| Percussive Clavinet | Keys | -39.4 | -14.1 | -34.4 | 40.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.14 | -45.1 | quiet, cut off, dull |
| Piano Chime | Keys | -33.4 | -19.1 | -35.9 | 46.0 | 822 | 0.11 | 341.228 | 0.3 | 4.0 | 0.19 | -60.8 | quiet, dull |
| Singing Metal | Keys | -27.0 | -12.3 | -38.3 | 64.0 | 122 | 0.19 | 558.055 | 0.0 | 5.3 | 0.07 | -54.7 |  |
| Warm Rhodes | Keys | -26.2 | -11.0 | -42.6 | 92.0 | 630 | 0.07 | 368.545 | 0.0 | 26.8 | 0.20 | -61.9 |  |
| Smooth Jazz Piano | Keys | -53.8 | -31.0 | -25.7 | 26.0 | 1088 | 0.51 | 2233.89 | 12.7 | 0.0 | 0.17 | -64.1 | quiet, linear tail |
| Sustained Wood | Keys | -20.2 | -10.3 | -47.4 | 668.0 | 1302 | 0.58 | 269.506 | 0.0 | 0.0 | 0.15 | -56.0 | linear tail, dull |
| Warm Vintage Piano | Keys | -34.3 | -20.8 | -34.1 | 958.0 | 678 | 0.00 | 1332.64 | 0.1 | 0.0 | 0.24 | -55.9 | quiet |
| Warm Wood | Keys | -27.1 | -17.4 | -34.6 | 46.0 | 416 | 0.22 | 912.352 | 2.2 | 0.0 | 0.12 | -59.1 |  |
| Detuned Brass Stab 2 | Lead | -43.2 | -10.8 | -4.5 | 0.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.05 | -65.2 | quiet, hard edge, cut off, dull |
| Detuned Brass Stab | Lead | -53.5 | -19.3 | -9.0 | 0.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.17 | -61.6 | quiet, cut off, dull |
| Digital Pulse | Lead | -66.1 | -30.8 | -37.0 | 8.0 | 306 | 0.06 | 8975.15 | 90.1 | 0.0 | 0.30 | -53.6 | quiet, fizzy |
| Glass Tine | Lead | -11.6 | -0.0 | -39.1 | 430.0 | 520 | 0.17 | 1211.53 | 0.0 | 0.0 | 0.11 | -61.2 |  |
| Gritty Acid | Lead | -30.0 | -12.0 | -47.4 | 196.0 | 160 | 0.60 | 1056.47 | 0.3 | 0.0 | 0.13 | -43.4 | linear tail |
| Gritty Lead | Lead | -46.8 | -29.0 | -26.5 | 30.0 | 770 | 0.13 | 6003.65 | 82.5 | 0.0 | 0.23 | -53.1 | quiet, fizzy |
| Industrial Scream | Lead | -42.9 | -20.0 | -28.7 | 24.0 | 184 | 0.16 | 3814.26 | 24.2 | 2.8 | 0.09 | -46.3 | quiet |
| Metallic Glitch | Lead | -35.9 | -9.0 | -25.5 | 4.0 | 316 | 0.35 | 2575.67 | 1.5 | 0.0 | 0.01 | -51.4 | quiet |
| Metallic Pulse | Lead | -28.4 | -13.1 | -70.2 | 1388.0 | 1354 | 0.05 | 1678.61 | 1.9 | 30.2 | 0.36 | -38.1 | aliasing |
| Metallic Rumble | Lead | -37.5 | -14.2 | -17.0 | 12.0 | 0 | 0.00 | 13.2019 | 0.0 | 100.0 | 0.03 | -57.7 | quiet, cut off, dull |
| Psychedelic Vibrato | Lead | -41.7 | -15.3 | -20.1 | 16.0 | 0 | 0.00 | 1141.65 | 5.7 | 0.0 | 0.23 | -54.0 | quiet, cut off |
| Resonant Pulse | Lead | -37.8 | -17.1 | -34.3 | 20.0 | 0 | 0.00 | 663.156 | 0.0 | 0.1 | 0.31 | -66.2 | quiet, cut off |
| Screaming Distortion | Lead | -30.0 | -7.6 | -35.1 | 10.0 | 0 | 0.00 | 13.2019 | 0.0 | 100.0 | 0.30 | -47.0 | quiet, cut off, dull |
| Shimmering Lead | Lead | -36.9 | -18.9 | -31.9 | 14.0 | 1374 | 0.32 | 1813.68 | 0.8 | 0.0 | 0.42 | -48.3 | quiet |
| Soulful Lead | Lead | -39.2 | -15.7 | -24.9 | 10.0 | 0 | 0.00 | 490.028 | 0.0 | 0.8 | 0.42 | -60.9 | quiet, cut off |
| Spectral Soar | Lead | -38.2 | -16.7 | -27.6 | 24.0 | 0 | 0.00 | 1571.17 | 3.7 | 0.1 | 0.32 | -65.7 | quiet, cut off |
| Stereo Future Bass | Lead | -64.3 | -30.7 | -42.6 | 10.0 | 0 | 0.00 | 5118.02 | 56.9 | 0.0 | 0.16 | -45.7 | quiet, cut off, fizzy |
| Trance Supersaw | Lead | -56.5 | -26.9 | -36.2 | 32.0 | 0 | 0.00 | 4514.64 | 34.7 | 0.0 | 0.18 | -46.7 | quiet, cut off |
| Vibrant Pulse | Lead | -31.6 | -13.4 | -31.5 | 26.0 | 412 | 0.23 | 1067.7 | 0.1 | 3.0 | 0.01 | -51.4 | quiet |
| Warm Glide | Lead | -36.0 | -17.2 | -54.7 | 36.0 | 612 | 0.08 | 330.227 | 0.0 | 0.6 | 0.02 | -45.1 | quiet, dull |
| Warm Vintage Lead | Lead | -43.1 | -17.3 | -24.2 | 16.0 | 0 | 0.00 | 855.201 | 1.9 | 0.0 | 0.00 | -60.7 | quiet, cut off |
| Wide Future Bass | Lead | -55.7 | -27.7 | -23.9 | 10.0 | 0 | 0.00 | 529.479 | 0.0 | 0.7 | 0.13 | -46.2 | quiet, cut off |
| Airy Flute | Lead | -20.9 | -11.9 | -24.6 | 28.0 | 346 | 0.07 | 395.22 | 0.0 | 3.6 | 0.18 | -45.1 |  |
| Ethereal Pulse | Lead | -101.5 | -63.8 | -37.6 | 16.0 | 0 | 0.00 | 3207.86 | 2.5 | 0.0 | 0.38 | -66.0 | quiet, cut off |
| Singing Wire | Lead | -36.2 | -18.2 | -25.5 | 16.0 | 1162 | 0.23 | 2142.29 | 20.3 | 19.9 | 0.12 | -61.9 | quiet |
| Vocal Growl | Lead | -44.7 | -18.6 | -35.2 | 22.0 | 290 | 0.33 | 9381.8 | 96.6 | 0.0 | 0.18 | -59.6 | quiet, fizzy |
| Vocal Whisper | Lead | -50.1 | -25.3 | -37.3 | 34.0 | 366 | 0.12 | 3107.42 | 10.2 | 0.0 | 0.12 | -53.7 | quiet |
| Wavering Glow | Lead | -73.4 | -42.6 | -44.4 | 28.0 | 0 | 0.00 | 9542.02 | 87.6 | 0.0 | 0.13 | -65.5 | quiet, cut off, fizzy |
| Whispering Wind | Lead | -31.2 | -19.2 | -19.8 | 10.0 | 96 | 0.24 | 298.57 | 0.0 | 1.1 | 0.25 | -91.4 | quiet, dull |
| Breath Of Light | Pad | -23.4 | -13.6 | -67.5 | 1008.0 | 2500 | 0.00 | 297.87 | 0.0 | 9.6 | 0.14 | -53.8 | dull, still ringing |
| Cinematic Choir | Pad | -40.7 | -17.3 | -76.4 | 1366.0 | 2240 | 0.10 | 169.006 | 0.0 | 71.9 | 0.54 | -53.7 | quiet, dull, still ringing |
| Distant Chorus | Pad | -31.3 | -19.4 | -49.3 | 1286.0 | 2500 | 0.10 | 348.329 | 0.0 | 0.2 | 0.44 | -50.3 | quiet, dull, still ringing |
| Gentle Vibrato Pad | Pad | -27.4 | -17.9 | -40.6 | 114.0 | 2500 | 0.00 | 323.62 | 0.0 | 1.0 | 0.31 | -55.6 | dull, still ringing |
| Glacial Sweep | Pad | -22.1 | -12.9 | -41.0 | 406.0 | 1002 | 0.13 | 390.688 | 0.0 | 12.0 | 0.15 | -57.6 |  |
| Glassy Bell | Pad | -23.1 | -13.0 | -62.1 | 1042.0 | 2500 | 0.61 | 667.271 | 0.3 | 1.4 | 0.16 | -62.2 | linear tail, still ringing |
| Glassy Drift | Pad | -22.8 | -10.0 | -61.7 | 408.0 | 2500 | 0.02 | 437.297 | 0.2 | 0.0 | 0.47 | -67.1 |  |
| Glitchy Vibe | Pad | -29.7 | -16.4 | -48.1 | 302.0 | 1624 | 0.12 | 638.736 | 0.2 | 8.1 | 0.23 | -55.0 |  |
| Glowing Formant Pad | Pad | -45.6 | -34.9 | -95.9 | 360.0 | 2500 | 0.02 | 128.793 | 0.0 | 99.0 | 0.12 | -52.9 | quiet, dull |
| Glowing Halo Pad | Pad | -23.9 | -9.8 | -52.9 | 654.0 | 2500 | 0.00 | 356.25 | 0.0 | 3.2 | 0.49 | -61.0 | still ringing |
| Silent Drift | Pad | -45.7 | -32.4 | -61.3 | 390.0 | 1380 | 0.09 | 1532.75 | 2.9 | 0.0 | 0.32 | -72.5 | quiet |
| Tape Memory | Pad | -23.5 | -8.0 | -56.2 | 508.0 | 2500 | 0.00 | 152.467 | 0.0 | 83.0 | 0.12 | -54.9 | dull, still ringing |
| Tapered Glow | Pad | -21.1 | -12.2 | -59.0 | 438.0 | 2500 | 0.00 | 399.442 | 0.1 | 2.8 | 0.12 | -65.9 | still ringing |
| Vocal Lullaby | Pad | -16.2 | -4.8 | -53.7 | 218.0 | 2500 | 0.00 | 582.784 | 0.0 | 13.7 | 0.40 | -53.5 | still ringing |
| Vocal Pulse | Pad | -26.0 | -12.5 | -143.4 | 336.0 | 1592 | 0.09 | 592.576 | 0.0 | 38.7 | 0.34 | -55.4 | still ringing |
| Warm Analog Pad | Pad | -26.4 | -16.4 | -51.6 | 300.0 | 2500 | 0.00 | 260.669 | 0.0 | 16.1 | 0.21 | -50.3 | dull, still ringing |
| Warm Analog | Pad | -33.5 | -14.5 | -54.0 | 284.0 | 1638 | 0.02 | 1787.77 | 0.6 | 0.1 | 0.14 | -49.1 | quiet |
| Warm Bell Pad | Pad | -25.0 | -11.9 | -56.0 | 412.0 | 2500 | 0.00 | 1403.18 | 0.0 | 2.0 | 0.26 | -52.3 | still ringing |
| Warm Breath | Pad | -22.8 | -8.7 | -97.8 | 758.0 | 2036 | 0.07 | 141.365 | 0.0 | 82.3 | 0.11 | -52.0 | dull |
| Warm Glass | Pad | -29.9 | -20.1 | -44.4 | 508.0 | 2328 | 0.00 | 256.224 | 0.0 | 2.7 | 0.27 | -58.8 | dull |
| Warm Supersaw Pad | Pad | -29.9 | -17.2 | -55.4 | 318.0 | 2500 | 0.00 | 782.921 | 0.3 | 7.5 | 0.31 | -45.3 | still ringing |
| Choir Blend | Pad | -22.5 | -11.3 | -48.4 | 316.0 | 1644 | 0.01 | 445.832 | 0.2 | 4.8 | 0.53 | -53.5 |  |
| Cold Formant | Pad | -30.7 | -19.5 | -57.2 | 470.0 | 2178 | 0.01 | 300.218 | 0.0 | 48.1 | 0.15 | -45.4 | quiet, dull |
| Glassy Evolution | Pad | -20.4 | -12.2 | -55.8 | 608.0 | 2500 | 0.00 | 283.085 | 0.2 | 26.7 | 0.11 | -58.7 | dull, still ringing |
| Glassy Halo | Pad | -25.6 | -10.4 | -62.1 | 434.0 | 2500 | 0.00 | 485.742 | 0.8 | 32.1 | 0.80 | -66.0 | still ringing |
| Vocal Breath | Pad | -22.9 | -10.1 | -58.3 | 490.0 | 2500 | 0.00 | 501.246 | 0.0 | 43.3 | 0.68 | -50.6 | still ringing |
| Vocal Harmonies | Pad | -17.2 | -6.0 | -56.1 | 532.0 | 1802 | 0.04 | 368.259 | 0.0 | 5.7 | 0.43 | -55.6 | still ringing |
| Vocal Resonance | Pad | -13.5 | -0.2 | -58.3 | 940.0 | 2500 | 0.00 | 1016.19 | 0.0 | 0.0 | 0.33 | -49.5 | still ringing |
| Vocal Shimmer | Pad | -40.3 | -28.3 | -41.2 | 196.0 | 986 | 0.42 | 255.751 | 0.0 | 11.2 | 0.16 | -49.8 | quiet, linear tail, dull |
| Warm Strings | Pad | -23.8 | -12.1 | -61.9 | 318.0 | 1114 | 0.39 | 266.595 | 0.0 | 0.7 | 0.14 | -46.4 | dull |
| 8bit Blip | Pluck | -76.7 | -36.8 | -28.7 | 2.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.00 | -58.9 | quiet, cut off, dull |
| Bounce Trigger | Pluck | -41.5 | -13.1 | -36.1 | 8.0 | 0 | 0.00 | 1921.27 | 6.6 | 0.0 | 0.11 | -45.5 | quiet, cut off |
| Brass Pluck 2 | Pluck | -34.7 | -15.1 | -15.1 | 2.0 | 214 | 0.20 | 997.688 | 0.0 | 3.8 | 0.27 | -53.4 | quiet |
| Brass Pluck | Pluck | -34.7 | -14.8 | -19.9 | 8.0 | 396 | 0.19 | 3082.83 | 31.7 | 4.4 | 0.18 | -55.6 | quiet |
| Faint Pluck | Pluck | -44.4 | -19.4 | -14.8 | 8.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.00 | -71.0 | quiet, cut off, dull |
| Future Saw | Pluck | -52.0 | -19.2 | -30.3 | 6.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.05 | -51.2 | quiet, cut off, dull |
| Glassy Pluck | Pluck | -82.2 | -45.1 | -26.4 | 10.0 | 0 | 0.00 | 7242.23 | 87.8 | 0.0 | 0.17 | -59.7 | quiet, cut off, fizzy |
| Guitar Pluck | Pluck | -52.7 | -18.8 | -23.3 | 8.0 | 0 | 0.00 | 1881.22 | 0.3 | 0.0 | 0.09 | -45.5 | quiet, cut off |
| Hard Trance Pluck | Pluck | -49.1 | -23.3 | -45.1 | 6.0 | 0 | 0.00 | 2915.7 | 3.8 | 0.0 | 0.39 | -45.8 | quiet, cut off |
| Metallic Pluck | Pluck | -37.9 | -16.7 | -20.8 | 8.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.11 | -91.4 | quiet, cut off, dull |
| Punchy Bass | Pluck | -64.6 | -29.3 | -43.5 | 12.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.25 | -56.8 | quiet, cut off, dull |
| Shimmering Pluck | Pluck | -44.5 | -16.4 | -23.3 | 10.0 | 0 | 0.00 | 2577.29 | 4.3 | 0.0 | 0.10 | -76.4 | quiet, cut off |
| Soft Click | Pluck | -41.8 | -18.9 | -14.4 | 8.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.38 | -91.4 | quiet, cut off, dull |
| Sub Pluck | Pluck | -43.2 | -14.3 | -26.6 | 8.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.14 | -54.9 | quiet, cut off, dull |
| Tape Bell | Pluck | -36.3 | -13.3 | -33.1 | 26.0 | 0 | 0.00 | 711.222 | 0.0 | 0.2 | 0.24 | -67.2 | quiet, cut off |
| Tape Dust | Pluck | -30.3 | -12.2 | -20.5 | 6.0 | 0 | 0.00 | 395.634 | 0.0 | 0.0 | 0.12 | -60.8 | quiet, cut off |
| Vintage Tape Pulse | Pluck | -32.9 | -18.6 | -43.4 | 94.0 | 1056 | 0.05 | 1162.41 | 0.2 | 45.3 | 0.11 | -46.4 | quiet |
| Vocal Kalimba | Pluck | -42.7 | -19.7 | -23.0 | 18.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.12 | -50.1 | quiet, cut off, dull |
| Warm Chime | Pluck | -44.7 | -24.9 | -28.4 | 48.0 | 0 | 0.00 | 263.44 | 0.0 | 0.4 | 0.24 | -70.7 | quiet, cut off, dull |
| Wobbly Tape Pluck | Pluck | -62.0 | -31.9 | -22.3 | 6.0 | 0 | 0.00 | 1241.43 | 1.6 | 0.1 | 0.11 | -70.9 | quiet, cut off |
| Glitch Pulse | Pluck | -51.5 | -16.9 | -20.3 | 2.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.00 | -49.5 | quiet, cut off, dull |
| Harp String | Pluck | -44.9 | -23.8 | -37.8 | 44.0 | 0 | 0.00 | 264.89 | 0.0 | 0.0 | 0.62 | -55.4 | quiet, cut off, dull |
| Metallic House Pulse | Pluck | -23.3 | -12.4 | -20.8 | 6.0 | 84 | 0.19 | 1335.56 | 4.7 | 6.5 | 0.00 | -90.9 |  |
| Punchy House Stab | Pluck | -21.5 | -10.6 | -23.5 | 10.0 | 102 | 0.16 | 641.769 | 0.0 | 5.0 | 0.30 | -86.8 |  |
| Deep Current | Texture | -30.1 | -16.7 | -65.7 | 402.0 | 2500 | 0.17 | 332.36 | 0.0 | 42.4 | 0.15 | -46.3 | quiet, dull |
| Digital Noise | Texture | -38.1 | -15.8 | -20.3 | 8.0 | 0 | 0.00 | 700.227 | 0.0 | 5.7 | 0.05 | -91.4 | quiet, cut off |
| Distorted Pulse | Texture | -34.3 | -10.9 | -29.3 | 24.0 | 0 | 0.00 | 292.479 | 0.0 | 0.1 | 0.86 | -56.3 | quiet, cut off, dull |
| Drizzle Pulse | Texture | -26.7 | -15.3 | -53.4 | 362.0 | 2500 | 0.43 | 199.633 | 0.0 | 57.3 | 0.09 | -74.6 | linear tail, dull |
| Echoing Ghost | Texture | -18.9 | -5.5 | -65.3 | 538.0 | 2500 | 0.00 | 1049.17 | 0.0 | 0.7 | 0.54 | -61.6 | still ringing |
| Gentle Wind | Texture | -24.6 | -15.6 | -63.1 | 802.0 | 2452 | 0.07 | 457.254 | 0.0 | 0.9 | 0.19 | -71.5 | still ringing |
| Ghostly Vocal | Texture | -12.0 | -0.0 | -63.4 | 574.0 | 2500 | 0.01 | 562.769 | 0.0 | 0.1 | 0.51 | -55.9 | still ringing |
| Glassy Hiss | Texture | -40.8 | -17.5 | -18.0 | 8.0 | 0 | 0.00 | 403.64 | 0.5 | 0.0 | 0.01 | -60.7 | quiet, cut off |
| Glowing Depths | Texture | -29.4 | -17.1 | -54.2 | 348.0 | 2368 | 0.20 | 472.513 | 0.0 | 28.6 | 0.11 | -56.6 |  |
| Grit Morph | Texture | -32.6 | -19.6 | -53.3 | 356.0 | 868 | 0.10 | 429.574 | 0.0 | 0.3 | 0.00 | -68.8 | quiet |
| Gritty Scrape | Texture | -33.6 | -15.4 | -37.6 | 34.0 | 244 | 0.15 | 1402.44 | 5.4 | 11.6 | 0.10 | -45.8 | quiet |
| Raindrop Texture | Texture | -25.0 | -15.2 | -58.5 | 452.0 | 2500 | 0.04 | 278.12 | 0.1 | 27.4 | 0.16 | -59.1 | dull, still ringing |
| Vocal Stutter | Texture | -48.3 | -24.4 | -15.8 | 6.0 | 0 | 0.00 | 455.588 | 0.0 | 1.0 | 1.10 | -50.5 | quiet, cut off |
| Vocal Texture Pad | Texture | -23.0 | -12.6 | -56.6 | 402.0 | 2500 | 0.00 | 466.686 | 0.0 | 0.4 | 0.29 | -51.8 | still ringing |
| Whispering Leaves | Texture | -23.4 | -13.5 | -80.5 | 1022.0 | 2500 | 0.26 | 247.121 | 0.0 | 11.8 | 0.10 | -64.8 | dull, still ringing |
| Deep Sea Whisper | Texture | -38.1 | -26.2 | -80.7 | 170.0 | 0 | 0.00 | 17.2013 | 0.0 | 99.9 | 0.00 | -54.6 | quiet, cut off, dull |
| Gamelan Hum | Texture | -32.4 | -17.5 | -27.9 | 22.0 | 814 | 0.18 | 6368.85 | 45.3 | 0.0 | 0.11 | -43.2 | quiet, fizzy |
| Glissando Pulse | Texture | -31.2 | -10.7 | -46.4 | 140.0 | 0 | 0.00 | 283.531 | 0.0 | 7.5 | 0.14 | -47.5 | quiet, cut off, dull |
| Pulse Texture | Texture | -34.2 | -11.7 | -41.1 | 94.0 | 0 | 0.00 | 510.993 | 0.0 | 3.0 | 0.17 | -46.7 | quiet, cut off |
| Tide Pulse | Texture | -34.8 | -17.1 | -38.8 | 96.0 | 0 | 0.00 | 385.41 | 0.0 | 10.7 | 0.12 | -75.0 | quiet, cut off |

165 patches at MIDI 60, 4.50864 s.
median rms -33.5 dB (10-90% spread 31.0 dB), median peak -15.6 dB, median attack 46 ms, median release 412 ms, median release shape 0.00, median centroid 424.544 Hz, median width 0.15, median alias -55.6 dB, worst alias -38.1 dB.
flags: aliasing 1, cut off 62, dc offset 1, dull 54, fizzy 8, hard edge 1, linear tail 6, no low end 6, quiet 97, still ringing 37
