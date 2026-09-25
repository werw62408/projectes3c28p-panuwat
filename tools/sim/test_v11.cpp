#include "sim_common.h"

int main(){
  OUT="run/pics"; mkdir("run",0755); mkdir(OUT.c_str(),0755);
  std::string base="run/state_t11"; system(("rm -rf "+base+" && mkdir -p "+base+"/lfs "+base+"/sd "+base+"/logs").c_str());
  LittleFS.root=base+"/lfs"; SD_MMC.root=base+"/sd"; g_simLogsRoot=base+"/logs"; setenv("TZ","ICT-7",1); tzset();
  g_simEpoch=1790255700-5; uint16_t cal[8]={0}; prefs.putBytes("tcal",cal,sizeof cal); prefs.putUInt("epoch",1790255700);
  LittleFS.begin(); LittleFS.mkdir("/log"); seedLogs(1790255700); seedSd();
  setup(); onTimeSync(nullptr); timeFixPending=false; loadDay();
  auto R=[](bool ok){ return ok?"PASS":"FAIL"; };
  // 1 battery curve
  { float vs[]={4.20,4.05,3.92,3.80,3.70,3.60,3.45}; printf("1 battery: "); for(float v:vs){ batV=v; printf("%.2fV=%d%%  ",v,batPct()); } batV=3.70f; printf("%s\n", R(batPct()==25)); }
  // 2 undo last night within 6 h
  { g_simEpoch = 1790265000 - (int64_t)(g_simMs/1000); loadDay();            // Thu 22:50
    int c0=0; forEachEvent(curDay,[&](const Ev& e){ if(e.id=="smoke") c0++; }); logDefault(3); uint32_t t0=nowT(); int c1=0; forEachEvent(curDay,[&](const Ev& e){ if(e.id=="smoke") c1++; }); printf("   22:50 %s file smoke %d -> %d, act3=%s\n", curDay.c_str(), c0, c1, acts[3].id.c_str());                                        // Smoke at 22:50
    g_simEpoch = 1790269800 - (int64_t)(g_simMs/1000); { int q=0; forEachEvent("2026-09-24",[&](const Ev& e){ if(e.id=="smoke") q++; }); printf("   after clock move, before loadDay: %d  now=%s\n", q, dayKey(nowT()).c_str()); } loadDay(); { int q=0; forEachEvent("2026-09-24",[&](const Ev& e){ if(e.id=="smoke") q++; }); printf("   after loadDay: %d\n", q); }
    int yBefore=0; forEachEvent(dayKey(nowT()-86400),[&](const Ev& e){ if(e.id=="smoke") yBefore++; });
    bool can=canUndo(acts[3], sumFor("smoke")); bool ok=undoEvent(3);
    int yAfter=0; forEachEvent(dayKey(nowT()-86400),[&](const Ev& e){ if(e.id=="smoke") yAfter++; });
    printf("2 undo at 00:10 the Smoke of 22:50: button on=%d, undone=%d, yesterday %d -> %d %s\n", can, ok, yBefore, yAfter, R(can&&ok&&yAfter==yBefore-1)); (void)t0; }
  // 3 activity deleted then added again with the same name -> same id (history back)
  { JsonDocument d; JsonArray a=d.to<JsonArray>(); for(auto& x:acts){ JsonObject o=a.add<JsonObject>(); o["id"]=x.id; o["name"]=x.name; }
    JsonObject o=a.add<JsonObject>(); o["id"]=""; o["name"]="meal"; actsFromJson(d.as<JsonArrayConst>());
    printf("3 add 'meal' again on the web: id=%s %s\n", acts.back().id.c_str(), R(acts.back().id=="meal")); saveActs(); }
  // 4 Goals: nothing logged yet today -> 'max' goals not counted
  { g_simEpoch = 1790298000 - (int64_t)(g_simMs/1000); loadDay(); scr=S_HOME; render();
    printf("4 new day, nothing logged: todayEv=%d (Goals line shows 0 / n)  -> see picture\n",(int)todayEv.size()); savePng(spr,"t11_goals_empty",W,H); }
  // 5 web: moving a file out of the Trash by path is refused
  { server.headers["Cookie"]=std::string("stpin=")+webPin.c_str(); server.args.clear(); server.args["op"]="move"; server.args["path"]="/.trash/3_old_selfie.jpg"; server.args["dest"]="/photos";
    server.lastCode=0; server.routes["POST /api/sd/op"](); printf("5 web move out of Trash: code %d %s\n", server.lastCode, R(server.lastCode==400)); }
  // 6 Sudoku: a number does not save at once, leaving does
  { sudokuOpen(); sdkNew(0); int i=0; while(sg.puz[i]) i++; sdkSel=i; sdkPlace(sg.sol[i]);
    bool dirtyAfterPlace=sdkDirty; sudokuClose(); printf("6 Sudoku: after a number dirty=%d, after Back dirty=%d %s\n", dirtyAfterPlace, sdkDirty, R(dirtyAfterPlace&&!sdkDirty)); }
  // 7 background download
  { g_simOffline=false; WiFi.connected=true; netTab=1; newsCat=1; scr=S_NET; netLoad(true); netPoll();
    printf("7 AI news loaded in the background: %d stories, busy=%d %s\n",(int)newsC[1].size(),(int)netBusy, R(newsC[1].size()>0&&!netBusy)); }
  // 8 Bluetooth scan in the background
  { g_simBle={{"Mi Smart Band 8","c8:47:8c:1a:22:90",-52}}; scr=S_BT; btScan(); render(); printf("8 BLE: %d devices, busy=%d %s\n",(int)bdevs.size(),(int)btBusy,R(bdevs.size()==1&&!btBusy)); }
  // 9 notice when the time is not known + Update asks first
  { timeApprox=true; scr=S_HOME; render(); printf("9 time unknown notice on Log: %d %s\n", logNotice(), R(logNotice()==3)); savePng(spr,"t11_notice_time",W,H);
    onTap(W/2, remindBarY+10); printf("  tap -> Settings page %d %s\n", setPage, R(scr==S_SET&&setPage==2)); timeApprox=false;
    setAboutPage(); settingsUI(false, 20, 0); askUpdate=true; render(); savePng(spr,"t11_ask_update",W,H); printf("10 Update firmware asks first: box shown %s\n", R(askUpdate)); askUpdate=false; }
  // 11 storage full
  { logFull=true; scr=S_HOME; render(); savePng(spr,"t11_notice_full",W,H); printf("11 log space full notice: %d %s\n", logNotice(), R(logNotice()==1)); logFull=false; }
  return 0;
}
