# Stray radio project

An open-source, ESP32-based digital walkie-talkie (VoIP) for local Wi-Fi networks.



## ⚠️ Project Status: Early MVP

**This is a work-in-progress project at an early Minimum Viable Product (MVP) stage.**

The code is not yet optimized and contains debug logs and comments (primarily in Ukrainian). The hardware is a functional prototype. Please use at your own risk.

Feedback, bug reports, and contributions are highly welcome!

## Table of Contents
1. [Key Features](#key-features)
2. [Tech Stack](#tech-stack)
3. [Getting Started](#getting-started)
4. [Roadmap](#roadmap)

---
## Key Features

Here's what the current MVP version can do:

* **Smart Wi-Fi Setup (AP+STA Mode):**
    * On boot, the device first tries to connect to a known network stored in NVS.
    * If no network is found, it launches a Wi-Fi Access Point (named `StrayRadio-Setup`).
    * **Web Configuration Portal:** Connect to this AP to access a simple web page where you can scan for, select, and add new Wi-Fi credentials. They are then saved to NVS for the next boot.
* **Auto-Discovery:** Automatically finds other `stray` devices on the same local Wi-Fi network.
* **Live User List:** Displays a real-time list of all active users on the screen.
* **Communication Modes:**
    * **Broadcast Mode:** Talk to all users at once (PTT on "broadcast").
    * **Unicast Mode:** Select a specific user from the list to talk to them privately (PTT on a user's name).
* **Basic UI:** A simple graphical interface built with LVGL, navigated by a rotary encoder.

## Tech Stack

* **Hardware:**
    * ESP32 (e.g., ESP32-WROOM-32 module)
    * TLV320AIC3120 (Low-Power Mono Audio Codec)
    * ST7789 (240x240 LCD Screen)
    * AS5601 (Programmable Contactless Encoder)
* **Framework:** ESP-IDF (using FreeRTOS)
* **Graphics:** LVGL (UI designed in SquareLine Studio)
* **Network:** Wi-Fi (AP+STA), `esp_http_server`, UDP (for discovery and audio packets), SNTP (for time)

---

## Getting Started

### 1. Hardware
*(A full Bill of Materials (BoM) and schematics will be added in the future.)*

### 2. Software (Build & Flash)

**Prerequisites:**
* (Work in progress - will add ESP-IDF setup guide)
1. ESP-IDF v5.3.1 or higher

**Configuration:**
* (Work in progress - will add `menuconfig` / `sdkconfig` details)
1.  Partition Table -> Partition Table -> Custom partition table CSV 
    Custom partition CSV file -> partitions.csv
2. Serial flasher config -> Flash size -> 4Mb
3. LVGL configuration -> Font Usage -> Enable built-in fonts -> Enable Montserrat 16 and 28
4. HTTP Server -> Max HTTP Request Header Length -> 1024

---

## ⚠️ License Information

The source code for this project is licensed under the **MIT License**. See the `LICENSE` file for details.

**UI & SquareLine Studio:**
The UI files in this repository were generated using **SquareLine Studio under a Personal (Non-Commercial) License**. This means you **CANNOT** use this project (or its UI files) for any commercial purpose.

To use this project commercially, you must purchase a commercial license from SquareLine Studio and regenerate the UI files yourself.

## Roadmap

This is just the beginning. Here are some ideas for the future:

- [ ] Code refactoring and optimization.
- [ ] English translation for all code comments.
- [ ] Investigate Codec2 implementation (50/50).
- [ ] Add WAN server support to connect `stray`s over the internet.
- [ ] Redesign PCB for v2 (ESP32-S3, simpler codec).

## How to Contribute

Feedback is the most valuable contribution right now! If you have ideas, find a bug, or just want to say hi, please **[open an Issue](https://github.com/your-username/stray_radioPRJ/issues)**.

If you want to contribute code, please fork the repository and submit a Pull Request.

## Author


* **[Oleksandr Kucherenko]** - [Link to your GitHub profile or personal site]

