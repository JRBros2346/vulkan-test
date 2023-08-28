import os, tomlkit
from pathlib import Path

os.chdir(Path(__file__).parent)

Path("bin").mkdir(exist_ok=True)
Path(".py").mkdir(exist_ok=True)

conf = tomlkit.parse(open(".py/config.toml").read())

def compile_shaders(input: Path, output: Path):
    print(f"Compiling {input}...")
    return os.system(f"glslc {input} -o {output}")

def main():
    print(f"Compiling Shaders...")
    for f in Path("shaders").rglob("shader.*"):
        try:
            _ = conf[str(f)]
        except tomlkit.exceptions.NonExistentKey:
            error = compile_shaders(f, Path(f"shaders/{f.suffix[1:]}.spv"))
            if error:
                exit(error)
        else:
            if os.path.getmtime(f) == conf[str(f)]:
                error = compile_shaders(f, Path(f"shaders/{f.suffix[1:]}.spv"))
                if error:
                    exit(error)
    timestamps = tomlkit.document()
    for f in Path("shaders").rglob("shader.*"):
        timestamps.add(str(f), os.path.getmtime(f))
    with open(".py/config.toml", "w") as f:
        tomlkit.dump(timestamps, f)
    print("Compiling All...")
    error = os.system(f"clang++ src/main.cpp -g -Og --std=c++20 -o bin/main.exe -I{os.getenv('VULKAN_SDK')}/Include -L{os.getenv('VULKAN_SDK')}/Lib -luser32 -lvulkan-1")
    if error:
        exit(error)

if __name__ == '__main__':
    main()