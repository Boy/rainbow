This is the edited version of the original readme file by Marine Kelley.
The file has been edited to take into account the improvements by Henri Beauchamp, as well as the Cool SL Viewer.

-------------
Version 1.23a


WHAT IS IT ?
------------

RestrainedLove is aimed at BDSM fans in SL who wish to enhance their experience by letting other people (such as their owners) take control of some of their abilities. In order to use its features, the Dom/me has to operate items (typically restraints) that the sub wears, but only the sub needs to use this viewer in that situation. Here is what a Dom/me can do to a sub who uses this viewer with RestrainedLofe enabled:

- Make an item undetachable (once locked, the sub has absolutely no way to detach it unless they relog with a different viewer or the item is unlocked).
- Prevent sending IMs, receiving IMs, sending chat or receiving chat (with exceptions if needed).
- Prevent teleporting (from the map, a landmark, or by a friend with exceptions if needed).
- Prevent rezzing, editing, using inventory, reading notecards, sending messages on non-public channels (again with exceptions if needed).
- Prevent standing up and force sitting.
- Prevent adding/removing clothes, force removing clothes and force detaching worn items (unless made undetachable).
- Force attaching clothes and items that are "shared" in the user's inventory (see below)
- Force teleporting the sub to an arbitrary location, without the ability to either refuse or cancel the teleport
- Hide names and/or location so that the sub cannot know who is around, or where they are

These features, when cleverly used together, make the sub truly *feel* the power of their Dom/me. Tested and approved by a lot of slaves.

Lockable items in SL work perfectly without this viewer but if you use it you'll find your experience... enhanced. But you don't *need* it to use them. Moreso, the Dom/me does not even need to use it at all, since it is made to enhance restraints (which are worn by the sub).



HOW TO ENABLE RESTRAINEDLOVE ?
---------------------------

- Get the Cool SL Viewer from http://sldev.free.fr/ for Linux or Windows, or from Hyang Zhao's blog for MacOS X (link to be found on http://sldev.free.fr/).
- Install the Cool SL Viewer following the instructions given on the download site.
- Start the viewer and log in.
- Open the preferences menu of the viewer.
- Select the "Cool features" tab.
- Select the "Miscellaneous" sub-tab.
- Check the "RestrainedLove mode" checkbox in this tab.
- Close the menu.
- Log off and quit the viewer.

Now, your viewer is RestrainedLofe enabled and you may log on again.



A WORD ABOUT SHARED FOLDERS
---------------------------

Read on, and also visit http://realrestraint.blogspot.com/2008/08/objects-sharing-tutorial.html for a tutorial explaining how to share folders properly, both with mod and no-mod objects.

Since v1.11, the viewer can "share" some of your items with scripts in world in order to let them force you to attach, detach and list what you have shared.

RL viewer v1.13 or above is able to share multiple levels of sub-folders to facilitate organization.

"Share" does NOT mean they will be taken by other people if they want to (some of the items may be no-transfer anyway), but only that they can force YOU to wear/unwear them at will through the use of a script YOUR restraints contain. They will remain in your inventory.

To do this :
* Create a folder named "#RLV" (without the quotes) directly under "My Inventory" (right-click on "My Inventory", select "New Folder"). We'll call this folder the "shared root".
* Move a folder containing restraints or other attachments directly into this new folder.
* Wear the contents of that folder, that's it !

So it would look like this :

 My Inventory
 |- #RLV
 |  |- cuffs
 |  |  |- left cuff (l forearm)   (no copy)
 |  |  \- right cuff (r forearm)   (no copy)
 |  \- gag
 |     \- gag (mouth)   (no copy)
 |- Animations
 |- Body Parts
 .
 .
 .

For example : If you're owning a set of RR Straps and want to share them, just move the folder "Straps BOXED" under the shared root.

Either wear all the items of the folders you have just moved (one folder at a time !) or rename your items yourself, so that each item name contains the name of the target attachment point. For example : "left cuff (l forearm)", "right ankle cuff (r lower leg)". Please note that no-modify items are a bit more complex to share, because they cannot be renamed either by you or by the viewer. More on that below.

The attachment point name is the same as the one you find in the "Attach To" menu of your inventory, and is case insensitive (for example : "chest", "skull", "stomach", "left ear", "r upper arm"...). If you wear the item without renaming it first it will be renamed automatically, but only if it is in a shared folder, and does not contain any attachment point name already, and is mod. If you want to wear it on another attachment point, you'll need to rename it by hand first.

Pieces of clothing are treated exactly the same way (in fact they can even be put in the folder of a set of restraints and be worn with the same command). Shoes, for instance, are a good example of mixed outfits : some attachments and the Shoes layer. Clothes are NOT renamed automatically when worn, since their very type decides where they are to be worn (skirt, jacket, undershirt...).

HOW TO SHARE NO-MODIFY ITEMS :
As you already know, no-mod items cannot be renamed so the technique is a bit more complex. Create a sub-folder inside the outfit folder (such as "cuffs" in the example above), put ONE no-modify item in it. When wearing the object, you'll see the folder itself be renamed (that's why you must not put more than one object inside it). So if your outfit contains several no-mod objects, you'll need to create as many folders and put the no-mod objects in them, one in each folder.

Example with no-modify shoes :

 My Inventory
 |- #RLV
 |  \- shoes
 |     |- left shoe (left foot)
 |     |  \- left shoe   (no modify) (no transfer)  <-- no-mod object
 |     |- right shoe (right foot)
 |     |  \- right shoe   (no modify) (no transfer) <-- no-mod object
 |     \- shoe base   (no modify) (no transfer)     <-- this is not an object
 |- Animations
 |- Body Parts
 .
 .
 .

GOTCHAS :
* Do NOT put a comma (',') in the name of the folders under the shared root or it would screw the list up.
* Don't forget to rename the items in the shared folders (or to wear these items at least once to have them be renamed automatically) or the force attach command will appear to do nothing at all.
* Avoid cluttering the shared root (or any folder under it) with many sub-folders, since some scripts may rely on the list they got with the @getinv command and chat messages are limited to 1023 characters. Choose wisely, and use short names. But with 9 characters per folder name average, you can expect to have about 100 folders available.
* Remember to put no-modify items in sub-folders, one each, so their names can be used by the viewer do find out where to attach them. They can't be shared like modify items since they can't be renamed, and the outfit folder itself will not be renamed (since it contains several items).



WHAT DOES IT DO IN DETAIL ?
---------------------------

Let's go down to business. There are 2 kinds of modifications :

* Permanent modifications. These are the ones that apply all the time, whether you're wearing a locked item or not:
- You cannot choose your log in location (it always log you in your last location).
- Automatic IM answer to anybody who sends "@version" (lowercase) to you, the viewer will answer its version so it's a quick way to check if a sub is using it or not. Note : some uneducated people send unsollicited "@version" IMs to check other people's viewers, without saying "Hi" or anything else. People using regular viewers do see these unsollicited IMs and associate them with spam, possibly going as far as ARing the initiator. I cannot be held responsible for that, just don't do this to people you don't know.

