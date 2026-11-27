## 1.8.3

**Fixed**
- Saving campaigns (.w3n) popped up a "0 errors, 1 warnings" dialog and packed through w3x2lni with a broken config encoding, which could prevent maps inside from saving
- "Test Map" (Ctrl+F9) on a new unsaved map (.w3m) did not launch the game, and fast save (MapHelper) always marked maps as TFT instead of RoC
- In the English UI, object names and comments from slk were still in Russian
- In the Ability Editor, the internal YDWETip field had the same name as the regular Tooltip (Tip), making it easy to confuse and break the ability. It is now labeled "KFWE friendly reminder"
- In the multi-window local test, one of the windows crashed with FATAL ERROR on game start (especially in slots 2-4) if the host started the countdown before map verification finished
- On every editor startup, loading WorldEditData.txt failed with an error in the log, preventing extra YDWE icons and models from loading
- Updating KFWE no longer overwrites user TESH settings, styles, includes and templates (config/options.ini, config/styles.ini, includes/, templates/)

**Changed**
- Campaigns (.w3n) are now packed directly with KFMO, faster and without extra w3x2lni round-tripping. Map and campaign packing in KFMO now streams to disk instead of loading the entire project into memory
- Cleaned up the KFWE menu in the top bar: shortened the title to KFWE without the version number, grouped items with separators and unified their naming
- Removed leftover files from old TESH and Horus in plugin\tesh (they are also deleted automatically on update)
- Updated KFWE icons across the loading screen, taskbar, launcher, config tool, editor, KFMO and installer

## 1.8.2

**New**
- Auto-null local variables. On save, KFWE adds set x = null for local handle variables (unit, group, location and so on) before every return and at the end of functions, so they do not leak. Your trigger code is not changed, only the saved map script. If the result fails the pjass check, the map is saved without it. On by default, turn it off in “Config” → “Map compilation”, or for one map with a `// @nonull` line anywhere in the map code

**Fixed**
- Objects created, changed or removed through slk showed up in the Object Editor only after reopening the map, and the notice about them popped up on every save
- Object text fields starting with a digit, a minus or a dot were written to slk instead of object data on slk save and could break in game. This also applies to KFMO
- Triggers that declare their own functions with “Custom script: endfunction” (for example the Unit Indexer from Damage Engine) failed to compile with fast save (MapHelper) with an “Undeclared variable” error. Such triggers are now converted like in the regular editor
- New maps no longer get the “Welcome to YDWE” trigger with YDWE PK ads
- The English UI still showed the “could not load file” error and the “Test as selected player” plugin name in Russian

**Changed**
- Fast save (MapHelper) is now a regular KFWE plugin in plugin\MapHelper. It is no longer copied into the Warcraft III folder when the editor starts and no longer stays there if the editor crashes
- The English UI calls map regions Rect everywhere, like the triggers do: palette, properties window, context menu, Layer menu

## 1.8.1

**Fixed**
- The Bloodlust buff was named “Earthquake (Caster)” in the Object Editor and had no tooltip
- Better wording in a few places of the English translation

## 1.8.0

**New**
- The editor and KFWE in English. Pick the language in “Config” → “Language”. Anyone can add their own language, see share\locale\README.md
- The unit palette now has hidden units that were missing before (for example the uloc locust). You can place them on the map, like in JNGP
- Trigger search can replace matches one at a time. Select a line and click “Replace” or press Enter in the replace field
- Search and replace can cover the whole map, only the current folder or only the current trigger. Switch with the buttons in the search window or Alt+T
- Ctrl+Shift+K in the code editor deletes the line under the cursor
- The trigger search and colorizer windows reopen where you closed them, with the same size
- The editor can use up to 4 GB of memory instead of 2, which helps with big maps
- “Test as player” works together with the single-window local test, then only the chosen player's slot is open in the lobby
- The KFWE menu has “Close all WC3 windows”, which closes every game window started from the editor

