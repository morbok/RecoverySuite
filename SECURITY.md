# Security and private data

RecoverySuite's recovery workflow requires no API keys or cloud credentials.

## What must stay private

Keep API tokens, passwords, private keys, personal assistant settings, signing credentials and environment files outside Git. Disk images, recovered files, source identifiers, scan catalogs and rescue reports can contain private data: keep them outside the repository and public issue attachments.

Local ignores cover common credential files, assistant settings, disk images, output folders and backup snapshots. A staged commit hook and the **Credential safety** workflow add Gitleaks detection and private-filename checks. Hooks must be enabled per checkout; see [CONTRIBUTING.md](CONTRIBUTING.md). A file can still be force-added, and no scanner detects every secret. Review changes before committing.

Do not add live credentials to examples, test fixtures, logs, documentation, workflow arguments, or screenshots. Workflows need read-only repository access; recovery has no third-party API secrets to configure.

## Report a security problem privately

If GitHub offers **Report a vulnerability** on this repository's Security page, use it. Otherwise contact the maintainer privately through an established channel and request a secure report route. Do not post credentials, private disk data, or exploitable details in a public issue. No security email address is published by this project.

For ordinary bugs, describe the OS, package commit, steps and a sanitized error message. Reproduce with synthetic data where possible.

## If a credential was committed

1. Revoke or rotate it at its provider immediately. Deleting a file does not revoke it.
2. Remove it from the current tree and review logs/artifacts that might contain it.
3. Coordinate any Git history cleanup with repository collaborators. Old commits, forks and caches can retain copies; avoid an uncoordinated force push.
4. Rerun credential checks before pushing.

## Validation boundaries

The repository audit on 2026-11-07 used Gitleaks 8.30.1 against all fetched refs/history and found no credential candidates before this cleanup. This is a point-in-time detection result, not a guarantee of absence. A tracked local assistant configuration was removed from the current tree as private machine configuration; no credential value was copied into this documentation.

Automated application tests run on disposable GitHub runners. Physical failing-drive qualification and independent release review remain open; this is an unsigned prototype.
