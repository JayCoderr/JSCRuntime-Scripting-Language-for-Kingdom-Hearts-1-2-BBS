# Kingdom Hearts 1 & 2 & BBS Runtime Hot Reload Scripting

**JayCoder's Runtime** is a runtime scripting framework designed for **Kingdom Hearts 1, Kingdom Hearts 2, and potentially Kingdom Hearts Birth by Sleep (BBS)**.

The project provides a foundation for hot-reloadable scripting, dynamic runtime libraries, custom menus and interfaces, and other functionality that can be added and updated without requiring users to constantly replace the entire runtime.

The project is currently **unreleased and under active development**.

## Runtime Scripting

The primary purpose of JayCoder's Runtime is to provide a flexible scripting environment that allows scripts to be loaded and reloaded while the game is running.

This makes it possible to develop and test scripts without repeatedly restarting the game whenever a script is changed.

The system is intended to support existing script formats and workflows, including formats such as:

```text
.asc
.khc
.anything

as long as it contains JayCoder's Runtime code
```

Users can continue working with their existing scripts while the runtime itself receives new features and improvements.

## Hot Reloading

Scripts can be reloaded during runtime, allowing changes to be tested without restarting the entire game.

This is intended to make development significantly faster when working on:

* Gameplay scripts
* Menus
* HUD elements
* Custom functions
* Runtime systems
* Game-specific functionality
* Experimental features

The runtime is designed to handle the underlying implementation while keeping the scripting experience as straightforward as possible for creators.

## Dynamic Runtime Libraries

JayCoder's Runtime is also designed around dynamically loading and unloading runtime libraries.

This allows additional functionality to be introduced independently of the main proxy/bootstrap component.

New runtime functionality can therefore be developed and distributed without requiring users to replace the entire installation.

This architecture also allows the runtime to continue evolving over time while the initial loader remains relatively stable.

## Automatic Runtime Updates

A major part of the system is its automatic update mechanism.

Users should not need to manually download every new runtime release.

When the game launches, JayCoder's Runtime checks for the latest available runtime version.

If the installed version is outdated, the newer runtime can be downloaded and installed automatically.

The intended process is:

```text
Install JayCoder's Runtime
          |
          v
     Launch Game
          |
          v
   Check for Updates
          |
     +----+----+
     |         |
     v         v
   Current   Update
     |         |
     |         v
     |      Download
     |         |
     |         v
     |       Install
     |         |
     +----+----+
          |
          v
   Load Latest Runtime
          |
          v
      Load Scripts
```

The goal is for users to **install the system once and then continue using it without manually managing runtime updates**.

As new versions are released, the runtime can automatically keep itself up to date when the game is launched.

## Source Code

Please **do not copy, redistribute, reuse, or present the source code of the JayCoder's Runtime proxy or `JSCRuntime` as your own without permission**.

This includes:

* The DXGI proxy/bootstrap source code
* `JSCRuntime` source code
* Runtime update and library-loading systems
* Other original source code and implementations contained within the project

The proxy is a core part of the project's infrastructure and was developed specifically to handle runtime loading, library management, and automatic runtime updates. `JSCRuntime` contains the scripting and runtime functionality built on top of that infrastructure.

You are welcome to **study the project and learn from its concepts and implementation**, but please do not directly copy the source code, redistribute it, or use substantial portions of it as the foundation of another project without permission.

If you would like to use the proxy, `JSCRuntime`, or portions of their source code in another project, **please ask first**.

I put significant time into developing these systems, and I would appreciate having the original work respected.

For example:

```text
raw/
└── gamescripts/
    ├── menu.asc
    ├── example.khc
    ├── weapons.asc
    └── custom_script.khc
```

The intention is that runtime updates should not require users to repeatedly download their scripts again.

As long as the scripting API remains compatible, scripts can continue working across runtime updates while new functionality is added underneath them.

## Potential Functionality

The runtime is being developed with support for a broad range of functionality, including:

* Runtime script execution
* Script hot reloading
* Dynamic library loading
* Dynamic library unloading
* Automatic runtime updates
* Custom menus
* HUD functionality
* Dynamic images and resources
* Custom scripting functions
* Runtime development tools
* Game-specific functionality
* Extensible scripting systems

Additional functionality will be added as development continues.

## Supported Games

The initial focus is:

* **Kingdom Hearts 1** - main focus
* **Kingdom Hearts 2**
* **Kingdom Hearts Birth by Sleep** — potential support

Support for additional functionality and games may be added over time.

## Development Status

JayCoder's Runtime is currently **unreleased and actively under development**.

The runtime architecture, scripting API, supported file formats, supported games, and available functionality may change as development progresses.

The current priority is establishing a stable runtime foundation that can be continuously expanded without requiring users to repeatedly reinstall or manually update the system.

## Project Direction

The long-term goal is to provide creators with a flexible runtime scripting environment where they can create their own scripts, menus, functions, interfaces, and gameplay systems while **JayCoder's Runtime handles the underlying execution and runtime functionality**.

The runtime is designed to evolve independently over time, allowing new features and improvements to be delivered automatically while keeping the creator-facing scripting workflow as stable and accessible as possible.

the hashes I got from OpenKH but it's not really needed right now as I have no got assets to decompile the right way yet 100% aswell as recompile.
