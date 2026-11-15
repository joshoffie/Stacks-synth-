# Audio audit 2026-11-15 04.26

One note (MIDI 60, velocity 0.8) held 1.5 s then released, 4 s in all, at 48 kHz through the voices and the effect chain. onset = the first millisecond's peak against the note's peak (0 = a hard edge); attack = time to 90% of the held level; release = time from note-off to -40 dB; shape = t(-6 dB) / t(-30 dB) of the release (0.2 exponential, 0.5 linear); onset, attack, release and shape are measured on the dry voice, the rest after the effects; alias = oscillator A alone at C7, energy between the harmonics against the harmonics.

| patch | kind | rms dB | peak dB | onset dB | attack ms | release ms | shape | centroid Hz | >5k % | <150 % | width | alias dB | flags |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Dark Techno Rumble | Bass | -22.4 | -10.7 | -51.8 | 962.0 | 1576 | 0.08 | 288.983 | 0.0 | 31.7 | 0.15 | -74.1 |  |
| Deep Pulse | Bass | -19.7 | -10.5 | -44.8 | 150.0 | 2500 | 0.00 | 359.547 | 0.0 | 63.3 | 0.20 | -79.8 | still ringing |
| Drenched Bass Pulse | Bass | -20.7 | -8.7 | -51.7 | 146.0 | 1310 | 0.01 | 669.874 | 0.0 | 11.5 | 0.11 | -91.3 |  |
| Gentle Moog Bass | Bass | -25.5 | -11.8 | -35.1 | 98.0 | 706 | 0.19 | 166.981 | 0.0 | 68.6 | 0.13 | -85.9 |  |
| Grit Rumble | Bass | -41.2 | -15.1 | -17.3 | 8.0 | 0 | 0.00 | 614.429 | 0.1 | 6.5 | 0.07 | -88.8 | quiet |
| Gritty FM Bass | Bass | -35.1 | -10.3 | -33.6 | 20.0 | 0 | 0.00 | 1072.94 | 0.1 | 12.5 | 0.48 | -80.0 | quiet |
| Growling Wobble | Bass | -23.8 | -8.6 | -20.8 | 6.0 | 0 | 0.00 | 13.2019 | 0.0 | 100.0 | 0.00 | -69.5 |  |
| Hammond Rumble | Bass | -16.8 | -3.1 | -67.8 | 52.0 | 1168 | 0.09 | 170.774 | 0.0 | 46.9 | 0.06 | -83.8 | dc offset |
| Metallic Thrum | Bass | -26.6 | -15.6 | -67.1 | 1276.0 | 2338 | 0.28 | 141.167 | 0.0 | 90.8 | 0.04 | -91.2 |  |
| Neuro Grit Bass | Bass | -31.3 | -7.4 | -29.4 | 24.0 | 0 | 0.00 | 655.768 | 0.0 | 14.0 | 0.05 | -75.0 | quiet |
| Pulse Bounce | Bass | -35.9 | -10.7 | -27.6 | 14.0 | 0 | 0.00 | 838.911 | 0.4 | 5.4 | 0.11 | -76.5 | quiet |
| Rubbery FM Bass | Bass | -31.6 | -8.7 | -38.9 | 36.0 | 0 | 0.00 | 758.52 | 0.0 | 15.8 | 0.14 | -86.1 | quiet |
| Slow Tape Hum | Bass | -20.1 | -10.3 | -49.4 | 574.0 | 2500 | 0.00 | 462.107 | 0.0 | 70.2 | 0.06 | -80.2 | still ringing |
| Vibrant Bass | Bass | -39.6 | -11.2 | -36.1 | 10.0 | 0 | 0.00 | 842.305 | 0.0 | 2.9 | 0.29 | -89.2 | quiet, no low end |
| Vintage Engine | Bass | -24.1 | -12.8 | -66.3 | 730.0 | 2500 | 0.17 | 407.04 | 0.0 | 8.5 | 0.08 | -75.9 | still ringing |
| Warm Analog Bass | Bass | -13.8 | -9.0 | -49.0 | 298.0 | 702 | 0.17 | 336.841 | 0.2 | 48.2 | 0.13 | -91.1 |  |
| Warm Funky Bass | Bass | -9.3 | -1.7 | -44.8 | 52.0 | 256 | 0.19 | 132.622 | 0.0 | 96.9 | 0.04 | -80.8 |  |
| Warm Moog Bass | Bass | -22.2 | -8.8 | -45.3 | 120.0 | 660 | 0.18 | 405.574 | 0.0 | 20.0 | 0.13 | -77.5 |  |
| Wobble Growl | Bass | -36.0 | -10.0 | -27.6 | 8.0 | 0 | 0.00 | 13.2019 | 0.0 | 100.0 | 0.47 | -78.4 | quiet |
| Wobbly Bass Drums | Bass | -19.0 | -10.6 | -49.0 | 234.0 | 422 | 0.00 | 273.967 | 0.0 | 21.6 | 0.09 | -89.3 |  |
| Growling Bass | Bass | -34.6 | -9.1 | -23.5 | 16.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.07 | -78.1 | quiet, no low end |
| Squelchy Acid Bass | Bass | -30.4 | -11.2 | -35.0 | 44.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.01 | -83.9 | quiet, no low end |
| Sub Growl | Bass | -30.2 | -5.1 | -37.5 | 8.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.03 | -81.4 | quiet, no low end |
| Wobble Bass | Bass | -30.1 | -8.4 | -31.4 | 44.0 | 0 | 0.00 | 429.564 | 0.0 | 40.7 | 0.17 | -88.9 | quiet |
| Wobbling Bass | Bass | -25.3 | -10.4 | -29.6 | 28.0 | 0 | 0.00 | 565.956 | 0.2 | 50.0 | 0.03 | -81.6 |  |
| Airborne Chime | Bell | -37.6 | -25.5 | -23.2 | 16.0 | 132 | 0.16 | 507.152 | 0.0 | 0.0 | 0.21 | -90.0 | quiet |
| Cold Glass Bell | Bell | -13.8 | -0.7 | -44.7 | 190.0 | 2018 | 0.07 | 507.24 | 0.0 | 2.1 | 0.58 | -78.8 |  |
| Deep Glass Bell | Bell | -25.9 | -10.2 | -37.3 | 100.0 | 672 | 0.06 | 399.253 | 0.4 | 48.3 | 0.19 | -81.7 |  |
| Deep Siren | Bell | -27.6 | -8.6 | -33.9 | 492.0 | 632 | 0.05 | 3099.6 | 14.4 | 0.0 | 0.21 | -84.7 |  |
| Distant Chime | Bell | -21.5 | -8.9 | -65.5 | 1170.0 | 1720 | 0.14 | 1355.62 | 0.1 | 0.0 | 0.37 | -83.4 |  |
| Frosted Chime | Bell | -30.7 | -9.6 | -19.7 | 6.0 | 0 | 0.00 | 283.815 | 0.1 | 0.1 | 0.00 | -89.2 | quiet, dull |
| Gentle Chime | Bell | -40.3 | -15.7 | -16.0 | 8.0 | 0 | 0.00 | 330.717 | 0.0 | 13.2 | 1.18 | -91.5 | quiet, dull |
| Glass Bell | Bell | -37.5 | -14.9 | -16.5 | 8.0 | 0 | 0.00 | 431.74 | 0.0 | 1.6 | 0.96 | -79.4 | quiet |
| Glassy Whisper | Bell | -38.3 | -18.7 | -32.3 | 42.0 | 178 | 0.26 | 380.799 | 0.0 | 0.0 | 0.12 | -80.0 | quiet |
| Metallic Chime | Bell | -21.0 | -9.7 | -37.9 | 142.0 | 272 | 0.15 | 533.043 | 0.0 | 0.9 | 0.18 | -86.1 |  |
| Wobbly Siren | Bell | -19.4 | -9.0 | -28.7 | 34.0 | 592 | 0.17 | 307.378 | 0.7 | 85.9 | 0.08 | -85.1 | dull |
| Crystal Chime | Bell | -36.1 | -9.9 | -20.5 | 8.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.24 | -82.4 | quiet, dull |
| Crystal Ring | Bell | -39.1 | -10.8 | -10.0 | 4.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.00 | -72.0 | quiet, dull |
| Metallic Bell | Bell | -22.3 | -9.6 | -43.7 | 344.0 | 2136 | 0.11 | 1265.18 | 0.0 | 0.0 | 0.41 | -78.6 | still ringing |
| Metallic Ring | Bell | -33.1 | -10.8 | -23.7 | 16.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.11 | -91.2 | quiet, dull |
| Vocal Bell | Bell | -24.7 | -13.4 | -42.8 | 186.0 | 1276 | 0.06 | 476.763 | 0.0 | 6.7 | 0.51 | -72.2 |  |
| Vocal Chime | Bell | -34.5 | -12.5 | -27.9 | 6.0 | 232 | 0.24 | 958.591 | 0.0 | 0.0 | 0.00 | -70.1 | quiet |
| Storm Echoes | Drone | -17.9 | -8.9 | -58.9 | 1132.0 | 2500 | 0.00 | 213.563 | 0.1 | 85.7 | 0.09 | -76.0 | dull, still ringing |
| Tape Worn Drums | Drone | -17.9 | -10.0 | -69.3 | 1082.0 | 2500 | 0.00 | 343.34 | 0.0 | 69.2 | 0.06 | -91.5 | dull, still ringing |
| Tape Worn Melancholy | Drone | -19.3 | -10.2 | -61.5 | 1080.0 | 2500 | 0.01 | 782.701 | 0.1 | 14.8 | 0.10 | -91.5 | still ringing |
| Warm Drawbar | Drone | -21.5 | -10.3 | -72.0 | 720.0 | 2500 | 0.17 | 371.704 | 0.0 | 3.5 | 0.11 | -90.1 | still ringing |
| Deep Space Hum | Drone | -23.3 | -10.9 | -63.7 | 1098.0 | 2500 | 0.00 | 568.452 | 0.0 | 51.1 | 0.13 | -74.8 | still ringing |
| Harp Drone | Drone | -24.5 | -13.2 | -58.5 | 1132.0 | 2500 | 0.25 | 274.899 | 0.0 | 0.9 | 0.16 | -84.4 | dull |
| Quantum Ripple | Drone | -22.5 | -9.4 | -67.9 | 1110.0 | 2500 | 0.00 | 288.871 | 0.0 | 42.7 | 0.23 | -80.6 | dull, still ringing |
| Crystal Rings | Keys | -26.9 | -10.7 | -28.2 | 34.0 | 142 | 0.30 | 1565.68 | 0.0 | 0.1 | 0.18 | -80.6 |  |
| Dreamy Piano | Keys | -22.1 | -10.0 | -49.3 | 132.0 | 1070 | 0.11 | 470.724 | 0.0 | 0.0 | 0.33 | -71.8 |  |
| Glass Chime | Keys | -20.5 | -8.5 | -31.5 | 46.0 | 728 | 0.18 | 451.19 | 0.0 | 0.6 | 0.73 | -81.9 | still ringing |
| Gritty Wurlitzer | Keys | -19.2 | -9.7 | -31.8 | 30.0 | 490 | 0.06 | 632.648 | 0.0 | 0.1 | 0.19 | -84.3 |  |
| Metallic Clavinet | Keys | -36.2 | -10.0 | -28.7 | 36.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.12 | -80.3 | quiet, dies early, dull |
| Percussive Clavinet | Keys | -33.6 | -8.9 | -31.1 | 32.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.13 | -71.1 | quiet, dies early, dull |
| Piano Chime | Keys | -24.9 | -10.4 | -33.2 | 44.0 | 736 | 0.12 | 294.757 | 0.1 | 3.0 | 0.17 | -84.9 | dull |
| Singing Metal | Keys | -21.0 | -6.5 | -38.9 | 630.0 | 76 | 0.25 | 1866.98 | 1.6 | 5.0 | 0.08 | -81.4 |  |
| Warm Rhodes | Keys | -25.3 | -10.3 | -43.4 | 88.0 | 444 | 0.20 | 330.374 | 0.0 | 33.1 | 0.16 | -82.9 | dull, dc offset |
| Smooth Jazz Piano | Keys | -22.6 | -11.6 | -31.5 | 68.0 | 816 | 0.67 | 648.154 | 0.0 | 0.0 | 0.18 | -85.5 | linear tail |
| Sustained Wood | Keys | -20.2 | -10.3 | -45.7 | 668.0 | 896 | 0.55 | 267.56 | 0.0 | 0.0 | 0.16 | -90.5 | linear tail, dull |
| Warm Vintage Piano | Keys | -18.2 | -10.6 | -31.4 | 50.0 | 462 | 0.06 | 518.807 | 0.0 | 0.1 | 0.22 | -79.8 |  |
| Warm Wood | Keys | -18.7 | -8.1 | -34.1 | 400.0 | 332 | 0.19 | 873.936 | 2.3 | 0.0 | 0.10 | -86.5 |  |
| Detuned Brass Stab 2 | Lead | -41.7 | -9.5 | -4.4 | 0.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.03 | -86.4 | quiet, hard edge, dies early, dull |
| Detuned Brass Stab | Lead | -45.5 | -13.4 | -2.7 | 0.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.18 | -83.8 | quiet, hard edge, dies early, dull |
| Digital Pulse | Lead | -27.9 | -12.2 | -22.7 | 8.0 | 298 | 0.05 | 738.316 | 0.4 | 0.0 | 0.20 | -80.9 |  |
| Glass Tine | Lead | -22.9 | -11.3 | -37.7 | 318.0 | 410 | 0.16 | 1177.21 | 0.0 | 0.0 | 0.11 | -82.0 |  |
| Gritty Acid | Lead | -24.0 | -9.9 | -42.6 | 144.0 | 444 | 0.13 | 898.746 | 0.3 | 0.2 | 0.10 | -69.6 | dies early |
| Gritty Lead | Lead | -28.5 | -6.8 | -30.4 | 20.0 | 790 | 0.25 | 4902.35 | 59.8 | 0.8 | 0.27 | -81.6 | fizzy |
| Industrial Scream | Lead | -34.7 | -10.4 | -27.7 | 28.0 | 146 | 0.15 | 3962.8 | 25.4 | 3.0 | 0.09 | -79.3 | quiet |
| Metallic Glitch | Lead | -31.9 | -9.7 | -16.6 | 4.0 | 230 | 0.17 | 446.698 | 0.0 | 0.0 | 0.01 | -76.9 | quiet |
| Metallic Pulse | Lead | -26.8 | -12.7 | -73.8 | 1434.0 | 1148 | 0.18 | 2002.47 | 8.3 | 29.6 | 0.35 | -71.2 |  |
| Metallic Rumble | Lead | -33.9 | -10.6 | -17.9 | 12.0 | 0 | 0.00 | 13.2019 | 0.0 | 100.0 | 0.04 | -85.5 | quiet, dies early, dull |
| Psychedelic Vibrato | Lead | -36.0 | -8.9 | -22.6 | 20.0 | 0 | 0.00 | 792.578 | 3.2 | 0.0 | 0.16 | -80.0 | quiet, dies early |
| Resonant Pulse | Lead | -29.5 | -9.7 | -21.5 | 16.0 | 0 | 0.00 | 675.14 | 0.0 | 0.1 | 0.32 | -87.8 | dies early |
| Screaming Distortion | Lead | -31.5 | -10.7 | -28.8 | 14.0 | 0 | 0.00 | 13.2019 | 0.0 | 100.0 | 0.15 | -72.1 | quiet, dies early, dull |
| Shimmering Lead | Lead | -27.6 | -11.0 | -27.4 | 24.0 | 1020 | 0.38 | 1639.55 | 0.2 | 0.1 | 0.36 | -80.1 |  |
| Soulful Lead | Lead | -35.1 | -12.0 | -22.2 | 6.0 | 0 | 0.00 | 510.917 | 0.0 | 0.9 | 0.17 | -87.0 | quiet, dies early |
| Spectral Soar | Lead | -32.1 | -10.8 | -23.9 | 22.0 | 0 | 0.00 | 1574.4 | 4.3 | 0.1 | 0.33 | -87.6 | quiet, dies early |
| Stereo Future Bass | Lead | -39.8 | -9.8 | -21.5 | 8.0 | 0 | 0.00 | 1128.61 | 3.4 | 16.6 | 0.12 | -71.6 | quiet, dies early |
| Trance Supersaw | Lead | -39.0 | -10.7 | -30.0 | 28.0 | 0 | 0.00 | 1868.89 | 2.6 | 3.5 | 0.28 | -79.9 | quiet, dies early |
| Vibrant Pulse | Lead | -26.9 | -8.4 | -29.6 | 26.0 | 318 | 0.03 | 923.596 | 0.1 | 5.8 | 0.01 | -76.3 |  |
| Warm Glide | Lead | -28.2 | -9.8 | -41.1 | 40.0 | 462 | 0.22 | 417.18 | 0.0 | 1.3 | 0.01 | -71.1 |  |
| Warm Vintage Lead | Lead | -34.7 | -9.0 | -25.6 | 14.0 | 0 | 0.00 | 787.354 | 1.4 | 0.1 | 0.00 | -83.0 | quiet, dies early |
| Wide Future Bass | Lead | -38.7 | -12.8 | -22.5 | 8.0 | 0 | 0.00 | 412.985 | 0.0 | 13.8 | 0.15 | -80.4 | quiet, dies early |
| Airy Flute | Lead | -18.6 | -9.9 | -27.0 | 32.0 | 454 | 0.24 | 396.188 | 0.0 | 3.6 | 0.18 | -71.1 |  |
| Ethereal Pulse | Lead | -29.9 | -10.0 | -36.7 | 48.0 | 0 | 0.00 | 314.721 | 0.0 | 0.2 | 0.15 | -86.0 | dies early, dull |
| Singing Wire | Lead | -33.7 | -15.2 | -28.7 | 18.0 | 888 | 0.06 | 1944.03 | 16.1 | 5.8 | 0.11 | -82.6 | quiet |
| Vocal Growl | Lead | -30.3 | -10.1 | -28.4 | 28.0 | 218 | 0.19 | 1911.04 | 13.3 | 0.8 | 0.24 | -79.9 | quiet |
| Vocal Whisper | Lead | -31.4 | -10.1 | -36.3 | 32.0 | 306 | 0.17 | 796.679 | 0.5 | 5.1 | 0.12 | -74.3 | quiet |
| Wavering Glow | Lead | -27.3 | -9.5 | -30.7 | 24.0 | 0 | 0.00 | 420.862 | 0.0 | 2.8 | 0.16 | -85.4 | dies early |
| Whispering Wind | Lead | -22.2 | -12.1 | -18.5 | 8.0 | 74 | 0.21 | 288.096 | 0.0 | 0.8 | 0.22 | -91.5 | dull |
| Breath Of Light | Pad | -21.3 | -12.2 | -75.8 | 1048.0 | 2500 | 0.00 | 285.44 | 0.0 | 5.1 | 0.14 | -78.8 | dull, still ringing |
| Cinematic Choir | Pad | -29.6 | -9.3 | -63.0 | 1314.0 | 2320 | 0.15 | 313.643 | 0.0 | 29.2 | 0.52 | -78.6 | dull, still ringing |
| Distant Chorus | Pad | -21.5 | -9.1 | -55.5 | 1288.0 | 1910 | 0.03 | 322.244 | 0.0 | 0.1 | 0.46 | -73.0 | dull |
| Gentle Vibrato Pad | Pad | -20.1 | -10.1 | -44.0 | 94.0 | 2500 | 0.18 | 323.019 | 0.0 | 0.7 | 0.31 | -79.6 | dull, still ringing |
| Glacial Sweep | Pad | -19.8 | -10.2 | -39.7 | 700.0 | 1058 | 0.43 | 460.536 | 0.0 | 9.5 | 0.16 | -78.8 | linear tail |
| Glassy Bell | Pad | -20.6 | -10.3 | -65.2 | 1042.0 | 1884 | 0.36 | 451.762 | 0.2 | 1.5 | 0.15 | -82.8 | still ringing |
| Glassy Drift | Pad | -20.6 | -8.6 | -60.2 | 416.0 | 1982 | 0.03 | 370.441 | 0.1 | 0.0 | 0.41 | -86.6 |  |
| Glitchy Vibe | Pad | -23.9 | -10.4 | -49.3 | 284.0 | 1250 | 0.12 | 625.277 | 0.3 | 6.8 | 0.23 | -81.1 |  |
| Glowing Formant Pad | Pad | -27.9 | -17.2 | -86.1 | 320.0 | 2500 | 0.04 | 135.692 | 0.0 | 95.9 | 0.15 | -74.8 | dull, still ringing |
| Glowing Halo Pad | Pad | -21.6 | -10.0 | -53.5 | 568.0 | 2500 | 0.00 | 313.417 | 0.0 | 1.3 | 0.48 | -81.8 | dull, still ringing |
| Silent Drift | Pad | -26.7 | -14.5 | -50.0 | 404.0 | 1102 | 0.07 | 736.673 | 0.7 | 0.0 | 0.20 | -89.4 |  |
| Tape Memory | Pad | -19.6 | -10.2 | -58.3 | 570.0 | 2500 | 0.19 | 239.752 | 0.0 | 17.1 | 0.23 | -77.5 | dull, still ringing |
| Tapered Glow | Pad | -18.3 | -9.2 | -58.7 | 994.0 | 2500 | 0.04 | 460.177 | 0.1 | 3.2 | 0.11 | -86.6 |  |
| Vocal Lullaby | Pad | -22.2 | -10.3 | -53.7 | 268.0 | 2500 | 0.06 | 629.388 | 0.0 | 30.1 | 0.44 | -79.2 | still ringing |
| Vocal Pulse | Pad | -23.4 | -9.8 | -145.6 | 336.0 | 1574 | 0.12 | 639.118 | 0.0 | 37.0 | 0.34 | -79.9 |  |
| Warm Analog Pad | Pad | -19.4 | -9.2 | -45.3 | 388.0 | 2500 | 0.00 | 262.59 | 0.0 | 16.3 | 0.20 | -75.1 | dull, still ringing |
| Warm Analog | Pad | -28.4 | -8.9 | -47.3 | 154.0 | 1182 | 0.07 | 1796.84 | 0.7 | 0.1 | 0.14 | -74.0 |  |
| Warm Bell Pad | Pad | -22.7 | -10.8 | -61.4 | 410.0 | 2500 | 0.08 | 1428.52 | 0.0 | 2.0 | 0.24 | -74.5 | still ringing |
| Warm Breath | Pad | -24.2 | -10.3 | -99.5 | 754.0 | 1988 | 0.06 | 141.425 | 0.0 | 82.0 | 0.11 | -77.1 | dull |
| Warm Glass | Pad | -20.1 | -10.1 | -60.9 | 504.0 | 1432 | 0.03 | 262.789 | 0.0 | 0.2 | 0.33 | -80.3 | dull |
| Warm Supersaw Pad | Pad | -21.8 | -9.7 | -49.1 | 306.0 | 2500 | 0.18 | 780.54 | 0.3 | 7.5 | 0.32 | -71.3 | still ringing |
| Choir Blend | Pad | -21.2 | -10.0 | -50.1 | 302.0 | 1448 | 0.05 | 440.757 | 0.1 | 5.4 | 0.53 | -77.3 |  |
| Cold Formant | Pad | -21.8 | -9.9 | -58.0 | 470.0 | 1658 | 0.01 | 297.162 | 0.0 | 57.1 | 0.15 | -69.8 | dull |
| Glassy Evolution | Pad | -18.2 | -9.6 | -57.7 | 600.0 | 2500 | 0.00 | 245.998 | 0.2 | 41.3 | 0.13 | -79.7 | dull, still ringing |
| Glassy Halo | Pad | -25.5 | -10.6 | -52.4 | 678.0 | 2500 | 0.00 | 479.453 | 0.8 | 31.6 | 0.80 | -85.8 | still ringing |
| Vocal Breath | Pad | -22.7 | -10.1 | -58.5 | 510.0 | 2500 | 0.00 | 501.798 | 0.0 | 43.2 | 0.68 | -72.3 | still ringing |
| Vocal Harmonies | Pad | -21.1 | -9.8 | -55.5 | 520.0 | 1766 | 0.07 | 367.481 | 0.0 | 5.8 | 0.43 | -80.6 |  |
| Vocal Resonance | Pad | -25.7 | -12.5 | -59.6 | 402.0 | 2500 | 0.00 | 1486.29 | 0.0 | 0.0 | 0.28 | -71.6 | still ringing |
| Vocal Shimmer | Pad | -21.0 | -4.6 | -47.6 | 270.0 | 580 | 0.15 | 355.594 | 0.0 | 2.9 | 0.20 | -72.3 |  |
| Warm Strings | Pad | -22.9 | -10.3 | -54.8 | 330.0 | 700 | 0.04 | 313.382 | 0.0 | 0.4 | 0.21 | -75.9 | dull |
| 8bit Blip | Pluck | -37.0 | -8.8 | -14.4 | 4.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.00 | -81.7 | quiet, dull |
| Bounce Trigger | Pluck | -33.7 | -10.1 | -16.7 | 8.0 | 0 | 0.00 | 492.212 | 0.0 | 0.0 | 0.19 | -71.5 | quiet |
| Brass Pluck 2 | Pluck | -27.8 | -8.6 | -12.4 | 2.0 | 156 | 0.22 | 996.983 | 0.0 | 4.0 | 0.27 | -77.3 |  |
| Brass Pluck | Pluck | -30.4 | -10.5 | -19.7 | 6.0 | 324 | 0.22 | 3153.79 | 31.3 | 4.4 | 0.18 | -81.0 | quiet |
| Faint Pluck | Pluck | -32.6 | -7.5 | -14.4 | 6.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.00 | -88.8 | quiet, dull |
| Future Saw | Pluck | -43.1 | -14.7 | -17.3 | 14.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.52 | -82.7 | quiet, dull |
| Glassy Pluck | Pluck | -38.0 | -10.3 | -17.3 | 6.0 | 0 | 0.00 | 1187.14 | 0.7 | 2.2 | 0.25 | -80.7 | quiet |
| Guitar Pluck | Pluck | -38.2 | -13.5 | -22.5 | 6.0 | 0 | 0.00 | 965.393 | 0.0 | 0.0 | 0.26 | -71.5 | quiet |
| Hard Trance Pluck | Pluck | -27.8 | -10.1 | -28.4 | 18.0 | 0 | 0.00 | 421.333 | 0.0 | 4.7 | 0.48 | -71.9 |  |
| Metallic Pluck | Pluck | -32.2 | -10.4 | -22.1 | 8.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.12 | -91.5 | quiet, dull |
| Punchy Bass | Pluck | -34.7 | -9.3 | -20.6 | 8.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.24 | -80.5 | quiet, dull |
| Shimmering Pluck | Pluck | -37.0 | -9.0 | -26.0 | 10.0 | 0 | 0.00 | 985.309 | 0.0 | 0.0 | 0.15 | -90.4 | quiet |
| Soft Click | Pluck | -33.2 | -10.1 | -14.5 | 8.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.37 | -91.5 | quiet, dull |
| Sub Pluck | Pluck | -36.1 | -10.8 | -24.3 | 10.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.15 | -81.3 | quiet, dull |
| Tape Bell | Pluck | -34.0 | -10.2 | -28.9 | 24.0 | 0 | 0.00 | 789.567 | 0.0 | 0.2 | 0.25 | -87.5 | quiet |
| Tape Dust | Pluck | -32.7 | -12.7 | -21.0 | 8.0 | 0 | 0.00 | 584.922 | 0.0 | 0.0 | 0.12 | -81.6 | quiet |
| Vintage Tape Pulse | Pluck | -24.2 | -10.0 | -43.3 | 100.0 | 1070 | 0.08 | 1200.69 | 0.2 | 38.0 | 0.15 | -72.6 |  |
| Vocal Kalimba | Pluck | -32.7 | -10.5 | -23.5 | 16.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.07 | -72.6 | quiet, dull |
| Warm Chime | Pluck | -33.2 | -10.8 | -18.4 | 16.0 | 0 | 0.00 | 353.326 | 0.0 | 0.3 | 0.63 | -88.4 | quiet |
| Wobbly Tape Pluck | Pluck | -34.4 | -9.8 | -24.5 | 8.0 | 0 | 0.00 | 319.534 | 0.0 | 0.6 | 0.11 | -88.8 | quiet, dull |
| Glitch Pulse | Pluck | -45.9 | -9.2 | -11.4 | 4.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.00 | -74.8 | quiet, dull |
| Harp String | Pluck | -33.0 | -11.2 | -32.9 | 40.0 | 0 | 0.00 | 270.847 | 0.0 | 0.0 | 0.59 | -86.7 | quiet, dull |
| Metallic House Pulse | Pluck | -20.4 | -9.8 | -20.8 | 8.0 | 64 | 0.20 | 1353.76 | 5.0 | 6.3 | 0.00 | -91.2 |  |
| Punchy House Stab | Pluck | -20.8 | -10.2 | -26.7 | 16.0 | 78 | 0.17 | 635.79 | 0.0 | 5.8 | 0.29 | -91.2 |  |
| Deep Current | Texture | -23.7 | -9.3 | -56.1 | 398.0 | 2390 | 0.21 | 327.052 | 0.0 | 42.9 | 0.16 | -70.7 | dull |
| Digital Noise | Texture | -31.8 | -9.9 | -17.7 | 8.0 | 0 | 0.00 | 691.048 | 0.0 | 4.8 | 0.05 | -91.4 | quiet, dies early |
| Distorted Pulse | Texture | -27.1 | -3.0 | -22.6 | 30.0 | 0 | 0.00 | 341.613 | 0.0 | 0.2 | 0.89 | -83.1 | dies early, dull |
| Drizzle Pulse | Texture | -21.2 | -9.8 | -57.0 | 364.0 | 1948 | 0.00 | 200.186 | 0.0 | 56.8 | 0.09 | -90.1 | dull |
| Echoing Ghost | Texture | -22.7 | -9.3 | -66.5 | 522.0 | 2500 | 0.00 | 1052.88 | 0.0 | 0.7 | 0.55 | -82.1 | still ringing |
| Gentle Wind | Texture | -19.1 | -10.2 | -61.5 | 1134.0 | 1758 | 0.07 | 455.572 | 0.0 | 0.9 | 0.19 | -90.0 |  |
| Ghostly Vocal | Texture | -24.4 | -10.0 | -66.9 | 586.0 | 2500 | 0.00 | 560.785 | 0.0 | 0.1 | 0.50 | -78.9 | still ringing |
| Glassy Hiss | Texture | -32.8 | -10.2 | -21.8 | 6.0 | 0 | 0.00 | 372.914 | 0.3 | 0.0 | 0.01 | -81.5 | quiet, dies early |
| Glowing Depths | Texture | -23.5 | -10.9 | -53.2 | 352.0 | 1840 | 0.27 | 517.351 | 0.0 | 28.4 | 0.11 | -77.9 |  |
| Grit Morph | Texture | -24.5 | -7.6 | -54.4 | 446.0 | 714 | 0.03 | 782.511 | 0.0 | 0.2 | 0.00 | -87.2 |  |
| Gritty Scrape | Texture | -28.3 | -10.2 | -37.2 | 34.0 | 222 | 0.20 | 1327.32 | 5.1 | 14.1 | 0.10 | -73.4 |  |
| Raindrop Texture | Texture | -19.8 | -10.0 | -52.0 | 444.0 | 2408 | 0.04 | 278.392 | 0.1 | 27.4 | 0.16 | -80.2 | dull |
| Vocal Stutter | Texture | -31.0 | -6.8 | -16.1 | 8.0 | 0 | 0.00 | 459.069 | 0.0 | 0.3 | 0.90 | -72.3 | quiet, dies early |
| Vocal Texture Pad | Texture | -23.1 | -10.7 | -50.6 | 426.0 | 2500 | 0.00 | 622.975 | 0.0 | 0.8 | 0.22 | -74.8 | still ringing |
| Whispering Leaves | Texture | -19.9 | -9.0 | -77.6 | 1018.0 | 2442 | 0.00 | 237.809 | 0.0 | 19.1 | 0.09 | -84.9 | dull |
| Deep Sea Whisper | Texture | -21.9 | -6.2 | -60.8 | 154.0 | 0 | 0.00 | 165.344 | 0.0 | 61.6 | 0.06 | -77.6 | dies early, dull, dc offset |
| Gamelan Hum | Texture | -24.4 | -9.7 | -24.8 | 18.0 | 596 | 0.15 | 6570.64 | 46.3 | 0.1 | 0.11 | -68.7 | fizzy |
| Glissando Pulse | Texture | -31.3 | -9.9 | -43.0 | 102.0 | 0 | 0.00 | 533.846 | 0.0 | 3.5 | 0.21 | -73.3 | quiet, dies early |
| Pulse Texture | Texture | -32.4 | -10.0 | -34.7 | 100.0 | 0 | 0.00 | 546.301 | 0.0 | 3.3 | 0.17 | -78.9 | quiet, dies early |
| Tide Pulse | Texture | -25.7 | -9.9 | -37.7 | 92.0 | 0 | 0.00 | 308.969 | 0.0 | 12.6 | 0.10 | -90.2 | dies early, dull |

165 patches at MIDI 60, 5.7153 s.
median rms -25.9 dB (10-90% spread 17.7 dB), median peak -10.1 dB, median attack 46 ms, median release 410 ms, median release shape 0.01, median centroid 440.757 Hz, median width 0.16, median alias -80.6 dB, worst alias -68.7 dB.
flags: dc offset 3, dies early 25, dull 52, fizzy 2, hard edge 2, linear tail 3, no low end 4, quiet 62, still ringing 29
