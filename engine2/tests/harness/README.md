# The harness header

The test author writes the harness header here, from section 7.1 of the spec: the exact
signatures of the engine's API (note ids, pitch input, per-voice modulation, state, observations).
The builder implements that header (spec 7.1; build stage 1, BD-8). Until it lands, nothing in
`core/api/` has a signature.
