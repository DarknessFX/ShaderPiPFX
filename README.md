     .----------------.  .----------------.  .----------------. 
    | .--------------. || .--------------. || .--------------. |
    | |  ________    | || |  _________   | || |  ____  ____  | |
    | | |_   ___ `.  | || | |_   ___  |  | || | |_  _||_  _| | |
    | |   | |   `. \ | || |   | |_  \_|  | || |   \ \  / /   | |
    | |   | |    | | | || |   |  _|      | || |    > `' <    | |
    | |  _| |___.' / | || |  _| |_       | || |  _/ /'`\ \_  | |
    | | |________.'  | || | |_____|      | || | |____||____| | |
    | |              | || |              | || |              | |
    | '--------------' || '--------------' || '--------------' |
     '----------------'  '----------------'  '----------------' 

           DarknessFX @ https://dfx.lv | Twitter: @DrkFX

# ShaderPiPFX

HLSL live player. Compile and play your HLSL shader automatically.<br/>

Project info/blog at https://dfx.lv/ShaderPiPFX .<br/><br/>

<img src="https://raw.githubusercontent.com/DarknessFX/ShaderPiPFX/main/.git_img/Sample01.png" width="640px" />

## Features

- Easy to use, drag-drop your HLSL file to ShaderPiPFX and it's ready to autoplay.
- Error message, display syntax warnings and compile errors.
- Compile HLSL to CSO with PDB and extras (needs change in the sourcecode FXApp::CompileShader).
- DirectX 12 with HLSL Shader Model 6.4.

# Requirements

To Build:
- Visual Studio 2019 with "Game development with C++" workload installed.

To Use:
- Windows 10 (updated).

# Usage

- SHIFT+ESC to exit.
- Left Click and drag window to move.
- Left DoubleClick to toogle fullscreen.
- Right Click to toogle window always on top.
- SHIFT+Mouse Click (and DoubleClick) send mouse buttons state to shader.

<br />

Open ShaderPiPFX.exe, if it's the first time running the program will create a new "Shader.HLSL" file, you can edit this file in any editor/IDE when you save your changes ShaderPiPFX will compile and play your shader. Use this "Shader.HLSL" as a template for your next shaders.<br />
You can drag-drop any HLSL file from Windows Explorer to ShaderPiPFX Window to change the selected HLSL file and start to play and monitor changes on the new file.<br />
You can drag-drop any HLSL file to ShaderPiPFX.exe (or shortcut) to start working directly with your selected file.<br />

# Video Sample
<a href="https://www.youtube.com/watch?v=AcovXaRJBqg" target="_blank"><img src="https://raw.githubusercontent.com/DarknessFX/ShaderPiPFX/main/.git_img/Sample02.png" width="640px" /><br/>Youtube Link</a><br/>

# Versioning
v0.4.7 - Temporary fixed unhandled exception when saving file to fast while last changes are still compiling.</br>
v0.4.6 - Fixed bug when files with spaces are loaded by droping hlsl file on .EXE .<br/>
v0.4.5 - Fixed loading shader using .EXE command argument. Fixed changing loaded shader by droping hlsl file on .EXE .<br/>
v0.4 - Included Mouse Coords and Button Clicks to Shader. SHIFT disable window mouse and send only to Shader.<br/>
v0.3 - Fixed crash when loaded HLSL is deleted.<br/>
v0.2 - Fixed AspectRatio on start. Fixed resolution.w (AspectRatio) HLSL initial value. Included UV Style Helpers on Shaders.hlsl template.<br/>
v0.1 - Alpha released.<br/>

## TODO

- (soon).

## Credits
DearImGui - https://github.com/ocornut/imgui<br/>
DirectX Compiler - https://github.com/microsoft/DirectXShaderCompiler<br/>
WindowStyle (adapted from) - https://github.com/melak47/BorderlessWindow + https://stackoverflow.com/a/39735058<br/>

## Sugestions
IDE Extension<br/>
HLSL Tools for Visual Studio - https://marketplace.visualstudio.com/items?itemName=TimGJones.HLSLToolsforVisualStudio<br/>
HLSL Tools for VS Code - https://marketplace.visualstudio.com/items?itemName=TimGJones.hlsltools<br/>
HLSL preview - https://marketplace.visualstudio.com/items?itemName=A2K.hlsl-preview<br/>

Shader Tools<br/>
Bonzomatic - https://github.com/Gargaj/Bonzomatic<br/>
ShaderToy - https://www.shadertoy.com/<br/>
SHADERed - https://shadered.org/<br/>
ShaderPlayground - http://shader-playground.timjones.io/<br/>

## License

MIT License (c) 2021 DarknessFX<br/><br/>
DarknessFX @ <a href="https://dfx.lv" target="_blank">https://dfx.lv</a> | Twitter: <a href="https://twitter.com/DrkFX" target="_blank">@DrkFX</a> <br/>https://github.com/DarknessFX/ShaderPiPFX
