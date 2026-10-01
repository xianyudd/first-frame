(function (root) {
  "use strict";
  var REPO = "first-frame", KEY = "rogue-hb", CACHE = "rogue-todo-cache";
  var NAME = /^[a-z\d](?:[a-z\d]|-(?=[a-z\d])){0,38}$/i;
  var TASK_COUNTS = {1:4, 2:6, 3:9}; // Automatic groups only; L3 task 10 is manual.
  function taskCount(lesson) { return TASK_COUNTS[lesson] || 0; }
  var LABEL = {blocked:"前置任务未通过（未执行）", idle:"未连接", none:"尚未提交", waiting:"等待本次评测", run:"评测中", pass:"通过", partial:"部分通过", fail:"未通过", unknown:"暂无逐项结果", unavailable:"评测不可用", rate:"GitHub 查询限流", network:"读取失败", cached:"上次记录（未核实最新）"};
  var POLL = 90000, MAX_POLL = 4, STALE = 180000;

  function parseStatuses(data, lesson) {
    var result = {}, seen = {};
    var statuses = data && Array.isArray(data.statuses) ? data.statuses : [];
    statuses.forEach(function (s) {
      var m = typeof s.context === "string" && s.context.match(/^course\/lesson-(\d+)\/todo-(\d+)$/);
      if (!m || Number(m[1]) !== lesson || Number(m[2]) < 1 || Number(m[2]) > taskCount(lesson)) return;
      var id = Number(m[2]);
      if (seen[id]) return; // GitHub returns the newest status for each context first.
      seen[id] = true;
      var blocker = typeof s.description === "string" && s.description.match(/^blocked by task ([1-9])$/);
      if (lesson === 3 && s.state === "error" && blocker && Number(blocker[1]) !== id) {
        result[id] = "blocked"; return;
      }
      var count = typeof s.description === "string" && s.description.match(/^(?:checks|source check) (\d+)\/(\d+)$/);
      if (s.state === "error" || !count) { result[id] = "unavailable"; return; }
      var passed = Number(count[1]), total = Number(count[2]);
      if (total < 1 || total > 100 || passed > total ||
          (s.state !== "success" && s.state !== "failure") ||
          (s.state === "success") !== (passed === total)) {
        result[id] = "unavailable";
      } else result[id] = passed === total ? "pass" : passed ? "partial" : "fail";
    });
    return result;
  }
  function normalize(raw) { return String(raw || "").trim().replace(/^@+/, ""); }
  function read(key) { try { return JSON.parse(localStorage.getItem(key)) || {}; } catch (e) { return {}; } }
  function write(key, value) { try { localStorage.setItem(key, JSON.stringify(value)); } catch (e) {} }
  function api(path) {
    return fetch("https://api.github.com/repos/" + path, {headers:{Accept:"application/vnd.github+json"}}).then(function (res) {
      if (res.status === 403 || res.status === 429) throw new Error("rate");
      if (res.status === 404) throw new Error("missing");
      if (!res.ok) throw new Error("network");
      return res.json();
    });
  }
  async function lookup(user, lesson) {
    var path = encodeURIComponent(user) + "/" + REPO;
    var branch = "my-lesson-" + lesson, head;
    try { head = await api(path + "/branches/" + branch); }
    catch (e) { if (e.message === "missing") return {state:"none"}; throw e; }
    var sha = head && head.commit && head.commit.sha;
    if (!/^[0-9a-f]{40}$/i.test(sha || "")) return {state:"unknown"};
    var runs;
    try { runs = await api(path + "/actions/workflows/grade.yml/runs?event=push&branch=" + branch + "&per_page=5"); }
    catch (e) { if (e.message === "missing") return {state:"unknown", sha:sha}; throw e; }
    var latest = runs && Array.isArray(runs.workflow_runs) && runs.workflow_runs[0];
    if (!latest || latest.head_sha !== sha) return {state:"waiting", sha:sha};
    var url = latest.html_url;
    if (latest.status !== "completed") return {state:"run", sha:sha, url:url};
    if (latest.conclusion !== "success" && latest.conclusion !== "failure") return {state:"unavailable", sha:sha, url:url};
    var data;
    try { data = await api(path + "/commits/" + sha + "/status?per_page=100"); }
    catch (e) { if (e.message === "missing") return {state:"unknown", sha:sha, url:url}; throw e; }
    var items = parseStatuses(data, lesson), total = taskCount(lesson);
    var complete = 0;
    for (var id=1; id<=total; id++) if (items[id] === "pass") complete++;
    return {state:!Object.keys(items).length ? "unknown" :
      Object.keys(items).length === total && Object.values(items).every(function(s){return s === "unavailable";}) ? "unavailable" :
      complete === total ? "pass" : "partial", sha:sha, url:url, items:items};
  }
  function itemState(result, id) {
    if (result.state === "cached") return "cached";
    if (result.items) return result.items[id] || "unknown";
    if (result.state === "unavailable") return "unavailable";
    return result.state === "pass" || result.state === "partial" ? "unknown" : result.state;
  }
  function paintDot(el, state, lesson, id) {
    el.className = (el.classList.contains("dot") ? "dot " : "todo-dot ") + "todo-" + state;
    el.textContent = state === "pass" ? "✓" : state === "fail" ? "×" : state === "partial" ? "◐" : state === "run" || state === "waiting" ? "…" : "?";
    var name = "第 " + lesson + " 课 TODO " + id + "：" + LABEL[state];
    if (lesson === 2 && (id === 1 || id === 5) && state === "pass") name += "（仅源码检查；请运行游戏确认画面）";
    el.title = name;
    el.setAttribute("aria-label", name);
  }
  function paintLesson(lesson, result) {
    var total = taskCount(lesson), done = 0;
    for (var id=1; id<=total; id++) {
      var state = itemState(result,id);
      if (state === "pass") done++;
      document.querySelectorAll('[data-todo="' + lesson + '-' + id + '"]').forEach(function(el){paintDot(el,state,lesson,id);});
      var nav = document.querySelector('.nav a[data-lv="' + id + '"]');
      if (nav) {
        var label = nav.textContent.trim();
        nav.setAttribute("aria-label", label + "，" + LABEL[state]);
      }
      var info = document.querySelector('[data-todo-label="' + lesson + '-' + id + '"]');
      if (info) info.textContent = LABEL[state] + (lesson === 2 && (id === 1 || id === 5) && state === "pass" ? " · 请运行游戏确认画面" : "");
    }
    var badge = document.getElementById("badge-" + lesson);
    if (badge) {
      badge.className = "badge " + (result.state === "pass" ? "pass" : result.state === "run" ? "run" : result.state === "partial" ? "fail" : "idle");
      badge.textContent = result.state === "pass" ? "✓ " + done + " / " + total + " 项通过" : result.state === "partial" ? done + " / " + total + " 项通过" : LABEL[result.state];
      if (lesson === 3) badge.textContent += " · 10 人工验收";
    }
    if (document.body.getAttribute("data-lesson") === String(lesson)) {
      var count = document.getElementById("pcount"), fill = document.getElementById("pfill"), bar = document.querySelector('[role="progressbar"]');
      if (count) count.textContent = done + " / " + total;
      if (fill) fill.style.transform = "scaleX(" + done / total + ")";
      if (bar) {
        bar.setAttribute("aria-valuenow", String(done));
        bar.setAttribute("aria-valuetext", "本次自动检查通过 " + done + " 项，共 " + total + " 项；" + LABEL[result.state]);
      }
      var note = document.getElementById("todo-feedback");
      if (note) note.textContent = result.state === "idle" ? "在首页输入 GitHub 用户名，push 练习分支后自动追踪。" :
        result.state === "cached" ? "这是上次记录；当前无法核实最新提交，请到首页刷新。" :
        result.state === "none" ? "尚无公开练习分支；提交并 push 后才会显示结果。" :
        result.state === "unknown" ? "暂无逐项结果；已有旧工作流的复刻需要更新 grade.yml。" :
        "自动检查：" + LABEL[result.state] + "。" + (lesson === 2 ? "①⑤仅检查绘制源码，实际外观请运行 make run。" : "") ;
      if (note && lesson === 3) note.textContent += " 仅统计 01–09；BLOCKED 表示未执行，0/0 不是通过。10 请演示自己的案例并解释，画面与声音仍需实际运行检查。";
    }
    return done;
  }
  function cached(user, lesson) {
    var c = read(CACHE), entry = c[user.toLowerCase() + "/" + lesson];
    return entry && entry.result && Date.now() - entry.at < 86400000 ? entry : null;
  }
  function storeResult(user, lesson, result) {
    if (result.items && result.sha) {
      var c=read(CACHE); c[user.toLowerCase() + "/" + lesson]={at:Date.now(),result:result}; write(CACHE,c);
    }
  }
  function init() {
    var active = 0, timer = null, last = 0, polls = MAX_POLL, currentUser = "", pending = false;
    var form=document.getElementById("tk-form"), input=document.getElementById("tk-user"), clear=document.getElementById("tk-clear"), msg=document.getElementById("tk-msg"), runs=document.getElementById("tk-runs");
    var page = Number(document.body.getAttribute("data-lesson"));
    var lessons = page ? [page] : Object.keys(TASK_COUNTS).map(Number);
    function stop() { active++; pending=false; if (timer) clearTimeout(timer); timer=null; }
    function show(user, results) {
      lessons.forEach(function(n,i){paintLesson(n,results[i]);});
      if (msg) {
        var done = results.reduce(function(acc,r){return acc + (r.items ? Object.values(r.items).filter(function(x){return x === "pass";}).length : 0);},0);
        // The summary counts only automatic groups, never manual acceptance.
        var limit = lessons.reduce(function (acc,n){return acc + taskCount(n);},0);
        msg.className="tk-msg";
        msg.textContent=results.some(function(r){return r.state === "cached";}) ? "网络或 GitHub 限流：只显示上次记录，未核实最新提交。稍后刷新。" :
          results.some(function(r){return r.state === "rate";}) ? "GitHub 查询已限流，请过几分钟再刷新；当前结果未知。" :
          results.some(function(r){return r.state === "network";}) ? "连接 GitHub 失败，请检查网络后刷新；当前结果未知。" :
          results.some(function(r){return r.state === "run" || r.state === "waiting";}) ? "正在等待最新提交的评测；稍后自动刷新。" :
          results.every(function(r){return r.state === "none";}) ? "找不到公开练习分支；请确认已复刻并 push my-lesson-N。" :
          "已自动检查通过 " + done + " / " + limit + " 项。灰色表示暂无可信结果，不是失败。";
        if (results.some(function(r){return r.state === "cached" || r.state === "rate" || r.state === "network";})) msg.className="tk-msg warn";
      }
      if (runs) {
        runs.replaceChildren();
        results.forEach(function(r,i){
          if (!/^https:\/\/github\.com\//.test(r.url || "")) return;
          var a=document.createElement("a"); a.href=r.url; a.target="_blank"; a.rel="noopener";
          a.textContent="第 " + lessons[i] + " 课评测详情";
          if (runs.childNodes.length) runs.appendChild(document.createTextNode(" · "));
          runs.appendChild(a);
        });
        runs.hidden=!runs.childNodes.length;
      }
    }
    function query(user) {
      stop(); pending=true; var turn=active;
      if (form) {document.getElementById("tk-go").disabled=true; document.getElementById("tk-go").textContent="查询中…";}
      return Promise.all(lessons.map(function(n){
        return lookup(user,n).catch(function(e){
          var c=cached(user,n);
          return c ? {state:"cached", previous:c.result, url:c.result.url} : {state:e.message === "rate" ? "rate" : "network"};
        });
      })).then(function(results){
        if (turn !== active) return;
        pending=false;
        results.forEach(function(r,i){storeResult(user,lessons[i],r);});
        last=Date.now(); show(user,results);
        if (form) {document.getElementById("tk-go").disabled=false; document.getElementById("tk-go").textContent="刷新";}
        if (results.some(function(r){return r.state === "run" || r.state === "waiting" || (r.state === "unknown" && r.sha);}) && polls-- > 0 && !document.hidden) {
          timer=setTimeout(function(){query(user);},POLL);
        }
      });
    }
    function activate(raw) {
      var user=normalize(raw); stop(); polls=MAX_POLL;
      if (!user || !NAME.test(user)) {
        if (msg) {msg.className="tk-msg warn"; msg.textContent="请输入有效的 GitHub 用户名。";}
        return;
      }
      currentUser=user; last=0;
      var store=read(KEY); store.ghUser=user; write(KEY,store);
      if (input) input.value=user;
      if (clear) clear.hidden=false;
      show(user,lessons.map(function(n){var c=cached(user,n); return c ? {state:"cached",previous:c.result} : {state:"idle"};}));
      query(user);
    }
    if (form) {
      form.addEventListener("submit",function(e){e.preventDefault(); activate(input.value);});
      clear.addEventListener("click",function(){
        stop(); currentUser=""; last=0; var s=read(KEY); s.ghUser=""; write(KEY,s);
        input.value=""; clear.hidden=true; document.getElementById("tk-go").disabled=false;
        document.getElementById("tk-go").textContent="查看进度";
        if (msg) {msg.className="tk-msg"; msg.textContent="";}
        if (runs) {runs.replaceChildren(); runs.hidden=true;}
        lessons.forEach(function(n){paintLesson(n,{state:"idle"});});
      });
    }
    function sync() {
      var user=normalize(read(KEY).ghUser);
      if (!NAME.test(user)) {stop(); currentUser=""; last=0; if(input) input.value=""; if(clear) clear.hidden=true; lessons.forEach(function(n){paintLesson(n,{state:"idle"});}); return;}
      if (input && input.value !== user) input.value=user;
      if (clear) clear.hidden=false;
      if (user !== currentUser || (!pending && (!last || Date.now()-last > STALE))) activate(user);
    }
    document.addEventListener("visibilitychange",function(){if(!document.hidden) sync();});
    root.addEventListener("storage",function(e){if(e.key === KEY || e.key === null) sync();});
    root.addEventListener("pageshow",sync);
    var initial=normalize(read(KEY).ghUser);
    if (NAME.test(initial)) activate(initial);
    else lessons.forEach(function(n){paintLesson(n,{state:"idle"});});
  }
  if (typeof module !== "undefined" && module.exports) module.exports={parseStatuses:parseStatuses, itemState:itemState, lookup:lookup, init:init};
  else if (root.document) init();
})(typeof window !== "undefined" ? window : globalThis);
