I have modified this version of lib-micro to work with 32 bit registers, cannot guarantee everything works, but it was ok in my tests :)

## Building

Clone with `git clone --recurse-submodules -j8 https://github.com/ceres-c/lib-micro` and build with `make build`

## 32 bit
There is a minimal working example for 32 bit code that can be build with `make mwe`.

## Running
Before running any of these programs in linux, you need to run
```bash
sudo wrmsr 0x1e6 0x200 -a
```
Otherwise, you'll get an `Illegal instruction` exception