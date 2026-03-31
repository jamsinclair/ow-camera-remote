# Contributing to OW Camera Remote

Thank you for considering contributions to this Pebble Watch app. This guide will help you align with our project philosophy and coding standards.

## Project Scope

This app serves a focused purpose: **remotely capture photos using a Pebble Watch as a controller**.

We intentionally keep features minimal. Only features that are:
- Essential for the core use case (controlling the camera remotely, taking pictures)
- Genuinely useful from the watch interface
- Simple and pragmatic to implement

...should be considered for inclusion.

If you're considering a new feature, start a discussion (issue or PR) to confirm it aligns with the project scope before investing time in implementation.

### Code Style & Standards Reference

**Philosophy**: Simple, pragmatic, self-documenting code. Avoid over-engineering and heavy abstractions.

**Naming**:
- Private/module functions: `prv_` prefix, `snake_case`
- Callbacks: `snake_case` describing action (e.g., `bluetooth_status_callback`)
- Module-level statics: `s_` prefix (e.g., `s_data`)
- Constants: `UPPER_SNAKE_CASE`
- Types: `PascalCase`

**Comments**:
- Only when logic isn't immediately obvious
- Explain *why*, not *what*
- Avoid obvious comments

**Code Structure**:
- Use guard clauses and early returns to reduce nesting
- Keep functions focused and reasonably short
- Check pointers only at API boundaries, trust internal code
- Use lookup arrays instead of large switch statements when appropriate

**Data Organization**:
- Group related data in structs, but don't over-structure
- Use enums to group related constants instead of scattered #defines


## Building & Testing

Using the Pebble SDK, you can build and deploy the app to your watch. Run the following command in the project directory:

```bash
pebble build
```

To install the app on your watch, use:
```bash
pebble install --cloudpebble
```
Note:
- Requires the Core Devices Mobile App and the dev connection to be enabled for the watch.
- Much of the watches functionality relies on the companion Android app, so we cannot use the emulator much for testing.

To debug logs from the watch, use:
```bash
pebble logs --cloudpebble
```

## Submitting Changes

1. **Create a branch** from `master` for your work
1. **Keep commits clean** - one logical change per commit
1. **Submit a PR** with a clear description of what changed and why
1. **Expect review** - we'll check alignment with project scope and standards

## Questions?

If something is unclear or you'd like feedback before investing time, open an issue to discuss.
