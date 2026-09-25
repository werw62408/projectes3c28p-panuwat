#include "sim_common.h"

static int countLines(const std::string& root){ std::string cmd="cat "+root+"/log/*.csv 2>/dev/null | wc -l"; FILE*p=popen(cmd.c_str(),"r"); int n=0; fscanf(p,"%d",&n); pclose(p); return n; }
int main(){
  OUT="run/pics"; mkdir("run",0755); mkdir(OUT.c_str(),0755);
  std::string base="run/state_t10"; system(("rm -rf "+base+" && mkdir -p "+base+"/lfs "+base+"/sd "+base+"/logs").c_str());
  LittleFS.root=base+"/lfs"; SD_MMC.root=base+"/sd"; g_simLogsRoot=base+"/logs"; setenv("TZ","ICT-7",1); tzset();
  g_simEpoch=1790255700-5; uint16_t cal[8]={0}; prefs.putBytes("tcal",cal,sizeof cal); prefs.putUInt("epoch",1790255700);
  LittleFS.begin(); LittleFS.mkdir("/log"); seedLogs(1790255700);
  int before=countLines(LittleFS.root);
  // old v9 acts list (with Meal and Snack)
  defaultActs(); { Act m; m.id="meal"; m.name="Meal"; acts.insert(acts.begin()+2,m); Act s2; s2.id="snack"; s2.name="Snack"; acts.insert(acts.begin()+3,s2);} saveActs();
  setup(); onTimeSync(nullptr); timeFixPending=false; loadDay();
  int after=countLines(g_simLogsRoot), left=countLines(LittleFS.root);
  printf("T1 move logs: old area had %d lines -> logs area %d lines, old area left %d  %s\n", before, after, left, (after==before&&left==0)?"PASS":"FAIL");
  { long cnt=0; for(auto&t:totals) cnt+=t.second.count; printf("T1 totals file counts %ld logs  %s\n", cnt, cnt==before?"PASS":"FAIL"); }
  bool hasMeal=false; for(auto&a:acts) if(a.id=="meal"||a.id=="snack") hasMeal=true;
  printf("T7 Meal/Snack removed: %s (acts now:", hasMeal?"FAIL":"PASS"); for(auto&a:acts) printf(" %s",a.name.c_str()); printf(")\n");
  // T2 keypad
  int w0=sumFor("water").count; kpAct=0; kpVal="3"; scr=S_KEYPAD; onTap(W-20,FTR_Y+10);
  printf("T2 typed 3 on Water: %d -> %d  %s\n", w0, sumFor("water").count, sumFor("water").count==w0+3?"PASS":"FAIL");
  // T5 undo keeps totals right
  int tw0=totals["water"].count; logDefault(0); undoEvent(0);
  printf("T5 +1 then -: totals %d -> %d  %s\n", tw0, totals["water"].count, tw0==totals["water"].count?"PASS":"FAIL");
  // T3 guessed time: restart at 07:30 Fri with clock stuck at Thu 23:00
  {
    prefs.putUInt("epoch", 1790265600);   // Thu 24 Sep 23:00
    g_simEpoch = 0; timeApprox=true; guessedEv.clear();
    setClock(1790265600+30, false); timeApprox=true; loadDay();
    int thuBefore=0; forEachEvent("2026-09-24",[&](const Ev&){thuBefore++;});
    logDefault(0); logDefault(1);   // really Fri 07:30
    printf("T3 while guessing: board thinks %s %s, logs went to file %s\n", dayKey(nowT()).c_str(), hhmm(nowT()).c_str(), curDay.c_str());
    server.args["t"]="1790296200"; apiTime();          // phone says Fri 25 Sep 07:30
    loop();
    int thuAfter=0, fri=0; uint32_t ft=0; forEachEvent("2026-09-24",[&](const Ev&){thuAfter++;}); forEachEvent("2026-09-25",[&](const Ev& e){fri++; ft=e.t;});
    printf("T3 after time fix: Thu file %d -> %d lines, Fri file %d lines (last at %s)  %s\n", thuBefore, thuAfter, fri, hhmm(ft).c_str(), (thuAfter==thuBefore && fri==2)?"PASS":"FAIL");
    printf("T3 today now %s, Water today = %d\n", curDay.c_str(), sumFor("water").count);
  }
  // T4 reminder while typing a Wi-Fi password
  {
    g_simEpoch = 1790298000+5*3600 - (int64_t)(g_simMs/1000);  // Fri 13:00
    loadDay(); lastSeen["water"]=1790298000; remindedFor.clear();
    kbdOpen("Password: KKU-WiFi","",nullptr); kbdText="abc";
    g_simMs+=1500; loop();
    printf("T4 reminder: screen stays %s, bar for %s  %s\n", scr==S_KBD?"keyboard":"OTHER", remindAct>=0?acts[remindAct].name.c_str():"-", (scr==S_KBD&&remindAct==0)?"PASS":"FAIL");
    scr=S_APPS; render(); savePng(spr,"remind_bar_apps",W,H);
  }
  // T6 PIN
  {
    auto call=[&](const char* cookie){ server.headers.clear(); if(cookie) server.headers["Cookie"]=cookie; server.lastCode=0; server.routes["GET /api/state"](); return server.lastCode; };
    printf("T6 PIN is %s\n", webPin.c_str());
    int a=call(nullptr), b=call((std::string("stpin=")+webPin.c_str()).c_str());
    int w=0; for(int k=0;k<5;k++) w=call("stpin=0000");
    int c=call((std::string("stpin=")+webPin.c_str()).c_str());
    printf("T6 no PIN -> %d, right PIN -> %d, 5 wrong -> %d, then right PIN -> %d (locked)  %s\n", a,b,w,c,(a==401&&b==200&&c==429)?"PASS":"FAIL");
  }
  printf("log space used: %d%%\n", logUsedPct());
  return 0;
}
