/**
 * engine.sim.js — JavaScript simulation of the C++ WASM engine.
 *
 * Mirrors the Embind API exactly:
 *   AlgoEngine.run(algoId, array, extra?) → Step[]   (legacy bridge helper)
 *   AlgoEngine.NAMES                                  (algorithm name list)
 *   AlgoEngine.getHistory()               → HistoryRow[]
 *
 * Step shape (matches C++ JSON output):
 *   { array[], a, b, action, phase, ln }
 *   ln = lineNumber (-1 for non-BubbleSort algorithms)
 *
 * PerformanceLogs row shape (matches Performance_Logs column names):
 *   { id, Run_Timestamp, Algorithm_Name, Dataset_Size,
 *     Total_Steps_Computed, Comparisons_Count, Swaps_Count }
 */

const AlgoEngine = (() => {

  // ── In-memory Performance_Logs (spec column names) ───────────────────────────
  const performanceLogs = [];
  let logId = 1;

  function dbLog(Algorithm_Name, Dataset_Size, Total_Steps_Computed,
                 Comparisons_Count, Swaps_Count) {
    performanceLogs.unshift({
      id: logId++,
      Run_Timestamp: new Date().toISOString().replace('T', ' ').slice(0, 19),
      Algorithm_Name,
      Dataset_Size,
      Total_Steps_Computed,
      Comparisons_Count,
      Swaps_Count,
    });
    if (performanceLogs.length > 50) performanceLogs.pop();
  }

  // ── Step recorder ────────────────────────────────────────────────────────────
  let steps = [];

  // ln = lineNumber for source-code highlight panel (-1 = no highlight)
  function record(arr, a, b, action, phase, ln = -1) {
    steps.push({ array: [...arr], a, b, action, phase, ln });
  }

  // ── BubbleSort — OOP mirror (generates lineNumber tags per spec) ──────────────
  //
  // Line map matches BubbleSort::sort() in engine.cpp exactly:
  //  1 = fn entry      2 = outer for      3 = swapped=false
  //  4 = inner for     5 = if condition   6 = swap()
  //  7 = swapped=true  10 = early-exit   12 = fn exit / done

  function bubbleSortOOP(arr) {
    const n = arr.length;
    record(arr, -1, -1, 'Starting Bubble Sort', 'pivot', 1);

    for (let i = 0; i < n - 1; i++) {
      record(arr, -1, -1,
        `Outer pass ${i} — bubble ceiling at ${n - 1 - i}`, 'pivot', 2);
      let swapped = false;
      record(arr, -1, -1, 'swapped = false', 'pivot', 3);

      for (let j = 0; j < n - i - 1; j++) {
        record(arr, j, j + 1,
          `Comparing arr[${j}]=${arr[j]} vs arr[${j+1}]=${arr[j+1]}`,
          'compare', 4);
        record(arr, j, j + 1,
          `Is arr[${j}] > arr[${j+1}] ?`, 'compare', 5);

        if (arr[j] > arr[j + 1]) {
          [arr[j], arr[j + 1]] = [arr[j + 1], arr[j]];
          record(arr, j, j + 1,
            `Swapped → ${arr[j]} ↔ ${arr[j+1]}`, 'swap', 6);
          swapped = true;
          record(arr, j, j + 1, 'swapped = true', 'swap', 7);
        }
      }

      record(arr, -1, -1,
        `Inner loop done — swapped=${swapped}`, 'pivot', 10);
      if (!swapped) {
        record(arr, -1, -1, 'No swaps — breaking early', 'done', 10);
        break;
      }
    }
    record(arr, -1, -1, 'Array is fully sorted!', 'done', 12);
  }

  // ── Procedural algorithms (ln = -1, no line highlighting) ────────────────────

  function selectionSort(arr) {
    const n = arr.length;
    for (let i = 0; i < n - 1; i++) {
      let minIdx = i;
      record(arr, i, -1, `Seeking minimum from index ${i}`, 'pivot');
      for (let j = i + 1; j < n; j++) {
        record(arr, minIdx, j,
          `Is [${arr[j]}] < current min [${arr[minIdx]}]?`, 'compare');
        if (arr[j] < arr[minIdx]) {
          minIdx = j;
          record(arr, minIdx, -1, `New minimum found: ${arr[minIdx]}`, 'pivot');
        }
      }
      if (minIdx !== i) {
        [arr[i], arr[minIdx]] = [arr[minIdx], arr[i]];
        record(arr, i, minIdx,
          `Placed minimum ${arr[i]} at position ${i}`, 'swap');
      }
    }
    record(arr, -1, -1, 'Array is sorted!', 'done');
  }

  function insertionSort(arr) {
    const n = arr.length;
    for (let i = 1; i < n; i++) {
      const key = arr[i];
      let j = i - 1;
      record(arr, i, -1, `Inserting [${key}] into sorted region`, 'pivot');
      while (j >= 0 && arr[j] > key) {
        record(arr, j, j + 1, `Shifting [${arr[j]}] right`, 'compare');
        arr[j + 1] = arr[j];
        record(arr, j, j + 1, `Shifted → position ${j + 1}`, 'swap');
        j--;
      }
      arr[j + 1] = key;
      record(arr, j + 1, -1, `Inserted [${key}] at position ${j + 1}`, 'swap');
    }
    record(arr, -1, -1, 'Array is sorted!', 'done');
  }

  function mergeSort(arr) {
    function mergeStep(lo, hi) {
      if (lo >= hi) return;
      const mid = (lo + hi) >> 1;
      record(arr, lo, hi, `Split [${lo}..${hi}] at mid=${mid}`, 'pivot');
      mergeStep(lo, mid);
      mergeStep(mid + 1, hi);
      const left  = arr.slice(lo, mid + 1);
      const right = arr.slice(mid + 1, hi + 1);
      let i = 0, j = 0, k = lo;
      while (i < left.length && j < right.length) {
        record(arr, lo + i, mid + 1 + j,
          `Merge: [${left[i]}] vs [${right[j]}]`, 'compare');
        arr[k++] = left[i] <= right[j] ? left[i++] : right[j++];
        record(arr, k - 1, -1, `Placed ${arr[k-1]} at position ${k-1}`, 'merge');
      }
      while (i < left.length)  { arr[k++] = left[i++];  record(arr, k-1, -1, `Drain left: ${arr[k-1]}`,  'merge'); }
      while (j < right.length) { arr[k++] = right[j++]; record(arr, k-1, -1, `Drain right: ${arr[k-1]}`, 'merge'); }
    }
    mergeStep(0, arr.length - 1);
    record(arr, -1, -1, 'Array is sorted!', 'done');
  }

  function quickSort(arr) {
    function partition(lo, hi) {
      const pivot = arr[hi];
      record(arr, hi, -1, `Pivot selected: ${pivot}`, 'pivot');
      let i = lo - 1;
      for (let j = lo; j < hi; j++) {
        record(arr, j, hi, `Compare [${arr[j]}] ≤ pivot [${pivot}]?`, 'compare');
        if (arr[j] <= pivot) {
          i++;
          if (i !== j) {
            [arr[i], arr[j]] = [arr[j], arr[i]];
            record(arr, i, j, `Swap: ${arr[i]} ↔ ${arr[j]}`, 'swap');
          }
        }
      }
      [arr[i + 1], arr[hi]] = [arr[hi], arr[i + 1]];
      record(arr, i + 1, hi,
        `Pivot [${pivot}] placed at position ${i + 1}`, 'swap');
      return i + 1;
    }
    function qs(lo, hi) {
      if (lo >= hi) return;
      const pi = partition(lo, hi);
      qs(lo, pi - 1);
      qs(pi + 1, hi);
    }
    qs(0, arr.length - 1);
    record(arr, -1, -1, 'Array is sorted!', 'done');
  }

  function binarySearch(arr, target) {
    arr.sort((a, b) => a - b);
    record(arr, -1, -1, `Array sorted. Searching for [${target}]`, 'pivot');
    let lo = 0, hi = arr.length - 1;
    while (lo <= hi) {
      const mid = (lo + hi) >> 1;
      record(arr, lo, hi, `Range [${lo}..${hi}] → mid=${mid}`, 'compare');
      record(arr, mid, -1,
        `Checking mid [${arr[mid]}] vs target [${target}]`, 'pivot');
      if (arr[mid] === target) {
        record(arr, mid, -1, `✓ Found [${target}] at index ${mid}!`, 'done');
        return;
      } else if (arr[mid] < target) {
        lo = mid + 1;
        record(arr, mid, -1,
          `[${arr[mid]}] < target → search right half`, 'compare');
      } else {
        hi = mid - 1;
        record(arr, mid, -1,
          `[${arr[mid]}] > target → search left half`, 'compare');
      }
    }
    record(arr, -1, -1, `✗ [${target}] not found in array`, 'done');
  }

  // ── Phase 5: LINEAR DS with structured extra ──────────────────────────────────

  function linkedListOps(arr) {
    const nodes = []; // {val, next}
    let head = -1;
    const disp = [];
    const serial = (active, op) => JSON.stringify({
      type:'ll', head, active, op,
      nodes: nodes.map(n => ({val:n.val, next:n.next}))
    });
    rec(disp,-1,-1,'Linked List — empty','pivot',-1,serial(-1,'init'));
    for (const v of arr) {
      const ni = nodes.length;
      nodes.push({val:v, next:-1}); disp.push(v);
      if (head === -1) { head = ni; }
      else { let c=head; while(nodes[c].next!==-1)c=nodes[c].next; nodes[c].next=ni; }
      rec(disp,ni,-1,`Insert ${v} at tail (node ${ni})`,'insert',-1,serial(ni,'insert'));
    }
    rec(disp,-1,-1,'Traverse head→tail','pivot',-1,serial(-1,'traverse'));
    let c=head,pos=0;
    while(c!==-1){
      rec(disp,pos,-1,`Visit node[${pos}]=${nodes[c].val}`,'visit',-1,serial(c,'visit'));
      c=nodes[c].next; pos++;
    }
    if (head!==-1 && nodes[head].next!==-1) {
      let len=0,cur=head; while(cur!==-1){len++;cur=nodes[cur].next;}
      const tp=Math.floor(len/2);
      let prev=-1; cur=head;
      for(let i=0;i<tp;i++){prev=cur;cur=nodes[cur].next;}
      const delIdx=cur;
      rec(disp,tp,-1,`Delete node at pos ${tp} (val=${nodes[cur].val})`,'delete',-1,serial(delIdx,'delete'));
      if(prev===-1) head=nodes[cur].next; else nodes[prev].next=nodes[cur].next;
      disp.splice(tp,1);
      rec(disp,-1,-1,`${disp.length} nodes remain`,'pivot',-1,serial(delIdx,'after_delete'));
    }
    rec(disp,-1,-1,'Linked List ops complete!','done',-1,serial(-1,'done'));
  }

  function stackOps(arr) {
    const stk = [];
    const serial = op => JSON.stringify({type:'stack', top:stk.length-1, op, items:[...stk]});
    rec(stk,-1,-1,'Stack — LIFO — empty','pivot',-1,serial('init'));
    for (const v of arr) { stk.push(v); rec(stk,stk.length-1,-1,`PUSH ${v}`,'insert',-1,serial('push')); }
    if (stk.length) rec(stk,stk.length-1,-1,`PEEK top=${stk[stk.length-1]}`,'compare',-1,serial('peek'));
    while (stk.length) {
      const top=stk.pop();
      rec(stk,-1,-1,`POP ${top}`,'delete',-1,serial('pop'));
    }
    rec(stk,-1,-1,'Stack empty!','done',-1,serial('empty'));
  }

  function queueOps(arr) {
    const q = [];
    const serial = op => JSON.stringify({type:'queue', front:0, rear:q.length-1, op, items:[...q]});
    rec(q,-1,-1,'Queue — FIFO — empty','pivot',-1,serial('init'));
    for (const v of arr) { q.push(v); rec(q,q.length-1,-1,`ENQUEUE ${v}`,'insert',-1,serial('enqueue')); }
    if (q.length) rec(q,0,-1,`FRONT=${q[0]}`,'compare',-1,serial('front'));
    while (q.length) {
      const front=q.shift();
      rec(q,-1,-1,`DEQUEUE ${front}`,'delete',-1,serial('dequeue'));
    }
    rec(q,-1,-1,'Queue empty!','done',-1,serial('empty'));
  }

  // ── Phase 6: BST with structured extra ───────────────────────────────────────

  function bstInsert(arr) {
    let nextId = 0;
    const nodesMap = {}; // id → {val,id,left,right,active,visited}
    let rootId = null;
    const visited = [];
    const disp = [];

    const serial = (activeId, op) => {
      const nodes = Object.values(nodesMap);
      return JSON.stringify({
        type:'bst', op,
        nodes: nodes.map(n => ({...n, active: n.id===activeId, visited: visited.includes(n.val)})),
        visited: [...visited]
      });
    };

    function ins(nodeId, v) {
      if (nodeId === null) {
        const id = nextId++;
        nodesMap[id] = {val:v, id, left:-1, right:-1};
        rec(disp,-1,-1,`Insert ${v} — create node`,'insert',-1,serial(id,'insert'));
        return id;
      }
      const n = nodesMap[nodeId];
      rec(disp,-1,-1,`[${v}] vs node [${n.val}]`,'compare',-1,serial(nodeId,'compare'));
      if (v < n.val) { rec(disp,-1,-1,`${v} < ${n.val} → left`,'pivot',-1,serial(nodeId,'left')); n.left = ins(n.left===-1?null:n.left, v); }
      else           { rec(disp,-1,-1,`${v} ≥ ${n.val} → right`,'pivot',-1,serial(nodeId,'right')); n.right = ins(n.right===-1?null:n.right, v); }
      return nodeId;
    }

    rec(disp,-1,-1,'BST — empty','pivot',-1,serial(null,'init'));
    for (const v of arr) { disp.push(v); rootId = ins(rootId, v); }

    rec(disp,-1,-1,'Inorder traversal:','pivot',-1,serial(null,'inorder'));
    const order = [];
    function inorder(nid) {
      if (nid === null || nid === -1) return;
      const n = nodesMap[nid];
      inorder(n.left === -1 ? null : n.left);
      order.push(n.val); visited.push(n.val);
      rec(order,order.length-1,-1,`Visit ${n.val}`,'visit',-1,serial(nid,'visit'));
      inorder(n.right === -1 ? null : n.right);
    }
    inorder(rootId);
    rec(order,-1,-1,'Inorder complete ✓','done',-1,serial(null,'done'));
  }

  function bstSearch(arr, target) {
    let nextId=0;
    const nm={};
    let rootId=null;
    function ins(nid,v){
      if(nid===null){nm[nextId]={val:v,id:nextId,left:-1,right:-1,active:false,visited:false};return nextId++;}
      if(v<nm[nid].val)nm[nid].left=ins(nm[nid].left===-1?null:nm[nid].left,v);
      else nm[nid].right=ins(nm[nid].right===-1?null:nm[nid].right,v);
      return nid;
    }
    const disp=[...arr];
    for(const v of arr) rootId=ins(rootId,v);
    const serial=(aid,op)=>JSON.stringify({type:'bst',op,nodes:Object.values(nm).map(n=>({...n,active:n.id===aid,visited:false})),visited:[]});
    rec(disp,-1,-1,`BST built. Search for ${target}`,'pivot',-1,serial(null,'search'));
    let cur=rootId;
    while(cur!==null&&cur!==-1){
      const n=nm[cur];
      rec(disp,-1,-1,`At [${n.val}] — compare with [${target}]`,'compare',-1,serial(cur,'compare'));
      if(n.val===target){rec(disp,-1,-1,`✓ Found ${target}`,'done',-1,serial(cur,'found'));return;}
      else if(target<n.val){rec(disp,-1,-1,`${target} < ${n.val} → left`,'pivot',-1,serial(cur,'left'));cur=n.left===-1?null:n.left;}
      else{rec(disp,-1,-1,`${target} > ${n.val} → right`,'pivot',-1,serial(cur,'right'));cur=n.right===-1?null:n.right;}
    }
    rec(disp,-1,-1,`✗ ${target} not in BST`,'done',-1,serial(null,'notfound'));
  }

  function graphBFS(arr) {
    if(!arr.length){rec(arr,-1,-1,'Empty input','done');return;}
    const g={};
    for(let i=0;i+1<arr.length;i+=2){
      const[u,v]=[arr[i],arr[i+1]];
      (g[u]=g[u]||[]).push(v);(g[v]=g[v]||[]).push(u);
    }
    const visited=[],frontier=[],seen={},q=[arr[0]];
    seen[arr[0]]=true;frontier.push(arr[0]);
    const gj=(cur)=>JSON.stringify({type:'bfs',edges:arr.reduce((a,_,i)=>i%2===0?[...a,[arr[i],arr[i+1]]]:a,[]),visited:[...visited],frontier:[...frontier],current:cur});
    rec(visited,-1,-1,`BFS from ${arr[0]}`,'pivot',-1,gj(arr[0]));
    while(q.length){
      const node=q.shift();
      frontier.splice(frontier.indexOf(node),1);
      visited.push(node);
      rec(visited,visited.length-1,-1,`Visit ${node}`,'visit',-1,gj(node));
      for(const nb of (g[node]||[])) if(!seen[nb]){seen[nb]=true;q.push(nb);frontier.push(nb);rec(visited,-1,-1,`Enqueue ${nb}`,'compare',-1,gj(node));}
    }
    rec(visited,-1,-1,`BFS complete! ${visited.length} nodes`,'done',-1,gj(-1));
  }

  function graphDFS(arr) {
    if(!arr.length){rec(arr,-1,-1,'Empty input','done');return;}
    const g={};
    for(let i=0;i+1<arr.length;i+=2){
      const[u,v]=[arr[i],arr[i+1]];
      (g[u]=g[u]||[]).push(v);(g[v]=g[v]||[]).push(u);
    }
    const visited=[],seen={};
    const gj=(cur)=>JSON.stringify({type:'dfs',edges:arr.reduce((a,_,i)=>i%2===0?[...a,[arr[i],arr[i+1]]]:a,[]),visited:[...visited],frontier:[],current:cur});
    rec(visited,-1,-1,`DFS from ${arr[0]}`,'pivot',-1,gj(arr[0]));
    function dfs(node){
      seen[node]=true;visited.push(node);
      rec(visited,visited.length-1,-1,`DFS visit ${node}`,'visit',-1,gj(node));
      for(const nb of (g[node]||[])) if(!seen[nb]){rec(visited,-1,-1,`Explore ${node}→${nb}`,'pivot',-1,gj(nb));dfs(nb);}
    }
    dfs(arr[0]);
    rec(visited,-1,-1,`DFS complete! ${visited.length} nodes`,'done',-1,gj(-1));
  }

  // ── Remaining algorithms (minHeap, fibDP, lcsDP, hashTable) — unchanged from v3 ──
  // (These use bar chart renderer — no structured extra needed)

  function minHeap(arr) {
    const heap=[];
    function siftUp(){let i=heap.length-1;while(i>0){const p=(i-1)>>1;if(heap[i]<heap[p]){[heap[i],heap[p]]=[heap[p],heap[i]];rec(heap,i,p,'Sift-up swap','swap');i=p;}else break;}}
    function siftDown(){const n=heap.length;let i=0;while(true){let sm=i,l=2*i+1,r=2*i+2;if(l<n&&heap[l]<heap[sm])sm=l;if(r<n&&heap[r]<heap[sm])sm=r;if(sm!==i){[heap[i],heap[sm]]=[heap[sm],heap[i]];rec(heap,i,sm,'Sift-down swap','swap');i=sm;}else break;}}
    rec(heap,-1,-1,'Min-Heap — empty','pivot');
    for(const v of arr){heap.push(v);rec(heap,heap.length-1,-1,`Insert ${v}`,'insert');siftUp();}
    rec(heap,-1,-1,'Extract-min sequence:','pivot');
    const sorted=[];
    while(heap.length){sorted.push(heap[0]);rec(heap,0,-1,`Extract-min: ${heap[0]}`,'delete');heap[0]=heap.pop()||0;if(heap.length)siftDown();}
    rec(sorted,-1,-1,'Sorted asc','done');
  }

  function fibDP(arr) {
    const n=Math.min(arr.length?arr[0]:10,30);
    const dp=Array(n+1).fill(0);if(n>0)dp[1]=1;
    rec(dp,-1,-1,`Fibonacci DP — F(0)..F(${n})`,'pivot');
    rec(dp,0,-1,'F(0)=0','dp');if(n>0)rec(dp,1,-1,'F(1)=1','dp');
    for(let i=2;i<=n;i++){rec(dp,i-1,i-2,`F(${i})=F(${i-1})+F(${i-2})`,'compare');dp[i]=dp[i-1]+dp[i-2];rec(dp,i,-1,`F(${i})=${dp[i]}`,'dp');}
    rec(dp,-1,-1,`F(${n})=${dp[n]}`,'done');
  }

  function lcsDP(arr) {
    const si=arr.indexOf(0);
    const A=si<0?arr:arr.slice(0,si),B=si<0?arr:arr.slice(si+1);
    const m=A.length,n=B.length;
    const dp=Array.from({length:m+1},()=>Array(n+1).fill(0));
    rec(arr,-1,-1,`LCS: A=[${A}] B=[${B}]`,'pivot');
    for(let i=1;i<=m;i++)for(let j=1;j<=n;j++){
      const row=[...dp[i]];
      if(A[i-1]===B[j-1]){dp[i][j]=dp[i-1][j-1]+1;row[j]=dp[i][j];rec(row,j,-1,`MATCH → ${dp[i][j]}`,'dp');}
      else{dp[i][j]=Math.max(dp[i-1][j],dp[i][j-1]);row[j]=dp[i][j];rec(row,j,-1,`no match → ${dp[i][j]}`,'compare');}
    }
    rec(arr,-1,-1,`LCS length=${dp[m][n]}`,'done');
  }

  function hashTable(arr) {
    const B=11,table=Array.from({length:B},()=>[]);
    const display=Array(B).fill(0);
    rec(display,-1,-1,`Hash Table — ${B} buckets`,'pivot');
    for(const v of arr){const bkt=((v%B)+B)%B;rec(display,bkt,-1,`INSERT ${v} → bucket[${bkt}]`,'insert');table[bkt].push(v);display[bkt]=table[bkt].length;rec(display,bkt,-1,`Bucket[${bkt}] = ${display[bkt]} items`,'swap');}
    for(const v of arr){const bkt=((v%B)+B)%B;rec(display,bkt,-1,`SEARCH ${v} → bucket[${bkt}]`,'compare');for(const x of table[bkt]){rec(display,bkt,-1,`Check ${x}==${v}?`,'compare');if(x===v){rec(display,bkt,-1,`✓ Found ${v}`,'found');break;}}}
    rec(display,-1,-1,'Hash Table ops complete!','done');
  }

  function heapSort(arr) {
    const n=arr.length;
    function heapify(sz,i){let lg=i,l=2*i+1,r=2*i+2;if(l<sz&&arr[l]>arr[lg])lg=l;if(r<sz&&arr[r]>arr[lg])lg=r;if(lg!==i){[arr[i],arr[lg]]=[arr[lg],arr[i]];rec(arr,i,lg,'Heap swap','swap');heapify(sz,lg);}}
    rec(arr,-1,-1,'Build max-heap','pivot');
    for(let i=Math.floor(n/2)-1;i>=0;i--)heapify(n,i);
    for(let i=n-1;i>0;i--){[arr[0],arr[i]]=[arr[i],arr[0]];rec(arr,0,i,'Extract max','swap');heapify(i,0);}
    rec(arr,-1,-1,'Heap Sort done!','done');
  }

  function shellSort(arr) {
    const n=arr.length;
    for(let gap=Math.floor(n/2);gap>0;gap=Math.floor(gap/2)){
      rec(arr,-1,-1,`Gap=${gap}`,'pivot');
      for(let i=gap;i<n;i++){const tmp=arr[i];let j=i;while(j>=gap&&arr[j-gap]>tmp){arr[j]=arr[j-gap];j-=gap;rec(arr,j,-1,'Shift','swap');}arr[j]=tmp;rec(arr,j,-1,`Place ${tmp}`,'swap');}
    }
    rec(arr,-1,-1,'Shell Sort done!','done');
  }

  function countingSort(arr) {
    if(!arr.length)return;const mx=Math.max(...arr);const cnt=Array(mx+1).fill(0);
    rec(arr,-1,-1,`Count freq (max=${mx})`,'pivot');
    for(let i=0;i<arr.length;i++){cnt[arr[i]]++;rec(arr,i,-1,`Count[${arr[i]}]=${cnt[arr[i]]}`,'compare');}
    let idx=0;for(let v=0;v<=mx;v++)while(cnt[v]-->0){arr[idx]=v;rec(arr,idx,-1,`Place ${v}`,'swap');idx++;}
    rec(arr,-1,-1,'Counting Sort done!','done');
  }

  function radixSort(arr) {
    if(!arr.length)return;const mx=Math.max(...arr);
    rec(arr,-1,-1,`Radix Sort max=${mx}`,'pivot');
    for(let exp=1;Math.floor(mx/exp)>0;exp*=10){
      const out=Array(arr.length).fill(0),cnt=Array(10).fill(0);
      for(let i=0;i<arr.length;i++){const d=Math.floor(arr[i]/exp)%10;cnt[d]++;rec(arr,i,-1,`Digit ${d}`,'compare');}
      for(let i=1;i<10;i++)cnt[i]+=cnt[i-1];
      for(let i=arr.length-1;i>=0;i--){const d=Math.floor(arr[i]/exp)%10;out[--cnt[d]]=arr[i];}
      for(let i=0;i<arr.length;i++){arr[i]=out[i];rec(arr,i,-1,`Place ${arr[i]}`,'swap');}
    }
    rec(arr,-1,-1,'Radix Sort done!','done');
  }

  function linearSearch(arr,target){
    rec(arr,-1,-1,`Linear Search for [${target}]`,'pivot');
    for(let i=0;i<arr.length;i++){rec(arr,i,-1,`Check arr[${i}]=${arr[i]}`,'compare');if(arr[i]===target){rec(arr,i,-1,`✓ Found at ${i}`,'done');return;}}
    rec(arr,-1,-1,'✗ Not found','done');
  }

  function jumpSearch(arr,target){
    arr.sort((a,b)=>a-b);const n=arr.length,step=Math.floor(Math.sqrt(n));
    rec(arr,-1,-1,`Jump Search step=${step}`,'pivot');
    let prev=0,cur=step;
    while(cur<n&&arr[Math.min(cur,n)-1]<target){rec(arr,prev,Math.min(cur,n)-1,'Jump','compare');prev=cur;cur+=step;if(prev>=n){rec(arr,-1,-1,'✗ Not found','done');return;}}
    while(prev<Math.min(cur,n)){rec(arr,prev,-1,`Check ${arr[prev]}`,'compare');if(arr[prev]===target){rec(arr,prev,-1,`✓ Found at ${prev}`,'done');return;}prev++;}
    rec(arr,-1,-1,'✗ Not found','done');
  }

  // ── Public API ────────────────────────────────────────────────────────────────

  const NAMES = [
    'Bubble Sort','Selection Sort','Insertion Sort','Merge Sort','Quick Sort',
    'Heap Sort','Shell Sort','Counting Sort','Radix Sort',
    'Linear Search','Binary Search','Jump Search',
    'Linked List','Stack','Queue',
    'BST Insert+Inorder','BST Search',
    'Graph BFS','Graph DFS',
    'Min-Heap',
    'Fibonacci DP','LCS DP',
    'Hash Table'
  ];

  const CATEGORY = [
    'sort','sort','sort','sort','sort','sort','sort','sort','sort',
    'search','search','search',
    'ds','ds','ds','tree','tree',
    'graph','graph',
    'heap',
    'dp','dp',
    'hash'
  ];

  function run(algoId, inputArray, extra = 0) {
    steps = [];
    const arr = [...inputArray];
    switch(algoId) {
      case  0: bubbleSortOOP(arr);           break;
      case  1: selectionSort(arr);           break;
      case  2: insertionSort(arr);           break;
      case  3: mergeSort(arr);               break;
      case  4: quickSort(arr);               break;
      case  5: heapSort(arr);                break;
      case  6: shellSort(arr);               break;
      case  7: countingSort(arr);            break;
      case  8: radixSort(arr);               break;
      case  9: linearSearch(arr, extra);     break;
      case 10: binarySearch(arr, extra);     break;
      case 11: jumpSearch(arr, extra);       break;
      case 12: linkedListOps(arr);           break;
      case 13: stackOps(arr);                break;
      case 14: queueOps(arr);                break;
      case 15: bstInsert(arr);               break;
      case 16: bstSearch(arr, extra);        break;
      case 17: graphBFS(arr);                break;
      case 18: graphDFS(arr);                break;
      case 19: minHeap(arr);                 break;
      case 20: fibDP(arr);                   break;
      case 21: lcsDP(arr);                   break;
      case 22: hashTable(arr);               break;
    }
    const cmps  = steps.filter(s => s.phase === 'compare').length;
    const swaps = steps.filter(s => s.phase === 'swap').length;
    dbLog(NAMES[algoId] || 'Unknown', inputArray.length, steps.length, cmps, swaps);
    return steps;
  }

  function getHistory() { return performanceLogs.slice(0, 100); }

  return { run, getHistory, NAMES, CATEGORY };
})();