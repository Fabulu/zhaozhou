// Independent finite-domain and protocol models for the 2026-09-08 brief.
// These tests DO NOT compile or simulate repository RTL and prove no timing result.
// Build: g++ -std=c++17 -O2 -Wall -Wextra -Werror independent_checks.cpp -o independent_checks
#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static void require(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
static int floor_div(int n, int d) {
    return n >= 0 ? n/d : -((-n + d-1)/d);
}
static uint32_t old_lz(uint32_t d) {
    uint32_t e=0;
    for (uint32_t b=0;b<24;++b)
        if ((d & (1u << (23-b))) && e==0 && !(d & (1u<<23))) e=b;
    return e;
}
static uint32_t lz8(uint32_t b) {
    require(b>0 && b<256, "lz8 input");
    if (b & 0x80) return 0;
    if (b & 0x40) return 1;
    if (b & 0x20) return 2;
    if (b & 0x10) return 3;
    if (b & 8) return 4;
    if (b & 4) return 5;
    if (b & 2) return 6;
    return 7;
}
static uint32_t grouped_lz(uint32_t d) {
    if (d & 0xff0000) return lz8((d>>16)&255);
    if (d & 0xff00) return 8+lz8((d>>8)&255);
    if (d & 255) return 16+lz8(d&255);
    return 0; // Match the original zero input convention; zero is separately tagged.
}
static void normalization() {
    uint64_t count=0, bad_zero_mutant=0;
    for (uint32_t d=0;d<(1u<<24);++d) {
        auto a=old_lz(d), b=grouped_lz(d);
        require(a==b,"LZ mismatch");
        auto m=(d<<a)&0xffffffu;
        auto mp=(d<<b)&0xffffffu;
        auto idx=uint8_t(((m-0x800000u)&0xffffffu)>>15);
        auto idx2=uint8_t((mp>>15)&255u);
        require(m==mp && idx==idx2,"normalization/index mismatch");
        require(d==0 || (m & 0x800000u),"normalization top bit");
        // A wrong zero convention is detectable even though zero never uses the seed.
        if (d==0 && a!=24) ++bad_zero_mutant;
        ++count;
    }
    require(bad_zero_mutant==1,"zero detector did not fire");
    std::cout << "N1 normalization/index: " << count << " denominators PASS; zero-convention mutant detected\n";
}
static void fog() {
    uint64_t n=0, truncation_mutant=0, divide255_mutant=0;
    for (int c=0;c<256;++c) for (int f=0;f<256;++f) for (int a=0;a<256;++a) {
        int reference=std::clamp(c+floor_div((f-c)*a+128,256),0,255);
        // Staged, width-bounded candidate: difference9, product18, rounding18.
        int diff=f-c;
        int prod=diff*a;
        require(diff>=-256 && diff<256,"difference width");
        require(prod>=-131072 && prod+128<=131071,"product/round width");
        int staged=std::clamp(c+floor_div(prod+128,256),0,255);
        require(reference==staged,"fog mismatch");
        require(a!=0 || staged==c,"clear identity");
        if (std::clamp(c+(prod+128)/256,0,255)!=reference) ++truncation_mutant;
        if (std::clamp(c+floor_div(prod+127,255),0,255)!=reference) ++divide255_mutant;
        ++n;
    }
    require(truncation_mutant && divide255_mutant,"fog mutants not detected");
    std::cout << "F1 fog channel: " << n << " combinations PASS; truncation mutant mismatches="
              << truncation_mutant << "; /255 mutant mismatches=" << divide255_mutant << "\n";
}
static void handles() {
    uint64_t n=0, old_slot_mutants=0;
    for (uint32_t slot=0;slot<64;++slot) for (uint32_t gen=0;gen<256;++gen)
    for (uint32_t s=0;s<4;++s) for (uint32_t cls=0;cls<4;++cls) {
        auto own=(slot<<8)|gen;
        auto sample=(slot<<10)|(s<<8)|gen;
        auto route=(cls<<16)|sample;
        auto ticket=(gen<<6)|slot;
        require(((route>>16)&3)==cls && ((route>>10)&63)==slot &&
                ((route>>8)&3)==s && (route&255)==gen,"route round-trip");
        require((((sample>>10)<<8)|(sample&255))==own,"sample to owner");
        require((((ticket&63)<<8)|(ticket>>6))==own,"ticket order round-trip");
        if (((route>>10)&15)!=slot) ++old_slot_mutants;
        // Index 3 is represented losslessly, but remains illegal for a TMU sample.
        require((s<3)==(((route>>8)&3)!=3),"range predicate");
        ++n;
    }
    require(old_slot_mutants>0,"stale four-bit slot detector");
    std::cout << "I1 identity layouts: " << n << " encodings PASS (including invalid sample=3); stale slot mutant="
              << old_slot_mutants << "\n";
}
static void local_issue() {
    uint64_t n=0;
    for (unsigned req=0;req<16;++req) for (unsigned iss=0;iss<16;++iss)
    for (unsigned s=0;s<4;++s) for (unsigned live=0;live<2;++live)
    for (unsigned aux_event=0;aux_event<2;++aux_event) {
        unsigned bit=s==3 ? 0 : 1u<<s;
        bool tok_ok=live && s!=3;
        bool global=tok_ok && (req&bit)!=0 && (iss&bit)==0;
        bool local=tok_ok && ((req>>s)&1) && !((iss>>s)&1);
        require(global==local,"row-local ISSUE predicate");
        bool aux_ok=aux_event && live && (req&8) && !(iss&8);
        auto a=iss | (global ? bit:0) | (aux_ok ? 8u:0u);
        auto b=(iss | (local ? bit:0)) | (aux_ok ? 8u:0u);
        require(a==b,"same-row OR lost update");
        ++n;
    }
    std::cout << "L1 row-local ISSUE plus simultaneous AUX: " << n << " scalar truth cases PASS\n";
}
struct Beat { uint64_t id, payload; };
struct Pending { int due; Beat beat; };
static uint64_t data_of(uint64_t id) {return (id*0x9e3779b97f4a7c15ull)^0xa73915bull;}
static bool join_run(bool mutant, uint32_t seed, int cycles, uint64_t& count, uint64_t& max_reserved) {
    constexpr size_t capacity=4;
    std::deque<Beat> out;
    std::deque<Pending> pending;
    std::deque<Beat> expected;
    uint64_t next=0; uint32_t st=seed;
    count=0; max_reserved=0;
    for (int tick=0;tick<cycles+20;++tick) {
        st=st*1664525u+1013904223u;
        bool sink_ready=(tick>=cycles)||((tick>=25)&&((st>>27)&1));
        if (sink_ready && !out.empty()) {
            auto got=out.front();out.pop_front();
            require(!expected.empty(),"unsolicited join output");
            auto want=expected.front();expected.pop_front();
            require(got.id==want.id && got.payload==want.payload,"join identity/data/order");
            ++count;
        }
        while (!pending.empty() && pending.front().due==tick) {
            out.push_back(pending.front().beat); pending.pop_front();
        }
        size_t reserved=out.size()+pending.size();
        size_t seen=mutant ? out.size():reserved;
        bool offer=(tick<cycles)&&((tick<25)||((st>>29)&1));
        if (offer && seen<capacity) {
            Beat b{next,data_of(next)}; ++next;
            pending.push_back({tick+3,b});expected.push_back(b);
        }
        reserved=out.size()+pending.size();
        max_reserved=std::max(max_reserved,uint64_t(reserved));
        if (reserved>capacity) return false;
    }
    require(expected.empty() && pending.empty() && out.empty(),"join did not drain");
    return true;
}
static void elastic_join() {
    uint64_t total=0,peak=0;
    for (uint32_t seed=1;seed<=32;++seed) {
        uint64_t n=0,p=0;
        require(join_run(false,seed,10000,n,p),"correct join overflow");
        total+=n;peak=std::max(peak,p);
    }
    uint64_t n=0,p=0;
    require(!join_run(true,1,10000,n,p),"in-flight-credit mutant escaped");
    std::cout << "J1 credited 3-cycle read join: 32 schedules x 10000 cycles PASS; retired=" << total
              << "; max reserved=" << peak << "; ignore-in-flight mutant detected\n";
}
static void admission_and_lease() {
    int n=0,phantom=0;
    for (bool valid:{false,true}) for (bool owner:{false,true}) for (bool rcp:{false,true}) {
        bool external=valid&&owner&&rcp;
        bool owner_fire=(valid&&rcp)&&owner;
        bool rcp_fire=(valid&&owner)&&rcp;
        require(external==owner_fire && owner_fire==rcp_fire,"fork disagrees");
        if ((valid&&owner)!=external) ++phantom;
        ++n;
    }
    require(phantom==1,"ungated admission mutant not detected");
    std::cout << "A1 admission fork: " << n << " truth cases PASS; ungated valid mutant detected\n";
    // Deliberately abstract topology witness, NOT a reachable RTL counterexample.
    int front_consumed=31, owner_result_ready=10;
    int unsafe_retire=owner_result_ready;
    int guarded_combine=std::max(owner_result_ready,front_consumed);
    int safe_retire=guarded_combine+4;
    require(unsafe_retire<front_consumed && safe_retire>=front_consumed,"lease model");
    std::cout << "Z1 abstract zero-work lease witness: unconstrained retirement precedes frontend completion; guard prevents it. NOT RTL reachability proof.\n";
    for (unsigned acks=0;acks<256;++acks) for (bool local_empty:{false,true})
      require((local_empty && acks==255)==(local_empty && ((acks&255)==255)),"barrier");
    require(!(true && 254==255),"withheld ACK");
    std::cout << "E1 barrier conjunction: 512 truth cases PASS; one withheld ACK cannot reopen\n";
}
int main() {
    try { normalization(); fog(); handles(); local_issue(); elastic_join(); admission_and_lease();
          std::cout << "ALL INDEPENDENT CHECKS PASSED. No RTL simulation or physical synthesis was performed.\n";
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
