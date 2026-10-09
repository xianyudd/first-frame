const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { parseStatuses, parseIntegration, itemState, lookup, paintLesson, taskName, cached, storeResult, init } = require('../todo-progress.js');

function status(n, passed, total, state, lesson = 2) {
  return { context: `course/lesson-${lesson}${lesson === 4 ? '/v2' : ''}/todo-${n}`,
    description: `checks ${passed}/${total}`, state, target_url: runUrl };
}
function response(value, code = 200) {
  return { status: code, ok: code >= 200 && code < 300, json: async () => value };
}
function mock(branch, runs, statuses) {
  const requests = [];
  global.fetch = async (url) => {
    requests.push(url);
    if (url.includes('/branches/')) return branch;
    if (url.includes('/runs?')) return runs;
    return statuses;
  };
  return requests;
}
const sha = 'a'.repeat(40);
const runUrl = 'https://github.com/student/first-frame/actions/runs/123';
function l4Report() { return Array.from({length:13},(_,i)=>status(i+1,12,12,'success',4)); }
function integration(passed=3,total=3,state='success') {
  return {context:'course/lesson-4/v2/integration',description:`checks ${passed}/${total}`,state,target_url:runUrl};
}
function completed(conclusion='success') {
  return response({workflow_runs:[{head_sha:sha,status:'completed',conclusion,html_url:runUrl}]});
}

test('parse newest matching statuses and preserve partial, failed, and unknown items', () => {
  const found = parseStatuses({ statuses: [
    status(1, 1, 1, 'success'), status(1, 0, 1, 'failure'),
    status(2, 2, 6, 'failure'), status(3, 0, 3, 'failure'),
    status(4, 2, 2, 'failure'), status(5, 1, 1, 'success', 1),
    {context:'course/lesson-2/todo-6',description:'grading unavailable',state:'error'}
  ]}, 2);
  assert.deepEqual(found, {1:'pass',2:'partial',3:'fail',4:'unavailable',6:'unavailable'});
  assert.equal(itemState({state:'partial',items:found},5),'unknown');
  assert.equal(itemState({state:'cached',previous:{items:found}},1),'cached');
});

test('unsubmitted branch has no inherited pass', async () => {
  const calls = mock(response({},404));
  assert.deepEqual(await lookup('student',1),{state:'none'});
  assert.equal(calls.length,1);
});

test('a completed run for an older commit cannot certify the newest commit', async () => {
  const calls = mock(response({commit:{sha}}),response({workflow_runs:[{head_sha:'b'.repeat(40),status:'completed',conclusion:'success'}]}));
  assert.deepEqual(await lookup('student',1),{state:'waiting',sha});
  assert.equal(calls.length,2);
});

test('pending run does not inherit statuses from previous rerun', async () => {
  const calls = mock(response({commit:{sha}}),response({workflow_runs:[{head_sha:sha,status:'in_progress',html_url:'https://github.com/example/run'}]}));
  assert.equal((await lookup('student',2)).state,'run');
  assert.equal(calls.length,2);
});

test('matching completed run exposes exact TODO outcomes only', async () => {
  const list = [1,2,3,4,5,6].map(n=>status(n,n===2 ? 1 : 0,3,'failure'));
  const calls = mock(response({commit:{sha}}),response({workflow_runs:[{head_sha:sha,status:'completed',conclusion:'failure'}]}),response({statuses:list}));
  const result = await lookup('student',2);
  assert.equal(result.state,'partial');
  assert.equal(result.items[2],'partial');
  assert.equal(result.items[1],'fail');
  assert.equal(calls.length,3);
});

test('old workflow with no published statuses remains unknown', async () => {
  mock(response({commit:{sha}}),response({workflow_runs:[{head_sha:sha,status:'completed',conclusion:'success'}]}),response({statuses:[]}));
  assert.equal((await lookup('student',1)).state,'unknown');
});

test('all published error statuses report unavailable, not zero passed', async () => {
  const list = [1,2,3,4].map(n=>({context:`course/lesson-1/todo-${n}`,description:'grading unavailable',state:'error'}));
  mock(response({commit:{sha}}),response({workflow_runs:[{head_sha:sha,status:'completed',conclusion:'failure'}]}),response({statuses:list}));
  const result = await lookup('student',1);
  assert.equal(result.state,'unavailable');
  assert.deepEqual(Object.values(result.items),Array(4).fill('unavailable'));
});

