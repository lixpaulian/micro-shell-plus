# micro-shell-plus
An extensible shell written in C++ for embedded systems. It was specifically developed for µOS++ but it can be ported to other POSIX compliant RTOSes.

## Version
* 0.4.2 under development (23 Dec 2022)

## License
* MIT

## Package
The class is provided as an xPack (for more details on xPacks see https://xpack.github.io). It can be installed in an Eclipse based project using either `xpm` or the attached script (however, the include and source paths must be manually added to the project in Eclipse). Of course, it can be installed without using the xPacks tools, either from the repository or manually, but then updating it later might be more difficult.

Note thathe xPacks project evolved with the time. Initially it was based on shell scripts, but the current version is based on a set of utilities, in partcular `xpm` plus a json description file. This project supports both versions, therefore you will still find the `xpacks-helper.sh` script in the `scripts` subdirectory, as well as the `package.json` file for `xpm`. However, the script based version is deprecated and will not be supported in the future.

## Dependencies
This software depends on the following packages, available as xPacks:
* µOS++ (https://github.com/micro-os-plus/micro-os-plus-iii)

