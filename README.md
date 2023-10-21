# Juiced: SilentPatch & Enhanced PC Demo

In this first-ever demo-oriented SilentPatch, I target all demo releases of Juiced. Due to this game's troubled development cycle,
demos published under Acclaim Entertainment present a state of the game that is noticeably different from the final game from THQ.
For this reason, I see value in SilentPatch improving the technical state of those demos (including the THQ ones) and unlocking all content included in them.
These early demos are a showcase of Juiced that "never came to be", so I want everyone to enjoy them to the fullest.

## Featured fixes:

Fixes marked with ⚙️ can be configured/toggled via the INI file.

### Acclaim Demos
* ⚙️ All shipped content has been unlocked. This increases the amount of in-game content from 1 route in the Race mode and 3 selectable cars to:
  * 4 selectable cars.
  * 4 routes in the Race mode + reverse variants + 1 Point-to-Point race.
  * Sprint race.
  * Showoff (Cruise) mode.
  * Solo mode.
* ⚙️ Widescreen from the console builds has been re-enabled and further improved. The default widescreen option slightly distorts the aspect ratio and appears horizontally squashed compared to 4:3. SilentPatch corrects this and additionally supports arbitrary aspect ratios, which means ultra widescreen now looks as expected.
* All known compatibility issues have been fixed. `JuicedConfig.exe` no longer needs Windows XP SP2 compatibility mode to run.
* Fixed an issue with menus and UI items flickering randomly.
* Fixed an issue with the race background music distorting and crackling at regular intervals.
* ⚙️ All menus can optionally be made accessible. Consider this a debug/testing option, since most menus that are locked are not finished and will softlock or crash the game.
* 3 teaser videos from the May demo have been included and re-enabled.
* The laps limit has been lifted from 2 to 6, like in the console prototype builds from a similar timeframe.
* In night and wet races, the limit of on-track cars has been lifted from 4 to 6.
* ⚙️ The default driver name `Player1` can now be overridden.
* May 2004 Demo can now be quit by pressing <kbd>Alt</kbd> + <kbd>F4</kbd>.
* All game settings have been moved from registry to `settings.ini`. This makes demos fully portable and prevents them from overwriting each other's settings.

### THQ Demos
* ⚙️ All shipped content has been unlocked. Since these demos don't include Custom Races, customization is done through the `SilentPatchJuicedDemo.ini` file and concerns only the second race. The following options are available:
  * 4 routes in the Race mode + reverse variants + 1 Point-to-Point race + its reverse variant.
  * Sprint race.
  * Showoff (Cruise) mode.
  * Solo mode.
  * Customizable time of day, weather, number of laps, and number of AI opponents.
* ⚙️ Demos have optionally been made endless. Instead of the game exiting after the second race, it returns to the garage and gives an option of tuning the car again and starting another race.
* The player's starter car can now be customized by editing the `scripts\CMSPlayersCrewCollection2.txt` file. The following cars are available: `gtr`, `nsx`, `supra`, `vette_zo6`, `viper`.
* A startup crash in the January 2005 demo occurring on PCs with more than 4 logical CPU cores has been fixed.
* `Juiced requires virtual memory to be enabled` error from the April and May 2005 demos has been fixed.
* Startup crashes in the May 2005 demo occurring when the OS locale is set to Czech, Polish or Russian have been fixed.
* `JuicedConfig.exe` no longer needs Windows XP SP2 compatibility mode or a Windows compatibility shim to run. This improves compatibility with environments where compatibility shims are not enabled by default, e.g. on Wine/Proton.
* ⚙️ All menus can optionally be made accessible. Consider this a debug/testing option, since most menus that are locked are not finished and will softlock or crash the game.
* ⚙️ The default driver name `Player` can now be overridden.
* ⚙️ Starting money can be adjusted.
* All game settings have been moved from registry to `settings.ini`. This makes demos fully portable and prevents them from overwriting each other's settings.

## Credits
* [**f4mi**](http://f4mi.com/) for preparing the showcase video
* [**Juiced Modding Community**](https://discord.com/invite/pu2jdxR/) for helping me find and dissect those demos and for answering all of my many questions regarding the game

## Related reads
* [Exploring the history of Juiced through prototypes and PC demos](https://cookieplmonster.github.io/2023/10/21/juiced-acclaim-and-pc-demos/)

[![Preview](http://img.youtube.com/vi/4lQ76h8KEkI/maxresdefault.jpg)](http://www.youtube.com/watch?v=4lQ76h8KEkI "Preview")
