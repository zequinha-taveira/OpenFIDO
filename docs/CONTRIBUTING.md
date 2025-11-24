# Contributing to OpenFIDO

Thank you for your interest in contributing to OpenFIDO! This document provides guidelines for contributing to the project.

## Code of Conduct

We are committed to providing a welcoming and inclusive environment. Please be respectful and constructive in all interactions.

## How to Contribute

### Reporting Bugs

Before creating a bug report:
1. Check existing issues to avoid duplicates
2. Collect relevant information (platform, version, logs)
3. Create a minimal reproducible example

When filing a bug report, include:
- **Description**: Clear description of the issue
- **Steps to Reproduce**: Detailed steps to reproduce the bug
- **Expected Behavior**: What should happen
- **Actual Behavior**: What actually happens
- **Environment**: Platform, OS, compiler version
- **Logs**: Relevant log output or error messages

### Suggesting Features

Feature requests are welcome! Please:
1. Check if the feature already exists or is planned
2. Explain the use case and benefits
3. Provide examples of how it would work
4. Consider implementation complexity

### Pull Requests

#### Before You Start

1. **Discuss First**: For major changes, open an issue first to discuss
2. **Check Existing Work**: Look for related PRs to avoid duplication
3. **Fork the Repository**: Work on your own fork

#### Development Workflow

1. **Create a Branch**
```bash
git checkout -b feature/my-new-feature
# or
git checkout -b fix/bug-description
```

2. **Make Changes**
- Write clean, readable code
- Follow the coding style (see below)
- Add tests for new functionality
- Update documentation as needed

3. **Test Your Changes**
```bash
# Run unit tests
cd tests && make test

# Test on hardware if applicable
# Verify FIDO2 conformance
```

4. **Commit**
```bash
git add .
git commit -m "feat: add new feature description"
```

Follow [Conventional Commits](https://www.conventionalcommits.org/):
- `feat:` New feature
- `fix:` Bug fix
- `docs:` Documentation changes
- `style:` Code style changes (formatting)
- `refactor:` Code refactoring
- `test:` Adding or updating tests
- `chore:` Maintenance tasks

5. **Push and Create PR**
```bash
git push origin feature/my-new-feature
```

Then create a Pull Request on GitHub with:
- Clear title and description
- Reference to related issues
- Screenshots/videos for UI changes
- Test results

## Coding Style

### C Code Style

We use `clang-format` with the provided `.clang-format` configuration.

**Format your code before committing:**
```bash
clang-format -i src/**/*.c src/**/*.h
```

**Key Style Points:**
- **Indentation**: 4 spaces (no tabs)
- **Line Length**: 100 characters maximum
- **Braces**: Linux kernel style (opening brace on same line for functions)
- **Naming**:
  - Functions: `snake_case`
  - Variables: `snake_case`
  - Constants: `UPPER_SNAKE_CASE`
  - Types: `snake_case_t`

**Example:**
```c
/**
 * @brief Calculate SHA-256 hash
 * 
 * @param data Input data
 * @param len Length of data
 * @param hash Output hash buffer (32 bytes)
 * @return CRYPTO_OK on success
 */
int crypto_sha256(const uint8_t *data, size_t len, uint8_t *hash)
{
    if (data == NULL || hash == NULL) {
        return CRYPTO_ERROR_INVALID_PARAM;
    }

    // Implementation here
    return CRYPTO_OK;
}
```

### Documentation

- **File Headers**: Include copyright and license
- **Function Comments**: Use Doxygen format
- **Inline Comments**: Explain complex logic
- **README Updates**: Document new features

### Testing

All new code should include tests:

**Unit Tests** (`tests/test_*.c`):
```c
void test_crypto_sha256(void)
{
    uint8_t data[] = "test";
    uint8_t hash[32];
    
    int ret = crypto_sha256(data, sizeof(data) - 1, hash);
    
    TEST_ASSERT_EQUAL(CRYPTO_OK, ret);
    // Verify hash value...
}
```

**Integration Tests**:
- Test full CTAP2 flows
- Verify storage persistence
- Check error handling

## Platform-Specific Contributions

### Adding a New Platform

1. Create `src/hal/<platform>/hal_<platform>.c`
2. Implement all HAL interface functions
3. Add build configuration
4. Add platform documentation
5. Test thoroughly

### HAL Implementation Checklist

- [ ] USB HID send/receive
- [ ] Flash read/write/erase
- [ ] Random number generation
- [ ] Button input
- [ ] LED output
- [ ] Timing functions
- [ ] Optional: Hardware crypto acceleration

## Security Considerations

### Security-Sensitive Code

When working on security-critical components:
- **Review Carefully**: Extra scrutiny for crypto and storage code
- **Test Thoroughly**: Include security-specific tests
- **Document Assumptions**: Explain security properties
- **Follow Best Practices**: Use constant-time operations where needed

### Reporting Security Issues

**DO NOT** open public issues for security vulnerabilities.

Instead, email: security@openfido.example.com

Include:
- Description of vulnerability
- Steps to reproduce
- Potential impact
- Suggested fix (if any)

## Review Process

### What to Expect

1. **Automated Checks**: CI runs tests and linters
2. **Code Review**: Maintainers review your code
3. **Feedback**: You may be asked to make changes
4. **Approval**: Once approved, PR will be merged

### Review Criteria

- **Functionality**: Does it work as intended?
- **Code Quality**: Is it clean and maintainable?
- **Tests**: Are there adequate tests?
- **Documentation**: Is it well-documented?
- **Style**: Does it follow coding standards?
- **Security**: Are there security implications?

## Getting Help

- **Questions**: Open a discussion on GitHub
- **Chat**: Join our community chat (link TBD)
- **Email**: contact@openfido.example.com

## Recognition

Contributors will be:
- Listed in CONTRIBUTORS.md
- Credited in release notes
- Acknowledged in documentation

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

---

**Thank you for contributing to OpenFIDO!** 🎉
