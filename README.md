# ADB Helper 
ADB Helper is a simple desktop tool that automates some of the adb functionalities. On startup, the app automatically detects if there is any adb device connected and if there is only one, it connects to it (WIP - add device selector).

The GUI is very minimal, it uses QT basic libraries to mimic a Windows Settings-like style. 

## Logcat
The logcat tab show the result of the last 20000 lines of `adb logcat`. It has an extra button to stop the stream of input, and there is also an extra text input field where the user can add parameters to the command ("-c", "-d", "| grep ...").

## Screencap
The screencap tab has only one button, and once pressed it takes a screenshot of what's showing on the adb device. At every press, the app shows only the latests picture, but in the working directory, a folder called "screenshots" is created containing all the pictures. In the folder there are multiple folders with names by date, and inside each foldere there are the pictures taken, named after their timestamp. (WIP - locate image folder and save images there).

## Recording
The recording tab has almost identical functionality of screencap, with the only difference that a screenrecord is taken.

## Sideload
The sideload tab is used to install APKs to the adb device. There is a text input at the top where the user has to enter the path of the apk, and after pressing the button, the installation process will start.

## Keyboard (WIP)
The keyboard tab is used to send keyevent to the adb device. There is a text input at the bottom of the page and if it's focused, it will send the keyevents. At the moment only letters, numbers, space and enter are supported.

## Info
The info tab simply ises `adb getprop` to show the device properties and print them on screen.

## Shell
The shell tab is used to mimic a shell inside the device. There is a text input at the bottom of the app where the user can send all the commands as `adb shell` (WIP - need to run adb shell once at the beginning). The tab is used to run all kind of commands on the abd device, commands that haven't been implemented yet on the app. (WIP - add root functionalities)


# Requirements (WIP)
- Have `adb` installed and set in the Environment Variables
- Have python installed 