test('clearing the account discards an in-flight old result', async () => {
  const saved = new Map([['rogue-hb',JSON.stringify({ghUser:'student'})]]);
  global.localStorage = {getItem:key=>saved.get(key) || null,setItem:(key,value)=>saved.set(key,value)};
  const listeners = {};
  global.addEventListener = (event,fn)=>{listeners[event]=fn;};
  const count = {textContent:''};
  global.document = {
    body:{getAttribute:()=> '1'},hidden:false,
    addEventListener:()=>{},
    getElementById:id=> id === 'pcount' ? count : null,
    querySelectorAll:()=>[],querySelector:()=>null
  };
  let resolveBranch;
  global.fetch = url => url.includes('/branches/') ? new Promise(resolve=>{resolveBranch=resolve;}) :
    Promise.resolve(url.includes('/runs?') ? response({workflow_runs:[{head_sha:sha,status:'completed',conclusion:'success'}]}) :
      response({statuses:[1,2,3,4].map(n=>status(n,1,1,'success',1))}));
  init();
  assert.equal(count.textContent,'0 / 4');
  saved.set('rogue-hb',JSON.stringify({ghUser:''}));
  listeners.storage({key:'rogue-hb'});
  resolveBranch(response({commit:{sha}}));
  await new Promise(resolve=>setImmediate(resolve));
  assert.equal(count.textContent,'0 / 4');
  assert.equal(saved.has('rogue-todo-cache'),false);
  delete global.document; delete global.localStorage; delete global.addEventListener;
});


test('lesson 3 accepts nine automatic tasks, excludes manual task ten, and preserves blockers', () => {
  const list = [1,2,3,4,5,8,9].map(n=>status(n,1,1,'success',3));
  list.push({context:'course/lesson-3/todo-6',description:'blocked by task 2',state:'error'},
    status(7,0,0,'success',3),status(10,1,1,'success',3));
  const found = parseStatuses({statuses:list},3);
  assert.equal(found[9],'pass');
  assert.equal(found[6],'blocked');
  assert.equal(found[7],'unavailable');
  assert.equal(found[10],undefined);
});

test('lesson 3 cannot pass with only six statuses; nine passes still require manual task ten', async () => {
  const run = response({workflow_runs:[{head_sha:sha,status:'completed',conclusion:'success'}]});
  mock(response({commit:{sha}}),run,response({statuses:[1,2,3,4,5,6].map(n=>status(n,1,1,'success',3))}));
  assert.equal((await lookup('student',3)).state,'partial');
  mock(response({commit:{sha}}),run,response({statuses:[1,2,3,4,5,6,7,8,9].map(n=>status(n,1,1,'success',3))}));
  assert.equal((await lookup('student',3)).state,'pass');
});

test('lesson 4 requires thirteen v2 tasks and independent integration, including multi-digit IDs', async () => {
  const list = l4Report().concat(integration());
  const found = parseStatuses({statuses:list},4);
  assert.equal(Object.keys(found).length,13);
  for (let n=1;n<=13;n++) assert.equal(found[n],'pass');
  mock(response({commit:{sha}}),completed(),response({statuses:list}));
  const result = await lookup('student',4);
  assert.equal(result.state,'pass');
  assert.equal(result.integration,'pass');
});

test('old five-group greens never map to v2 tasks', async () => {
  const list = Array.from({length:5},(_,i)=>({context:`course/lesson-4/todo-${i+1}`,description:'checks 1/1',state:'success'}));
  assert.deepEqual(Object.values(parseStatuses({statuses:list},4)),Array(13).fill('unavailable'));
  mock(response({commit:{sha}}),completed(),response({statuses:list}));
  assert.equal((await lookup('student',4)).state,'unavailable');
});

