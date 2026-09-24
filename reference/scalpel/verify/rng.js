// Seeded xorshift32 installed over Math.random, so oracle renders are reproducible.
// The HORDE port must use named, seeded RNG streams (SPEC §9); this only makes the JS oracle repeatable.
function seed(n = 0x1234567){
  let s = (n >>> 0) || 1;
  Math.random = () => { s ^= s << 13; s ^= s >>> 17; s ^= s << 5; return ((s >>> 0) % 1e9)/1e9; };
}
module.exports = {seed};
