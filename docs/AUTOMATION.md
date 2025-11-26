# GitHub Actions Automation

This document describes the automated workflows configured for the OpenFIDO project.

## Overview

OpenFIDO uses GitHub Actions for complete CI/CD automation. All workflows run automatically without manual intervention.

For detailed CI pipeline architecture, see [CI.md](CI.md).

## Automated Workflows

### 1. Core CI & Build (`ci.yml`, `build.yml`)

**Triggers**: Push and PR to `main` or `develop`

**What it does**:
- ✅ Validates code formatting
- ✅ Builds and tests natively (GCC & Clang)
- ✅ Builds firmware for all platforms (ESP32, STM32, nRF52)
- ✅ Tracks binary size
- ✅ Uploads artifacts

### 2. Security & Quality (`security-scan.yml`, `code-quality.yml`)

**Triggers**: Push/PR to `main`/`develop`, Weekly Schedule

**What it does**:
- 🔒 Scans for vulnerabilities (CodeQL, Trivy)
- 🔒 Detects secrets (Gitleaks)
- 🔍 Analyzes code quality (Cppcheck, Clang-Tidy)
- 📊 Checks code complexity and duplication

### 3. Code Coverage (`coverage.yml`)

**Triggers**: Push/PR to `main`/`develop`

**What it does**:
- 📈 Measures test coverage
- 📝 Generates HTML reports
- 💬 Comments on PRs with coverage stats

### 4. Auto-Release (`auto-release.yml`)

**Triggers**: Push tags matching `v*.*.*` (e.g., `v1.0.0`)

**What it does**:
- 🚀 Creates GitHub release automatically
- 📦 Packages firmware with checksums
- 📝 Generates changelog from git history

### 5. Documentation & Web (`pages-deploy.yml`)

**Triggers**: Push to `main`

**What it does**:
- 📚 Generates Doxygen docs
- 🌐 Builds Web Flasher tool
- 🚀 Deploys both to GitHub Pages (docs in `/docs`, flasher in `/flasher`)

### 6. Containers (`container.yml`)

**Triggers**: Push to `main`, Tags

**What it does**:
- 🐳 Builds multi-platform Docker images
- 📤 Pushes to GitHub Container Registry

### 7. Benchmarks (`benchmarks.yml`)

**Triggers**: Push/PR to `main`/`develop`

**What it does**:
- ⚡ Measures crypto and protocol performance
- ⚠️ Alerts on performance regressions

---

## Manual Triggers

Most workflows support manual triggering via the "Actions" tab in GitHub:
1. Go to Actions tab
2. Select the workflow
3. Click "Run workflow"

## Permissions

All workflows use minimal required permissions following security best practices.

## Workflow Status

View all workflow runs at:
```
https://github.com/yourusername/OpenFIDO/actions
```
