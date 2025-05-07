# Sentinel Monitor System Performance and Security

A simple system monitor built with C++ and OpenGL/freeglut for Windows.

---

## Prerequisites

- [MSYS2](https://www.msys2.org/) (Windows package manager)
- OpenGL
- freeglut
- G++ (Mingw-w64 toolchain)

---

## Setup Instructions

### 1. Install MSYS2

Download and install MSYS2 from [here](https://www.msys2.org/).

### 2. Update MSYS2

Open the MSYS2 terminal and run:
```sh
pacman -Syu
```
Restart the terminal if prompted, then run:
```sh
pacman -Su
```

### 3. Install Build Tools and Libraries

In the MSYS2 terminal, run:
```sh
pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-freeglut mingw-w64-x86_64-mesa
```

### 4. Add MSYS2 to System PATH

1. Go to **Control Panel → System → Advanced system settings**.
2. Click **Environment Variables**.
3. Edit the `Path` variable under "System variables".
4. Add:
   ```
   C:\msys64\mingw64\bin
   ```
5. Click OK to save.

---

## Building the Project

Open the **MSYS2 MinGW 64-bit** terminal.

Navigate to your project folder and run:
```sh
g++ main.c -o main -lfreeglut -lopengl32 -lglu32
```
*Adjust `main.c` to your source file name if needed.*

---

## Running

```sh
./main.exe
```

---

## Notes

- Always use the **MSYS2 MinGW 64-bit** terminal for building/running.
- If you get missing header errors, double-check your package installations.
- For more advanced builds, consider adding a `Makefile` or CMake script.

---

## License

[MIT](LICENSE)

---

## Contributing

Pull requests are welcome! For major changes, please open an issue first to discuss what you would like to change.

---

**Happy coding!**

---