**Fixed**
- Clicking a trigger search result sometimes opened the trigger but did not select the match, or selected the wrong place
- “Replace all” sometimes silently skipped big triggers (the status line said “skipped”)
- Comment triggers showed the code of the neighboring trigger in search results
- /* */ comments were not highlighted green in the search preview
- After a “not enough memory” error the map could not be saved until the editor restarted
- After a few dozen saves in a row (especially campaigns) the editor stopped saving and lost GUI texts
- A campaign could not be saved with a long name, author or description
- Objects created through slk were treated as changed on every save, even though nobody touched them
- Objects created or removed through slk appeared in or disappeared from the Object Editor only after a restart
- The player list in “Test as player” did not update after saving the map
- With test as player selected, the local test started as a normal test
- The multi-window local test started extra windows for computer and empty slots, and they hung with “Unable to join the specified game”
- The multi-window local test crashed with FATAL ERROR when joining the lobby on big maps
- Players got the wrong colors in the local test, for example player 12 became blue

## 1.7.0

**New**
- Search across all triggers at once. Ctrl+F in code opens a window with results by trigger and a preview, Enter jumps to the line
- Replace in all triggers at once
- Search supports regular expressions, match case and whole word, and F3 steps through matches right in the code
- Dark theme in the code editor
- Local test starts faster, the game menu no longer slows down joining
- New presets and GUI texts from YDWE PK 1.8.4

**Fixed**
- Maps in lni format open with terrain and triggers again
- Local test works in both modes again
- Local test windows no longer end up in the game loading screen and no longer crash when joining at the same time
- Saved campaigns no longer lose their name or break maps
- KFMO no longer removes unused objects when testing
- Objects created through slk with fractional values were treated as changed and rewritten on every save
- If you remove the code that creates objects through slk, those objects are now removed on save, even without require 'slk'

## 1.6.0

**New**
- Files and folders can be dragged into the Import Manager straight from Explorer
- Folders are imported with all subfolders, paths inside the map keep the structure
- “Import files…” can pick several files at once
- After an update the list of changes is shown, you can open it again from the KFWE menu
- The update window shows what changed in the new version
- Terrain limits are removed, any slope and height from -1920 to 1920
- Reworked the window for entering a new object ID. The ID is checked as you type, there are “Suggested” and “Next in series” buttons, Esc uses the default ID

**Fixed**
- An object copied with manual ID input could lose the copied fields
- Fonts and scaling broke in the editor after the ID window
- Gameplay constants no longer show “Missing string” errors and are fully translated
- “Balance: can cancel” in the constants shows which ability it is
- “Missing string: WESTRING_AEVAL_YTIP” error
- The name of the Phoenix order of the Blood Mage

## 1.5.1

**Fixed**
- “Undeclared function CreateAllUnits / CreateAllItems / CreateAllDestructables” error with fast save in maps with libraries in code
- Error sound and a Windows crash report when closing the editor
- Ctrl+Shift+C works as “Go to game camera” again

**Changed**
- Copying coordinates is now Ctrl+Shift+X, with Alt it also copies the height

**Experimental**
- Faster saving: if the map code did not change, the code build is skipped

## 1.5.0

**New**
- Auto update: after start KFWE checks for a new version and offers to update
- New welcome message at startup, with GitHub and Discord links

**Fixed**
- “not enough memory” error when saving big maps with templates after a few hours of work
- Opening the editor while a map test was running lost the dark theme, splash and all KFWE plugins
- “Undeclared function CreateAllUnits” error in maps with their own main and fast save on
- “Undeclared function PK_IgnoreMapHelper” error in maps made in YDWE PK 1.8.4
- Dark theme crash when closing the editor
- “Missing string: WESTRING_TRIGCAT_…” messages at startup

## 1.4.1

**Fixed**
- Saving a map could fail with “Could not create directory ...w3xTemp...”
- Colorizer: Ctrl+Enter breaks the line again
