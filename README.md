# TizenClaw LLM Plugin Sample

This project is a sub-project for [tizenclaw](https://github.com/hjhun/tizenclaw). 
It serves as an official sample demonstrating how to add and integrate an external LLM (Large Language Model) backend as a **plugin (shared library `.so`)** within the TizenClaw system.

Through this project, you can easily create and test your custom LLM response backend by implementing TizenClaw's standardized C API interfaces.

---

## 🚀 Quick Start Guide

This guide is designed to help even first-time users quickly build and deploy the project to a device.

### 1. Prerequisites
To build and test this project, the following tools must be installed in your local development environment:
- **Tizen GBS (Git Build System)**: Required to build the project for the Tizen platform and generate RPM packages.
- **Tizen SDB (Smart Development Bridge)**: Required to communicate with the target device (emulator or physical device) and install packages.
- A **Tizen Emulator** or a **physical Tizen device** must be connected to your PC (you can verify this using the `sdb devices` command).

### 2. Automated Build and Installation
The project root directory includes a `deploy.sh` script that automates the entire process from GBS build, transfer to the target device, to app framework registration.

With your device connected to the development environment, open a terminal and run the following commands:

```bash
# Verify execution permissions, grant if necessary
chmod +x deploy.sh

# Run the complete automated pipeline (Build -> Transfer -> Register Plugin -> Restart TizenClaw)
./deploy.sh
```

**Useful options for `deploy.sh`:**
- `./deploy.sh -a <arch>` : Forcibly specify the architecture to build (by default, it auto-detects the connected device's architecture).
- `./deploy.sh -n` : Skips the build environment initialization process (useful for quick rebuilds after minor code changes).
- `./deploy.sh -s` : Skips the build process entirely (used when you only want to deploy an already built RPM file).
- `./deploy.sh --dry-run` : Previews the processes that will be executed without actually running the commands.

> **💡 Note:** During the deployment script execution, your developed plugin is registered into the Tizen system (`org.tizen.tizenclaw-llm-plugin-sample`) using the `unified-backend` command. Immediately after completion, the `tizenclaw` daemon process is restarted so that the plugin is applied.

### 3. Verification
Upon successful deployment, the restarted TizenClaw daemon will dynamically find and load the sample plugin (`.so`) we just installed.
You can verify if the plugin was successfully initialized and attached by checking the real-time logs on the target device:

```bash
# Check real-time TizenClaw logs
sdb shell dlogutil TIZENCLAW
```

---

## 📚 Plugin Creation Principles and Detailed Guide

If you want to write logic for your own custom plugin, or are curious about the specific principles and methods of how TizenClaw calls the designated C API specifications, please refer to the detailed guide document below:

- [🇺🇸 TizenClaw LLM Plugin Creation Guide (English)](docs/plugin_creation_guide_en.md)

---

## Project Structure

- `src/` : Core C/C++ internal behavior code of the sample plugin (e.g., `plugin-sample.cc`)
- `inc/` : Header file management
- `res/` : Global configuration files required when the plugin is loaded (e.g., `plugin_llm_config.json`)
- `packaging/` : Directory containing the `.spec` definition for Tizen platform GBS build and RPM packaging
- `docs/` : Markdown documentation providing plugin implementation guides
- `deploy.sh` : Utility script for automated project build and deployment
