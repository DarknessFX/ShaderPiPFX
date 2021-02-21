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

Project info/blog at https://dfx.lv/ShaderPiPFX (soon) .<br/><br/>

<img src="https://raw.githubusercontent.com/DarknessFX/ShaderPiPFX/main/.git_img/Sample01.png" width="640px" />

## Features

- Easy to use, drag-drop your HLSL file to ShaderPiPFX and it's ready to autoplay.
- Error message, display syntax warnings and compile errors.
- Compile HLSL to CSO with PDB and extras (needs change in the sourcecode FXApp::CompileShader).
- DirectX 12 with HLSL Shader Model 6.4.

# Requirements

# Usage

- Left Click and drag window to move.
- Right Click to toogle window always on top.
- Shift+ESC to exit.

<br />

Open ShaderPiPFX.exe, if it's the first time running the program will create a new "Shader.HLSL" file, you can edit this file in any editor/IDE when you save your changes ShaderPiPFX will compile and play your shader. Use this "Shader.HLSL" as a template for your next shaders.<br />
You can drag-drop any HLSL file from Windows Explorer to ShaderPiPFX Window to change the selected HLSL file and start to play and monitor changes on the new file.<br />
You can drag-drop any HLSL file to ShaderPiPFX.exe (or shortcut) to start working directly with your selected file.<br />

# Video Sample
<a href="https://www.youtube.com/watch?v=AcovXaRJBqg" target="_blank"><img src="https://raw.githubusercontent.com/DarknessFX/ShaderPiPFX/main/.git_img/Sample02.png" width="640px" /></a><br/>

# Versioning
v0.1 - Alpha released.

## TODO

- (soon).

## Credits
DearImGui - https://github.com/ocornut/imgui
DirectX Compiler - https://github.com/microsoft/DirectXShaderCompiler
WindowStyle (adapted from) - https://github.com/melak47/BorderlessWindow + https://stackoverflow.com/a/39735058

## Sugestions
IDE Extension
HLSL Tools for Visual Studio - https://marketplace.visualstudio.com/items?itemName=TimGJones.HLSLToolsforVisualStudio
HLSL Tools for VS Code - https://marketplace.visualstudio.com/items?itemName=TimGJones.hlsltools
HLSL preview - https://marketplace.visualstudio.com/items?itemName=A2K.hlsl-preview

Shader Tools
Bonzomatic - https://github.com/Gargaj/Bonzomatic
ShaderToy - https://www.shadertoy.com/
SHADERed - https://shadered.org/
ShaderPlayground - http://shader-playground.timjones.io/

## License

MIT License<br/><br/>
DarknessFX @ <a href="https://dfx.lv" target="_blank">https://dfx.lv</a> | Twitter: <a href="https://twitter.com/DrkFX" target="_blank">@DrkFX</a> <br/>https://github.com/DarknessFX/ShaderPiPFX
