# vlc-tip-plugin
TIP (translate it, please) is a plugin for VLC media player that helps you to study languages by watching videos. 

## Features
---
The plugin allows you to repeat the past few seconds of video but with other audio track by pressing just a key. Audio track will be switched back to the previous one at the moment in video where you pressed the key.
You can select the audio tracks to switch between and period of time to translate in Preferences (in main menu go to *Tools* > *Preferences*, switch the *Show settings* radio button to *All* and in the left hand menu select *Interface* > *Control interfaces* > *TIP*).

The plugin adds new keyboard shortcuts to VLC:
* __`/`__ to translate
* __`Shift` + `/`__ to repeat the last translated period

## Installation
---
TIP works with [VLC](https://www.videolan.org/) version __3.0.0__ or higher.

To install the TIP plugin just copy the __libtip_plugin.so__ file (libtip_plugin.dll for Windows) to the __\<path-to-vlc\>/plugins__ directory.

| OS | 64-bit | 32-bit |
| ------ | ------ | ------ |
| Linux | [libtip_plugin.so](build/linux/64/libtip_plugin.so) | [libtip_plugin.so](build/linux/32/libtip_plugin.so) |
| Windows | [libtip_plugin.dll](build/win/64/libtip_plugin.dll) | [libtip_plugin.dll](build/win/32/libtip_plugin.dll)|