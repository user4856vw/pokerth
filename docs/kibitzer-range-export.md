# Time-range analysis export

Full backup and restore remain separate from the new Android file export.
In the local history backup section choose a recent-hours shortcut or enter
local wall times in YYYY-MM-DD HH:mm format. The start is inclusive and the
end is exclusive. Ambiguous or nonexistent local DST endpoints are rejected.
Hand records are selected by completedAt, tournament summaries by finishedAt.
A tournament summary can cover hands outside the requested interval; do not
divide its full-tournament statistics by a partial hand count.

The original schema-1 archive fields and records are retained, with an
exportScope object recording UTC endpoints and selection rules. This is
a partial analysis export; current Restore rejects it to protect the full
journal. Do not restore it on older builds, which do not recognize that marker.
The complete durable JSONL journal is scanned, not only the RAM window.
Invalid or missing record timestamps abort filtered export rather than silently
discarding potentially relevant data. No gameplay or inference changes.

Test coverage: local-to-UTC conversion, half-open intervals at seconds and
milliseconds, offsets, invalid dates/ranges, DST ambiguity and gaps,
selection of tournament summaries, and export metadata. The Android
file picker still requires device smoke testing. Recent-hours shortcuts
round to minute resolution and include the current minute.
