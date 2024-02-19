# Kernel Module: System Information Fetching

## Description

This project involves implementing a Linux kernel module, named `kfetch_mod`, which serves as a character device driver. The module creates a device named `/dev/kfetch`, and a user-space program named `kfetch` can retrieve system information by reading from this device.

## Features

- **Kernel Module**: `kfetch_mod` is responsible for fetching system information and providing it to user-space.
- **Device Interface**: A character device `/dev/kfetch` is created for accessing system information.
- **Customizable Information**: Users can specify information they want to retrieve using a bitmask.
- **Device Operations**: The module supports operations like open, release, read, and write.

## System Information

The kernel module fetches the following system information:

- **Kernel**: The kernel release version.
- **CPU**: The CPU model name.
- **CPUs**: The number of CPU cores.
- **Memory**: Free and total memory.
- **Processes**: The number of processes.
- **Uptime**: System uptime.

## Usage

### Compile and Load Module:

- Use `make` command to compile the kernel module.
- Use `make load` to insert the kernel module.

### User-space Program (`kfetch`):

- Compile the program using `cc kfetch.c -o kfetch`.
- Run the program as root: `sudo ./kfetch [options]`.

#### Options:

- `-a`: Show all information.
- `-c`: Show CPU model name.
- `-m`: Show memory information.
- `-n`: Show the number of CPU cores.
- `-p`: Show the number of processes.
- `-r`: Show the kernel release information.
- `-u`: Show system uptime.

#### Example:

- `sudo ./kfetch -c -m`: Display CPU model name and memory information.
