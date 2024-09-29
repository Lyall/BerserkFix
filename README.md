# BERSERK: Band of the Hawk Fix
[![Patreon-Button](https://github.com/user-attachments/assets/56f4da45-0ed0-4210-a4a7-c381612370db)](https://www.patreon.com/Wintermance) [![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/W7W01UAI9) <br />
[![Github All Releases](https://img.shields.io/github/downloads/Lyall/BerserkFix/total.svg)](https://github.com/Lyall/BerserkFix/releases)

This is a fix for BERSERK: Band of the Hawk that adds ultrawide/narrower support and more.

## Features

### General
- Custom resolution support.
- Adjust framerate cap. (Experimental, see [known issues](#known-issues).)
- Remove Windows 7 compatibility nag message.

### Ultrawide/narrower
- Support for any aspect ratio.
- Fixed HUD stretching.
- Fixed movie stretching.

## Installation
- Grab the latest release of BerserkFix from [here.](https://github.com/Lyall/BerserkFix/releases)
- Extract the contents of the release zip in to the the game folder. e.g. ("**steamapps\common\BERSERK and the Band of the Hawk**" for Steam).

### Steam Deck/Linux Additional Instructions
🚩**You do not need to do this if you are using Windows!**
- Open up the game properties in Steam and add `WINEDLLOVERRIDES="dinput8=n,b" %command%` to the launch options.

## Configuration
- See **BerserkFix.ini** to adjust settings for the fix.

## Known Issues
Please report any issues you see.
This list will contain bugs which may or may not be fixed.

- Some movies may show visual glitches near the beginning.
- Enemy nameplates are stretched at non-16:9 resolutions.
- Adjusting the framerate cap can cause bugs. Some UI speeds up, for example.

## Screenshots
| ![ezgif-4-c1525c200a](https://github.com/user-attachments/assets/57ad687c-28d1-445e-8176-c37f0b3175b5) |
|:--:|
| Gameplay |

## Credits
Thanks to Terry on the WSGF Discord for providing a copy of the game! <br/>
[Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) for ASI loading. <br />
[inipp](https://github.com/mcmtroffaes/inipp) for ini reading. <br />
[spdlog](https://github.com/gabime/spdlog) for logging. <br />
[safetyhook](https://github.com/cursey/safetyhook) for hooking.
