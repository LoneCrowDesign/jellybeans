// Copyright (C) 2026 Lone Crow Design, LLC
// Licensed under GPLv3 — see LICENSE
//
// The browser client, split into two shared pieces:
//   WEBCONSOLE_CSS  — the stylesheet, reused by the SPA home page AND by
//                     WebConsole::pageShell() so every user-facing page (home +
//                     any host escape-hatch page) matches.
//   WEBCONSOLE_BODY — the live console SPA body. Schema-driven: it draws nothing
//                     of its own; on the WebSocket "hello" manifest it builds the
//                     log pane, command palette, variable table, record
//                     collections, uploads, files, and the top-bar nav from
//                     whatever the device advertised.
// Both are served by WebConsole.cpp. Kept as editable raw string literals — tweak
// the markup in place, no build/minify step. Keep self-contained (no external
// assets) so it works offline on a SoftAP.
#pragma once
#include <Arduino.h>

static const char WEBCONSOLE_CSS[] PROGMEM = R"CSS(
 :root{--bg:#0f1115;--fg:#e6e6e6;--mut:#8a93a2;--card:#181b22;--acc:#4aa3ff;--line:#262b34}
 @media(prefers-color-scheme:light){:root{--bg:#f4f5f7;--fg:#1a1d22;--mut:#5a6472;--card:#fff;--acc:#1769d6;--line:#e2e5ea}}
 *{box-sizing:border-box}body{margin:0;font:14px/1.45 system-ui,sans-serif;background:var(--bg);color:var(--fg)}
 header{display:flex;align-items:center;gap:.6rem;padding:.7rem 1rem;border-bottom:1px solid var(--line);flex-wrap:wrap}
 header b,.brand{font-size:1.05rem;font-weight:700}
 .dot{width:.6rem;height:.6rem;border-radius:50%;background:#c0392b}.dot.on{background:#2ecc71}
 .nav{display:flex;gap:.3rem;flex-wrap:wrap}
 .navbtn{padding:.3rem .6rem;border-radius:6px;text-decoration:none;color:var(--fg);border:1px solid var(--line);font-size:.85rem}
 .navbtn.active{background:var(--acc);color:#fff;border-color:var(--acc)}
 main{max-width:960px;margin:0 auto;padding:1rem;display:grid;gap:1rem}
 .card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:.9rem 1rem}
 h2{margin:.1rem 0 .7rem;font-size:.8rem;letter-spacing:.06em;text-transform:uppercase;color:var(--mut)}
 #log{height:44vh;overflow:auto;margin:0;padding:.6rem;background:#0008;border-radius:8px;font:12px/1.4 ui-monospace,monospace;white-space:pre-wrap}
 @media(prefers-color-scheme:light){#log{background:#0d1117;color:#d6dae0}}
 .row{display:flex;gap:.5rem;align-items:center;flex-wrap:wrap;padding:.35rem 0;border-top:1px solid var(--line)}
 .row:first-of-type{border-top:0}.row .nm{min-width:8rem;font-weight:600}.row .hp{color:var(--mut);font-size:.85em;flex:1 1 100%}
 ul{margin:.2rem 0}
 input,button{font:inherit;color:var(--fg);background:var(--bg);border:1px solid var(--line);border-radius:6px;padding:.35rem .55rem}
 input[type=text],input[type=number]{min-width:9rem}button{cursor:pointer;background:var(--acc);color:#fff;border:0}
 button:active{transform:translateY(1px)}.tok{margin-left:auto}
 table{border-collapse:collapse;width:100%;font-size:.85em}
 th,td{border:1px solid var(--line);padding:.3rem .5rem;text-align:left;white-space:nowrap}
 th{color:var(--mut);font-weight:600}
 .repl{display:flex;align-items:center;gap:.4rem;margin-top:.55rem}
 .repl .pr{color:var(--acc);font:13px/1 ui-monospace,monospace;font-weight:700}
 #cmdline{flex:1;min-width:0;font:12px/1.4 ui-monospace,monospace;background:var(--bg)}
 .al{display:inline-flex;align-items:center;gap:.3rem;color:var(--mut);font-size:.85em}
 .al input,.al select{color:var(--fg)}
)CSS";

static const char WEBCONSOLE_BODY[] PROGMEM = R"HTMLDELIM(
<header><span class="dot" id="dot"></span><b class="brand" id="name">Web Console</b>
 <nav class="nav" id="nav"></nav>
 <input class="tok" id="token" type="text" placeholder="token (if required)" style="display:none;max-width:12rem">
</header>
<main>
 <section class="card"><h2>Console</h2><pre id="log"></pre>
  <div class="repl"><span class="pr">&gt;</span>
   <input id="cmdline" type="text" autocomplete="off" autocapitalize="off" spellcheck="false"
    placeholder="type a command — 'help' for the list, Tab to complete">
  </div></section>
 <section class="card" id="cmdCard" style="display:none"><h2>Controls</h2><div id="cmds"></div></section>
 <section class="card" id="varCard" style="display:none"><h2>Variables</h2><div id="vars"></div></section>
 <section class="card" id="colCard" style="display:none"><h2>Records</h2><div id="cols"></div></section>
 <section class="card" id="upCard" style="display:none"><h2>Uploads</h2><div id="ups"></div></section>
 <section class="card" id="fileCard" style="display:none"><h2>Files</h2><div id="files"></div></section>
</main>
<script>
const $=s=>document.querySelector(s), logEl=$('#log');
let ws, tok=()=>$('#token').value.trim();
function line(t){const at=logEl.scrollTop+logEl.clientHeight>=logEl.scrollHeight-4;
 logEl.textContent+=t+'\n';if(logEl.textContent.length>60000)logEl.textContent=logEl.textContent.slice(-40000);
 if(at)logEl.scrollTop=logEl.scrollHeight;}
function send(o){if(tok())o.token=tok();ws&&ws.readyState===1&&ws.send(JSON.stringify(o));}
function esc(s){return String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));}

// Top-bar nav — Home plus every host-registered page (manifest.pages).
function renderNav(pages){const nav=$('#nav');if(!nav)return;nav.innerHTML='';
 const mk=(label,href)=>{const a=document.createElement('a');a.className='navbtn';a.href=href;a.textContent=label;
  if(href===location.pathname)a.classList.add('active');nav.appendChild(a);};
 mk('Home','/');(pages||[]).forEach(p=>mk(p.label,p.path));}

// ── Commands ────────────────────────────────────────────────────────────────
// The manifest's command list, kept for the console input line + help/completion.
let cmds=[];
function runCmd(name,args){send({t:'cmd',id:Date.now()&0xffff,name,args:args||{}});}

// Only *pinned* commands get a button (in the Controls card); the rest are typed
// into the console. A pinned command with an arg schema gets a typed mini-form.
function buildCmds(list){cmds=list||[];const pinned=cmds.filter(c=>c.pinned);
 const box=$('#cmds');box.innerHTML='';$('#cmdCard').style.display=pinned.length?'':'none';
 pinned.forEach(c=>{const r=document.createElement('div');r.className='row';
  const fields=c.args||[];
  const form=fields.map(f=>`<label class="al">${esc(f.label||f.name)}${fieldInput(f,null)}</label>`).join('');
  r.innerHTML=`<span class="nm">${esc(c.name)}</span>${form}<button>Run</button>${c.help?`<span class="hp">${esc(c.help)}</span>`:''}`;
  r.querySelector('button').onclick=()=>{const args=collectArgs(r,fields);if(args===null)return;
   if(c.confirm&&!confirm('Run '+c.name+'?'))return;runCmd(c.name,args);};
  box.appendChild(r);});}

// Read a command's typed inputs into an args object. Bools always emit true/false;
// other fields emit when non-empty (required-but-empty is an error). null = abort.
function collectArgs(el,fields){const o={};
 for(const f of fields){const inp=el.querySelector(`[data-fld="${CSS.escape(f.name)}"]`);if(!inp)continue;
  if(f.type==='bool'){o[f.name]=inp.checked;continue;}
  const raw=inp.value.trim();
  if(raw===''){if(f.required){line('! '+f.name+' is required');return null;}continue;}
  if(f.type==='number'){const n=Number(raw);if(Number.isNaN(n)){line('! '+f.name+' must be a number');return null;}o[f.name]=n;}
  else o[f.name]=raw;}
 return o;}

// ── Console input line (REPL) ─────────────────────────────────────────────────
// `<name>`                → no args        `<name> {..json..}`  → JSON args
// `<name> key=val val …`  → parsed against the command's arg schema, typed.
// Built-ins handled client-side: help / ? (list or one command), clear.
const hist=[];let histIdx=0;
function runLine(s){line('> '+s);
 const sp=s.search(/\s/);const name=sp<0?s:s.slice(0,sp);const rest=(sp<0?'':s.slice(sp+1)).trim();
 if(name==='help'||name==='?'){printHelp(rest);return;}
 if(name==='clear'){logEl.textContent='';return;}
 const c=cmds.find(x=>x.name===name);
 if(!c){line("! unknown command: "+name+"  (type 'help')");return;}
 let args;try{args=parseArgs(c,rest);}catch(e){line('! '+e.message);return;}
 if(c.confirm&&!confirm('Run '+name+'?'))return;
 runCmd(name,args);}

function parseArgs(c,rest){
 const fields=c.args||[];
 if(rest===''){for(const f of fields)if(f.required)throw new Error(f.name+' is required');return {};}
 if(rest[0]==='{'||rest[0]==='[')return JSON.parse(rest);   // explicit JSON always allowed
 if(!fields.length)throw new Error(c.name+' takes no arguments (pass JSON to send raw args)');
 const o={};let p=0;
 for(const t of tokenize(rest)){let key,val;const eq=t.indexOf('=');
  if(eq>0&&fields.some(f=>f.name===t.slice(0,eq))){key=t.slice(0,eq);val=t.slice(eq+1);}
  else{if(p>=fields.length)throw new Error('too many arguments for '+c.name);key=fields[p++].name;val=t;}
  const f=fields.find(x=>x.name===key);if(!f)throw new Error('unknown arg: '+key);
  o[key]=coerce(f,val);}
 for(const f of fields)if(f.required&&!(f.name in o))throw new Error(f.name+' is required');
 return o;}

function coerce(f,v){
 if(f.type==='bool'){if(/^(1|true|on|yes|y)$/i.test(v))return true;if(/^(0|false|off|no|n)$/i.test(v))return false;
  throw new Error(f.name+' must be true/false');}
 if(f.type==='number'){const n=Number(v);if(Number.isNaN(n))throw new Error(f.name+' must be a number');return n;}
 return v;}   // text / enum → string (enum values aren't restricted client-side)

// Quote-aware split: `a "b c" d` → ['a','b c','d'].
function tokenize(s){const out=[];let cur='',q=0,used=0;
 for(const ch of s){if(q){if(ch===q)q=0;else cur+=ch;used=1;}
  else if(ch==='"'||ch==="'"){q=ch;used=1;}
  else if(/\s/.test(ch)){if(used){out.push(cur);cur='';used=0;}}
  else{cur+=ch;used=1;}}
 if(used)out.push(cur);return out;}

function fmtArg(f){const t=f.type==='enum'?f.type+'('+(f.options||'')+')':f.type;
 const s=f.name+':'+t;return f.required?s:'['+s+']';}
function printHelp(arg){
 if(arg){const c=cmds.find(x=>x.name===arg);if(!c){line('! no such command: '+arg);return;}
  line('  '+c.name+(c.args&&c.args.length?' '+c.args.map(fmtArg).join(' '):'')+(c.help?'   — '+c.help:''));return;}
 line('  commands:');
 if(!cmds.length)line('   (none registered)');
 cmds.forEach(c=>line('   '+c.name.padEnd(14)+(c.args&&c.args.length?c.args.map(fmtArg).join(' '):'')
  +(c.pinned?'  [pinned]':'')+(c.help&&!(c.args&&c.args.length)?'  — '+c.help:'')));
 line("  usage: <name> [key=val …] | <name> {json};  'clear' wipes the log.");}

function completeLine(){const el=$('#cmdline'),s=el.value;
 if(/\s/.test(s.trim())||!s.trim())return;      // only complete the (first) command name
 const names=['help','clear'].concat(cmds.map(c=>c.name));
 const m=names.filter(n=>n.startsWith(s.trim()));
 if(m.length===1){el.value=m[0]+' ';}
 else if(m.length>1){line('  '+m.join('  '));
  let cp=m[0];m.forEach(n=>{while(!n.startsWith(cp))cp=cp.slice(0,-1);});if(cp.length>s.length)el.value=cp;}}

function initRepl(){const el=$('#cmdline');if(el.dataset.on)return;el.dataset.on='1';
 el.addEventListener('keydown',e=>{
  if(e.key==='Enter'){const s=el.value.trim();if(s){runLine(s);if(hist[hist.length-1]!==s)hist.push(s);histIdx=hist.length;}el.value='';}
  else if(e.key==='ArrowUp'){if(hist.length){histIdx=Math.max(0,histIdx-1);el.value=hist[histIdx];e.preventDefault();}}
  else if(e.key==='ArrowDown'){if(histIdx<hist.length){histIdx++;el.value=hist[histIdx]||'';}e.preventDefault();}
  else if(e.key==='Tab'){e.preventDefault();completeLine();}});}

function buildVars(vars){const box=$('#vars');box.innerHTML='';$('#varCard').style.display=vars.length?'':'none';
 vars.forEach(v=>{const r=document.createElement('div');r.className='row';r.dataset.v=v.name;
  let ctl;
  if(v.type==='bool'){ctl=`<input type="checkbox" data-in ${v.value==='1'||v.value===true?'checked':''}>`;}
  else if(v.type==='int'||v.type==='float'){ctl=`<input type="number" step="${v.type==='float'?'any':'1'}" data-in value="${esc(v.value)}">`;}
  else{ctl=`<input type="text" data-in value="${esc(v.value)}">`;}
  r.innerHTML=`<span class="nm">${esc(v.name)}</span>${ctl}<button>Set</button>
   ${v.persist?'<span class="hp">persisted</span>':''}${v.help?`<span class="hp">${esc(v.help)}</span>`:''}`;
  const inp=r.querySelector('[data-in]');
  const set=()=>send({t:'var_set',name:v.name,value:inp.type==='checkbox'?(inp.checked?'1':'0'):inp.value});
  r.querySelector('button').onclick=set; if(inp.type==='checkbox')inp.onchange=set;
  box.appendChild(r);});}

let colDefs={};
function buildColls(cols){colDefs={};const box=$('#cols');box.innerHTML='';$('#colCard').style.display=cols.length?'':'none';
 cols.forEach(c=>{colDefs[c.name]=c;const sec=document.createElement('div');sec.dataset.col=c.name;
  sec.innerHTML=`<div class="row"><span class="nm">${esc(c.name)}</span><button data-add>Add</button>
   ${c.help?`<span class="hp">${esc(c.help)}</span>`:''}</div><div data-form></div><div data-tbl></div>`;
  sec.querySelector('[data-add]').onclick=()=>showForm(c.name,-1,{});
  box.appendChild(sec);loadRecs(c.name);});}

function fieldInput(f,val){const v=val==null?'':val;
 if(f.type==='bool')return `<input type="checkbox" data-fld="${esc(f.name)}" ${v===true||v==='1'?'checked':''}>`;
 if(f.type==='enum'){const opts=(f.options||'').split(',').map(o=>`<option ${o===v?'selected':''}>${esc(o)}</option>`).join('');
  return `<select data-fld="${esc(f.name)}"><option value=""></option>${opts}</select>`;}
 if(f.type==='number')return `<input type="number" step="any" data-fld="${esc(f.name)}" value="${esc(v)}">`;
 return `<input type="text" data-fld="${esc(f.name)}" value="${esc(v)}">`;}

function showForm(col,index,rec){const c=colDefs[col];
 const holder=document.querySelector(`[data-col="${CSS.escape(col)}"] [data-form]`);
 holder.innerHTML=`<div class="card" style="margin:.5rem 0">
  ${c.fields.map(f=>`<div class="row"><span class="nm">${esc(f.label)}${f.required?' *':''}</span>${fieldInput(f,rec[f.name])}</div>`).join('')}
  <div class="row"><button data-save>${index<0?'Create':'Save'}</button><button data-cancel>Cancel</button></div></div>`;
 holder.querySelector('[data-cancel]').onclick=()=>holder.innerHTML='';
 holder.querySelector('[data-save]').onclick=()=>{
  const body=new URLSearchParams();  // record fields only — coll id/index go in the query
  holder.querySelectorAll('[data-fld]').forEach(el=>{const val=el.type==='checkbox'?(el.checked?'1':''):el.value;
   if(val!=='')body.append(el.dataset.fld,val);});
  const q='?name='+encodeURIComponent(col)+(index>=0?'&index='+index:'');
  fetch('/api/collection/'+(index<0?'create':'edit')+q,{method:'POST',headers:{'X-Auth-Token':tok()},body})
   .then(r=>{if(r.status!==202)line('! save failed ('+r.status+')');holder.innerHTML='';});};}

function loadRecs(col){fetch('/api/collection?name='+encodeURIComponent(col)).then(r=>r.json()).then(recs=>{
 const c=colDefs[col];const tbl=document.querySelector(`[data-col="${CSS.escape(col)}"] [data-tbl]`);if(!tbl)return;
 if(!recs.length){tbl.innerHTML='<p class="hp">No records yet.</p>';return;}
 const head=c.fields.map(f=>`<th>${esc(f.label)}</th>`).join('');
 const rows=recs.map((r,i)=>`<tr>${c.fields.map(f=>`<td>${esc(fmtVal(r[f.name]))}</td>`).join('')}
  <td><button data-e="${i}">edit</button> <button data-d="${i}">del</button></td></tr>`).join('');
 tbl.innerHTML=`<div style="overflow-x:auto"><table><tr>${head}<th></th></tr>${rows}</table></div>`;
 tbl.querySelectorAll('[data-e]').forEach(b=>b.onclick=()=>showForm(col,+b.dataset.e,recs[+b.dataset.e]));
 tbl.querySelectorAll('[data-d]').forEach(b=>b.onclick=()=>{if(!confirm('Delete record '+b.dataset.d+'?'))return;
  fetch('/api/collection/delete?name='+encodeURIComponent(col)+'&index='+b.dataset.d,
   {method:'POST',headers:{'X-Auth-Token':tok()}});});}).catch(()=>{});}

function fmtVal(v){return v==null?'':v===true?'✓':String(v);}

function buildUploads(ups){const box=$('#ups');box.innerHTML='';$('#upCard').style.display=ups.length?'':'none';
 ups.forEach(u=>{const r=document.createElement('div');r.className='row';
  r.innerHTML=`<span class="nm">${esc(u.name)}</span><input type="file" data-f><button>Upload</button>
   ${u.help?`<span class="hp">${esc(u.help)}</span>`:''}`;
  const inp=r.querySelector('[data-f]');
  r.querySelector('button').onclick=()=>{const f=inp.files[0];if(!f){line('! pick a file first');return;}
   const fd=new FormData();fd.append('file',f);
   fetch('/api/upload?name='+encodeURIComponent(u.name),{method:'POST',headers:{'X-Auth-Token':tok()},body:fd})
    .then(res=>line(res.status===202?'uploading '+u.name+'…':'upload failed ('+res.status+')'))
    .catch(()=>line('! upload error'));};
  box.appendChild(r);});}

function buildFiles(files){const box=$('#files');box.innerHTML='';$('#fileCard').style.display=files.length?'':'none';
 files.forEach(f=>{const r=document.createElement('div');r.className='row';
  r.innerHTML=`<span class="nm">${esc(f.name)}</span>
   <a href="/api/file?name=${encodeURIComponent(f.name)}"><button type="button">Download</button></a>
   ${f.clear?'<button data-clear>Erase</button>':''}`;
  const cb=r.querySelector('[data-clear]');
  if(cb)cb.onclick=()=>{if(!confirm('Erase '+f.name+'?'))return;
   fetch('/api/file/clear',{method:'POST',headers:{'X-Auth-Token':tok()},body:new URLSearchParams({name:f.name})})
    .then(()=>line('erased '+f.name));};
  box.appendChild(r);});}

function onVar(name,value){const r=document.querySelector(`[data-v="${CSS.escape(name)}"] [data-in]`);
 if(!r)return; if(r.type==='checkbox')r.checked=(value==='1'||value===true||value===1); else r.value=value;}

function connect(){ws=new WebSocket('ws://'+location.host+'/ws');
 ws.onopen=()=>{$('#dot').classList.add('on');line('— connected —');};
 ws.onclose=()=>{$('#dot').classList.remove('on');line('— disconnected, retrying —');setTimeout(connect,1500);};
 ws.onmessage=e=>{let m;try{m=JSON.parse(e.data)}catch(_){return;}
  if(m.t==='hello'){$('#name').textContent=m.name||'Web Console';document.title=m.name||'Web Console';
   $('#token').style.display=m.auth?'':'none';renderNav(m.pages);initRepl();buildCmds(m.commands||[]);buildVars(m.vars||[]);
   buildColls(m.collections||[]);buildUploads(m.uploads||[]);buildFiles(m.files||[]);}
  else if(m.t==='log'){line(m.line);}
  else if(m.t==='cmd_result'){line('» '+m.name+': '+(m.result??''));}
  else if(m.t==='upload_result'){line('» '+m.name+': '+(m.result??''));if(colDefs)Object.keys(colDefs).forEach(loadRecs);}
  else if(m.t==='collection_changed'){if(!m.ok)line('! '+m.name+': '+(m.result??''));loadRecs(m.name);}
  else if(m.t==='var'){onVar(m.name,m.value);}
  else if(m.t==='event'){line('* '+m.name+' '+JSON.stringify(m.data??''));}
  else if(m.t==='error'){line('! '+(m.msg||'error'));}};}
connect();
</script>
)HTMLDELIM";
