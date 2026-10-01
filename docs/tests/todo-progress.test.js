const test = require('node:test');
const assert = require('node:assert/strict');
const { parseStatuses, itemState, lookup, init } = require('../todo-progress.js');

function status(n, passed, total, state, lesson = 2) {
  return { context: `course/lesson-${lesson}/todo-${n}`,
    description: `checks ${passed}/${total}`, state };
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

test('GitHub rate limiting is distinct from a missing branch', async () => {
  mock(response({},403));
  await assert.rejects(lookup('student',1),/rate/);
});