test('integration failure preserves local scores but never course green', async () => {
  const list = l4Report().concat(integration(1,3,'failure'));
  mock(response({commit:{sha}}),completed('failure'),response({statuses:list}));
  const result = await lookup('student',4);
  assert.equal(result.state,'partial');
  assert.equal(result.integration,'partial');
  assert.deepEqual(Object.values(result.items),Array(13).fill('pass'));
  // A failed latest run cannot be hidden by even all-green published statuses.
  mock(response({commit:{sha}}),completed('failure'),response({statuses:l4Report().concat(integration())}));
  assert.equal((await lookup('student',4)).state,'partial');
});

test('missing or invalid integration invalidates the whole report', async () => {
  const cases = [null,integration(0,0),integration(4,3),integration(1,101,'failure'),
    {...integration(),state:'failure'}, {...integration(),description:'source check 3/3'},
    {...integration(),target_url:'https://github.com/student/first-frame/actions/runs/122'}];
  for (const entry of cases) {
    const list = l4Report().concat(entry ? [entry] : []);
    mock(response({commit:{sha}}),completed(),response({statuses:list}));
    const result = await lookup('student',4);
    assert.equal(result.state,'unavailable');
    assert.equal(result.integration,'unavailable');
    assert.deepEqual(Object.values(result.items),Array(13).fill('unavailable'));
  }
});

test('lesson 4 malformed, unavailable or missing tasks invalidate every item', async () => {
  const cases = [null,status(10,0,0,'success',4),status(10,2,1,'success',4),status(10,1,101,'failure',4),
    {...status(10,1,1,'success',4),description:'grading unavailable',state:'error'},
    {...status(10,1,1,'success',4),description:'checks -1/1'},
    {...status(10,1,1,'success',4),description:'checks 1/1 trailing'},
    {...status(10,1,1,'success',4),description:'source check 1/1'},
    {...status(10,1,1,'success',4),target_url:'https://github.com/student/first-frame/actions/runs/122'}];
  for (const entry of cases) {
    const list = l4Report().filter(s=>!s.context.endsWith('todo-10')).concat(entry ? [entry] : []).concat(integration());
    assert.deepEqual(Object.values(parseStatuses({statuses:list},4,runUrl)),Array(13).fill('unavailable'));
    mock(response({commit:{sha}}),completed(),response({statuses:list}));
    assert.equal((await lookup('student',4)).state,'unavailable');
  }
});

test('lesson 4 blockers require declared dependencies with non-green predecessor', () => {
  const list = l4Report();
  list[7] = status(8,11,12,'failure',4);
  list[8] = {...status(9,0,0,'error',4),description:'blocked by task 8'};
  list[9] = status(10,0,12,'failure',4);
  list[12] = {...status(13,0,0,'error',4),description:'blocked by task 10'};
  const found = parseStatuses({statuses:list},4);
  assert.equal(found[8],'partial');
  assert.equal(found[9],'blocked');
  assert.equal(found[10],'fail');
  assert.equal(found[13],'blocked');
  for (const change of [
    {...list[8],description:'blocked by task 10'},
    {...list[8],description:'blocked by task 08'},
    {...list[8],state:'failure'}
  ]) {
    const bad = list.slice(); bad[8] = change;
    assert.deepEqual(Object.values(parseStatuses({statuses:bad},4)),Array(13).fill('unavailable'));
  }
  const bad = list.slice(); bad[9] = status(10,12,12,'success',4);
  assert.deepEqual(Object.values(parseStatuses({statuses:bad},4)),Array(13).fill('unavailable'));
});

test('newest v2 context wins and previous runs cannot certify latest run', async () => {
  const list = l4Report().concat(integration());
  list.unshift({...status(10,1,1,'success',4),target_url:'https://github.com/student/first-frame/actions/runs/122'});
  mock(response({commit:{sha}}),completed(),response({statuses:list}));
  assert.equal((await lookup('student',4)).state,'unavailable');
  const newest = l4Report(); newest[9]=status(10,1,12,'failure',4);
  assert.equal(parseStatuses({statuses:newest.concat(l4Report())},4)[10],'partial');
  assert.equal(parseIntegration({statuses:[integration(0,3,'failure'),integration()]}),'fail');
});

test('lesson 4 older SHA cannot certify the newest commit', async () => {
  const calls = mock(response({commit:{sha}}),response({workflow_runs:[{head_sha:'b'.repeat(40),status:'completed',conclusion:'success'}]}));
  assert.deepEqual(await lookup('student',4),{state:'waiting',sha});
  assert.equal(calls.length,2);
});

