# GitHub Actions Automation

This document describes the automated workflows configured for the OpenFIDO project.

## Overview

OpenFIDO uses GitHub Actions for complete CI/CD automation. All workflows run automatically without manual intervention.

## Automated Workflows

### 1. CI Workflow (`ci.yml`)

**Triggers**: Push and PR to `main` or `develop`

**What it does**:
- ✅ Validates code formatting
- ✅ Builds and tests natively
- ✅ Builds firmware for all platforms
- ✅ Uploads artifacts

**Fully automatic** - no manual steps required.

---

### 2. Auto-Format (`auto-format.yml`)

**Triggers**: Push to `main` or `develop` (when `.c` or `.h` files change)

**What it does**:
- 🤖 Automatically formats all C code with `clang-format`
- 🤖 Commits changes back to the repository
- 🤖 No manual formatting needed!

**How it works**:
1. Detects code changes
2. Runs `clang-format` on all files
3. Commits formatted code automatically
4. Pushes back to the same branch

---

### 3. Auto-Release (`auto-release.yml`)

**Triggers**: Push tags matching `v*.*.*` (e.g., `v1.0.0`)

**What it does**:
- 🚀 Creates GitHub release automatically
- 🚀 Builds firmware for all platforms
- 🚀 Attaches firmware binaries to release
- 🚀 Generates release notes

**How to use**:
```bash
git tag v1.0.0
git push origin v1.0.0
```

The workflow will automatically:
1. Create a release on GitHub
2. Build firmware for ESP32-S3, ESP32-S2, STM32F4, nRF52840
3. Package and upload binaries
4. Publish the release

---

### 4. PR Comment (`pr-comment.yml`)

**Triggers**: Pull request opened or updated

**What it does**:
- 💬 Posts build status comment on PRs
- 💬 Updates comment as builds complete
- 💬 Shows status for each platform
- 💬 Links to detailed results

**Example comment**:
```
## 🤖 CI Build Status

### Code Quality
✅ Format Check: success

### Tests
✅ Native Tests: success

### Platform Builds
✅ esp32s3: success
✅ esp32s2: success
✅ stm32f4: success
✅ nrf52840: success

---
📊 View detailed results
```

---

### 5. Scheduled Build (`scheduled.yml`)

**Triggers**: 
- Every Monday at 00:00 UTC (automatic)
- Manual trigger via GitHub UI

**What it does**:
- 📅 Runs weekly build to catch issues early
- 📅 Checks for dependency updates
- 📅 Creates issues for available updates

**Benefits**:
- Ensures code still builds with latest dependencies
- Proactive dependency management
- Early detection of breaking changes

---

## Permissions

All workflows use minimal required permissions:
- `contents: write` - For auto-format commits and releases
- `pull-requests: write` - For PR comments
- `contents: read` - For reading repository

## Workflow Status

View all workflow runs at:
```
https://github.com/yourusername/OpenFIDO/actions
```

## Manual Triggers

Some workflows support manual triggering:

**Scheduled Build**:
1. Go to Actions tab
2. Select "Scheduled Build"
3. Click "Run workflow"

## Disabling Workflows

To disable a workflow temporarily, add this to the workflow file:
```yaml
on:
  workflow_dispatch: # Manual only
```

Or delete the workflow file from `.github/workflows/`.

## Customization

### Change Auto-Format Trigger

Edit `.github/workflows/auto-format.yml`:
```yaml
on:
  push:
    branches: [ develop ] # Only on develop
```

### Change Schedule

Edit `.github/workflows/scheduled.yml`:
```yaml
schedule:
  - cron: '0 0 * * *' # Daily instead of weekly
```

### Modify Release Template

Edit `.github/workflows/auto-release.yml` release body section.

## Troubleshooting

**Auto-format not committing**:
- Check repository permissions
- Ensure `GITHUB_TOKEN` has write access

**Release not creating**:
- Verify tag format matches `v*.*.*`
- Check workflow permissions

**PR comments not appearing**:
- Ensure `pull-requests: write` permission is set
- Check if bot comments are enabled

## Best Practices

1. **Always use semantic versioning** for releases (`v1.2.3`)
2. **Review auto-formatted changes** in the commit history
3. **Monitor scheduled build results** weekly
4. **Keep workflows updated** with latest action versions
