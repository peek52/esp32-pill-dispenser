#pragma once

// Initializes the DFPlayer Mini via HardwareSerial
void audioSetup();

// Plays a specific track number from the SD card
// track 1: 10-minute warning
// track 2: Dispense time
void audioPlay(int track);

// Sets the volume (0-30) and persists it
void audioSetVolume(int volume);

// Retrieves the current volume
int audioGetVolume();
