## Overview

This repo demonstrates how to incorporate some best practices into STM32CubeIDE-based projects. It preserves ST's .ioc workflow with autogeneration and easy pin reconfiguration in a git-friendly way, and also showcases:
- C++ used appropriately in a constrained embedded environment (no dynamic allocations - see [no_new.cpp](common/src/no_new.cpp)).
- Convenience wrappers for static allocation of FreeRTOS components (see [static_rtos.h](common/inc/static_rtos.h))
- Code deduplication:
  - Linking to versioned vendor firmware
  - Common code shared across projects
- Unit testing with [CppUTest](https://cpputest.github.io/) (not used extensively yet, [example](common/tests/src/test_basic.cpp))
- Code coverage with lcov
- Autoformatting with [clang-format](https://clang.llvm.org/docs/ClangFormat.html)
- ITM debug logging
- FreeRTOS task profiling
- High-performance UART and USB communication interface abstractions (see the [loopback](loopback) project)

CI checks on each pull request ensure that all projects compile, pass unit tests, and are formatted correctly. Here are example PRs demonstrating:
- [Broken build](https://github.com/milesfrain/stm32template/pull/2)
- [Failing unit tests](https://github.com/milesfrain/stm32template/pull/3)
- [Incorrect formatting](https://github.com/milesfrain/stm32template/pull/4)
- [Passing all checks](https://github.com/milesfrain/stm32template/pull/5)

## Linux setup instructions

These instructions were verified on a fresh install of Ubuntu 20.04.

### Get STM32CubeIDE

Download the "Generic Linux Installer" (NOT the debian installer, which requires root to update) from:
https://www.st.com/en/development-tools/stm32cubeide.html

Extract. Then install by running:
```
sh st-stm32cubeide_1.4.0_7511_20200720_0928_amd64.sh
```
Note that `ctrl-C` will let you jump to the bottom of the licenses.

Download ST-Link Server from:
https://www.st.com/en/development-tools/st-link-server.html

Extract. Then install with:
```
sudo dpkg -i st-stlink-server-1.3.0-4-linux-amd64.deb
```

#### To run the IDE:
Open launcher with super-key (windows key), then start typing "STM32CubeIDE". There might be two results. I go for the one with the blue background.

Select a "dummy" workspace for this initial setup (e.g. `~/projects/dummy`).

Then update from 1.4.0 to 1.4.2:
```
Help > Check for updates
```

Don't worry about the unsigned content warnings.


### Get the STM firmware

Create a new project in your dummy workspace:
```
File > New -> STM32 Project
```

Select our chip:
```
STM32F413ZH
```

Click through the prompts.

Build and flash this dummy project to ensure that tooling is setup correctly so far.
You can just click "Ok" to accept the default debug config.

If you encounter a arm / **libcurses error**, try:
```
sudo apt install libncurses5
```

### Setting-up the real workspace

#### Firmware linking
We need to setup a symlink to the STM FW repo to ensure we all share the same absolute path to shared project assets.
```
sudo ln -s ~/STM32Cube/Repository /usr/share/stm_repo
```
Switch your workspace to the checked-out repo.
```
Files > Switch Workspace
```
Use the symlinked firmware location in this workspace.
```
Window > Preferences > STM32Cube > Firmware Updater > Firmware Repository
```
Set to `/usr/share/stm_repo`.

#### Project import
Add each folder to this workspace.
```
File > Open Projects from Files System
```
You should be able to just point to the repo directory and then un-check all non-projects (likely just the first directory). As of this writing, there should only be one project selected:
```
loopback
```
Now try building and flashing each project. Sometimes you'll need to expand the project folder in order for the build hammer to work.

### Debugging info

When you first debug, you'll see a popup. This default configuration works fine, but if you want to see ITM traces you'll need to enable SWV:
`Debugger > Serial Wire Viewer`, check `Enable`, set `Core Clock` to `96.0`. This needs to match `SYSCLK` in the .ioc clock configuration.

If you skip this step for the first pop-up, you can find the setting later under `Run > Debug Configurations`.

To view ITM traces while debugging, open `Window > Show View > SWV > SWV ITM Data Console`. Click on the wrench icon in this new terminal and check `ITM Stimulus Ports` `2` and `0`. Logs are written to port `0`, but we're using port `2` as a workaround for this issue:
https://community.st.com/s/question/0D53W00000Hx6dxSAB/bug-itm-active-port-ter-defaults-to-port-0-enabled-when-tracing-is-disabled

Then click the red circle to "Start Trace". Note that this can only be toggled when execution is paused.

For FreeRTOS task monitoring, install the extension described here:
https://blog.the78mole.de/freertos-debugging-on-stm32-cpu-usage/


## New STM32CubeIDE project setup

Here are some notes on the steps to create a new C++ project. You can either start a fresh project, or import an existing .ioc. This guide shows the "from .ioc" method.

#### Import from .ioc
```
File > New > STM32 Project ... from .ioc
```
Change `Targeted Language` to `C++`, then click `Next`.

Change `Code Generator Options` to `... as reference ...`, then click `Finish`.

See screenshots for relevant settings.

![](https://user-images.githubusercontent.com/3578681/97056394-c2c7e280-153d-11eb-9c93-f8246ff421a2.png)

![](https://user-images.githubusercontent.com/3578681/97056480-f7d43500-153d-11eb-8f21-c5102bfef6d5.png)

Some .ioc settings to double check are:
* `Project Manager > Project` check `Do not generate the main()`
* `Project Manager > Code Generator` select `Add ... as reference ...`
* `Project Manager > Code Generator > Generated files` check `Generate ... as a pair ...`

#### Convert to C++ (... didn't we already do this in the .ioc?)
In the `Project Explorer`, right click on the active project and select `Convert to C++`.

#### Link to common code
In the `Project Explorer`, right click on the active project, `New > Folder > Advaced > Link to ...`, paste:
```
WORKSPACE_LOC/common
```

#### Setup include and source paths
`Project > Properties > C/C++ General`
From `Configuration` dropdown select `All`
In `Includes` tab, for languages `GNU C` and `GNU C++`, add these paths:
```
custom/inc
${WorkspaceDirPath}/common/inc
```

For this next step the `Debug`, and then `Release` configurations must be selected individually. In the `Source Location` tab `Add Folder`, select `common` and `custom`.

### STM32CubeIDE workarounds

The STM32CubeIDE is not fully baked, and there are lots of annoying workarounds required to get things running smoothly in a team setting. Here are the workarounds you'll need to keep in mind during development:

#### IDE overrides absolute paths when linking to firmware

The IDE likes to replace the absolute paths in `.project` with relative paths containing stuff like `PARENT-5-Project_LOC`, etc. when regenerating code from .ioc. This can break the build for other developers and CI. More details about this issue can be found in [this post](https://community.st.com/s/question/0D53W00000Ioh2LSAR/stm32cubeide-uses-relative-paths-when-linking-to-firmware-at-an-absolute-path).

Use the `fix-paths.sh` script to restore these absolute paths.

### A note on project directory structure
```bash
common          # Code that's 100% user-generated and shared across all projects
├── inc
├── src
└── tests       # Unit tests for common code
some_project
├── Core        # autogenerated, but editable
│   ├── Inc
│   ├── Src
│   └── Startup
├── custom      # 100% user-generated code that's project-specific
│   ├── inc
│   ├── src
│   └── tests   # Unit tests for custom project code
│               # The following directories and files are autogenerated
├── Drivers     # Just empty directories for links to common ST FW
│   └── STM32F4xx_HAL_Driver
├── Middlewares # Just empty directories for links to common ST FW
│   ├── FreeRTOS
│   └── USB_Device_Library
└── USB_DEVICE  # autogenerated, but editable
    ├── App
    └── Target
some_other_project
├── ...
...
```

### A note on Continuous Integration

For each PR, the following is checked:
- All STM32CubeIDE projects must compile
- Unit tests must pass
- Code must be formatted correctly
- Code coverage report generated

It uses this GitHub action: https://github.com/milesfrain/stm32-action

Which uses this docker image: https://github.com/milesfrain/stm32-docker

### A note on Unit Tests
This repo uses the [CppUTest](https://cpputest.github.io/) unit testing framework. These tests are automatically run as part of CI for each pull request. To run locally, install the dependency, and run the test script. Projects / directories to test are tracked in the `test-dirs.txt` configuration file.
```
sudo apt install cpputest
./unit.sh
```

### A note on Autoformatting

Coding style for this repo is managed with [`clang-format`](https://clang.llvm.org/docs/ClangFormatStyleOptions.html).

This tool is not mandatory for development, but your life will be easier if you install it:
```
sudo apt install clang-format
```

Then you can check if you're messing anything up by running:
```
format/check.sh
```

If you'd like to automatically fix these formatting mismatches, run:
```
format/do.sh
```

Note that we're only autoformatting code that is 100% user-created, so we're not constantly fighting with ST's autogeneration tools. You can add new directories to track to `format/targets.txt`.

If you're not happy with the formatting rules, you can adjust the top-level `.clang-format` config file, and then we can adopt this new style.