test('GitHub rate limiting is distinct from a missing branch', async () => {
  mock(response({},403));
  await assert.rejects(lookup('student',1),/rate/);
});

test('L4 cache is version-isolated while old lesson cache remains compatible', () => {
  const saved = new Map([['rogue-todo-cache',JSON.stringify({
    'student/4':{at:Date.now(),result:{state:'pass',items:{1:'pass',2:'pass',3:'pass',4:'pass',5:'pass'},sha}},
    'student/3':{at:Date.now(),result:{state:'pass',items:{1:'pass'},sha}}
  })]]);
  global.localStorage={getItem:key=>saved.get(key)||null,setItem:(key,value)=>saved.set(key,value)};
  try {
    assert.equal(cached('student',4),null);
    assert.equal(cached('student',3).result.state,'pass');
    const result={state:'pass',items:parseStatuses({statuses:l4Report()},4),integration:'pass',sha};
    storeResult('Student',4,result);
    assert.deepEqual(cached('student',4).result,result);
    const data=JSON.parse(saved.get('rogue-todo-cache'));
    assert.equal(data['student/4'].result.items[6],undefined);
    assert.ok(data['student/4/L4-v2']);
  } finally { delete global.localStorage; }
});

function element(text='') {
  return {textContent:text,className:'',attributes:{},style:{},classList:{contains:()=>false},
    setAttribute(name,value){this.attributes[name]=value;}};
}
function l4Dom() {
  const dots=Array.from({length:13},()=>element('?'));
  const labels=Array.from({length:13},()=>element());
  const nav=Array.from({length:6},(_,i)=>element(`章节 ${i+1}`));
  const elements={pcount:element(),pfill:element(),'badge-4':element(),'todo-feedback':element()};
  const bar=element(), integrated=[element(),element()];
  const selectors=[];
  global.document={
    body:{getAttribute:()=> '4'},
    getElementById:id=>elements[id]||null,
    querySelectorAll(selector){
      let m=selector.match(/^\[data-todo="4-(\d+)"\]$/);
      if(m) return [dots[Number(m[1])-1]];
      m=selector.match(/^\[data-todo-label="4-(\d+)"\]$/);
      if(m) return [labels[Number(m[1])-1]];
      if(selector==='[data-todo-integration="4"]') return integrated;
      return [];
    },
    querySelector(selector){
      selectors.push(selector);
      if(selector==='[role="progressbar"]') return bar;
      const m=selector.match(/^\.nav a\[data-lv="(\d+)"\]$/);
      return m ? nav[Number(m[1])-1]||null : null;
    }
  };
  return {dots,labels,nav,elements,bar,integrated,selectors};
}

test('L4 DOM renders thirteen original TODO names and six explicit chapter mappings', () => {
  const dom=l4Dom();
  try {
    const items=parseStatuses({statuses:l4Report()},4);
    items[3]='fail'; items[8]='partial'; items[9]='blocked'; items[13]='fail';
    assert.equal(paintLesson(4,{state:'partial',items,integration:'fail'}),9);
    const names=['01','02-A','02-B','02-C','03-A','03-B','04-A','04-B','05-A','05-B','05-C','05-D','06'];
    names.forEach((name,i)=>{
      assert.equal(taskName(4,i+1),name);
      assert.match(dom.dots[i].attributes['aria-label'],new RegExp(`TODO ${name}：`));
    });
    assert.match(dom.nav[1].attributes['aria-label'],/TODO 02-A：通过；TODO 02-B：未通过；TODO 02-C：通过/);
    assert.match(dom.nav[3].attributes['aria-label'],/TODO 04-A：通过；TODO 04-B：部分通过/);
    assert.match(dom.nav[4].attributes['aria-label'],/TODO 05-A：前置任务未通过/);
    assert.match(dom.nav[5].attributes['aria-label'],/TODO 06：未通过/);
    assert.ok(dom.selectors.every(s=>!s.includes('data-lv="7"')&&!s.includes('data-lv="13"')));
    assert.equal(dom.elements.pcount.textContent,'9 / 13');
    assert.equal(dom.bar.attributes['aria-valuemax'],'13');
    assert.equal(dom.bar.attributes['aria-valuenow'],'9');
    assert.equal(dom.labels[12].textContent,'未通过');
    assert.deepEqual(dom.integrated.map(e=>e.textContent),['整帧回归：未通过','整帧回归：未通过']);
    assert.doesNotMatch(dom.elements['badge-4'].textContent,/人工验收/);
  } finally { delete global.document; }
});

