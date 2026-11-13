# b671-first-nights — the nightly's first real nights read; B671 closed

- **Queue item:** B671 (ADR-393 item 3); opens B679 and B680.
- **Why:** B671's last step was to read a real night's receipts and report the minutes.
- **Evidence consulted:** `gh run list --workflow nightly.yml` (runs 43007445035 and 43205570893); each run's jobs and `timing` (billable 0, public repo); the 2026-11-12 receipts `nightly-receipt-linux.json` and `nightly-receipt-macos.json` (verdict green, all six jobs ran at their floors, commit 8c9521db); the 2026-11-11 Linux log ("The operation was canceled." at the 120-minute limit).
- **A correction:** the lead told the human on 2026-11-11 that the first scheduled run had not fired. It had not fired *yet*: GitHub started it at 10:54 UTC, about 7.5 hours after the 03:17 cron.
- **Also here:** Loupe's notice 006 (thread loupe-map, seq 15, ball none) landed.
- **Verify:** `nice -n 10 ./verify fast` exit 0 (106 s); roadmap_check GREEN (709 rows).
- **Open:** B679 (Linux headroom, the human's choice); B680 (armor rows cite the night).
