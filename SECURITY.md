# Security

## Flashing is a risk of its own

TIDE is firmware for the M-VAVE FM-1. The FM-1 has a single copy of its firmware and no known rollback: a failed write can leave the unit dark, and then only a hardware recovery helps. That is a risk of installing, not a security hole, and it is described in [docs/INSTALL.md](docs/INSTALL.md) and [docs/RECOVERY.md](docs/RECOVERY.md). Back up first. No warranty (GPL-3.0-only).

Only install firmware from this repository's [releases](https://github.com/pssdrnk-cmyk/DUELL-TIDE/releases) or build it yourself from the source. Compare the SHA-256 the installer page shows with the one in the release.

## Reporting a security problem

Security problems are, for example: a way to make the firmware misbehave through crafted USB-MIDI or MIDI data, a backup or patch file that harms the unit or the editor, a problem in the installer or update loader that writes something the user did not choose, or a problem in a workflow or web page of this repository.

Please report it privately: on the repository's **Security** tab choose **Report a vulnerability**. If that is not offered, open an issue that says only that you have a security report, with no details, and the maintainer will arrange a private channel. Please do not post details in a public issue.

Say which build (stable or beta, and its tag), what you did, and what happened. There is one maintainer and no bounty; you will get an answer as soon as it can be given.

## What is supported

The newest **stable** and **beta** builds. Older builds are not patched; install a newer one.

Problems in plain DUELL, Felucca or M-VAVE's own firmware are not this project's to fix: report them to [OSCILL/R](https://github.com/oscillr/DUELL), [Hügelton Instruments](https://github.com/hugelton/Felucca) or M-VAVE.
