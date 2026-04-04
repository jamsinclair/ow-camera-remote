# OW Camera Remote

A Pebble Watch app that lets you preview your phone's camera and remotely take pictures with a timer.

Set a timer, strike a pose, and have your pic taken.

[Repebble Appstore Listing](https://apps.rePebble.com/57542b7f70c28c587d000004)

Requires the Android companion app, [OW Camera 2 for Pebble](https://play.google.com/store/apps/details?id=com.github.jamsinclair.owcamera2).

Supported Pebble Apps:
- [Pebble / Core app](https://github.com/coredevices/mobileapp) (from `1.0.7.7`)
- [microPebble](https://github.com/matejdro/microPebble) (from `1.0.0-alpha35`)

## Controls

- **Up**: Toggle timer duration (max 15 seconds)
- **Middle**: Start the timer capture
- **Down**: Open the settings window

## Settings

- **Switch Camera**: Toggle between front and rear phone cameras
- **Preview**: Toggle whether to show a live preview from your camera
- **Timer**: Toggle timer duration (max 15 seconds)
- **Timer Vibration**: Toggle whether to vibrate on each countdown second
- **Dither**: Choose the dither mode for the preview image (defaults to Floyd-Steinberg)
- **Color Mode**: Choose between color and black-and-white preview (color devices only). Black-and-white previews have a faster frame rate.

## Building

Built with the Pebble SDK. See [CONTRIBUTING.md](CONTRIBUTING.md) for build and development details.

## History

**2016:** Hastily hacked together over two weekends... 6 months apart. I travel solo and wanted a way to take pictures of myself in different locations away from my phone.

**2026:** After receiving the new Pebble Duo 2, I originally fixed the, much simpler, original app. A friend suggested adding a live preview and I couldn't say no to the challenge. So after a few months of development, here we are.