* Global modifications : These are the ones that occur when an object triggers them, modifying the global behaviour of the viewer, such as:
- No IM
- No chat
- No read notecards
- etc...

* Temporary modifications. These are the ones that occur when an object is locked on your avatar.
- No HUD attachments hiding. If you don't know what I mean don't ask, but that's a way to cheat restraints that prevent you from interacting with your environment, you can't cheat here.
- No Wireframe view (same remark as above).
- No "Dump All Attachments" (same remark as above).
- No "Attach To" or "Wear" when right clicking on an object in world. That could kick a locked object otherwise.
- "Edit" > "Detach Object..." in the top menubar is not working on locked objects.
- No "Release Keys" button (this is independant from the HideReleaseKeys setting of the Cool SL Viewer) and the "Tools" > "Release Keys" item in the top menubar is inactive.
- No Drag-and-drop of objects and folders from your inventory to your avatar.
- No "Detach and Drop" on the Pie menu (right click on it in-world).
- No "Detach" on the Pie menu when right clicking on your avatar.
- No "Detach All" on the Pie menu when right clicking on your avatar.
- "Attach To" from the inventory: if you try to attach on an attach point that contains a locked object will do nothing. It works for other attach points though, of course.
- No Detach from Yourself menu item in the inventory on locked objects.
- No "Tools" > "Reset/Recompile" scripts in selection on locked objects.
- No New Script on a locked item.
- No "Add to Outfit", "Take Off Items" from the folders menu actions while wearing a locked item as they do the same thing as "Wear" (see below).
- No "Wear" menu item in the inventory when you're wearing at least one locked object (*)
- No attach/detach by double-clicking on objects in the inventory (Cool SL Viewer feature).
- No modifying the inventory of a locked item

(*) : a bit harsh indeed... but it's the only solution for design reasons (regular viewer design that is). When trying to Wear an object, the viewer does not know where to attach the item so it waits for the server to send an "attach there" message, and the viewer has no choice but to comply. You may "Attach To" instead but be very careful with it: know where your object has to attach first or you'll end up having to reposition it by hand. It might be a good idea to rename the objects (when they are mod-ok) so that their normal attachement point is appended to their name.

Since 1.11 you are able to use Wear again if the name of the object contains the name of an attachment point (named like the ones in the "Attach To" submenu). This fake Wear is actually a disguised Attach To command.

