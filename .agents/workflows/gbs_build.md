---
description: Tizen gbs build workflow
---

# TizenClaw GBS Build Workflow

Whenever build-related changes occur (code modifications, `CMakeLists.txt` modifications, packaging spec changes), execute the automated build in the following order and verify.

1. **Execute the Build**: Tizen `gbs build` creates a tarball based on the committed source in the git repository, but adding the `--include-all` option allows building with uncommitted changes included.
   - **Architecture Detection**: Automatically detect via the `cpu_arch` field of `sdb capability` (Fallback to `x86_64` if detection fails)
     ```bash
     ARCH=$(sdb capability 2>/dev/null | grep '^cpu_arch:' | cut -d':' -f2)
     [ -z "${ARCH}" ] && ARCH=x86_64
     ```
   - **Build Command**: `gbs build -A ${ARCH} --include-all`

   > [!CAUTION]
   > Do not use the `--noinit` option. It may cause build environment inconsistencies such as missing dependency installations. Always perform a full build including initialization.

2. **Verify Build Completion**: If the build completes normally, an `info: Done` message is output at the end. When this message appears, the build is successful.

3. **Check Build Logs**:
   - On success: `~/GBS-ROOT/local/repos/tizen/${ARCH}/logs/success/`
   - On failure: `~/GBS-ROOT/local/repos/tizen/${ARCH}/logs/fail/`

   You can verify the build results through the log files generated in the directories above.

4. **Caution: Prohibition of Pipe (`|`) Usage**
   Do not filter the output of the `gbs build` command with pipes like `| tail` or `| grep`. Pipes buffer the output, causing the build to appear as if it has hung.
   - ❌ `gbs build -A ${ARCH} --include-all 2>&1 | tail -50`
   - ✅ `gbs build -A ${ARCH} --include-all 2>&1`

   If you need to verify the build results, check the log files directly after the build is complete.

5. **AGENT Only: How to Detect Build Completion**
   Since `gbs build` is executed internally within `sudo` and `chroot` environments, the `command_status` tool cannot capture stdout/stderr at all (it is always displayed as "No output").

   Therefore, detect the completion of the build using the following method:
   ```bash
   # Execute Build (switch to background with WaitMsBeforeAsync=3000)
   gbs build -A ${ARCH} --include-all 2>&1

   # Detect Build Completion: Check the modification time of the RPM file (Polling)
   # Set WaitDurationSeconds=60 in command_status,
   # and check if the RPM file is up to date up to 5 times
   stat -c '%Y' ~/GBS-ROOT/local/repos/tizen/${ARCH}/RPMS/tizenclaw-*.${ARCH}.rpm 2>/dev/null
   ```

   **Determining Build Success/Failure:**
   - Success: Check the latest log in the `~/GBS-ROOT/local/repos/tizen/${ARCH}/logs/success/` directory
   - Failure: Check the latest log in the `~/GBS-ROOT/local/repos/tizen/${ARCH}/logs/fail/` directory
   ```bash
   # Check build results (for success)
   ls -lt ~/GBS-ROOT/local/repos/tizen/${ARCH}/logs/success/ 2>/dev/null | head -5
   ls -lt ~/GBS-ROOT/local/repos/tizen/${ARCH}/logs/fail/ 2>/dev/null | head -5
   ```

// turbo-all