test('integration failure with thirteen local passes renders no green badge', () => {
  const dom=l4Dom();
  try {
    paintLesson(4,{state:'partial',items:parseStatuses({statuses:l4Report()},4),integration:'fail'});
    assert.equal(dom.elements.pcount.textContent,'13 / 13');
    assert.equal(dom.elements['badge-4'].className,'badge fail');
    assert.match(dom.elements['badge-4'].textContent,/整帧未通过/);
    assert.match(dom.elements['todo-feedback'].textContent,/不算课程全通过/);
    paintLesson(4,{state:'cached',previous:{state:'pass',integration:'pass'}});
    assert.deepEqual(dom.dots.map(e=>e.textContent),Array(13).fill('?'));
    assert.match(dom.integrated[0].textContent,/上次记录/);
  } finally { delete global.document; }
});

test('completed L4 run needs a verifiable run URL and missing status response is unavailable', async () => {
  mock(response({commit:{sha}}),response({workflow_runs:[{head_sha:sha,status:'completed',conclusion:'success'}]}));
  const noUrl=await lookup('student',4);
  assert.equal(noUrl.state,'unavailable');
  assert.equal(noUrl.integration,'unavailable');
  mock(response({commit:{sha}}),completed(),response({},404));
  const missing=await lookup('student',4);
  assert.equal(missing.state,'unavailable');
  assert.deepEqual(Object.values(missing.items),Array(13).fill('unavailable'));
});

test('actual HTML exposes thirteen dots and integration hooks without confusing chapter IDs', () => {
  for(const file of ['index.html','lesson-4/index.html']) {
    const html=fs.readFileSync(path.join(__dirname,'..',file),'utf8');
    const ids=[...html.matchAll(/\bdata-todo="4-(\d+)"/g)].map(m=>Number(m[1]));
    assert.deepEqual(ids.sort((a,b)=>a-b),Array.from({length:13},(_,i)=>i+1),file);
    assert.match(html,/data-todo-integration="4"/);
    assert.match(html,/id="badge-4"/);
    if(file.startsWith('lesson-4')) {
      assert.match(html,/<body[^>]*data-lesson="4"/);
      const chapters=[...html.matchAll(/<a\b[^>]*data-lv="(\d+)"/g)].map(m=>Number(m[1]));
      assert.deepEqual(chapters,[1,2,3,4,5,6]);
      assert.deepEqual([...html.matchAll(/data-todo-label="4-(\d+)"/g)].map(m=>Number(m[1])).sort((a,b)=>a-b),Array.from({length:13},(_,i)=>i+1));
      assert.match(html,/aria-valuemax="13"/);
      for(const id of ['pcount','pfill','todo-feedback']) assert.ok(html.includes(`id="${id}"`),id);
    }
  }
});

test('old L1-L3 DOM count and visual/manual notes are retained', () => {
  for(const lesson of [1,2,3]) {
    const total={1:4,2:6,3:9}[lesson];
    const badge=element(),count=element(),note=element(),bar=element(),dot=element();
    global.document={body:{getAttribute:()=>String(lesson)},getElementById:id=>id===`badge-${lesson}`?badge:id==='pcount'?count:id==='todo-feedback'?note:null,
      querySelectorAll:s=>s===`[data-todo="${lesson}-1"]`?[dot]:[],querySelector:s=>s==='[role="progressbar"]'?bar:null};
    try {
      const items={}; for(let n=1;n<=total;n++)items[n]='pass';
      assert.equal(paintLesson(lesson,{state:'pass',items}),total);
      assert.equal(count.textContent,`${total} / ${total}`);
      assert.equal(badge.className,'badge pass');
      if(lesson===2) assert.match(dot.title,/仅源码检查/);
      if(lesson===3) assert.match(badge.textContent,/10 人工验收/);
    } finally {delete global.document;}
  }
});
