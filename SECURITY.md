# Security policy

horde is an audio plugin. It runs inside your DAW, with your DAW's permissions, so a flaw in it
can reach your machine. If you find one, please tell us privately first.

## Reporting a vulnerability

Use GitHub's private vulnerability reporting:
**[Report a vulnerability](https://github.com/Julian-B-Smith/horde/security/advisories/new)**
(or this repository's **Security** tab → **Report a vulnerability**).

Please do **not** open a public issue, pull request or discussion for a security problem. A
private report lets us fix it before anyone can use it against users.

A useful report includes:
- what you found, and where (a file and line, or the feature);
- how to reproduce it: a preset or session file, a sequence of actions, the host and the version;
- what an attacker could do with it, as far as you know.

## What happens next

- We aim to acknowledge a report within a week. horde is a small project, so please allow time.
- We confirm the issue and work out a fix. We may ask you questions in the private report.
- We publish an advisory when a fixed version is available, and credit you there unless you
  would rather not be named.
- There is no bug bounty.

## Scope

**In scope:**
- the plugin (CLAP, VST3, AU) and its built-in interface;
- loading presets, corners and saved DAW sessions;
- the release artifacts published on this repository's Releases page.

**Out of scope:**
- the design labs under `docs/design/` and `reference/`, which are browser prototypes and
  development tools, not shipped code;
- issues that need an attacker who already controls your machine or your DAW;
- third-party components, which belong upstream. If horde is affected, tell us as well.

## Supported versions

horde has not had a 1.0 release yet. Until it does, fixes land on `main` and in the next release.
After 1.0, this section will list which versions receive security fixes.
