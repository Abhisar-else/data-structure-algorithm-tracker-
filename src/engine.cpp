/**
 * Algorithm Flight Recorder v3 — Complete DSA Engine
 * Emscripten + Embind + SQLite
 *
 * emcc engine.cpp -o ../public/engine.js \
 *   -sWASM=1 -sALLOW_MEMORY_GROWTH=1 -sFORCE_FILESYSTEM=1 \
 *   -sMODULARIZE=1 -sEXPORT_NAME="AlgoEngineWASM" \
 *   -sUSE_SQLITE3=1 --bind -lidbfs.js -O2 -std=c++17
 *
 * Algorithm ID map (used by run_algorithm):
 * ── SORTING ──────────────────────────────────
 *  0  Bubble Sort          O(n²)
 *  1  Selection Sort       O(n²)
 *  2  Insertion Sort       O(n²)
 *  3  Merge Sort           O(n log n)
 *  4  Quick Sort           O(n log n)
 *  5  Heap Sort            O(n log n)
 *  6  Shell Sort           O(n log² n)
 *  7  Counting Sort        O(n+k)
 *  8  Radix Sort           O(nk)
 * ── SEARCHING ────────────────────────────────
 *  9  Linear Search        O(n)
 * 10  Binary Search        O(log n)
 * 11  Jump Search          O(√n)
 * ── LINKED LIST ──────────────────────────────
 * 12  Linked List Insert/Delete/Traverse
 * ── STACK & QUEUE ────────────────────────────
 * 13  Stack (push/pop/peek)
 * 14  Queue (enqueue/dequeue)
 * ── TREES ────────────────────────────────────
 * 15  BST Insert + Inorder
 * 16  BST Search
 * ── GRAPHS ───────────────────────────────────
 * 17  BFS (Breadth-First Search)
 * 18  DFS (Depth-First Search)
 * ── HEAP ─────────────────────────────────────
 * 19  Min-Heap Insert + Extract
 * ── DYNAMIC PROGRAMMING ──────────────────────
 * 20  Fibonacci (memoised)
 * 21  Longest Common Subsequence
 * ── HASHING ──────────────────────────────────
 * 22  Hash Table Insert/Search
 */

#include <emscripten/bind.h>
#include <sqlite3.h>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <cmath>
#include <unordered_map>
#include <queue>
#include <stack>
#include <map>
#include <set>

using namespace emscripten;

// ══════════════════════════════════════════════════════════════════════════════
// STEP & STEPRECORDER
// ══════════════════════════════════════════════════════════════════════════════

struct Step {
    std::vector<int> array;  // primary data snapshot (array / structure state)
    int  indexA    = -1;     // highlighted index A (red)
    int  indexB    = -1;     // highlighted index B (yellow)
    std::string action;      // human-readable description
    std::string phase;       // compare|swap|pivot|merge|insert|delete|visit|found|done|dp
    int  lineNumber = -1;    // source line for code-panel highlight
    std::string extra;       // algorithm-specific extra data (graph edges, DP table, etc.)
};

class StepRecorder {
public:
    using DBHook = std::function<void(const std::string&,int,int,int,int)>;
    explicit StepRecorder(DBHook hook = nullptr) : db_hook_(std::move(hook)) {}

    void clear() { steps_.clear(); }

    void record(const std::vector<int>& arr, int a, int b,
                const std::string& action, const std::string& phase,
                int ln = -1, const std::string& extra = "")
    {
        steps_.push_back({arr, a, b, action, phase, ln, extra});
    }

    int  size()  const { return (int)steps_.size(); }
    bool empty() const { return steps_.empty(); }

    int count_phase(const std::string& p) const {
        int n=0; for(auto& s:steps_) if(s.phase==p) ++n; return n;
    }

    void flush_db(const std::string& algo, int ds) {
        if(db_hook_) db_hook_(algo,ds,size(),count_phase("compare"),count_phase("swap"));
    }

    std::string to_json() const {
        std::ostringstream o;
        o << "[";
        for(int i=0;i<size();++i){
            const Step& s=steps_[i];
            if(i) o << ",";
            o << "{"
              << "\"array\":"    << arr_json(s.array) << ","
              << "\"a\":"        << s.indexA          << ","
              << "\"b\":"        << s.indexB          << ","
              << "\"action\":\"" << esc(s.action)     << "\","
              << "\"phase\":\""  << esc(s.phase)      << "\","
              << "\"ln\":"       << s.lineNumber       << ","
              << "\"extra\":\""  << esc(s.extra)      << "\""
              << "}";
        }
        o << "]";
        return o.str();
    }

    std::string to_pipe() const {
        std::ostringstream o;
        for(const Step& s:steps_){
            for(size_t i=0;i<s.array.size();++i){ if(i) o<<","; o<<s.array[i]; }
            o<<"|"<<s.indexA<<"|"<<s.indexB<<"|"<<s.phase<<"|"<<s.lineNumber<<";";
        }
        return o.str();
    }

private:
    std::vector<Step> steps_;
    DBHook db_hook_;

    static std::string arr_json(const std::vector<int>& a){
        std::ostringstream o; o<<"[";
        for(size_t i=0;i<a.size();++i){if(i)o<<",";o<<a[i];}
        o<<"]"; return o.str();
    }
    static std::string esc(const std::string& s){
        std::string o;
        for(char c:s){
            if(c=='"') o+="\\\"";
            else if(c=='\\') o+="\\\\";
            else if(c=='\n') o+="\\n";
            else o+=c;
        }
        return o;
    }
};

// ══════════════════════════════════════════════════════════════════════════════
// ABSTRACT BASE + OOP ALGORITHMS
// ══════════════════════════════════════════════════════════════════════════════

