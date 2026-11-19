# Astra Studio security model

This document describes what the current build protects, what it deliberately does not,
and how the licensing design changes in engine 0.2.

## Assets to protect

1. **Engine access** — only people the studio owner has issued a code to can create an
   account and open the studio.
2. **Local account registry** — credentials live on the machine, not in a cloud service.
3. **GitHub token** — the token used to list repositories.
4. **Owner tooling** — `AstraKeyForge.exe` mints codes; it must not be usable by anyone
   who receives the engine build.

## Access codes

A code is a **signed, self-describing token**, not a random string in a database:

```
10 payload bytes ──► 16 Crockford base32 characters ──► K7QM-3XPD-9RTA-B2VH
payload: version(4) | role(3) | durationIndex(6) | issuedDay(12) | nonce(15)
tag:     first 40 bits of HMAC-SHA256(masterKey, payload)
```

* Verification is fully offline: Studio checks the HMAC and the date window locally.
* A code encodes its own role (`User`, `Staff`, `Owner`) and lifetime (24 h, 7, 14, 30, 90,
  180, 365 days, or never) so no server round-trip is needed.
* **Single use per install**: the hash of every redeemed code is written to
  `engine/licensing/used_codes.json` before the account is created. Re-using the same code on the
  same machine is rejected with `AlreadyUsed`.
* Codes are forgiving to type: case-insensitive, dashes and spaces ignored, and the
  visually ambiguous `O/I/L/U` map onto `0/1/1/V`.
* Errors are specific (`Malformed`, `BadSignature`, `Expired`, `NotYetValid`,
  `AlreadyUsed`, `RoleNotAllowed`) so support can diagnose without revealing the secret.

### Known limitation (and the plan)

Milestone 1 signs codes with a **shared HMAC secret** compiled into both Studio and
KeyForge. That is enough to make codes unforgeable *without the binary*, but it means the
secret is extractable by someone with the executable and reverse-engineering time.

Engine 0.2 replaces this with **Ed25519**: KeyForge holds the private key (encrypted at rest
with DPAPI), Studio embeds only the public key, and codes carry a 64-byte signature. Studio
then physically cannot mint codes. The wire format keeps the leading version nibble so old
codes can be rejected cleanly instead of silently misinterpreted.

### Owner tool

* First run creates an owner passphrase (minimum 12 characters, PBKDF2-HMAC-SHA256 with
  200 000 iterations, 16-byte random salt) stored in `keyforge/owner.json`.
* Every launch requires that passphrase; there is no "remember me".
* The CLI (`AstraKeyForge.exe --issue-code`) requires the same passphrase via
  `--passphrase` or `ASTRA_KEYFORGE_PASSPHRASE`. It cannot mint codes on a machine with no
  owner record.
* Issued codes land in a ledger with role, duration, note, batch, issue date and expiry.
  Revocation marks the ledger row; because codes are verified offline, revocation matters
  for the export/audit trail and for future online validation.

## Accounts

* Username: 3–24 characters, `[a-z0-9._-]`; display name 2–48 characters.
* Passwords: PBKDF2-HMAC-SHA256, 120 000 iterations, 16-byte salt, constant-time compare.
  Minimum length 10 (12 for staff). Strength is scored and surfaced in the UI.
* Failure handling: after 5 failed attempts the account locks for 30 seconds. The UI says
  so honestly instead of silently failing.
* Remember-me: a 24-byte random token with a 30-day expiry in `session.json`; the token is
  cleared on sign-out. Only one session at a time.
* Accounts are **local to the machine**. There is no server. Uninstalling the engine (or
  deleting the data folder) removes the accounts with it — this is documented in the UI.

## Secrets at rest

`crypto::ProtectSecret` / `UnprotectSecret`:

* On Windows the GitHub token is sealed with **DPAPI** (`CryptProtectData`, current-user
  scope) and stored base64 in `settings.json`. Another user account on the same machine
  cannot read it.
* Where DPAPI is unavailable the fallback is `raw:` + XOR-hex obfuscation. This is
  obfuscation, not encryption, and the code says so; it exists so the non-Windows headless
  tools can run the same flows in tests.

## Network

* The only outbound calls are to `api.github.com`, made by `WinHTTP` with
  `Authorization: Bearer <token>` and `X-GitHub-Api-Version: 2022-11-28`.
* No telemetry, no analytics, no auto-update, no other hosts.
* Without a token the hub uses the bundled demo fixture (`assets/demo/github_demo.json`)
  and labels the workspace `DEMO WORKSPACE` in the top bar and the footer.

## Logging

`log::Sink` keeps an in-memory ring buffer (512 entries) shown in the Output Log panel.
Tokens, passwords and full codes are never written to it; codes are logged masked
(`K7QM-****-****-B2VH`). Logs are not persisted to disk in this milestone.

## Reporting

Security issues: contact Astra Productions directly rather than opening a public issue.
Include the build version (`Help → About`), the affected data file, and reproduction steps.
