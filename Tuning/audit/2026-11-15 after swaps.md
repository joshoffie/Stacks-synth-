# Audio audit 2026-11-15 16.03

One note (MIDI 60, velocity 0.8) held 1.5 s then released, 4 s in all, at 48 kHz through the voices and the effect chain. onset = the first millisecond's peak against the note's peak (0 = a hard edge); attack = time to 90% of the held level; release = time from note-off to -40 dB; shape = t(-6 dB) / t(-30 dB) of the release (0.2 exponential, 0.5 linear); onset, attack, release and shape are measured on the dry voice, the rest after the effects; alias = oscillator A alone at C7, energy between the harmonics against the harmonics.

| patch | kind | rms dB | peak dB | onset dB | attack ms | release ms | shape | centroid Hz | >5k % | <150 % | width | alias dB | flags |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Dark Techno Rumble | Bass | -22.0 | -10.4 | -55.3 | 340.0 | 1652 | 0.00 | 280.201 | 0.0 | 26.8 | 0.14 | -88.7 |  |
| Deep Pulse | Bass | -18.0 | -9.8 | -50.3 | 196.0 | 2500 | 0.00 | 194.943 | 0.0 | 78.6 | 0.11 | -89.1 | still ringing |
| Drenched Bass Pulse | Bass | -21.4 | -8.2 | -51.0 | 202.0 | 1294 | 0.04 | 725.07 | 0.0 | 27.4 | 0.09 | -91.4 |  |
| Gentle Moog Bass | Bass | -25.4 | -11.2 | -37.8 | 72.0 | 610 | 0.02 | 168.758 | 0.0 | 63.7 | 0.14 | -91.1 |  |
| Grit Rumble | Bass | -40.8 | -13.5 | -27.1 | 12.0 | 0 | 0.00 | 539.596 | 0.0 | 6.9 | 0.04 | -91.2 | quiet |
| Gritty FM Bass | Bass | -33.1 | -10.3 | -33.0 | 26.0 | 0 | 0.00 | 631.357 | 0.0 | 32.8 | 0.13 | -89.3 | quiet |
| Growling Wobble | Bass | -24.0 | -8.4 | -23.6 | 6.0 | 0 | 0.00 | 13.2019 | 0.0 | 100.0 | 0.00 | -88.0 |  |
| Hammond Rumble | Bass | -19.4 | -6.9 | -60.5 | 98.0 | 862 | 0.00 | 221.357 | 0.0 | 26.5 | 0.18 | -91.4 |  |
| Metallic Thrum | Bass | -26.4 | -15.7 | -62.4 | 1142.0 | 1596 | 0.02 | 170.484 | 0.0 | 66.6 | 0.04 | -91.4 |  |
| Neuro Grit Bass | Bass | -29.5 | -7.3 | -29.9 | 24.0 | 0 | 0.00 | 567.547 | 0.0 | 19.1 | 0.05 | -89.4 |  |
| Pulse Bounce | Bass | -36.4 | -8.8 | -32.3 | 16.0 | 0 | 0.00 | 766.965 | 0.4 | 2.3 | 0.09 | -90.2 | quiet, no low end |
| Rubbery FM Bass | Bass | -31.2 | -11.1 | -34.6 | 48.0 | 0 | 0.00 | 398.71 | 0.0 | 53.3 | 0.07 | -89.2 | quiet |
| Slow Tape Hum | Bass | -22.4 | -12.0 | -47.8 | 340.0 | 2500 | 0.00 | 447.025 | 0.0 | 71.5 | 0.06 | -89.1 | still ringing |
| Vintage Engine | Bass | -21.6 | -9.7 | -62.7 | 1232.0 | 2500 | 0.05 | 448.8 | 0.0 | 37.5 | 0.06 | -90.4 | still ringing |
| Warm Analog Bass | Bass | -14.5 | -9.8 | -48.4 | 318.0 | 838 | 0.29 | 361.978 | 0.3 | 45.5 | 0.10 | -91.5 |  |
| Warm Funky Bass | Bass | -10.8 | -3.1 | -56.2 | 1046.0 | 252 | 0.30 | 130.544 | 0.0 | 96.0 | 0.04 | -90.2 |  |
| Warm Moog Bass | Bass | -21.7 | -8.9 | -44.5 | 112.0 | 784 | 0.20 | 356.77 | 0.0 | 27.1 | 0.10 | -90.4 |  |
| Wobble Growl | Bass | -34.0 | -9.4 | -39.1 | 12.0 | 0 | 0.00 | 13.2019 | 0.0 | 100.0 | 0.21 | -90.4 | quiet |
| Wobbly Bass Drums | Bass | -19.3 | -10.6 | -49.3 | 182.0 | 448 | 0.17 | 230.385 | 0.0 | 53.1 | 0.07 | -91.4 |  |
| Growling Bass | Bass | -33.0 | -7.7 | -24.2 | 16.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.08 | -88.8 | quiet, no low end |
| Squelchy Acid Bass | Bass | -33.8 | -10.3 | -36.3 | 46.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.03 | -89.2 | quiet, no low end |
| Sub Growl | Bass | -31.3 | -5.9 | -44.2 | 8.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.03 | -89.1 | quiet, no low end |
| Wobble Bass | Bass | -31.6 | -9.2 | -31.1 | 38.0 | 0 | 0.00 | 342.777 | 0.0 | 8.2 | 0.20 | -91.4 | quiet |
| Wobbling Bass | Bass | -25.3 | -10.5 | -27.4 | 40.0 | 0 | 0.00 | 344.044 | 0.1 | 71.2 | 0.04 | -89.1 |  |
| Airborne Chime | Bell | -42.6 | -28.3 | -14.8 | 16.0 | 168 | 0.23 | 474.068 | 0.0 | 0.0 | 0.15 | -91.5 | quiet |
| Cold Glass Bell | Bell | -15.7 | -1.5 | -44.3 | 120.0 | 1792 | 0.11 | 422.969 | 0.0 | 2.6 | 0.31 | -91.1 |  |
| Deep Glass Bell | Bell | -25.4 | -9.3 | -40.8 | 74.0 | 818 | 0.19 | 379.704 | 0.4 | 57.2 | 0.20 | -91.3 |  |
| Deep Siren | Bell | -27.9 | -8.3 | -18.8 | 8.0 | 676 | 0.04 | 2765.67 | 12.0 | 0.0 | 0.20 | -89.1 |  |
| Distant Chime | Bell | -22.1 | -10.6 | -63.6 | 1204.0 | 1774 | 0.15 | 914.977 | 0.0 | 0.0 | 0.41 | -91.4 |  |
| Frosted Chime | Bell | -30.7 | -9.9 | -16.0 | 6.0 | 0 | 0.00 | 281.735 | 0.0 | 0.1 | 0.00 | -91.4 | quiet, dull |
| Gentle Chime | Bell | -32.9 | -10.3 | -16.8 | 6.0 | 0 | 0.00 | 271.081 | 0.0 | 0.2 | 0.06 | -91.5 | quiet, dull |
| Glass Bell | Bell | -32.6 | -9.7 | -27.4 | 6.0 | 0 | 0.00 | 272.503 | 0.0 | 0.0 | 0.07 | -91.0 | quiet, dull |
| Glassy Whisper | Bell | -43.7 | -22.0 | -36.9 | 46.0 | 148 | 0.27 | 639.503 | 0.0 | 0.4 | 0.09 | -91.2 | quiet |
| Metallic Chime | Bell | -19.1 | -7.0 | -43.3 | 148.0 | 238 | 0.02 | 514.436 | 0.0 | 0.4 | 0.18 | -91.4 |  |
| Wobbly Siren | Bell | -18.8 | -7.4 | -28.9 | 20.0 | 532 | 0.11 | 368.603 | 1.0 | 83.7 | 0.07 | -89.1 |  |
| Crystal Chime | Bell | -34.3 | -9.9 | -17.4 | 8.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.07 | -91.3 | quiet, dull |
| Crystal Ring | Bell | -39.3 | -11.9 | -12.5 | 4.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.00 | -90.2 | quiet, dull |
| Metallic Bell | Bell | -21.4 | -9.1 | -41.7 | 410.0 | 2232 | 0.13 | 1305.9 | 0.0 | 0.0 | 0.47 | -91.1 | still ringing |
| Metallic Ring | Bell | -29.4 | -9.0 | -22.7 | 16.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.02 | -91.4 | dull |
| Vocal Bell | Bell | -23.3 | -11.5 | -56.1 | 110.0 | 1334 | 0.03 | 464.913 | 0.0 | 3.8 | 0.53 | -88.0 |  |
| Vocal Chime | Bell | -34.1 | -12.4 | -12.5 | 8.0 | 176 | 0.00 | 947.571 | 0.0 | 0.0 | 0.00 | -88.0 | quiet |
| Storm Echoes | Drone | -19.6 | -10.0 | -68.3 | 1174.0 | 2500 | 0.00 | 249.511 | 0.1 | 61.6 | 0.09 | -90.9 | dull, still ringing |
| Tape Worn Drums | Drone | -18.3 | -10.3 | -57.9 | 646.0 | 2500 | 0.00 | 329.642 | 0.0 | 54.2 | 0.04 | -91.5 | dull, still ringing |
| Tape Worn Melancholy | Drone | -18.9 | -10.0 | -63.1 | 1146.0 | 2500 | 0.00 | 962.592 | 0.1 | 16.7 | 0.10 | -91.5 | still ringing |
| Warm Drawbar | Drone | -21.4 | -11.0 | -64.9 | 728.0 | 2500 | 0.11 | 623.507 | 0.0 | 7.9 | 0.08 | -90.2 | still ringing |
| Deep Space Hum | Drone | -22.9 | -10.7 | -61.0 | 918.0 | 2500 | 0.04 | 487.892 | 0.0 | 60.6 | 0.13 | -90.8 | still ringing |
| Harp Drone | Drone | -24.3 | -11.7 | -48.7 | 80.0 | 1522 | 0.06 | 274.282 | 0.0 | 0.1 | 0.13 | -89.2 | dull |
| Quantum Ripple | Drone | -22.3 | -10.0 | -68.4 | 1042.0 | 2500 | 0.00 | 337.371 | 0.0 | 24.4 | 0.24 | -89.1 | dull, still ringing |
| Crystal Rings | Keys | -24.4 | -10.0 | -27.0 | 46.0 | 156 | 0.19 | 1548.81 | 0.1 | 0.0 | 0.16 | -91.2 |  |
| Dreamy Piano | Keys | -21.5 | -10.6 | -38.0 | 114.0 | 1108 | 0.09 | 308.39 | 0.0 | 0.0 | 0.24 | -89.0 | dull |
| Glass Chime | Keys | -22.1 | -10.0 | -34.0 | 26.0 | 666 | 0.06 | 444.195 | 0.0 | 0.1 | 0.93 | -91.3 | still ringing |
| Gritty Wurlitzer | Keys | -19.7 | -10.0 | -35.6 | 26.0 | 524 | 0.60 | 624.744 | 0.0 | 0.1 | 0.09 | -91.4 | linear tail |
| Metallic Clavinet | Keys | -34.9 | -10.0 | -33.3 | 34.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.05 | -90.5 | quiet, dies early, dull |
| Percussive Clavinet | Keys | -34.6 | -10.0 | -36.4 | 44.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.04 | -88.6 | quiet, dies early, dull |
| Singing Metal | Keys | -22.6 | -9.9 | -32.2 | 54.0 | 72 | 0.15 | 2256.12 | 0.9 | 0.7 | 0.07 | -90.1 |  |
| Warm Organ | Keys | -22.3 | -9.2 | -48.8 | 102.0 | 1896 | 0.07 | 406.257 | 0.0 | 1.3 | 0.26 | -91.2 |  |
| Warm Rhodes | Keys | -26.4 | -10.0 | -44.9 | 70.0 | 498 | 0.17 | 411.918 | 0.0 | 8.9 | 0.25 | -91.3 |  |
| Smooth Jazz Piano | Keys | -23.6 | -10.0 | -34.8 | 18.0 | 650 | 0.15 | 772.694 | 0.0 | 0.0 | 0.12 | -90.7 |  |
| Sustained Wood | Keys | -18.2 | -10.3 | -39.7 | 456.0 | 764 | 0.28 | 280.278 | 0.0 | 0.0 | 0.10 | -91.0 | dull |
| Warm Vintage Piano | Keys | -18.6 | -10.0 | -34.8 | 56.0 | 456 | 0.11 | 454.825 | 0.0 | 0.0 | 0.13 | -90.3 |  |
| Warm Wood | Keys | -18.4 | -10.4 | -29.6 | 38.0 | 310 | 0.21 | 439.054 | 0.6 | 0.0 | 0.06 | -90.6 |  |
| Analog Glint | Lead | -23.8 | -10.0 | -25.1 | 20.0 | 274 | 0.18 | 356.671 | 0.0 | 7.2 | 0.11 | -89.5 |  |
| Detuned Brass Stab 2 | Lead | -41.2 | -10.0 | -1.4 | 2.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.05 | -90.9 | quiet, hard edge, dies early, dull |
| Detuned Brass Stab | Lead | -41.4 | -10.0 | -2.9 | 0.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.16 | -90.3 | quiet, hard edge, dies early, dull |
| FM Square Lead | Lead | -27.2 | -9.3 | -31.9 | 36.0 | 312 | 0.24 | 1586.88 | 9.5 | 2.4 | 0.23 | -88.8 |  |
| Folded Organ | Lead | -28.1 | -10.0 | -30.4 | 4.0 | 496 | 0.08 | 236.951 | 0.0 | 23.7 | 0.11 | -91.5 | dull |
| Folded Sine | Lead | -21.6 | -10.0 | -22.9 | 20.0 | 478 | 0.08 | 531.965 | 0.7 | 18.5 | 0.22 | -89.4 |  |
| Folded Whistle | Lead | -31.9 | -10.0 | -22.6 | 18.0 | 336 | 0.20 | 816.064 | 4.0 | 64.7 | 0.20 | -45.7 | quiet, dies early |
| Formant Vibe | Lead | -29.0 | -10.2 | -24.4 | 14.0 | 0 | 0.00 | 713 | 0.0 | 0.0 | 0.31 | -86.4 | dies early |
| Glass Siren | Lead | -27.5 | -9.9 | -36.9 | 42.0 | 462 | 0.14 | 1157.2 | 1.1 | 13.5 | 0.20 | -91.3 |  |
| Glass Tine | Lead | -21.7 | -10.9 | -39.4 | 98.0 | 392 | 0.17 | 1072.47 | 0.0 | 0.0 | 0.09 | -91.3 |  |
| Gritty Acid | Lead | -24.6 | -10.9 | -28.9 | 118.0 | 442 | 0.11 | 758.203 | 0.3 | 0.0 | 0.10 | -87.9 | dies early |
| Hard Sync Buzz | Lead | -28.9 | -10.0 | -25.1 | 30.0 | 314 | 0.15 | 236.982 | 0.0 | 70.1 | 0.20 | -87.9 | dies early, dull |
| Industrial Scream | Lead | -34.2 | -10.7 | -26.3 | 26.0 | 148 | 0.20 | 3919.35 | 24.8 | 5.6 | 0.09 | -89.2 | quiet |
| Metallic Pulse | Lead | -24.4 | -10.0 | -69.1 | 1228.0 | 1110 | 0.18 | 1954.26 | 8.3 | 31.5 | 0.37 | -88.3 |  |
| Metallic Rumble | Lead | -33.2 | -10.1 | -19.6 | 12.0 | 0 | 0.00 | 13.2019 | 0.0 | 100.0 | 0.02 | -89.3 | quiet, dies early, dull |
| Resonant Pulse | Lead | -29.0 | -10.0 | -26.2 | 16.0 | 0 | 0.00 | 753.522 | 0.0 | 0.0 | 0.26 | -91.1 | dies early |
| Rhythmic Pulse | Lead | -27.1 | -10.0 | -34.9 | 26.0 | 224 | 0.29 | 487.728 | 0.0 | 18.2 | 0.14 | -89.3 | dies early |
| Screaming Distortion | Lead | -32.2 | -10.6 | -25.8 | 10.0 | 0 | 0.00 | 13.2019 | 0.0 | 100.0 | 0.16 | -88.8 | quiet, dies early, dull |
| Soulful Lead | Lead | -32.3 | -10.0 | -26.9 | 10.0 | 0 | 0.00 | 512.76 | 0.0 | 0.7 | 0.16 | -90.9 | quiet, dies early |
| Spectral Soar | Lead | -32.7 | -9.4 | -27.4 | 26.0 | 0 | 0.00 | 1409.27 | 2.2 | 0.1 | 0.36 | -91.4 | quiet, dies early |
| Synced Shimmer | Lead | -24.6 | -10.2 | -47.5 | 120.0 | 1856 | 0.23 | 1640.39 | 9.0 | 35.1 | 0.18 | -88.9 |  |
| Vibrant Pulse | Lead | -28.4 | -10.1 | -29.9 | 26.0 | 214 | 0.14 | 654.864 | 0.0 | 8.3 | 0.02 | -89.6 |  |
| Vocal-voice | Lead | -31.6 | -10.0 | -36.2 | 10.0 | 94 | 0.19 | 573.053 | 0.0 | 0.1 | 0.23 | -89.6 | quiet |
| Warm Glide | Lead | -27.8 | -10.2 | -56.1 | 44.0 | 398 | 0.23 | 322.899 | 0.0 | 1.6 | 0.01 | -88.6 | dull |
| Warm Vintage Lead | Lead | -31.7 | -10.2 | -25.4 | 16.0 | 0 | 0.00 | 635.775 | 0.5 | 0.1 | 0.00 | -89.9 | quiet, dies early |
| Warped Oscillator | Lead | -22.7 | -9.1 | -40.7 | 48.0 | 502 | 0.16 | 607.657 | 2.2 | 18.3 | 0.09 | -91.3 |  |
| Wide Future Bass | Lead | -36.3 | -10.1 | -18.4 | 8.0 | 0 | 0.00 | 411.733 | 0.0 | 16.3 | 0.23 | -89.0 | quiet, dies early |
| Airy Flute | Lead | -18.7 | -9.7 | -26.6 | 16.0 | 432 | 0.05 | 368.272 | 0.0 | 2.5 | 0.17 | -88.6 |  |
| Singing Wire | Lead | -30.9 | -10.0 | -29.7 | 16.0 | 890 | 0.15 | 2737.47 | 21.6 | 2.9 | 0.16 | -91.3 | quiet |
| Vocal Growl | Lead | -31.6 | -10.0 | -32.2 | 24.0 | 236 | 0.25 | 1365.59 | 7.9 | 0.7 | 0.20 | -91.3 | quiet |
| Vocal Whisper | Lead | -29.0 | -10.0 | -32.2 | 36.0 | 264 | 0.14 | 670.519 | 0.5 | 4.0 | 0.05 | -90.7 |  |
| Breath Of Light | Pad | -19.3 | -9.7 | -64.9 | 1266.0 | 2500 | 0.00 | 264.515 | 0.0 | 1.8 | 0.07 | -90.8 | dull, still ringing |
| Breathing Formant | Pad | -21.0 | -10.5 | -58.6 | 772.0 | 2258 | 0.05 | 355.993 | 0.0 | 15.4 | 0.19 | -89.1 | still ringing |
| Cinematic Choir | Pad | -29.4 | -11.0 | -65.6 | 1400.0 | 2500 | 0.27 | 279.758 | 0.0 | 7.6 | 0.43 | -90.6 | dull, still ringing |
| Distant Chorus | Pad | -21.7 | -10.0 | -54.3 | 262.0 | 1758 | 0.09 | 439.257 | 0.0 | 0.4 | 0.31 | -88.2 |  |
| Gentle Vibrato Pad | Pad | -18.2 | -10.0 | -38.6 | 120.0 | 2500 | 0.11 | 384.101 | 0.0 | 1.2 | 0.12 | -90.8 | still ringing |
| Glacial Sweep | Pad | -19.4 | -10.0 | -40.2 | 82.0 | 668 | 0.16 | 376.569 | 0.0 | 13.3 | 0.17 | -91.1 |  |
| Glass Bell II | Pad | -20.3 | -9.2 | -51.9 | 280.0 | 694 | 0.13 | 630.873 | 0.0 | 29.3 | 0.39 | -91.2 | still ringing |
| Glassy Bell | Pad | -18.3 | -9.6 | -63.3 | 1314.0 | 1952 | 0.05 | 342.158 | 0.1 | 0.1 | 0.09 | -91.3 | dull, still ringing |
| Glassy Drift | Pad | -21.4 | -9.8 | -61.7 | 544.0 | 1984 | 0.03 | 313.762 | 0.1 | 0.0 | 0.39 | -91.4 | dull |
| Glitchy Vibe | Pad | -23.3 | -10.0 | -46.6 | 240.0 | 1294 | 0.06 | 548.52 | 0.2 | 5.6 | 0.21 | -90.2 |  |
| Glowing Formant Pad | Pad | -30.0 | -19.1 | -73.2 | 282.0 | 2500 | 0.00 | 160.697 | 0.0 | 77.0 | 0.17 | -89.1 | dull, still ringing |
| Glowing Halo Pad | Pad | -20.1 | -10.0 | -50.8 | 364.0 | 2500 | 0.08 | 324.312 | 0.0 | 0.6 | 0.23 | -91.3 | dull, still ringing |
| Silent Drift | Pad | -22.1 | -9.9 | -49.4 | 416.0 | 1086 | 0.08 | 746.658 | 0.8 | 0.0 | 0.20 | -91.4 |  |
| Tape Memory | Pad | -18.6 | -10.0 | -50.5 | 260.0 | 2498 | 0.07 | 227.762 | 0.0 | 26.3 | 0.19 | -90.8 | dull, still ringing |
| Tapered Glow | Pad | -17.9 | -10.0 | -57.6 | 586.0 | 2500 | 0.03 | 309.966 | 0.0 | 0.1 | 0.10 | -90.9 | dull, still ringing |
| Vocal Lullaby | Pad | -21.1 | -9.1 | -49.0 | 822.0 | 2500 | 0.04 | 589.991 | 0.0 | 20.2 | 0.42 | -90.1 | still ringing |
| Vocal Resonance II | Pad | -21.3 | -9.2 | -47.5 | 466.0 | 2500 | 0.00 | 1618.84 | 4.7 | 10.0 | 0.37 | -87.9 | still ringing |
| Warm Analog Pad | Pad | -18.7 | -9.2 | -52.2 | 120.0 | 2500 | 0.03 | 258.207 | 0.0 | 4.7 | 0.20 | -89.7 | dull, still ringing |
| Warm Breath | Pad | -20.8 | -10.0 | -83.2 | 816.0 | 1948 | 0.06 | 152.065 | 0.0 | 85.6 | 0.29 | -90.4 | dull |
| Warm Glass | Pad | -19.5 | -9.6 | -54.2 | 340.0 | 1178 | 0.03 | 263.361 | 0.0 | 0.2 | 0.20 | -91.2 | dull |
| Warm Supersaw Pad | Pad | -21.2 | -10.0 | -44.5 | 176.0 | 2500 | 0.19 | 444.902 | 0.1 | 4.2 | 0.17 | -88.7 |  |
| Wavefolded Supersaw | Pad | -19.0 | -9.1 | -41.7 | 74.0 | 1286 | 0.09 | 378.457 | 0.0 | 10.4 | 0.25 | -51.2 |  |
| Choir Blend | Pad | -21.6 | -10.3 | -51.8 | 356.0 | 1404 | 0.10 | 385.14 | 0.1 | 36.7 | 0.42 | -90.4 |  |
| Cold Formant | Pad | -22.7 | -10.0 | -53.7 | 578.0 | 1724 | 0.28 | 273.962 | 0.0 | 35.3 | 0.14 | -88.1 | dull |
| Glassy Evolution | Pad | -19.9 | -10.6 | -55.1 | 652.0 | 2500 | 0.00 | 345.736 | 0.3 | 25.9 | 0.15 | -91.2 | dull, still ringing |
| Glassy Halo | Pad | -22.9 | -9.8 | -54.3 | 494.0 | 2500 | 0.08 | 216.591 | 0.3 | 70.5 | 0.63 | -91.4 | dull, still ringing |
| Vocal Breath | Pad | -23.0 | -9.5 | -54.4 | 414.0 | 2500 | 0.03 | 588.066 | 0.0 | 32.0 | 0.69 | -88.1 | still ringing |
| Vocal Harmonies | Pad | -21.7 | -11.0 | -56.5 | 494.0 | 1926 | 0.05 | 307.031 | 0.0 | 3.4 | 0.38 | -90.4 | dull |
| Vocal Resonance | Pad | -22.8 | -10.0 | -54.0 | 370.0 | 2500 | 0.14 | 1257.84 | 0.0 | 0.0 | 0.27 | -87.9 | still ringing |
| Vocal Shimmer | Pad | -24.3 | -10.0 | -41.3 | 246.0 | 568 | 0.05 | 340.745 | 0.0 | 3.9 | 0.26 | -88.1 | dull |
| Warm Strings | Pad | -19.9 | -10.0 | -48.2 | 296.0 | 830 | 0.33 | 277.293 | 0.0 | 0.2 | 0.29 | -89.7 | dull |
| 8bit Blip | Pluck | -37.4 | -9.0 | -11.7 | 4.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.00 | -90.7 | quiet, dull |
| Bounce Trigger | Pluck | -33.7 | -9.9 | -18.9 | 8.0 | 0 | 0.00 | 512.395 | 0.0 | 0.0 | 0.14 | -88.9 | quiet |
| Brass Pluck 2 | Pluck | -31.4 | -10.0 | -12.3 | 4.0 | 120 | 0.28 | 1012.56 | 0.0 | 3.1 | 0.13 | -90.0 | quiet |
| Brass Pluck | Pluck | -29.4 | -10.2 | -19.4 | 8.0 | 312 | 0.19 | 3040.52 | 28.0 | 3.8 | 0.13 | -90.5 |  |
| Faint Pluck | Pluck | -34.4 | -9.8 | -21.7 | 4.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.00 | -91.4 | quiet, dull |
| Future Saw | Pluck | -36.2 | -9.9 | -23.9 | 6.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.01 | -89.6 | quiet, dull |
| Glassy Pluck | Pluck | -37.9 | -10.0 | -21.9 | 6.0 | 0 | 0.00 | 596.51 | 0.1 | 0.2 | 0.04 | -91.3 | quiet |
| Guitar Pluck | Pluck | -34.7 | -10.0 | -21.0 | 4.0 | 0 | 0.00 | 621.651 | 0.0 | 0.0 | 0.02 | -88.9 | quiet |
| Hard Trance Pluck | Pluck | -26.1 | -10.0 | -31.1 | 54.0 | 0 | 0.00 | 413.056 | 0.0 | 0.5 | 0.43 | -89.2 |  |
| Metallic Pluck | Pluck | -31.6 | -10.2 | -16.3 | 8.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.04 | -91.5 | quiet, dull |
| Punchy Bass | Pluck | -36.6 | -10.1 | -19.5 | 10.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.26 | -90.5 | quiet, dull |
| Shimmering Pluck | Pluck | -37.5 | -10.0 | -25.5 | 14.0 | 0 | 0.00 | 677.157 | 0.0 | 0.0 | 0.05 | -91.5 | quiet |
| Soft Click | Pluck | -33.5 | -10.9 | -23.0 | 10.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.02 | -91.5 | quiet, dull |
| Sub Pluck | Pluck | -37.5 | -10.0 | -28.9 | 10.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.05 | -90.3 | quiet, dull |
| Tape Bell | Pluck | -34.9 | -10.0 | -26.5 | 28.0 | 0 | 0.00 | 921.164 | 0.0 | 0.1 | 0.27 | -91.4 | quiet |
| Tape Dust | Pluck | -29.8 | -10.6 | -24.1 | 6.0 | 0 | 0.00 | 405.511 | 0.0 | 0.0 | 0.11 | -91.3 |  |
| Vintage Tape Pulse | Pluck | -25.7 | -9.9 | -39.3 | 80.0 | 962 | 0.02 | 838.126 | 0.1 | 29.5 | 0.12 | -89.0 |  |
| Vocal Kalimba | Pluck | -37.5 | -10.0 | -29.1 | 18.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.05 | -88.3 | quiet, dull |
| Warm Chime | Pluck | -31.4 | -9.6 | -22.4 | 10.0 | 0 | 0.00 | 288.279 | 0.0 | 3.6 | 0.05 | -91.4 | quiet, dull |
| Wobbly Tape Pluck | Pluck | -35.3 | -10.3 | -18.2 | 8.0 | 0 | 0.00 | 321.823 | 0.0 | 0.6 | 0.11 | -91.4 | quiet, dull |
| Glitch Pulse | Pluck | -46.5 | -10.0 | -3.8 | 4.0 | 0 | 0.00 | 0 | 0.0 | 0.0 | 0.00 | -89.9 | quiet, dull |
| Harp String | Pluck | -30.5 | -10.0 | -40.1 | 38.0 | 0 | 0.00 | 266.858 | 0.0 | 0.0 | 0.10 | -89.2 | quiet, dull |
| Metallic House Pulse | Pluck | -19.8 | -9.4 | -21.3 | 8.0 | 62 | 0.21 | 1268.41 | 3.9 | 6.4 | 0.00 | -91.3 |  |
| Punchy House Stab | Pluck | -21.8 | -10.5 | -27.2 | 12.0 | 78 | 0.00 | 579.213 | 0.0 | 8.4 | 0.27 | -91.4 |  |
| Deep Current | Texture | -25.4 | -10.0 | -57.0 | 486.0 | 1772 | 0.03 | 413.814 | 0.0 | 34.3 | 0.12 | -88.6 |  |
| Digital Noise | Texture | -35.6 | -10.9 | -19.7 | 8.0 | 0 | 0.00 | 590.829 | 0.0 | 0.0 | 0.44 | -91.5 | quiet, dies early |
| Distorted Pulse | Texture | -34.4 | -10.0 | -34.3 | 26.0 | 0 | 0.00 | 326.923 | 0.0 | 0.1 | 0.14 | -90.3 | quiet, dies early, dull |
| Drizzle Pulse | Texture | -21.2 | -11.0 | -51.4 | 272.0 | 1580 | 0.06 | 259.217 | 0.0 | 11.0 | 0.08 | -91.5 | dull |
| Echoing Ghost | Texture | -23.7 | -9.3 | -58.7 | 608.0 | 2500 | 0.00 | 737.673 | 0.0 | 0.5 | 0.50 | -91.3 | still ringing |
| Gentle Wind | Texture | -18.5 | -10.0 | -54.9 | 618.0 | 1956 | 0.17 | 386.319 | 0.0 | 0.1 | 0.19 | -91.3 |  |
| Ghostly Vocal | Texture | -24.9 | -10.5 | -67.9 | 510.0 | 2500 | 0.04 | 566.059 | 0.0 | 1.0 | 0.50 | -90.9 | still ringing |
| Glowing Depths | Texture | -23.1 | -10.0 | -50.6 | 314.0 | 1902 | 0.18 | 391.671 | 0.0 | 13.1 | 0.13 | -91.1 |  |
| Granular Buzz | Texture | -22.7 | -9.8 | -54.9 | 188.0 | 460 | 0.16 | 169.8 | 0.0 | 85.1 | 0.23 | -89.1 | dull |
| Grit Morph | Texture | -26.7 | -10.2 | -54.3 | 370.0 | 680 | 0.00 | 602.388 | 0.0 | 0.5 | 0.00 | -90.5 |  |
| Gritty Scrape | Texture | -28.6 | -9.9 | -32.3 | 42.0 | 220 | 0.22 | 1350.11 | 5.2 | 17.1 | 0.10 | -89.0 |  |
| Raindrop Texture | Texture | -20.0 | -10.8 | -50.0 | 304.0 | 2314 | 0.14 | 313.054 | 0.1 | 12.9 | 0.16 | -91.2 | dull, still ringing |
| Vocal Stutter | Texture | -33.4 | -10.0 | -22.0 | 8.0 | 0 | 0.00 | 279.433 | 0.0 | 0.1 | 0.07 | -88.1 | quiet, dies early, dull |
| Vocal Texture Pad | Texture | -21.2 | -10.8 | -50.5 | 634.0 | 2500 | 0.04 | 415.04 | 0.0 | 0.1 | 0.38 | -89.3 | still ringing |
| Whispering Leaves | Texture | -19.6 | -9.6 | -78.9 | 1220.0 | 2500 | 0.33 | 257.412 | 0.0 | 3.4 | 0.07 | -91.4 | dull, still ringing |
| Deep Sea Whisper | Texture | -25.5 | -10.0 | -72.8 | 136.0 | 0 | 0.00 | 191.186 | 0.0 | 50.6 | 0.11 | -91.1 | dies early, dull |
| Gamelan Hum | Texture | -24.4 | -9.3 | -25.9 | 18.0 | 586 | 0.15 | 7516.96 | 53.0 | 0.1 | 0.10 | -87.7 | fizzy |
| Glissando Pulse | Texture | -31.9 | -10.0 | -42.4 | 120.0 | 0 | 0.00 | 519.112 | 0.0 | 2.1 | 0.14 | -89.5 | quiet, dies early |
| Pulse Texture | Texture | -31.3 | -10.0 | -41.3 | 44.0 | 0 | 0.00 | 472.016 | 0.0 | 1.0 | 0.07 | -90.0 | quiet, dies early |
| Tide Pulse | Texture | -27.4 | -10.8 | -44.0 | 88.0 | 0 | 0.00 | 303.178 | 0.0 | 20.9 | 0.22 | -91.4 | dies early, dull |

167 patches at MIDI 60, 7.92449 s.
median rms -25.3 dB (10-90% spread 16.7 dB), median peak -10.0 dB, median attack 48 ms, median release 448 ms, median release shape 0.04, median centroid 386.319 Hz, median width 0.13, median alias -90.4 dB, worst alias -45.7 dB.
flags: dies early 23, dull 58, fizzy 1, hard edge 2, linear tail 1, no low end 4, quiet 55, still ringing 33