class Algorithm {
public:
    explicit Algorithm(StepRecorder& r) : R(r) {}
    virtual ~Algorithm() = default;
    virtual void run(std::vector<int>& arr, int extra=0) = 0;
protected:
    StepRecorder& R;
    void rec(const std::vector<int>& a,int i,int j,
             const std::string& msg,const std::string& ph,
             int ln=-1,const std::string& ex=""){
        R.record(a,i,j,msg,ph,ln,ex);
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// SORTING ALGORITHMS
// ──────────────────────────────────────────────────────────────────────────────

class BubbleSort : public Algorithm {
public:
    explicit BubbleSort(StepRecorder& r):Algorithm(r){}
    void run(std::vector<int>& arr,int=0) override {
        int n=arr.size();
        rec(arr,-1,-1,"Start Bubble Sort","pivot",1);
        for(int i=0;i<n-1;++i){
            rec(arr,-1,-1,"Outer pass "+std::to_string(i),"pivot",2);
            bool sw=false;
            rec(arr,-1,-1,"swapped=false","pivot",3);
            for(int j=0;j<n-i-1;++j){
                rec(arr,j,j+1,"Compare arr["+std::to_string(j)+"]="+
                    std::to_string(arr[j])+" vs arr["+std::to_string(j+1)+"]="+
                    std::to_string(arr[j+1]),"compare",4);
                rec(arr,j,j+1,"arr["+std::to_string(j)+"] > arr["+std::to_string(j+1)+"] ?","compare",5);
                if(arr[j]>arr[j+1]){
                    std::swap(arr[j],arr[j+1]);
                    rec(arr,j,j+1,"Swapped "+std::to_string(arr[j])+" ↔ "+std::to_string(arr[j+1]),"swap",6);
                    sw=true;
                    rec(arr,j,j+1,"swapped=true","swap",7);
                }
            }
            rec(arr,-1,-1,"Inner done swapped="+std::string(sw?"true":"false"),"pivot",10);
            if(!sw){rec(arr,-1,-1,"No swaps—early exit","done",10);break;}
        }
        rec(arr,-1,-1,"Array fully sorted!","done",12);
    }
};

class SelectionSort : public Algorithm {
public:
    explicit SelectionSort(StepRecorder& r):Algorithm(r){}
    void run(std::vector<int>& arr,int=0) override {
        int n=arr.size();
        rec(arr,-1,-1,"Start Selection Sort","pivot",1);
        for(int i=0;i<n-1;++i){
            int mn=i;
            rec(arr,i,-1,"Find min from index "+std::to_string(i),"pivot",2);
            rec(arr,i,-1,"Assume arr["+std::to_string(i)+"]="+std::to_string(arr[i])+" is min","pivot",3);
            for(int j=i+1;j<n;++j){
                rec(arr,mn,j,"Is ["+std::to_string(arr[j])+"] < min ["+std::to_string(arr[mn])+"]?","compare",4);
                if(arr[j]<arr[mn]){mn=j;rec(arr,mn,-1,"New min: "+std::to_string(arr[mn])+" at pos "+std::to_string(mn),"pivot",5);}
            }
            if(mn!=i){std::swap(arr[i],arr[mn]);rec(arr,i,mn,"Swap min "+std::to_string(arr[i])+" to pos "+std::to_string(i),"swap",6);}
        }
        rec(arr,-1,-1,"Array sorted!","done",7);
    }
};

class InsertionSort : public Algorithm {
public:
    explicit InsertionSort(StepRecorder& r):Algorithm(r){}
    void run(std::vector<int>& arr,int=0) override {
        int n=arr.size();
        rec(arr,-1,-1,"Start Insertion Sort","pivot",1);
        for(int i=1;i<n;++i){
            int key=arr[i],j=i-1;
            rec(arr,i,-1,"key = arr["+std::to_string(i)+"]="+std::to_string(key),"pivot",2);
            rec(arr,i,-1,"Insert ["+std::to_string(key)+"] into sorted region [0.."+std::to_string(i-1)+"]","pivot",3);
            while(j>=0&&arr[j]>key){
                rec(arr,j,j+1,"arr["+std::to_string(j)+"]="+std::to_string(arr[j])+" > key="+std::to_string(key)+"?","compare",4);
                arr[j+1]=arr[j];
                rec(arr,j,j+1,"Shift ["+std::to_string(arr[j+1])+"] right to pos "+std::to_string(j+1),"swap",5);
                --j;
            }
            arr[j+1]=key;
            rec(arr,j+1,-1,"Place key="+std::to_string(key)+" at pos "+std::to_string(j+1),"swap",6);
        }
        rec(arr,-1,-1,"Array sorted!","done",7);
    }
};

class MergeSort : public Algorithm {
public:
    explicit MergeSort(StepRecorder& r):Algorithm(r){}
    void run(std::vector<int>& arr,int=0) override {
        rec(arr,-1,-1,"Start Merge Sort","pivot",1);
        ms(arr,0,arr.size()-1);
        rec(arr,-1,-1,"Sorted!","done",9);
    }
private:
    void ms(std::vector<int>& a,int l,int r){
        rec(a,l,r,"mergeSort(["+std::to_string(l)+".."+std::to_string(r)+"])","pivot",2);
        if(l>=r){rec(a,l,r,"Base case — single element","pivot",2);return;}
        int m=l+(r-l)/2;
        rec(a,l,r,"mid="+std::to_string(m),"pivot",3);
        rec(a,l,m,"Recurse left ["+std::to_string(l)+".."+std::to_string(m)+"]","pivot",4);
        ms(a,l,m);
        rec(a,m+1,r,"Recurse right ["+std::to_string(m+1)+".."+std::to_string(r)+"]","pivot",5);
        ms(a,m+1,r);
        rec(a,l,r,"Merge ["+std::to_string(l)+".."+std::to_string(m)+"] + ["+std::to_string(m+1)+".."+std::to_string(r)+"]","pivot",6);
        merge(a,l,m,r);
    }
    void merge(std::vector<int>& a,int l,int m,int r){
        std::vector<int> L(a.begin()+l,a.begin()+m+1),R(a.begin()+m+1,a.begin()+r+1);
        int i=0,j=0,k=l;
        while(i<(int)L.size()&&j<(int)R.size()){
            rec(a,l+i,m+1+j,"Compare ["+std::to_string(L[i])+"] vs ["+std::to_string(R[j])+"]","compare",7);
            a[k++]=L[i]<=R[j]?L[i++]:R[j++];
            rec(a,k-1,-1,"Place "+std::to_string(a[k-1])+" at pos "+std::to_string(k-1),"merge",8);
        }
        while(i<(int)L.size()){a[k++]=L[i++];rec(a,k-1,-1,"Drain L: "+std::to_string(a[k-1]),"merge",8);}
        while(j<(int)R.size()){a[k++]=R[j++];rec(a,k-1,-1,"Drain R: "+std::to_string(a[k-1]),"merge",8);}
    }
};

class QuickSort : public Algorithm {
public:
    explicit QuickSort(StepRecorder& r):Algorithm(r){}
    void run(std::vector<int>& arr,int=0) override {
        rec(arr,-1,-1,"Start Quick Sort","pivot",1);
        qs(arr,0,arr.size()-1);
        rec(arr,-1,-1,"Sorted!","done",9);
    }
private:
    int part(std::vector<int>& a,int lo,int hi){
        rec(a,lo,hi,"partition(["+std::to_string(lo)+".."+std::to_string(hi)+"])","pivot",3);
        int pv=a[hi];
        rec(a,hi,-1,"Pivot = arr["+std::to_string(hi)+"]="+std::to_string(pv),"pivot",4);
        int i=lo-1;
        for(int j=lo;j<hi;++j){
            rec(a,j,hi,"["+std::to_string(a[j])+"] ≤ pivot ["+std::to_string(pv)+"]?","compare",5);
            if(a[j]<=pv){
                ++i;
                if(i!=j){std::swap(a[i],a[j]);rec(a,i,j,"Swap "+std::to_string(a[i])+" ↔ "+std::to_string(a[j]),"swap",6);}
            }
        }
        std::swap(a[i+1],a[hi]);
        rec(a,i+1,hi,"Place pivot "+std::to_string(pv)+" at pos "+std::to_string(i+1),"swap",7);
        return i+1;
    }
    void qs(std::vector<int>& a,int lo,int hi){
        rec(a,lo,hi,"quickSort(["+std::to_string(lo)+".."+std::to_string(hi)+"])","pivot",1);
        if(lo>=hi){rec(a,lo,hi,"Base case","pivot",2);return;}
        int p=part(a,lo,hi);
        rec(a,lo,p-1,"Recurse left ["+std::to_string(lo)+".."+std::to_string(p-1)+"]","pivot",8);
        qs(a,lo,p-1);
        rec(a,p+1,hi,"Recurse right ["+std::to_string(p+1)+".."+std::to_string(hi)+"]","pivot",9);
        qs(a,p+1,hi);
    }
};

class HeapSort : public Algorithm {
public:
    explicit HeapSort(StepRecorder& r):Algorithm(r){}
    void run(std::vector<int>& arr,int=0) override {
        int n=arr.size();
        rec(arr,-1,-1,"Build max-heap","pivot");
        for(int i=n/2-1;i>=0;--i) heapify(arr,n,i);
        rec(arr,-1,-1,"Heap built — now extract","pivot");
        for(int i=n-1;i>0;--i){
            rec(arr,0,i,"Extract max "+std::to_string(arr[0])+" → pos "+std::to_string(i),"swap");
            std::swap(arr[0],arr[i]);
            rec(arr,0,i,"Swapped root with pos "+std::to_string(i),"swap");
            heapify(arr,i,0);
        }
        rec(arr,-1,-1,"Heap Sort complete!","done");
    }
private:
    void heapify(std::vector<int>& a,int n,int i){
        int lg=i,l=2*i+1,r=2*i+2;
        if(l<n){rec(a,i,l,"Compare parent["+std::to_string(a[i])+"] vs left["+std::to_string(a[l])+"]","compare");if(a[l]>a[lg])lg=l;}
        if(r<n){rec(a,i,r,"Compare parent["+std::to_string(a[i])+"] vs right["+std::to_string(a[r])+"]","compare");if(a[r]>a[lg])lg=r;}
        if(lg!=i){std::swap(a[i],a[lg]);rec(a,i,lg,"Swap "+std::to_string(a[i])+" ↔ "+std::to_string(a[lg]),"swap");heapify(a,n,lg);}
    }
};

class ShellSort : public Algorithm {
public:
    explicit ShellSort(StepRecorder& r):Algorithm(r){}
    void run(std::vector<int>& arr,int=0) override {
        int n=arr.size();
        for(int gap=n/2;gap>0;gap/=2){
            rec(arr,-1,-1,"Gap = "+std::to_string(gap),"pivot");
            for(int i=gap;i<n;++i){
                int tmp=arr[i],j=i;
                rec(arr,i,-1,"Insert arr["+std::to_string(i)+"]="+std::to_string(tmp)+" with gap="+std::to_string(gap),"pivot");
                while(j>=gap&&arr[j-gap]>tmp){
                    rec(arr,j,j-gap,"Shift ["+std::to_string(arr[j-gap])+"] by gap","compare");
                    arr[j]=arr[j-gap]; j-=gap;
                    rec(arr,j,-1,"Shifted to pos "+std::to_string(j),"swap");
                }
                arr[j]=tmp;
                rec(arr,j,-1,"Placed "+std::to_string(tmp)+" at pos "+std::to_string(j),"swap");
            }
        }
        rec(arr,-1,-1,"Shell Sort complete!","done");
    }
};

class CountingSort : public Algorithm {
public:
    explicit CountingSort(StepRecorder& r):Algorithm(r){}
    void run(std::vector<int>& arr,int=0) override {
        if(arr.empty()) return;
        int mn=*std::min_element(arr.begin(),arr.end());
        int mx=*std::max_element(arr.begin(),arr.end());
        int offset = -mn;  // shift so minimum maps to index 0
        std::vector<int> cnt(mx-mn+1,0);
        rec(arr,-1,-1,"Count frequencies (min="+std::to_string(mn)+", max="+std::to_string(mx)+")","pivot");
        for(int i=0;i<(int)arr.size();++i){
            cnt[arr[i]-mn]++;
            rec(arr,i,-1,"Count["+std::to_string(arr[i])+"]="+std::to_string(cnt[arr[i]-mn]),"compare");
        }
        rec(cnt,-1,-1,"Frequency array built","pivot");
        int idx=0;
        for(int v=0;v<=mx;++v){
            while(cnt[v]-->0){
                arr[idx]=v;
                rec(arr,idx,-1,"Place value "+std::to_string(v)+" at pos "+std::to_string(idx),"swap");
                idx++;
            }
        }
        rec(arr,-1,-1,"Counting Sort complete!","done");
    }
};

class RadixSort : public Algorithm {
public:
    explicit RadixSort(StepRecorder& r):Algorithm(r){}
    void run(std::vector<int>& arr,int=0) override {
        if(arr.empty()) return;
        int mx=*std::max_element(arr.begin(),arr.end());
        rec(arr,-1,-1,"Radix Sort — max value: "+std::to_string(mx),"pivot");
        for(int exp=1;mx/exp>0;exp*=10){
            rec(arr,-1,-1,"Counting sort by digit (exp="+std::to_string(exp)+")","pivot");
            count_by_digit(arr,exp);
        }
        rec(arr,-1,-1,"Radix Sort complete!","done");
    }
private:
    void count_by_digit(std::vector<int>& a,int exp){
        int n=a.size();
        std::vector<int> out(n),cnt(10,0);
        for(int i=0;i<n;++i){int d=(a[i]/exp)%10;cnt[d]++;rec(a,i,-1,"Digit "+std::to_string(d)+" for val "+std::to_string(a[i]),"compare");}
        for(int i=1;i<10;++i) cnt[i]+=cnt[i-1];
        for(int i=n-1;i>=0;--i){int d=(a[i]/exp)%10;out[--cnt[d]]=a[i];}
        for(int i=0;i<n;++i){a[i]=out[i];rec(a,i,-1,"Placed "+std::to_string(a[i])+" at pos "+std::to_string(i),"swap");}
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// SEARCHING ALGORITHMS
// ──────────────────────────────────────────────────────────────────────────────

class LinearSearch : public Algorithm {
public:
    explicit LinearSearch(StepRecorder& r):Algorithm(r){}
    void run(std::vector<int>& arr,int target=0) override {
        rec(arr,-1,-1,"Linear Search for ["+std::to_string(target)+"]","pivot");
        for(int i=0;i<(int)arr.size();++i){
            rec(arr,i,-1,"Check arr["+std::to_string(i)+"]="+std::to_string(arr[i]),"compare");
            if(arr[i]==target){rec(arr,i,-1,"✓ Found ["+std::to_string(target)+"] at index "+std::to_string(i),"done");return;}
        }
        rec(arr,-1,-1,"✗ ["+std::to_string(target)+"] not found","done");
    }
};

class BinarySearch : public Algorithm {
public:
    explicit BinarySearch(StepRecorder& r):Algorithm(r){}
    void run(std::vector<int>& arr,int target=0) override {
        std::sort(arr.begin(),arr.end());
        rec(arr,-1,-1,"Array sorted. Binary Search for ["+std::to_string(target)+"]","pivot",1);
        int lo=0,hi=arr.size()-1;
        rec(arr,lo,hi,"lo="+std::to_string(lo)+" hi="+std::to_string(hi),"pivot",2);
        while(lo<=hi){
            rec(arr,lo,hi,"lo="+std::to_string(lo)+" ≤ hi="+std::to_string(hi)+"? (loop condition)","compare",3);
            int mid=lo+(hi-lo)/2;
            rec(arr,mid,-1,"mid="+std::to_string(mid)+" → arr[mid]="+std::to_string(arr[mid]),"pivot",4);
            rec(arr,mid,-1,"arr["+std::to_string(mid)+"]="+std::to_string(arr[mid])+" vs target="+std::to_string(target),"compare",5);
            if(arr[mid]==target){rec(arr,mid,-1,"✓ Found at index "+std::to_string(mid),"done",6);return;}
            else if(arr[mid]<target){lo=mid+1;rec(arr,lo,hi,"Too small → lo="+std::to_string(lo),"compare",7);}
            else{hi=mid-1;rec(arr,lo,hi,"Too large → hi="+std::to_string(hi),"compare",8);}
        }
        rec(arr,-1,-1,"✗ ["+std::to_string(target)+"] not found","done",9);
    }
};

class JumpSearch : public Algorithm {
public:
    explicit JumpSearch(StepRecorder& r):Algorithm(r){}
    void run(std::vector<int>& arr,int target=0) override {
        std::sort(arr.begin(),arr.end());
        int n=arr.size();
        int step=(int)std::sqrt(n);
        rec(arr,-1,-1,"Jump Search — step size="+std::to_string(step),"pivot");
        int prev=0;
        while(prev<n&&arr[std::min(step,n)-1]<target){
            rec(arr,prev,std::min(step,n)-1,"Jump: arr["+std::to_string(std::min(step,n)-1)+"]="+std::to_string(arr[std::min(step,n)-1])+" < target","compare");
            prev=step; step+=std::sqrt(n);
            if(prev>=n){rec(arr,-1,-1,"✗ Not found","done");return;}
        }
        rec(arr,prev,-1,"Linear scan from index "+std::to_string(prev),"pivot");
        while(prev<std::min(step,n)){
            rec(arr,prev,-1,"Check arr["+std::to_string(prev)+"]="+std::to_string(arr[prev]),"compare");
            if(arr[prev]==target){rec(arr,prev,-1,"✓ Found at index "+std::to_string(prev),"done");return;}
            prev++;
        }
        rec(arr,-1,-1,"✗ Not found","done");
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// LINKED LIST — proper Node struct with next-pointer chain
// extra JSON: {"type":"ll","nodes":[{"val":N,"next":IDX_OR_-1}],"head":IDX,"op":"insert|delete|visit","target":IDX}
// ──────────────────────────────────────────────────────────────────────────────

class LinkedListOps : public Algorithm {
    struct Node { int val; int next; }; // next = index into nodes[], -1 = null
public:
    explicit LinkedListOps(StepRecorder& r):Algorithm(r){}

    void run(std::vector<int>& arr,int=0) override {
        std::vector<Node> nodes; // arena — never shrinks
        int head = -1;
        std::vector<int> disp; // display array = node values in insertion order

        rec(disp,-1,-1,"Linked List — empty (head=null)","pivot",-1,
            serial(nodes,head,-1,"init"));

        // ── Insert at tail ───────────────────────────────────────────────────
        for(int v : arr){
            int newIdx = nodes.size();
            nodes.push_back({v,-1});
            disp.push_back(v);

            if(head == -1){
                head = newIdx;
                rec(disp,newIdx,-1,"Insert "+std::to_string(v)+" — new head","insert",-1,
                    serial(nodes,head,newIdx,"insert"));
            } else {
                // walk to tail
                int cur = head;
                while(nodes[cur].next != -1) cur = nodes[cur].next;
                nodes[cur].next = newIdx;
                rec(disp,newIdx,-1,"Insert "+std::to_string(v)+" at tail (node "+std::to_string(newIdx)+")","insert",-1,
                    serial(nodes,head,newIdx,"insert"));
            }
        }

        // ── Traverse ─────────────────────────────────────────────────────────
        rec(disp,-1,-1,"Traverse head → tail","pivot",-1,serial(nodes,head,-1,"traverse"));
        int cur = head;
        int pos = 0;
        while(cur != -1){
            rec(disp,pos,-1,"Visit node["+std::to_string(pos)+"]="+std::to_string(nodes[cur].val),"visit",-1,
                serial(nodes,head,cur,"visit"));
            cur = nodes[cur].next;
            pos++;
        }

        // ── Delete middle node ────────────────────────────────────────────────
        if(head != -1 && nodes[head].next != -1){
            // find node before mid
            int len = 0; cur = head; while(cur!=-1){len++;cur=nodes[cur].next;}
            int target_pos = len/2;
            int prev = -1; cur = head;
            for(int i=0;i<target_pos;++i){prev=cur;cur=nodes[cur].next;}

            rec(disp,target_pos,-1,"Delete node at pos "+std::to_string(target_pos)+" (val="+std::to_string(nodes[cur].val)+")","delete",-1,
                serial(nodes,head,cur,"delete"));

            if(prev == -1) head = nodes[cur].next;
            else           nodes[prev].next = nodes[cur].next;
            // mark deleted by setting val sentinel (keep in arena for display)
            int del_idx = cur;
            disp.erase(disp.begin()+target_pos);
            rec(disp,-1,-1,"After delete — "+std::to_string(disp.size())+" nodes remain","pivot",-1,
                serial(nodes,head,del_idx,"after_delete"));
        }

        rec(disp,-1,-1,"Linked List ops complete!","done",-1,serial(nodes,head,-1,"done"));
    }

private:
    // Serialise current node arena as structured JSON into the extra field
    std::string serial(const std::vector<Node>& nodes, int head, int active, const std::string& op){
        std::ostringstream o;
        o << "{\"type\":\"ll\","
          << "\"head\":" << head << ","
          << "\"active\":" << active << ","
          << "\"op\":\"" << op << "\","
          << "\"nodes\":[";
        for(int i=0;i<(int)nodes.size();++i){
            if(i) o<<",";
            o<<"{\"val\":"<<nodes[i].val<<",\"next\":"<<nodes[i].next<<"}";
        }
        o<<"]}";
        return o.str();
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// STACK — LIFO, top = back of vector
// extra JSON: {"type":"stack","items":[...],"top":IDX,"op":"push|pop|peek|empty"}
// ──────────────────────────────────────────────────────────────────────────────

class StackOps : public Algorithm {
public:
    explicit StackOps(StepRecorder& r):Algorithm(r){}
    void run(std::vector<int>& arr,int=0) override {
        std::vector<int> stk;
        rec(stk,-1,-1,"Stack — LIFO — empty","pivot",-1,serial(stk,"init"));

        for(int v:arr){
            stk.push_back(v);
            rec(stk,(int)stk.size()-1,-1,"PUSH "+std::to_string(v)+" → top","insert",-1,
                serial(stk,"push"));
        }

        if(!stk.empty())
            rec(stk,(int)stk.size()-1,-1,"PEEK top = "+std::to_string(stk.back()),"compare",-1,
                serial(stk,"peek"));

        while(!stk.empty()){
            int top=stk.back();
            rec(stk,(int)stk.size()-1,-1,"POP "+std::to_string(top)+" from top","delete",-1,
                serial(stk,"pop"));
            stk.pop_back();
            rec(stk,-1,-1,"Stack size → "+std::to_string(stk.size()),"pivot",-1,
                serial(stk,"after_pop"));
        }

        rec(stk,-1,-1,"Stack empty — done!","done",-1,serial(stk,"empty"));
    }
private:
    std::string serial(const std::vector<int>& s, const std::string& op){
        std::ostringstream o;
        o<<"{\"type\":\"stack\","
         <<"\"top\":"<<(int)s.size()-1<<","
         <<"\"op\":\""<<op<<"\","
         <<"\"items\":[";
        for(int i=0;i<(int)s.size();++i){if(i)o<<",";o<<s[i];}
        o<<"]}";
        return o.str();
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// QUEUE — FIFO, front=index 0, rear=back
// extra JSON: {"type":"queue","items":[...],"front":0,"rear":IDX,"op":"enqueue|dequeue|front|empty"}
// ──────────────────────────────────────────────────────────────────────────────

class QueueOps : public Algorithm {
public:
    explicit QueueOps(StepRecorder& r):Algorithm(r){}
    void run(std::vector<int>& arr,int=0) override {
        std::vector<int> q;
        rec(q,-1,-1,"Queue — FIFO — empty","pivot",-1,serial(q,"init"));

        for(int v:arr){
            q.push_back(v);
            rec(q,(int)q.size()-1,-1,"ENQUEUE "+std::to_string(v)+" at rear","insert",-1,
                serial(q,"enqueue"));
        }

        if(!q.empty())
            rec(q,0,-1,"FRONT = "+std::to_string(q.front()),"compare",-1,
                serial(q,"front"));

        while(!q.empty()){
            int front=q.front();
            rec(q,0,-1,"DEQUEUE "+std::to_string(front)+" from front","delete",-1,
                serial(q,"dequeue"));
            q.erase(q.begin());
            rec(q,-1,-1,"Queue size → "+std::to_string(q.size()),"pivot",-1,
                serial(q,"after_dequeue"));
        }

        rec(q,-1,-1,"Queue empty — done!","done",-1,serial(q,"empty"));
    }
private:
    std::string serial(const std::vector<int>& q, const std::string& op){
        std::ostringstream o;
        o<<"{\"type\":\"queue\","
         <<"\"front\":0,"
         <<"\"rear\":"<<(int)q.size()-1<<","
         <<"\"op\":\""<<op<<"\","
         <<"\"items\":[";
        for(int i=0;i<(int)q.size();++i){if(i)o<<",";o<<q[i];}
        o<<"]}";
        return o.str();
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// BST — Insert then inorder traversal (stores level-order in array for display)
// ──────────────────────────────────────────────────────────────────────────────

class BSTOps : public Algorithm {
    struct Node { int val; Node* l=nullptr; Node* r=nullptr; int id=0; };
    int nextId=0;
public:
    explicit BSTOps(StepRecorder& r):Algorithm(r){}

    void run(std::vector<int>& arr,int=0) override {
        Node* root=nullptr;
        std::vector<int> display;
        std::vector<int> visited;
        rec(display,-1,-1,"BST — empty","pivot",-1, treeJson(root,visited,"init"));
        for(int v:arr){
            display.push_back(v);
            root=insert(root,v,display,visited);
        }
        rec(display,-1,-1,"Inorder traversal:","pivot",-1,treeJson(root,visited,"inorder"));
        std::vector<int> order;
        inorder(root,display,order,visited);
        rec(order,-1,-1,"Inorder complete ✓","done",-1,treeJson(root,visited,"done"));
        free_tree(root);
    }
private:
    Node* insert(Node* n,int v,std::vector<int>& d,std::vector<int>& vis){
        if(!n){
            Node* nn=new Node{v,nullptr,nullptr,nextId++};
            rec(d,-1,-1,"Insert "+std::to_string(v)+" — create node","insert",-1,treeJson(nn,vis,"insert",n));
            return nn;
        }
        rec(d,-1,-1,"["+std::to_string(v)+"] vs node ["+std::to_string(n->val)+"]","compare",-1,treeJson(n,vis,"compare"));
        if(v<n->val){ rec(d,-1,-1,std::to_string(v)+" < "+std::to_string(n->val)+" → left","pivot",-1,treeJson(n,vis,"left")); n->l=insert(n->l,v,d,vis); }
        else         { rec(d,-1,-1,std::to_string(v)+" ≥ "+std::to_string(n->val)+" → right","pivot",-1,treeJson(n,vis,"right")); n->r=insert(n->r,v,d,vis); }
        return n;
    }
    void inorder(Node* n,std::vector<int>& d,std::vector<int>& o,std::vector<int>& vis){
        if(!n) return;
        inorder(n->l,d,o,vis);
        o.push_back(n->val); vis.push_back(n->val);
        rec(o,(int)o.size()-1,-1,"Visit "+std::to_string(n->val),"visit",-1,treeJson(n,vis,"visit"));
        inorder(n->r,d,o,vis);
    }
    // Flatten tree into JSON for SVG renderer
    std::string treeJson(Node* root, const std::vector<int>& vis, const std::string& op, Node* cur=nullptr){
        std::ostringstream o;
        o<<"{\"type\":\"bst\",\"op\":\""<<op<<"\",\"nodes\":[";
        bool first=true;
        serNode(root,o,first,vis,cur);
        o<<"],\"visited\":[";
        for(int i=0;i<(int)vis.size();++i){if(i)o<<",";o<<vis[i];}
        o<<"]}";
        return o.str();
    }
    void serNode(Node* n,std::ostringstream& o,bool& first,const std::vector<int>& vis,Node* cur){
        if(!n) return;
        if(!first) o<<","; first=false;
        bool visited=std::find(vis.begin(),vis.end(),n->val)!=vis.end();
        o<<"{\"val\":"<<n->val
         <<",\"id\":"<<n->id
         <<",\"left\":"<<(n->l?n->l->id:-1)
         <<",\"right\":"<<(n->r?n->r->id:-1)
         <<",\"active\":"<<(cur&&cur->val==n->val?"true":"false")
         <<",\"visited\":"<<(visited?"true":"false")
         <<"}";
        serNode(n->l,o,first,vis,cur);
        serNode(n->r,o,first,vis,cur);
    }
    void free_tree(Node* n){ if(!n)return; free_tree(n->l); free_tree(n->r); delete n; }
};

class BSTSearch : public Algorithm {
    struct Node { int val; Node* l=nullptr; Node* r=nullptr; };
public:
    explicit BSTSearch(StepRecorder& r):Algorithm(r){}
    void run(std::vector<int>& arr,int target=0) override {
        Node* root=nullptr;
        std::vector<int> display;
        for(int v:arr){ root=ins(root,v); display.push_back(v); }
        rec(display,-1,-1,"BST built. Search for "+std::to_string(target),"pivot");
        Node* cur=root;
        while(cur){
            rec(display,-1,-1,"At node ["+std::to_string(cur->val)+"] — compare with target ["+std::to_string(target)+"]","compare");
            if(cur->val==target){rec(display,-1,-1,"✓ Found "+std::to_string(target),"done");free_tree(root);return;}
            else if(target<cur->val){rec(display,-1,-1,std::to_string(target)+" < "+std::to_string(cur->val)+" → left","pivot");cur=cur->l;}
            else{rec(display,-1,-1,std::to_string(target)+" > "+std::to_string(cur->val)+" → right","pivot");cur=cur->r;}
        }
        rec(display,-1,-1,"✗ "+std::to_string(target)+" not in BST","done");
        free_tree(root);
    }
private:
    Node* ins(Node* n,int v){ if(!n)return new Node{v}; if(v<n->val)n->l=ins(n->l,v); else n->r=ins(n->r,v); return n; }
    void free_tree(Node* n){ if(!n)return; free_tree(n->l); free_tree(n->r); delete n; }
};

// ──────────────────────────────────────────────────────────────────────────────
// GRAPH BFS / DFS (adjacency list from arr pairs: arr[0]-arr[1], arr[2]-arr[3] …)
// Visualised as the visited-order array
// ──────────────────────────────────────────────────────────────────────────────

static std::map<int,std::vector<int>> build_graph(const std::vector<int>& arr){
    std::map<int,std::vector<int>> g;
    for(int i=0;i+1<(int)arr.size();i+=2){
        g[arr[i]].push_back(arr[i+1]);
        g[arr[i+1]].push_back(arr[i]);
    }
    return g;
}

// Graph extra JSON: {"type":"graph","edges":[[u,v],...],"visited":[...],"queue":[...],"current":N}

static std::string graph_json(const std::string& type,
    const std::vector<int>& arr,
    const std::vector<int>& visited,
    const std::vector<int>& frontier,
    int current)
{
    std::ostringstream o;
    o<<"{\"type\":\""<<type<<"\","
     <<"\"current\":"<<current<<","
     <<"\"edges\":[";
    bool first=true;
    for(int i=0;i+1<(int)arr.size();i+=2){
        if(!first) o<<","; first=false;
        o<<"["<<arr[i]<<","<<arr[i+1]<<"]";
    }
    o<<"],\"visited\":[";
    for(int i=0;i<(int)visited.size();++i){if(i)o<<",";o<<visited[i];}
    o<<"],\"frontier\":[";
    for(int i=0;i<(int)frontier.size();++i){if(i)o<<",";o<<frontier[i];}
    o<<"]}";
    return o.str();
}

class GraphBFS : public Algorithm {
public:
    explicit GraphBFS(StepRecorder& r):Algorithm(r){}
    void run(std::vector<int>& arr,int=0) override {
        if(arr.empty()){rec(arr,-1,-1,"Empty input","done");return;}
        auto g=build_graph(arr);
        std::vector<int> visited_order;
        std::map<int,bool> seen;
        std::queue<int> q;
        std::vector<int> frontier_list;
        int src=arr[0];
        q.push(src); seen[src]=true; frontier_list.push_back(src);
        rec(visited_order,-1,-1,"BFS from node "+std::to_string(src),"pivot",-1,
            graph_json("bfs",arr,visited_order,frontier_list,src));
        while(!q.empty()){
            int node=q.front(); q.pop();
            frontier_list.erase(std::remove(frontier_list.begin(),frontier_list.end(),node),frontier_list.end());
            visited_order.push_back(node);
            rec(visited_order,(int)visited_order.size()-1,-1,"Visit node "+std::to_string(node),"visit",-1,
                graph_json("bfs",arr,visited_order,frontier_list,node));
            for(int nb:g[node]){
                if(!seen[nb]){
                    seen[nb]=true; q.push(nb); frontier_list.push_back(nb);
                    rec(visited_order,-1,-1,"Enqueue neighbour "+std::to_string(nb),"compare",-1,
                        graph_json("bfs",arr,visited_order,frontier_list,node));
                }
            }
        }
        rec(visited_order,-1,-1,"BFS complete! Visited "+std::to_string(visited_order.size())+" nodes","done",-1,
            graph_json("bfs",arr,visited_order,frontier_list,-1));
    }
};

class GraphDFS : public Algorithm {
public:
    explicit GraphDFS(StepRecorder& r):Algorithm(r){}
    void run(std::vector<int>& arr,int=0) override {
        if(arr.empty()){rec(arr,-1,-1,"Empty input","done");return;}
        auto g=build_graph(arr);
        std::vector<int> visited_order;
        std::map<int,bool> seen;
        int src=arr[0];
        rec(visited_order,-1,-1,"DFS from node "+std::to_string(src),"pivot",-1,
            graph_json("dfs",arr,visited_order,{},src));
        dfs(g,src,seen,visited_order,arr);
        rec(visited_order,-1,-1,"DFS complete! Visited "+std::to_string(visited_order.size())+" nodes","done",-1,
            graph_json("dfs",arr,visited_order,{},-1));
    }
private:
    void dfs(std::map<int,std::vector<int>>& g,int node,std::map<int,bool>& seen,
             std::vector<int>& vo,const std::vector<int>& arr){
        seen[node]=true; vo.push_back(node);
        rec(vo,(int)vo.size()-1,-1,"DFS visit "+std::to_string(node),"visit",-1,
            graph_json("dfs",arr,vo,{},node));
        for(int nb:g[node])
            if(!seen[nb]){
                rec(vo,-1,-1,"Explore edge "+std::to_string(node)+"→"+std::to_string(nb),"pivot",-1,
                    graph_json("dfs",arr,vo,{},nb));
                dfs(g,nb,seen,vo,arr);
            }
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// MIN-HEAP — insert all, then extract-min repeatedly
// ──────────────────────────────────────────────────────────────────────────────

class MinHeap : public Algorithm {
public:
    explicit MinHeap(StepRecorder& r):Algorithm(r){}
    void run(std::vector<int>& arr,int=0) override {
        std::vector<int> heap;
        rec(heap,-1,-1,"Min-Heap — start empty","pivot");
        for(int v:arr){
            heap.push_back(v);
            rec(heap,(int)heap.size()-1,-1,"Insert "+std::to_string(v),"insert");
            sift_up(heap);
        }
        rec(heap,-1,-1,"Heap built — extract-min sequence:","pivot");
        std::vector<int> sorted;
        while(!heap.empty()){
            sorted.push_back(heap[0]);
            rec(heap,0,-1,"Extract-min: "+std::to_string(heap[0]),"delete");
            heap[0]=heap.back(); heap.pop_back();
            if(!heap.empty()) sift_down(heap,0);
            rec(heap,-1,-1,"After sift-down","pivot");
        }
        rec(sorted,-1,-1,"Min-Heap extract order (sorted asc)","done");
    }
private:
    void sift_up(std::vector<int>& h){
        int i=h.size()-1;
        while(i>0){
            int p=(i-1)/2;
            rec(h,i,p,"Sift-up: child ["+std::to_string(h[i])+"] vs parent ["+std::to_string(h[p])+"]","compare");
            if(h[i]<h[p]){std::swap(h[i],h[p]);rec(h,i,p,"Sift-up swap","swap");i=p;}
            else break;
        }
    }
    void sift_down(std::vector<int>& h,int i){
        int n=h.size();
        while(true){
            int sm=i,l=2*i+1,r=2*i+2;
            if(l<n){rec(h,i,l,"Sift-down: parent["+std::to_string(h[i])+"] vs left["+std::to_string(h[l])+"]","compare");if(h[l]<h[sm])sm=l;}
            if(r<n){rec(h,i,r,"Sift-down: parent["+std::to_string(h[i])+"] vs right["+std::to_string(h[r])+"]","compare");if(h[r]<h[sm])sm=r;}
            if(sm!=i){std::swap(h[i],h[sm]);rec(h,i,sm,"Sift-down swap","swap");i=sm;}
            else break;
        }
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// DYNAMIC PROGRAMMING
// ──────────────────────────────────────────────────────────────────────────────

class FibDP : public Algorithm {
public:
    explicit FibDP(StepRecorder& r):Algorithm(r){}
    void run(std::vector<int>& arr,int=0) override {
        int n=arr.empty()?10:std::min(arr[0],30);
        std::vector<int> dp(n+1,0);
        dp[0]=0; if(n>0)dp[1]=1;
        rec(dp,-1,-1,"Fibonacci DP — compute F(0)..F("+std::to_string(n)+")","pivot");
        rec(dp,0,-1,"Base: F(0)=0","dp");
        if(n>0)rec(dp,1,-1,"Base: F(1)=1","dp");
        for(int i=2;i<=n;++i){
            rec(dp,i-1,i-2,"F("+std::to_string(i)+") = F("+std::to_string(i-1)+")+F("+std::to_string(i-2)+") = "+std::to_string(dp[i-1])+"+"+std::to_string(dp[i-2]),"compare");
            dp[i]=dp[i-1]+dp[i-2];
            rec(dp,i,-1,"F("+std::to_string(i)+") = "+std::to_string(dp[i]),"dp");
        }
        rec(dp,-1,-1,"Fibonacci table complete! F("+std::to_string(n)+")="+std::to_string(dp[n]),"done");
    }
};

class LCSAlgo : public Algorithm {
public:
    explicit LCSAlgo(StepRecorder& r):Algorithm(r){}
    void run(std::vector<int>& arr,int=0) override {
        // arr = concatenation of two sequences separated by 0
        // e.g. [3,1,4,1,5,0,1,4,2] → seq A=[3,1,4,1,5] seq B=[1,4,2]
        int split=-1;
        for(int i=0;i<(int)arr.size();++i)if(arr[i]==0){split=i;break;}
        std::vector<int> A,B;
        if(split<0){A=arr;B=arr;}
        else{A=std::vector<int>(arr.begin(),arr.begin()+split);B=std::vector<int>(arr.begin()+split+1,arr.end());}
        int m=A.size(),n=B.size();
        std::vector<std::vector<int>> dp(m+1,std::vector<int>(n+1,0));
        rec(arr,-1,-1,"LCS: seq A has "+std::to_string(m)+" elements, seq B has "+std::to_string(n)+" elements","pivot");
        for(int i=1;i<=m;++i){
            for(int j=1;j<=n;++j){
                std::string cmp="dp["+std::to_string(i)+"]["+std::to_string(j)+"]: A["+std::to_string(i-1)+"]="+std::to_string(A[i-1])+" vs B["+std::to_string(j-1)+"]="+std::to_string(B[j-1]);
                if(A[i-1]==B[j-1]){
                    dp[i][j]=dp[i-1][j-1]+1;
                    // flatten DP table row i into display
                    std::vector<int> row(dp[i].begin(),dp[i].end());
                    rec(row,j,-1,cmp+" — MATCH → dp="+std::to_string(dp[i][j]),"dp");
                } else {
                    dp[i][j]=std::max(dp[i-1][j],dp[i][j-1]);
                    std::vector<int> row(dp[i].begin(),dp[i].end());
                    rec(row,j,-1,cmp+" — no match → dp="+std::to_string(dp[i][j]),"compare");
                }
            }
        }
        rec(arr,-1,-1,"LCS length = "+std::to_string(dp[m][n]),"done");
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// HASH TABLE — separate chaining (array shows bucket occupancy counts)
// ──────────────────────────────────────────────────────────────────────────────

class HashTableOps : public Algorithm {
public:
    explicit HashTableOps(StepRecorder& r):Algorithm(r){}
    void run(std::vector<int>& arr,int=0) override {
        const int BUCKETS=11;
        std::vector<std::vector<int>> table(BUCKETS);
        std::vector<int> display(BUCKETS,0); // show bucket sizes
        rec(display,-1,-1,"Hash Table — "+std::to_string(BUCKETS)+" buckets (separate chaining)","pivot");
        for(int v:arr){
            int bkt=((v%BUCKETS)+BUCKETS)%BUCKETS;
            rec(display,bkt,-1,"INSERT "+std::to_string(v)+" → bucket["+std::to_string(bkt)+"] (hash="+std::to_string(v)+"%"+std::to_string(BUCKETS)+")","insert");
            table[bkt].push_back(v);
            display[bkt]=table[bkt].size();
            rec(display,bkt,-1,"Bucket["+std::to_string(bkt)+"] now has "+std::to_string(display[bkt])+" item(s)","swap");
        }
        // Search for each inserted value
        for(int v:arr){
            int bkt=((v%BUCKETS)+BUCKETS)%BUCKETS;
            rec(display,bkt,-1,"SEARCH "+std::to_string(v)+" → bucket["+std::to_string(bkt)+"]","compare");
            for(int x:table[bkt]){
                rec(display,bkt,-1,"Check chain: ["+std::to_string(x)+"] == "+std::to_string(v)+"?","compare");
                if(x==v){rec(display,bkt,-1,"✓ Found "+std::to_string(v),"found");break;}
            }
        }
        rec(display,-1,-1,"Hash Table ops complete!","done");
    }
};

// ══════════════════════════════════════════════════════════════════════════════
// SQLITE
// ══════════════════════════════════════════════════════════════════════════════

static sqlite3* g_db = nullptr;

static void db_init() {
    if(g_db) return;
    if(sqlite3_open("/data/performance.db",&g_db)!=SQLITE_OK) sqlite3_open(":memory:",&g_db);
    sqlite3_exec(g_db,
        "CREATE TABLE IF NOT EXISTS Performance_Logs("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " Run_Timestamp TEXT DEFAULT(datetime('now')),"
        " Algorithm_Name TEXT NOT NULL,"
        " Dataset_Size INTEGER NOT NULL,"
        " Total_Steps_Computed INTEGER NOT NULL,"
        " Comparisons_Count INTEGER NOT NULL,"
        " Swaps_Count INTEGER NOT NULL);",
        nullptr,nullptr,nullptr);
}

static void db_log(const std::string& algo,int size,int steps,int cmps,int swaps){
    db_init();
    sqlite3_stmt* s=nullptr;
    if(sqlite3_prepare_v2(g_db,
        "INSERT INTO Performance_Logs(Algorithm_Name,Dataset_Size,Total_Steps_Computed,Comparisons_Count,Swaps_Count) VALUES(?,?,?,?,?);",
        -1,&s,nullptr)==SQLITE_OK){
        sqlite3_bind_text(s,1,algo.c_str(),-1,SQLITE_STATIC);
        sqlite3_bind_int(s,2,size); sqlite3_bind_int(s,3,steps);
        sqlite3_bind_int(s,4,cmps); sqlite3_bind_int(s,5,swaps);
        sqlite3_step(s); sqlite3_finalize(s);
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// GLOBAL RECORDER + DISPATCH
// ══════════════════════════════════════════════════════════════════════════════

static StepRecorder* g_recorder = nullptr;

static void record(const std::vector<int>& a,int i,int j,
                   const std::string& msg,const std::string& ph,int ln=-1){
    if(g_recorder) g_recorder->record(a,i,j,msg,ph,ln);
}

static int count_phase(const std::string& p){ return g_recorder?g_recorder->count_phase(p):0; }

// ──────────────────────────────────────────────────────────────────────────────
// DIJKSTRA — shortest-path, input = triplets (u, weight, v)
// extra JSON: {"type":"dijkstra","edges":[{u,v,w}...],"visited":[...],"frontier":[{node,dist}...],"dist":{node:val},"current":N,"path":{node:{prev}}}
// ──────────────────────────────────────────────────────────────────────────────

class Dijkstra : public Algorithm {
    struct Edge { int u, v, w; };
    struct FrontierNode { int node, dist; };
public:
    explicit Dijkstra(StepRecorder& r):Algorithm(r){}

    void run(std::vector<int>& arr, int source=0) override {
        if(arr.empty()){rec(arr,-1,-1,"Empty input","done");return;}
        if(arr.size()%3!=0){
            rec(arr,-1,-1,"Dijkstra expects triplets: u weight v","done");return;
        }

        std::vector<Edge> edges;
        std::map<int,std::vector<std::pair<int,int>>> adj; // node -> [(neighbour, weight)]
        std::set<int> nodeSet;

        for(int i=0;i+2<(int)arr.size();i+=3){
            int u=arr[i],w=arr[i+1],v=arr[i+2];
            if(w<0){rec(arr,-1,-1,"Negative edge weights not supported","done");return;}
            edges.push_back({u,v,w});
            adj[u].push_back({v,w});
            nodeSet.insert(u); nodeSet.insert(v);
        }

        std::vector<int> nodeList(nodeSet.begin(),nodeSet.end());
        std::sort(nodeList.begin(),nodeList.end());

        int src = nodeSet.count(source)?source:nodeList[0];

        std::map<int,int>  dist, prev_node;
        for(int n:nodeList){ dist[n]=-1; prev_node[n]=-1; }
        dist[src]=0;

        std::vector<int> visited_order;
        std::vector<FrontierNode> frontier={{src,0}};
        std::set<int> settled;

        auto snap=[&](int current)->std::string{
            std::ostringstream o;
            o<<"{\"type\":\"dijkstra\","
             <<"\"current\":"<<current<<","
             <<"\"edges\":[";
            bool first=true;
            for(auto& e:edges){
                if(!first)o<<","; first=false;
                o<<"{\"u\":"<<e.u<<",\"v\":"<<e.v<<",\"w\":"<<e.w<<"}";
            }
            o<<"],\"visited\":[";
            for(int i=0;i<(int)visited_order.size();++i){if(i)o<<",";o<<visited_order[i];}
            o<<"],\"frontier\":[";
            bool ff=true;
            for(auto& f:frontier){
                if(!ff)o<<","; ff=false;
                o<<"{\"node\":"<<f.node<<",\"dist\":"<<f.dist<<"}";
            }
            o<<"],\"dist\":{";
            bool fd=true;
            for(auto& kv:dist){
                if(!fd)o<<","; fd=false;
                o<<"\""<<kv.first<<"\":"<<kv.second;
            }
            o<<"},\"path\":{";
            bool fp=true;
            for(auto& kv:prev_node){
                if(!fp)o<<","; fp=false;
                o<<"\""<<kv.first<<"\":{\"prev\":"<<kv.second<<"}";
            }
            o<<"}}";
            return o.str();
        };

        std::vector<int> disp=nodeList;
        rec(disp,-1,-1,"Dijkstra from source "+std::to_string(src),"pivot",-1,snap(src));

        while(!frontier.empty()){
            // find min-dist in frontier (priority queue behaviour)
            auto minIt=std::min_element(frontier.begin(),frontier.end(),
                [](const FrontierNode& a,const FrontierNode& b){
                    return a.dist<b.dist||(a.dist==b.dist&&a.node<b.node);
                });
            FrontierNode cur=*minIt;
            frontier.erase(minIt);

            int u=cur.node, du=cur.dist;
            if(settled.count(u)) continue;
            if(du!=dist[u]&&dist[u]!=-1) continue;

            settled.insert(u);
            visited_order.push_back(u);
            rec(disp,-1,-1,"Settle "+std::to_string(u)+" (d="+std::to_string(du)+")","visit",-1,snap(u));

            for(auto& [v,w]:adj[u]){
                if(settled.count(v)) continue;
                int alt=du+w;
                rec(disp,-1,-1,"Relax "+std::to_string(u)+"→"+std::to_string(v)+" via w="+std::to_string(w),"compare",-1,snap(u));
                if(dist[v]==-1||alt<dist[v]){
                    dist[v]=alt;
                    prev_node[v]=u;
                    // remove old frontier entry if exists
                    frontier.erase(std::remove_if(frontier.begin(),frontier.end(),
                        [v](const FrontierNode& f){return f.node==v;}),frontier.end());
                    frontier.push_back({v,alt});
                    rec(disp,-1,-1,"Update dist["+std::to_string(v)+"]="+std::to_string(alt),"pivot",-1,snap(v));
                }
            }
        }

        rec(disp,-1,-1,"Dijkstra complete — "+std::to_string(visited_order.size())+" nodes settled","done",-1,snap(-1));
    }
};

static const char* ALGO_NAMES[] = {
    "Bubble Sort","Selection Sort","Insertion Sort","Merge Sort","Quick Sort",
    "Heap Sort","Shell Sort","Counting Sort","Radix Sort",
    "Linear Search","Binary Search","Jump Search",
    "Linked List","Stack","Queue",
    "BST Insert+Inorder","BST Search",
    "Graph BFS","Graph DFS",
    "Min-Heap",
    "Fibonacci DP","LCS DP",
    "Hash Table",
    "Dijkstra"
};
static const int NUM_ALGOS = 24;

static int run_algorithm_impl(int id,const std::string& data_json,int extra){
    if(!g_recorder){
        g_recorder=new StepRecorder([](const std::string& a,int s,int t,int c,int sw){ db_log(a,s,t,c,sw); });
    }
    g_recorder->clear();

    // Parse input
    std::vector<int> arr;
    std::string nums;
    for(char c:data_json) if(c!='['&&c!=']'&&c!=' ') nums+=c;
    std::istringstream ss(nums); std::string tok;
    while(std::getline(ss,tok,',')){
        if(!tok.empty()) try{ arr.push_back(std::stoi(tok)); }catch(...){}
    }
    if(arr.empty()&&id<12) return 0; // searches/sorts need data

    std::string name=(id>=0&&id<NUM_ALGOS)?ALGO_NAMES[id]:"Unknown";

    switch(id){
        case  0:{ BubbleSort   a(*g_recorder); a.run(arr,extra); break; }
        case  1:{ SelectionSort a(*g_recorder); a.run(arr,extra); break; }
        case  2:{ InsertionSort a(*g_recorder); a.run(arr,extra); break; }
        case  3:{ MergeSort    a(*g_recorder); a.run(arr,extra); break; }
        case  4:{ QuickSort    a(*g_recorder); a.run(arr,extra); break; }
        case  5:{ HeapSort     a(*g_recorder); a.run(arr,extra); break; }
        case  6:{ ShellSort    a(*g_recorder); a.run(arr,extra); break; }
        case  7:{ CountingSort a(*g_recorder); a.run(arr,extra); break; }
        case  8:{ RadixSort    a(*g_recorder); a.run(arr,extra); break; }
        case  9:{ LinearSearch a(*g_recorder); a.run(arr,extra); break; }
        case 10:{ BinarySearch a(*g_recorder); a.run(arr,extra); break; }
        case 11:{ JumpSearch   a(*g_recorder); a.run(arr,extra); break; }
        case 12:{ LinkedListOps a(*g_recorder); a.run(arr,extra); break; }
        case 13:{ StackOps     a(*g_recorder); a.run(arr,extra); break; }
        case 14:{ QueueOps     a(*g_recorder); a.run(arr,extra); break; }
        case 15:{ BSTOps       a(*g_recorder); a.run(arr,extra); break; }
        case 16:{ BSTSearch    a(*g_recorder); a.run(arr,extra); break; }
        case 17:{ GraphBFS     a(*g_recorder); a.run(arr,extra); break; }
        case 18:{ GraphDFS     a(*g_recorder); a.run(arr,extra); break; }
        case 19:{ MinHeap      a(*g_recorder); a.run(arr,extra); break; }
        case 20:{ FibDP        a(*g_recorder); a.run(arr,extra); break; }
        case 21:{ LCSAlgo      a(*g_recorder); a.run(arr,extra); break; }
        case 22:{ HashTableOps a(*g_recorder); a.run(arr,extra); break; }
        case 23:{ Dijkstra     a(*g_recorder); a.run(arr,extra); break; }
        default: break;
    }

    g_recorder->flush_db(name,(int)arr.size());
    return g_recorder->size();
}

static std::string get_steps_json_impl()  { return g_recorder?g_recorder->to_json():"[]"; }
static std::string get_steps_pipe_impl()  { return g_recorder?g_recorder->to_pipe():""; }

static std::string get_history_json_impl(){
    db_init();
    std::ostringstream o; o<<"[";
    auto cb=[](void* ud,int,char** v,char**)->int{
        auto* o=(std::ostringstream*)ud;
        if(o->tellp()>1) *o<<",";
        *o<<"{"
           <<"\"id\":"<<(v[0]?v[0]:"0")<<","
           <<"\"Run_Timestamp\":\""<<(v[1]?v[1]:"")<<"\","
           <<"\"Algorithm_Name\":\""<<(v[2]?v[2]:"")<<"\","
           <<"\"Dataset_Size\":"<<(v[3]?v[3]:"0")<<","
           <<"\"Total_Steps_Computed\":"<<(v[4]?v[4]:"0")<<","
           <<"\"Comparisons_Count\":"<<(v[5]?v[5]:"0")<<","
           <<"\"Swaps_Count\":"<<(v[6]?v[6]:"0")
           <<"}";
        return 0;
    };
    sqlite3_exec(g_db,
        "SELECT id,Run_Timestamp,Algorithm_Name,Dataset_Size,"
        "Total_Steps_Computed,Comparisons_Count,Swaps_Count "
        "FROM Performance_Logs ORDER BY id DESC LIMIT 100;",
        cb,&o,nullptr);
    o<<"]"; return o.str();
}

static void db_sync_impl(){
    EM_ASM(if(typeof FS!=='undefined'&&typeof FS.syncfs==='function')FS.syncfs(false,function(e){if(e)console.warn('[AlgoEngine] sync err',e);}););
}

EMSCRIPTEN_BINDINGS(algo_engine){
    function("run_algorithm",    &run_algorithm_impl);
    function("get_steps_json",   &get_steps_json_impl);
    function("get_steps_pipe",   &get_steps_pipe_impl);
    function("get_history_json", &get_history_json_impl);
    function("db_sync",          &db_sync_impl);
}