Since v1.21a you may set the RestrainedLofeAllowWear advanced setting to TRUE so to allow the Wear commmand again on all items. In this case, the RestrainedLofe code will attempt to reattach automatically any kicked off locked item. However, be warned that because of a serious bug in the asset server (see https://jira.secondlife.com/browse/SVC-3579), attachments that are reworn too fast (less than 15 seconds or so, but up to 30 seconds, depending on asset servers lag) after being detached will loose all the modifications (including script states) that were made to them after the last time they were attached or the last time you TPed or logged in with them on (whichever occured last). Starting from RestrainedLofe v1.22f, you may use the RestrainedLofeReattachDelay advanced setting (defaults to 15s) to adjust the reattach delay.


RELEASE NOTES :
---------------
1.23a (@versionnum = 1230001) by Henri Beauchamp:
- Inclusion of Marine Kelley's change:
	- changed : The name of the viewer itself. It is still known as "the RLV", but the meaning of the letters "RLV" change from "Restrained Life Viewer" to "Restrained Love Viewer". This is due to the Third Party Viewer policy that Linden Lab has published on 02/22/2010, forbidding to use the word "Life" or any synonym in the name of a Third Party Viewer such as the RLV.
	- added : due to the name change, a new command appears : @versionnew, which returns "RestrainedLove viewer v1.23.0 (SL 1.23.5)". The old @version command still does the same as before, i.e. return "RestrainedLife viewer v1.23.0 (SL 1.23.5)", but you are encouraged to not use this command anymore in new scripts, and to do your best to hide the name "RestrainedLife" from the user. Use "RestrainedLove" instead, or better, "RLV".
- Changed: commands taking a channel number to send their reply to now accept negative channels (handy to prevent cheating during RestrainedLove detection, e.g. @version=-1 prevents the user to cheat since they can't chat on a negative channel and spoof a RestrainedLove viewer reply on it). Note however that the length of the message returned by the commands on negative channels cannot be greater than 255 characters (instead of 1023 for positive channel numbers), so a negative channel number is not a good choice for commands that could return long strings (such as @getstatusall or @getinv, for example).


1.22h (@versionnum = 1220106) by Henri Beauchamp:
- Added: support for the new Alpha and Tattoo wearable (in @getoufit, @remoutfit, etc). Note that for compatibility reasons, the Alpha and Tattoo flags appear (in this order) after the shape flag in the string returned by @getoutfit=<channel>.
- Changed: The "Hair" bodypart cannot anymore be removed with @remoutfit. This is because baked hair is now a requirement in v2.0 and later viewers, and an avatar without hair may also result in an unrezzed avatar in v1.23 anyway. If you want to hide the hair bodypart, you can use the new Alpha wearable.


1.22g (@versionnum = 1220105) by Henri Beauchamp:
- Fixed: @redirchat does not truncate emotes any more, like originally intended and documented in the API.


1.22f (@versionnum = 1220104) by Henri Beauchamp:
- Changed: code cleanup.
- Changed: reworked the auto-reattachment feature to make it more reliable and to avoid state loss in reattached items and allow retries on reattach failures. New RestrainedLofeReattachDelay advanced setting implemented.


1.22e (@versionnum = 1220103) by Henri Beauchamp:
- Changed: Speed optimization relative to the RestrainedLofeDebug flag.


1.22d (@versionnum = 1220102) by Henri Beauchamp:
- Added: a RestrainedLofe sub-menu in the Advanced menu of the viewer (or Client menu for v1.19.0.5), allowing to easily toggle the advanced RestrainedLofe settings.
- Fixed: a bug introduced in Marine's v1.22 and that made the "Empty Lost And Found" item vanish from the "Lost And Found" folder context menu.


1.22c (@versionnum = 1220101) by Henri Beauchamp (equivalent to Marine's v1.22.1):
- Inclusion of Marine Kelley's change:
	- changed (*): The user is now able to focus their camera on objects even through a HUD (except when in Mouselook). This is handy for people who like to spend time bound and "blocked" (meaning their clicks are intercepted by a huge HUD prim across their screen), but who dislike being unable to focus.
(*) does not (yet) apply to viewer v1.19.0.5.


1.22b (@versionnum = 1220002) by Henri Beauchamp:
- Fixed: crash bugs that could occur while the avatar is rezzing (during logins or after TPs), especially when the viewer window is minimized or not displayed (i.e. on another workspace than the current one).


1.22a (@versionnum = 1220001) by Henri Beauchamp (equivalent to Marine's v1.22):
- Inclusion of Marine Kelley's changes:
	- fixed : inventory context menu would not refresh properly after everything is unlocked.
	- added : @addattach and @remattach commands, to do what the old @detach:point=n command did (this command keeps being valid, and must be seen as an alias to @addattach and @remattach used at the same time). Thanks to all who gave their opinions and allowed a little brainstorm on this one !
	- added : @viewscript and @viewtexture, which work exactly like @viewnote, on scripts and textures (and snapshots) respectively. Thank you Yar Telling for the hint ! 
- Adaptation of the new code to v1.19.0.5.
- Minor cleanup and optimizations.


1.21a (@versionnum = 1210101) by Henri Beauchamp (equivalent to Marine's v1.21.1):
- Inclusion of Marine Kelley's changes from v1.21:
	- fixed : A clever way to cheat around @shownames (thank you Talisha Allen).
	- changed (1): Reinstated "Wear" on the contextual menu even when something is locked and no attach point is contained in the name of the item. This holds the risk of kicking a locked object off, but it will be reattached automatically after 5 seconds anyway. Even "Add To Outfit" and "Take Off Items" work. This was a MUCH awaited feature !
	- changed : Added support for reattaching several objects at the same time. Objects will be reattached at 1 second interval.
	- added : @defaultwear restriction. When this restriction is set, the "Wear" command will work like it did before this version, i.e. disappear if something is locked and no attach point information is contained within the name. This is for subs who tend to abuse the Wear menu and kicking off locked objects a little too often.
	- added : @versionnum command to retrieve the version number directly, instead of having parse the "RestrainedLofe viewer v1.20.2 (1.23.4)" string. Here it will return "1210000".
	- added : @permissive command that tells the viewer that any exception to @sendim, @recvim, @recvchat, @tplure, @recvemote and @sendchannel MUST come from the object that issued it or will be ignored (without this command, any object can set an exception to the restrictions issued by any other object).
	- added : @sendim_sec, @recvim_sec, @recvchat_sec, @tplure_sec, @recvemote_sec and @sendchannel_sec to do the same as @permissive, but one restriction at a time (i.e. exceptions to @sendchannel from other objects won't be ignored if @sendim_sec is set).
- Inclusion of Marine Kelley's changes from v1.21.1:
	- fixed : locking attachment points was not working anymore.
	- fixed : massive reattach after an "Add To Outfit" command could fail with a "pending attachment" kind of error message in laggy areas (thank you Henri Beauchamp).
	- changed (2): "Add To Outfit", "Take Off Items" and "Replace Outfit" menu items will be hidden if something is locked (object or clothing) inside the folder you have selected, or any of the folders it contains, recursively.
	- known issue : These menu items will be hidden even if there are only clothes in the folder and none of them is locked, but another piece of clothing is locked specifically (for instance, you are trying to unwear pants, but the shirt is locked). This is because of a limitation in the code and will be corrected in a future version.
- Work around for (1): Allowing the "Wear" command in inventory context menus makes it possible to have locked attachments kicked. Theorically, the RestrainedLofe code is able to reattach these kicked locked attachments. Alas, and because of the low reliability and extremely variable delays encountered when asset server operations are involved, the reattachment may fail. This is why I implemented a new debug setting ("RestrainedLofeAllowWear"): when FALSE (which is the default), the "Wear" command is unavailable and RestrainedLofe behaves like in v1.20 and previous versions. When TRUE, the "Wear" command availability is goverened by the @defaultwear command (i.e. "Wear" is available by default), like in Marine's v1.21.
- Work around for (2): Because of automatic reattachment failures, "Add To Outfit", "Take Off Items" and "Replace Outfit" are for now disabled (when at least one worn attachment is locked) for all folders and not just for folders containing locked items. 


1.20c by Henri Beauchamp (equivalent to Marine's v1.20.2):
- Inclusion of Marine Kelley's changes:
	- fixed : a nasty crash when reattaching a locked object, introduced in 1.20. Thanks to all who helped tracking this down.
	- fixed : a workaround @tplm that was supposedly fixed in 1.20.
- Code clean up and optimizations.


1.20b by Henri Beauchamp (equivalent to Marine's v1.20.1):
- Inclusion of Marine Kelley's changes:
	- fixed : crash to desktop when hearing chat from an unrezzed avatar while under @shownames (bug introduced in 1.20).
	- fixed : crash to desktop when forcing an object to be detached then locking its attach point right away, which would trigger an infinite loop (introduced in 1.20).
	- fixed : a cheat around @shownames (thanks Jolene Tatham). (*)
(*) Did not affect the Cool SL Viewer v1.19.0.5.


1.20a by Henri Beauchamp (equivalent to Marine's v1.20):
- Inclusion of Marine Kelley's changes:
	- added : @notify command to let scripts be notified when a particular restriction (or just any restriction) is issued or lifted by an object. It does not disclose the object itself, just the fact a restriction has changed. (thank you Corvan Nansen for the idea)
	- added : @detach:<attach_point> command to lock a particular attachment point. When using this command, any object worn there is locked on, even if it is not even scripted, and no other object can kick it off. If the attachment point is empty, this command will lock it empty, even if another object is attached to it with llAttachToAvatar(). (thank you Chorazin Allen for the idea)
	- changed : improved the attachment point calculation in the names of inventory items. Now it looks from right to left (to be consistent with how the RLV renames items when worn), and will select the candidates with the longest names first. In other words, it makes the RLV ready if the number of attachment points is increased (like adding "chest (2)" and the like).
	- changed : hide custom text in friendship offers when unable to receive IMs.
	- fixed : HUDs and unrezzed objects and avatars were immune to @recvchat (they could always be heard). (thank you Jennifer Ida for reporting this)
- Code clean up and optimizations.


1.19b by Henri Beauchamp:
- Never redirect (@redirchar, @rediremote) Out Of Character chat (text starting and ending with double parenthesis): the players must be able to safeword or voice a personal problem/concern.


1.19a by Henri Beauchamp (equivalent to Marine's v1.19):
- Inclusion of Marine Kelley's changes:
	- added : now allows to hide the hovertext floating over one prim in particular (not necessarily the one that issues the command), or all the hovertexts, or only the ones on the HUD, or only the ones in-world. Thank you Lyllani Bellic for the idea.
	- added : @rediremote to redirect emotes to private channels like @redirchat does. Now that one was a popular request !
	- added : @recvemote to prevent hearing emotes like @recvchat prevents hearing chat, also with exceptions. Not as popular but as handy !
	- changed : Now the hovertexts are refreshed immediately when issuing some of the RLV commands (sounds easy, but it was a pain in the **** to implement).
	- fixed : @acceptpermission was broken in 1.18. Partly my fault, sorry.
	- fixed : chat messages in history were showing a weird dot (".") on a single line when prevented from hearing chat. An old bug.
	- fixed : @getpath didn't work in a child prim. Thank you Henri Beauchamp for the tip.
	- fixed : "@attach:main=force" unified with ".Backup (main)" !


1.18a by Henri Beauchamp (equivalent to Marine's v1.18):
- Inclusion of Marine Kelley's changes:
	- changed : now showing (PG), (Mature) or (Adult) even when the location is hidden.
	- fixed : the world map and minimap buttons were not turning themselves off properly when @showloc was issued while they were activated.
	- fixed : now unable to chat on CHANNEL_DEBUG while under @sendchat (thank you Sophia Barrett).
	- fixed : "so and so gave you..." now hides the name while under @shownames.


1.17b by Henri Beauchamp:
- changed: removed the "llOwnerSay() beginning with two spaces not displayed to RL users" feature (introduced by Marine in v1.16), since it breaks existing scripted items that have nothing to do with RestrainedLove...


1.17a by Henri Beauchamp:
- fixed : allow again non-prim hair to be removed via @remoutfit.
- fixed : a crash bug when the #RLV folder is missing and llGiveInventoryList() is used to give a sub-folder to #RLV.
- changed: do not prevent "Go To" or DoubleClickAutoPilot when some device is locked (which does not make sense), but only when llTakeControl() was used to take control on CONTROL_FWD (thus preventing to override any speed limitation or movement restriction).
- Inclusion of Marine Kelley's changes (from v1.17):
	- fixed : visual clues about the map and minimap were a bit... clueless at times.
	- changed : don't go to third view while in Mouselook and switching back to SL from another application. Doesn't work if the window was minimized or hidden (on MacOS X for example).
	- changed : don't allow partial matches on folders prefixes with a "~" character anymore, to avoid taking precedence over the "regular" folders. Thank you Mo Noel for the heads up.
	- changed : now PERMISSION_TRIGGER_ANIMATION is also granted when sitting while @acceptpermission is active, even if the object we are sitting on does not actually contain the animation. Useful for rezzable poseballs.
	- added : @setrot:angle=force. This command allows you to make the avatar turn to a direction, in radians from the north. This is not possible through a LSL function call so here it is. Be aware that this command is not more precise than the llGetRot() LSL call (for instance the avatar won't rotate if the rotation is less than a few degrees), but it is better than nothing. It is much more precise while in Mouselook, and does not do anything while sitting.


1.16g by Henri Beauchamp:
- Inclusion of Marine Kelley's changes (from v1.16.2):
	- fixed : RLV rarely forgets to activate restrictions on relog in particularly laggy areas. This was due to the viewer calling its garbage collector too early, hence clearing restrictions while the restraints had not rezzed yet. The solution I used to fix that is very simple (don't call the garbage collector before a few minutes on startup), but that should do the trick.
	- fixed : RLV checking whether you had locked HUDs at every frame. Now cached to improve performance.
	- fixed : @shownames was showing "An unknown person" for about 20% of the people around. Well silly me. Using a signed char (-128 > +127) for a positive hash (0 > 255) was not a brillant idea. (*)
	- fixed : minor memory leak in RRInterface::forceAttach. (*)
	- fixed : @unsit was not always unsitting you. This bug has been there since the beginning and was very annoying. (*)
	- fixed : a few clever partial workarounds for some restrictions... (*)
	- fixed : @chatshout and @chatwhisper were also changing the range of the automatic viewer responses.
	- fixed : @remoutfit:xxx=force also allowed to remove bodyparts (but it was not visible on the screen).
	- fixed : @acceptpermission was too... permissive.
	- removed : @denypermission is now deprecated. It was there to prevent a script from kicking a locked object with a llRequestPermissions(PERMISSION_ATTACH) followed by a llAttachToAvatar() but since locked objects now automatically reattach themselves, this restriction makes no sense anymore and is only annoying people who do want their HUDs to attach automatically.
	- changed : removed the throttle on permissions concerned by @acceptpermission (since we don't see the dialogs anyway). Thank you Mo Noel for reporting this.
	- added : Visual clues on the lower toolbar : Map, Minimap, Build and Inventory now update themselves according to their respective restrictions. (*) (1)
	- added : @detachme=force. This "simple" command just makes an attachment detach itself and only itself if not locked. There was a need (even if a script could do it with a llDetachFromAvatar call, after granting permission) because the script needs to make sure the restrictions are cleared before detaching, by issuing a @clear,detachme=force list of commands. Before that, you had to call "@clear", wait a little, then detach the item and pray that it would not reattach itself after 5 seconds.
	- added : @sit=n. This simple restriction has been added to reinforce the security of most cages, in which the prisoner does not have any opportunity to sit anyway. Thank you Chorazin Allen for the idea.
- changed : Inventory offers to #RLV is now enabled by default (since it is now officialy supported in Marine's v1.16.2). It can be disabled by setting RestrainedLofeForbidGiveToRLV to TRUE.
- bugfix: fixed a bug in (1) (see above) which prevented the toolbar "Build" button to get properly updated when RestrainedLofe is disabled.


1.16f by Henri Beauchamp:
- changed: @putinv has been removed and only the #RLV/~folder redirection has been kept (meaning a standard Keep/Discard/Mute dialog is always presented to the user). This inventory redirection is only active when the RestrainedLofeAllowGiveToRLV environment variable is set to TRUE.


1.16e by Henri Beauchamp:
- fixed: removed the (undocumented) limitation that made it imposible to force-sit an avatar under @fartouch=n restriction (bug introduced in v1.16), as it breaks existing contents and is very disputable.
- changed: @putinv now only accepts #RLV/~folder (the tilde prefix is mandatory) for items given via llGiveInventoryList(), and cannot be used by an item held into such a #RLV/~folder.


1.16d by Henri Beauchamp:
- changed: @putinv is now disabled by default and can be enabled by setting the RestrainedLofeAllowPutInv debug setting to TRUE.
- changed: when @putinv is in force, do not hide any more in the chat log the name of the folder given via llGiveInventoryList(id, "#RLV/folder", list_of_stuff).


1.16c by Henri Beauchamp:
- Inclusion of Marine Kelley's changes (from v1.16.1):
	- fixed : removed a way to force an avatar to talk on channel 0. Thanks Maike Short
	- fixed : @getinvworn would return wrong results when a modifiable object was contained inside a folder named ".(right hand)", for instance. Thanks Satomi Ahn
	- fixed : the viewer would not automatically answer RLV queries when minimized or hidden on MacOS X
	- fixed : a clever cheat around @showloc
	- fixed : a clever cheat around undetachable HUDs.
	- fixed : @getpath:shirt would return the path to the shirt item even if it was not shared.
	- changed : "dummy names" begin with a capital again. It was a try, but it didn't look good.
	- changed : first and last names only won't be hidden anymore, only full names. Thanks and sorry Mo "My short name messes my dialog boxes up !" Noel
	- added (1): when a locked object is detached anyway, by any means, it is automatically reattached 5 seconds later (not sooner, to avoid rollbacks), and in the meantime every RLV commands are ignored to avoid infinite loops.
	(1) This feature does not work with llDetachFromAavatar(). See the work around below.
- fixed: crash bug while under @fartouch restriction and CTRL-selecting a prim. Fix by Kitty Barnett.
- fixed: a glitch allowing to circumvent the @fartouch restriction. Fix by Kitty Barnett.
- fixed: a couple of bugs in v1.16.1 changes above.
- work around: when a locked object is detached and fails to be reattached (see (1) above), do not block the RLV commands after the reattach delay has elapsed.
- changed: changes to RestrainedLofeAllowSetEnv now only take effect after a viewer restart, so to be consistent with Marine's v1.16.1 RLV behaviour. Note that Marine's RLV v1.16.1 still does not handle correctly this flag (@setenv=n is still possible with Marine's code when RestrainedLofeAllowSetEnv is TRUE, which is a bug).
- added: new @putinv:avatar_id=add/rem command, allowing avatar_id to issue (via a relay if avatar_id != victim_id) llGiveInventoryList(victim_id, "#RLV/subfolder", list_of stuff), so that objects ("list_of_stuff") is added to the #RLV folder of victim_id, into a new "subfolder". Adapted from a proposal and patch by Saunuk Flatley.


1.16b by Henri Beauchamp:
- Fixed the @getdebug_* and @setdebug_* bugs.
- Removed the llGetAgentLanguage() identification method as it is going to be removed from Marine's RL 1.16.1 and breaks some existing contents.


1.16a by Henri Beauchamp (equivalent to Marine's v1.16):
- Inclusion of Marine Kelley's changes:
	- fixed (1) : improved touch. Now the viewer compares with the actual point the user clicks on, instead of the center of the root prim.
	- fixed : could bypass the @sittp restriction under special conditions (depending on the sit target).
	- fixed : improved speed (muchly) while under many restrictions, by caching them.
	- added : @accepttp restriction has been extended to accept TP offers from anyone when no parameter is given (before a parameter was mandatory).
	- changed : now even hovertexts and dialog boxes are "censored" when prevented from seeing names or location. This will make it difficult to cheat with a radar now !
	- changed : friends won't show in yellow in the minimap under a show names restriction (it is a new feature in the SL viewer v1.22.*).
	- added : @setdebug and @getdebug, working exactly like @setenv and @getenv, but for debug settings. For the moment only AvatarSex (to get/set gender) and RenderResolutionDivisor (to make the screen blurry) are accepted. All the other debug settings are ignored.
	- added : @redirchat to redirect chat spoken on channel 0 to any other private channel, thusly prevent the user from speaking on channel) 0 at all (not even a "..."). This does not apply to emotes, and if several @redirchat restrictions are issued, all of them are taken into account (i.e. chat will be dispatched over several channels at once). This was a very popular request !
	- changed : @getstatus now prepends a slash ("/") before the returned message to prevent from griefing. This does not confuse llParseString2List() calls in a script, but does confuse llParseStringKeppNulls().
	- changed : the case is now ignored when names are censored.
	- added : @getpath to get the path from #RLV to the object, or to the object which occupies the attachment point given as parameter, or to the piece of clothing given as parameter. The object or clothig must be shared, otherwise it returns nothing.
	- added : @attachthis, @attachallthis, @detachthis, @detachallthis commands, which are shortcuts to a @getpath call followed by an @attach, @attachall, @detach or @detachall command respectively. Very handy to manage outfits without breaking the privacy of the user's inventory !
	- added : if an owner message (llOwnerSay) begins with two spaces, it will be hidden to the user. Like a remark or a comment. Regular viewer users will see it normally, of course.
	- changed : added many more "dummy names" for the @shownames restriction. There are 28 of them now. The hash function should be better as well, it was choosing the name based on the length of the name of the avatar before, which could end up in many times the same name around, confusing the user.
	- fixed : don't prevent teleporting when unable to unsit but not currently sitting.
	- added (1): when a script calls llGetAgentLanguage() on a RLV user, the result will be "RestrainedLofe Viewer v1....", exactly like a @version call. The user cannot prevent the viewer from returning this, no matter which language they are using and whether they have checked the "make language public" checkbox or not. This is experimental, if it bothers too many people I will remove it in the next version, but not later. In other words, if it is still there in the next version, it will stay there.
	- added : @acceptpermission to automatically accept permissions to attach and to take controls, @denypermission to automatically deny those permissions (the latter takes precedence over the former, of course).
	- fixed : a small cheat around @sendchat. Thank you Vanilla Meili !
(1) This does not apply to v1.19.0.5 based viewers since it relies on code present in v1.21 or later viewers.


1.15c by Henri Beauchamp (equivalent to Marine's v1.15.2):
- Inclusion of Marine Kelley's changes:
	- fixed : invisible folders (starting with ".") were taken into account in the @attachall command
	- fixed : items that are neither objects nor pieces of clothing were taking into account in the @getinvwon command
	- fixed : skin and hair did not register in the @getinvworn command
	- fixed : viewer was freezing when using @getinvworn while RLV debug is active. Thank you Mastaminder McDonnell !
	- fixed : @getinvworn was seeing every item contained directly under #RLV, but did not allow the user to attach nor detach them. As #RLV is not an outfit, now @getinvworn ignores them
	- changed : now when the user is unable to edit things, they are also unable to see any beacon, including invisible objects
- fixed: because of a typo, RestrainedLofeAllowSetEnv was not working properly. It has been replaced by RestrainedLofeNoSetEnv (to ignore both @setenv and @setenv_* commands when set to TRUE) and is now working properly.


1.15b by Henri Beauchamp (equivalent to Marine's v1.15.1):
- Inclusion of Marine Kelley's changes:
	- fixed : an empty shared folder would, in certain cases, mess the information provided by @getinvworn (saying nothing to wear while there are items there). Thank you Julia Banshee !
	- fixed : a piece of clothing alone in a folder, and no-mod would be treated as a no-mod object
	- fixed : a no-mod object which name contains the name of an attachment point would use it even if it was contained inside a folder which name contains another attachment point. Thank you Charon Carfagno !
- added: implemented the RestrainedLofeAllowSetEnv flag (TRUE by default) to allow or forbid (when set to FALSE) environment (day time, Windlight) changes via @setenv_* commands.
- added: implemented @getenv_daytime for v1.19.0.5 viewers.


1.15a by Henri Beauchamp (equivalent to Marine's v1.15):
- Inclusion of Marine Kelley's changes:
	- fixed : order of HUD attachments would make a "top" HUD attach to "top left" and a "bottom" HUD attach to "bottom right", messing them.
	- fixed : detaching a shared folder through a script would also detach one item from a sub-folder regardless of its perms (it was done so primarily for no-mod items before instating sub-folder sharing). Thank you Mastaminder McDonnell for the bug report !
	- fixed : prevent grabbing/spinning when unable to edit things as well.
	- fixed : an old loophole that surfaced again. Thanks TNT74 Pennell and Giri Gritzi !
	- fixed : @setenv_densitymultiplier and @setenv_distancemultiplier were not accurate
	- changed : when trying to attach/detach a folder through a script, whatever is after a pipe ("|") in the name is ignored (pipe included). This is for convenience after using the @getinvworn commmand.
	- changed : now unable to teleport when unable to unsit (that needed unnecessary additional restrictions such as a leash that was not really needed)
	- added : @accepttp command to force the sub to accept a teleport from someone (not necessarily a friend). This does not deprecate @tpto which teleports to an arbitraty location, while @accepttp teleports to an avatar. To the sub it will look like they have been teleported by a Linden (no confirmation box, no Cancel button).
	- added : @getinvworn command to which folders are containing worn items. It roughly works like @getinv, with more information... but it's quite uneasy to explain here, please refer to the API.
	- added : @chatwhisper, @chatnormal and @chatshout commands, which prevent from whispering, chatting normally or shouting respectively. It is different from @sendchat because they do not discard chat messages, they just transform a whisper to normal, normal to whisper, and shout to normal respectively. If all of these restrictions are active, the avatar can only whisper. This kind of command is useful in prisons where some prisoners like to shout all the time.
	- added : @getstatusall command that acts exactly like a @getstatus, but will list all the restrictions the avatar is currently under, without of course disclosing which object issued which restriction.
	- added : @attachall and @detachall commands, which work exactly like @attach (a folder) and @detach (a folder), but recursively. This means it will attach/detach whatever is inside a folder, and in its children as well.
	- added : @getenv_...=nnnn command to get the current Windlight parameters. Works exactly like @setenv_...=force, with the same names. (1)
- Fixed a problem with wrong animation being played whenever @chatwhisper, @chatnormal or @chatshout are in force.
- Fixed a bug that would have crept up when RestrainedLofe is disabled in the viewer (which is impossible in Marine's viewer).
(1) This new feature cannot be backported to v1.19 viewers, since they don't implement the Windlight renderer. "@getenv_*" commands are therefore not implemented for v1.19 viewers and are ignored.


1.14c by Henri Beauchamp (equivalent to Marine's v1.14.2):
- Inclusion of Marine Kelley's changes:
	- fixed : crashing when editing something beyond 1.5m, while being prevented from touching things over that distance.
	- fixed : very odd behaviour when clicking on something while being prevented from touching things more than 1.5m away.


1.14b by Henri Beauchamp (equivalent to Marine's v1.14.1):
- Inclusion of Marine Kelley's changes:
	- fixed : a bug that should have been fixed in 1.14, but was not tested. And when it's not tested, it's not working, says murphy's law. My bad.
	- fixed : two Windlight control commands were not implemented in 1.14 (@setenv_sunglowfocus and @setenv_sunglowsize). Does not impact v1.19 viewers.
	- fixed : can't detach an attachment when unable to edit (that's a bug introduced in 1.14).


1.14a by Henri Beauchamp (equivalent to Marine's v1.14):
- Inclusion of Marine Kelley's changes:
	- added : WindLight control, so land owners (for instance) can control the way the visitors see their place, provided they use a RLV and a relay. This is a powerful feature meant for scripters, but not really BDSM-related. (1)
	- fixed : a few small bugs, thanks Laylaa Magic and Crystals Galicia !
	- changed : if the sub is prevented from seeing location and their owner is sending a TP (2) offer but forgot to change the text (hence having "Join me in..."), the text will be hidden.
- fixed: a crash in v1.21 viewers when force-sat on login and the object is not yet rezzed.
- fixed: a bug (typo) in Marine's change (2) above.
- changed: the RestrainedLofeDebug flag now also toggles the log messages in RRInterface.cpp.
(1) This new feature cannot be fully backported to v1.19 viewers, since they don't implement the Windlight renderer. Only the "@setenv_daytime" setting is supported, the others are ignored.


1.13a by Henri Beauchamp (equivalent to Marine's v1.13.1):
- Inclusion of Marine Kelley's changes:
	- added : new command @findfolder:<part1&&part2&&...&&partN>=2222 to find a particular folder (it returns the full path of the first occurrence, in depth first).
	- changed : the viewer can now handle sub-folders under the shared root (see API). Current scripts that are used to force wearing/unwearing shared outfits need to be modified in order to use that feature, though, otherwise they can only use the first level of folders.
	- changed : shared folders can now be "disabled", which means they won't be seen by the viewer when forcing to attach, detach and getting a list. You can disable by adding a dot (".") at the beginning of the name of the folder.
	- changed : now no-mod items see their parent folder being renamed differently : it becomes ".(<attachpointname>)" instead of "<name> (attachpointname)". That way it won't be seen by the viewer anymore when getting the list (see above), and there is no risk of getting a comma (",") in their name anymore. Of course they still attach their no-mod contents like before.


1.12f by Henri Beauchamp (equivalent to Marine's v1.12.5):
- Inclusion of Marine Kelley's fix:
	- fixed : crash (the viewer hangs, it doesn't crash to desktop) under certain circumstances while prevented from seeing the names of people around.


1.12e by Henri Beauchamp (equivalent to Marine's v1.12.4):
- Inclusion of Marine Kelley's fixes and new features:
	- added : command "@getsitid=nnnn" to allow a script to know the UUID of the object we're sitting on. Useful only for scripters and not really a BDSM feature per se. But the viewer could get that information whereas the scripts couldn't, so here it is. Note : although it is a new feature, it is not important enough to justify changing the Minor Version of the viewer (1.13)
	- added : the location and names are now hidden on the Abuse Report window when prevented from seeing location and names respectively, BUT the Abuse Report will be valid nonetheless (ie the Lindens will be able to read it clearly but not the sub)
	- added : now unable to change the Busy automatic response when unable to send IMs. That can be used to set a humiliating message before preventing the sub from sending IMs, for instance *grins* (thanks Eggzist Boccaccio and Neelah Sivocci)
	- added : owner messages are now hiding the region and parcel name when prevented from seeing the location (was done only on object IMs before)
	- added : the URL to the Objects Sharing Tutorial on my blog is added to this notecard in the Shared Folders section above.
	- changed : now unable to drag-select when unable to touch far objects (since you can't Edit-click on far objects either)
	- changed : now unable to shift-drag an object in-world when unable to rez.
	- changed : reinstate Attach To on the pie menu even when something is locked on you (of course trying to attach on an attachment point occupied by a locked item will silently fail). This allows a sub to carry things even when her inventory is locked away.
	- changed : now the names are not clickable anymore while prevented from seeing names.
	- changed : now the Active Speakers window showing who talked recently (both on chat and on voice) is hidden when prevented from seeing names.
	- changed : now the names and locations are "censored" on any message except avatar chat when unable to see names and location respectively. This will defeat the usual radars (known issue : when someone is detected by a radar and not rezzed on the viewer yet, it won't be hidden)
	- changed : now unable to drag things from the texture picker when unable to open inventory.
	- changed : now unable to see scripted beacons when prevented from editing.
	- changed : the viewer won't answer to @getstatus, @getoutfit etc RLV commands on channel 0. It was a way to mimic someone talking.
	- changed : the viewer now *shouts* the automatic responses to @getstatus, @getoutfit etc RLV commands instead of just "saying" them.
	- fixed : no-mod items sometimes wouldn't load automatically in the shared folders (thanks Diablo Payne)
	- fixed (again) : now unable to send chat through emotes. It was already working before, then got broken for some reason (thanks Peyote Short)
	- fixed : crash when giving empty coordinates to @tpto.
- Allow legit emotes (emotes without said text, and possibly truncated in length) in gestures when sendchat=n.


1.12d by Henri Beauchamp (equivalent to Marine's v1.12.3):
- Inclusion of Marine Kelley's fixes and new features:
	- fixed : crash when saving a script in an object that is out of range 
	- fixed : double name in the IM panel when prevented from receiving IMs (but it introduces another bug : no name in the floating chat in this case)
	- fixed : ironing out a way to cheat around @shownames (not an easy cheat this time)
	- changed : owner of the land cannot fly even with admin options on
	- changed : cannot change the inventory of an object we're sitting on when prevented from unsitting (thanks Mo Noel)
	- changed (*): little particle twirls around an owned object do not show anymore (that brings some nice applications such as periodically checking the outfit of a sub)
(*) This feature has been implemented differently than in Marine's patch, so that the particles still appear for llOwnerSay() as long as it does not deal with RestrainedLofe commands (as this is quite useful to spot an object saying something to you).


1.12c by Henri Beauchamp (equivalent to Marine's v1.12.2):
- Inclusion of Marine Kelley's fixes and new features:
	- added : new @fly command.
	- added : the ability to share no-mod items, and also to Wear them when something is locked, provided the folder that contains them is properly named. See above. That one was long planned and is finally working !
	- fixed : removed the ability to use the pie menu on avatars when names are hidden. That means no direct interaction, but you can of course still reach people through Search etc. (thanks JiaDragon Allen)
	- fixed : removed the ability to see the region name while it was hidden through an easy trick. (thanks Nilla Hax)
	- fixed : snapshots would not force the HUDs to show if the checkbox was not checked first (but it would stay on afterwards). Now it is forced on whenever a HUD is locked.
	- changed : separated the shownames restriction and the showloc restriction, to have one @shownames command.
	- changed : added a couple more "dummy names" when the names are hidden.


1.12b by Henri Beauchamp (equivalent to Marine's v1.12.1):
- Inclusion of Marine Kelley's fixes and new features (but one):
	- fixed : a couple of ways to cheat around location hiding.
	- changed : when unable to see the current location, the names are hidden on the screen, on the tooltips, in the chat, in the edit window, and profiles cannot be opened directly (they still can in Search of course).
	- changed : now unable to teleport a friend when unable to see the location.
	- changed : improved touching far objects, it's more consistent now.
Note: the feature "now unable to fly when unable to teleport" was not implemented in the Cool SL Viewer, because it breaks the leashes of the collars (it becomes impossible for the sub to fly with their dom while leashed). I suggested to Marine to implement a new "@fly=n" command to take care of the fly restriction.


1.12a by Henri Beauchamp (equivalent to Marine's v1.12):
- Inclusion of Marine Kelley's fixes and new features:
	- added : Force Teleport feature. The sub can be forced to teleport to any location, without asking for permission and without providing a Cancel button. Known issue : if the destination land has a telehub or a landing point, the sub will teleport there.
	- added : No Show Location feature. The sub can be prevented from seeing in which region and parcel they currently are, teleported from, creating a landmark in, trying to buy etc. World Map is hidden too. This is still experimental, I have gone a long way to hide this information everywhere (even "censoring" system and object messages to hide the location), but there might still be places I have missed. I will not, however, "censor" Owner messages so radars can still overcome this restriction.
	- changed : cannot Return/Delete/Take Copy/Unlink objects we are sitting on when we are prevented from unsitting or sit-tping.
	- changed : cannot Open or Edit anything that is further than 1.5 m away when we are prevented from touching far objects.
	- changed : cannot Open objects when we are unable to Edit. It might change later though.
	- fixed : long item names would be renamed improperly by the viewer, now they are truncated before being renamed.
	- fixed : @detach:china=force would detach whatever you wear on Chin instead of whatever is contained into the China shared folder. Thanks Mo Noel for seeing this.

1.11h by Henri Beauchamp:
- Fixed a bug introduced by Marine in her 1.11.5.1 patch (spurious and excessive log lines issued when HUDs are attached, which lead to a slow down and huge log files).


1.11g by Henri Beauchamp (equivalent to Marine's v1.11.5.1):
- Inclusion of Marine Kelley's fixes:
    - fixed : a weird crash when holding Alt while in Mouselook.
    - changed : Hide HUDs in snapshots is now prevented only when a HUD is locked, not when just any attachment is locked.
    - changed : Zoom out on the HUDs is now restricted only when a HUD is locked, not when just any attachment is locked.


1.11f by Henri Beauchamp (equivalent to Marine's v1.11.5):
- Inclusion of Marine Kelley's fixes:
    - changed : cannot change the group tag if you are unable to send IMs anymore.
    - changed : cannot Take and Return objects if you are unable to Rez things.
    - changed : could not Delete objects when unable to Edit, now it is linked to Rez instead, since Rez is dedicated to build/remove objects while Edit is dedicated to modify existing content.
    - fixed : bodyparts (shape, skin, eyes and hair) could not be shared. Keep in mind that bodyparts cannot be removed, only replaced by other bodyparts.
    - fixed : a way to cheat through the "no edit" restriction (thanks Katie Paine !).
    - fixed : "fartouch" was not preventing from clicking on objects that use the touch_end() LSL event (thanks Ibrew Meads !).


1.11e by Henri Beauchamp:
- Inclusion of Boy Lane's fix to a blocked IMs cosmetic bug.


1.11d by Henri Beauchamp:
- Inclusion of Boy Lane's fix to group IMs failing to be blocked.


1.11c by Henri Beauchamp (equivalent to Marine's v1.11.3):
- Inclusion of Marine Kelley's fix to v1.11 crash bugs at login time.


1.11b by Henri Beauchamp:
- Fixed the systematic camera zooming/paning bug when entering build mode.


1.11a by Henri Beauchamp (equivalent to Marine's v1.11.2):
- Inclusion of Marine Kelley's changes (from RestrainedLofe v1.11 to 1.11.2):
    - added : a way to "share" inventory items. The user can now be forced to attach/detach objects and clothes by putting them in a folder contained in the root shared folder (for now named "#RLV").
    - added : when wearing a shared object, the name of the attachment point is added to its own for future use. It won't do that on non-shared items nor on no-modify items.
    - added : fake Wear option if the object name contains the name of the target attachment point.
    - added : force attach/wear and force detach/unwear by folder name (in the shared root only).
    - added : list shared inventory with "@getinv=nnnn" command.
    - added : always show HUD objects in snapshots when at least one object is locked.
    - added : restrict show mini map and show world map (thanks Maike Short for the code).
    - added : "@fartouch=n" command  to restrict touch on objects farther than 1.5 meters away.
    - fixed : a couple of loopholes including one discovered by Maike Short.
    - known bugs : "bottom left" and "bottom right" HUD locations are going to "bottom", "top left" and "top right" to "top".


1.10i by Henri Beauchamp (this version has been used after a crash bug was found in v1.11a):
- Fixed the systematic camera zooming/paning bug when entering build mode.


1.10h by Henri Beauchamp:
- Inclusion of Marine Kelley's changes (from RestrainedLofe v1.10.5.2):
    - changed : emotes crunched down to 20 characters instead of 30.
    - fixed : a loophole discovered by Moss Hastings. Thanks Moss !


1.10g by Henri Beauchamp:
- Inclusion of most (all but the fly interdiction) of Marine Kelley's changes (from RestrainedLofe v1.10.3 and v1.10.4):
    - changed : Now when an object forces the user to sit on a furniture, it ignores its own @sittp=n restriction for the time of the command. This means that a Serious Shackles Collar which leash is active will still be able to force sit, regardless of the "@sittp=n" restriction issued by the *Leash plugin. It won't override the arms shackles leash, though (to prevent cheating).
    - changed : Admin Commands is now rendered useless, as it was an easy way to cheat around certain restrictions. Thanks Anaxagoras McMillan for the bug report !
    - changed : Removed a potential cheat around the outfit restrictions.
    - added : Can't delete objects you're sitting on (*).
    - added : Can't edit objects someone is sitting on when sit-tp is prevented.
- Changed (*) so that this restriction only applies whenever you are prevented to unsit.


1.10f by Henri Beauchamp:
- When in RestrainedLofe mode, make sure it is impossible to use slurls to change the start location (always log in at the last location).


1.10e by Henri Beauchamp:
- Inclusion of Marine Kelley's changes (from RestrainedLofe v1.10.2):
    - fixed : a bug with @edit and @viewnote. Thanks to those who pointed them out, they were really not easy to spot.
    - added : @getstatus to let the script know what restrictions the avatar is submitted to.


1.10d by Henri Beauchamp:
- Fixes minor bugs in the client debug menu.


1.10c by Henri Beauchamp:
- Lift all restrictions in debug menu options as long as no item is locked.


1.10b by Henri Beauchamp:
- Inclusion of Marine Kelley's changes (from RestrainedLofe v1.10.1):
    - fixed : @remoutfit=n didn't prevent from replacing clothes, unless @addoutfit=n was set
    - fixed : could stand up with the pie menu and appearance even when prevented
    - added : eyes, hair and shape for addoutfit and remoutfit
- Make sure the HUD zooming is not restricted as long as no RL object is locked.


1.10a by Henri Beauchamp:
- Inclusion of Marine Kelley's changes (from RestrainedLofe v1.10):
    - changed: when the avatar is prevented from sending chat, using gestures no more produce "...".
    - fixed : a couple of bugs about rez and edit that were still possible through some non-obvious options even when prevented.
    - added : force sit on an object in-world, given its UUID. Thank you Shinji Lungu !
    - added : prevent standing up from the object you're sitting on. Also removes the "Stand Up" button. A VERY popular request.
    - added : force unsit. Strangely seems to randomly fail. (*)
    - added : prevent adding/removing clothes (all or selectively).
    - added : force removing clothes (*)
    - added : force detaching items (*)
    - added : prevent reading notecards (doesn't close the already open ones and doesn't prevent from receiving them, for safety reasons).
    - added : prevent opening inventory (closes all the inventory windows when activated).
    - added : check clothes (gives the list of occupied layers, not the names of the clothes for privacy reasons).
    - added : check attachments (gives the list of occupied attachment points, not the names of the items for privacy reasons).
    - added : prevent sending messages on non-public chat channels, with exceptions. Doesn't prevent the "@version=nnnn" automatic reply.
    - added : prevent customizing the TP invites when prevented from sending IMs.
    - added : prevent reading the customized TP invites when prevented from reading IMs.
    - added : ability for the viewer to execute several commands at the same time, separated by commas, only the first one beginning with '@'.
    - added : "garbage collector" : when you unrez an item, all the restrictions attached to it are automatically lifted after a moment.
    - added : commands are delayed until the avatar is fully operational when logging on, to avoid some "race conditions", typically when force-sitting on relog.
    (*) Silently discarded if the user is prevented from doing so by the corresponding restriction. This is on purpose.
        Ex : Force detach won't work if the object is undetachable. Force undress won't work if the user is prevented from undressing.
- Do not forbid debug features in menus when RestrainedLofe is not enabled.


1.05c by Henri Beauchamp:
- When in RestrainedLofe mode, make sure it is impossible (including by using the preferences menu or SLURLs), to change the start location (always login in the last location).


1.05b by Henri Beauchamp:
- Integrate the changes from Marine's v1.04:
    - fixed Copy & Wear bug. It was working before, then stopped working. Works again. Thanks LXIX Tomorrow.
    - now @recvim also prevents the user from receiving group chat. A VERY popular request.
- Prevent the use of the debug tool "Dump All Attachments" when the viewer is in RestrainedLofe mode.


1.05a by Henri Beauchamp:
- Gag bug fixed (all the text was suppressed in v1.03, preventing the toys to trigger their retorsion measures).
- The start location pull down menu is now suppressed altogether from the login screen when in RestrainedLofe mode.


1.04a:
- Fixes the bug in which @clear was clearing all the RestrainedLofe settings for all the attachments (instead of just for the calling attachment).


1.03:
- Manual @version checking in IM is now totally silent to the user so they never know when they're checked :)
- Slashed commands like "/ao off", "/hug X" on channel 0 allowed even when prevented from chatting (max 7 characters including '/')
- @edit and @rez viewer commands : to prevent Editing and Rezzing stuff respectively (useful for cages and very hard restraints)
- Can hear owner's attachments messages even when prevented from hearing chat. Cannot hear in-world objects nor other's attachments.
- Crash bug fixed in @clear commands.


1.02 by Henri Beauchamp:
- OOC bug fixed.
- Better algorithm for deciding whether an emote contains "spoken" text or not.
- Gagged text is now emitted as "..." to allow scripted gags to trigger their own retorsion measures. ;-P
- Emotes are no more truncated after the first period when @emote=add is in force.
- Commands are no more echoed to the main chat, unless RestrainedLofeDebug is set to TRUE.
- More bug fix and code cleanup.


1.01:
- Changed the way emotes are handled when prevented from chatting : now (( )) are authorized, signs like ()"*-_^= will discard the message, otherwise emotes are truncated to 30 chars (unless "@emote=add" command is issued, see below), and if a period is present whatever is after it is discarded
- Added "@emote=<rem/add>" to ignore the truncation when sending or hearing emotes in public chat
- Integrated Henri Beauchamp's fixes and additions (such as being able to switch all the features off after relog if needed). Thank you Henri !
- Fixed modification of inventory of locked attachments, thank you for pointing that out Devious Lei !
- Fixed text going through with chat bubbles (although emotes are totally erased, will be fixed later). Thank you for pointing that out Rylla Jewell !


1.01a by Henri Beauchamp:
- Compiled within v1.18.5.1 for Linux.
- Most restrictions lifted when no locked item is worn.
- Made the viewer switchable (after a restart) between a normal viewer and a RestrainedLofe viewer.


1.0:
- Compiled under 1.18.4.3 for Windows
- Added No-teleport (Landmark, Location, Friend + Exceptions)
- Added No-Sit-TP over 1.5 meters away
- Added the patch and custom package source code
- Added the viewer API
- Removed ability to log in where you want => forced to My Last Location
- Removed ability to see in Wireframe as it could be used to cheat through blindfolds
- Fixed emotes going through No-receive-chat, now truncated


1.0b:
- Added No-send-chat, No-receive-chat, No-send-IM, No-receive-IM features. Exceptions can be specified to all these behaviours except No-send-chat, for instance to allow IM reception only from your keyholder.
- Added "@version=<channel>" so a script can expect the viewer to say its version on the specified channel. Useful for automated version checking. Thank you Amethyst Rosencrans for suggesting that solution !


1.0a:
- First release.
