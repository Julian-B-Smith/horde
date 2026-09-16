// Presets reference curves by name; stage fields not listed fall back to defaults.
const PRESETS={
 'init':{},
 'roar growl · sine → tube → diode, env→cutoff':{src:'sine',stages:[{a:'tube',b:'polynomial',morph:0.15,shape:0.5,drive:20,ft:1,fpos:1,cut:900,res:0.5},{a:'diode',drive:14,bias:0.2,shape:0.5,ft:2,fpos:0,cut:1400,res:0.4},{on:false}],mod:{ec:0.6,ed:0.3},envA:3,envR:120,eco:0.3},
 'cheby ladder · drive-ref on, 0 dB = pure 12th':{src:'sine',driveRef:1,stages:[{a:'cheby',shape:0.5,drive:-3,mix:0.8},{a:'soft',drive:6,shape:0.3},{on:false}],mod:{ld:0.25},lfoRate:0.15,adaa:true},
 'fold · pre-fold lowpass, shape = ridge hardness':{src:'saw',stages:[{a:'fold',shape:0.25,drive:14,ft:1,fpos:0,cut:700,res:0.2},{a:'soft',drive:4,shape:0.2},{on:false}],mod:{ed:0.4},envA:5,envR:150,adaa:true},
 'polynomial screech · env→morph':{src:'sine',stages:[{a:'soft',b:'polynomial',morph:0.7,shape:0.8,drive:18,floor:0.01,ft:1,fpos:1,cut:3000,res:0.3},{a:'overdrive',drive:8,shape:0.4},{on:false}],mod:{em:-0.5,ec:0.5},envA:2,envR:200},
 'fractal grit · multiband':{route:2,src:'super',xo1:180,xo2:2400,stages:[{a:'tube',drive:10,shape:0.6,ft:1,fpos:1,cut:400},{a:'fractal',shape:0.8,drive:6,floor:0.01},{a:'shards',b:'crush',morph:0.3,shape:0.6,drive:8,floor:0.01,level:-4}],eco:0.4},
 'pitch-tracked feedback · tamed':{route:4,src:'saw',fbAmt:0.75,fbSemi:7,fbDamp:3500,fbHp:120,fbTap:0,stages:[{a:'overdrive',drive:10,shape:0.3},{a:'hard clip',shape:0.5,drive:6,ft:3,fpos:1,cut:200},{a:'diode',drive:4,ft:1,cut:5000}],out:-10},
 'mid/side · clean centre, wrapped sides':{route:3,src:'super',spread:35,stages:[{a:'soft',drive:4,shape:0.2},{a:'clip→wrap',shape:0.7,drive:28,ft:3,fpos:1,cut:200},{a:'diode',drive:6,shape:0.3}]},
 'inertia sweep · lfo→cutoff rings':{src:'pulse',stages:[{a:'fold',shape:0.6,drive:12,ft:2,fpos:1,cut:1200,res:0.6},{a:'diode',drive:10,bias:0.1},{on:false}],mod:{lc:0.8,ld:0.3},lfoRate:0.3,inertia:0.7},
 'flux chaos · parallel, stages feed each other':{route:1,src:'square',flux:0.8,stages:[{a:'fold',b:'rectify',morph:0.5,drive:12,level:-6},{a:'clip→wrap',b:'cheby',morph:0.5,shape:0.5,drive:6,level:-6},{a:'polynomial',b:'fractal',morph:0.5,shape:0.7,drive:12,floor:0.01,level:-6}],eco:-0.4},
};
function applyPresetTo(base,pr,SHAPERS){const idx=v=>typeof v==='string'?SHAPERS.findIndex(s=>s.name.startsWith(v)):v;
  for(const k in pr){if(k==='stages'){pr.stages.forEach((s,i)=>{const t=Object.assign({},s);if('a'in t)t.a=idx(t.a);if('b'in t)t.b=idx(t.b);Object.assign(base.stages[i],t);});}
    else if(k==='mod')Object.assign(base.mod,pr.mod);else base[k]=pr[k];}return base;}
if(typeof module!=='undefined')module.exports={PRESETS,applyPresetTo};
